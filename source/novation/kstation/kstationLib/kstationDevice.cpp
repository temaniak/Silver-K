#include "kstationDevice.h"
#include "kstationHardware.h"
#include "kstationQueue.h"
#include "kstationRomLoader.h"
#include "baseLib/workerWakeup.h"

#include "dsp56kBase/threadtools.h"
#include "dsp56kBase/audioworkgroup.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace kstation
{
    namespace
    {
        constexpr size_t DefaultLeadFrames = 1024;
        constexpr size_t MinimumLeadFrames = 256;
        constexpr size_t LeadMarginFrames = 128;
        constexpr uint8_t StateHeader[]{'K','S','T','S','T',1,0,0};
        constexpr size_t UserFlashStart = 0x1fc00;
        constexpr size_t UserFlashSize = ControllerFlashSize - UserFlashStart;
        constexpr size_t StateSize = sizeof(StateHeader) + UserFlashSize + 128 + 16 * 128;
        constexpr uint8_t ProgramHeader[]{0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};
        struct MidiByte { uint64_t due; uint8_t value; bool first; };
        struct InputSample { uint64_t due; int32_t value; };
        struct TimedAudioFrame { uint64_t due; DSP::AudioFrame frame; };
        int32_t inputWord(float sample)
        {
            if (!std::isfinite(sample)) return 0;
            return static_cast<int32_t>(std::max(-1.0f, std::min(sample, 0.99999988f)) * 8388608.0f);
        }
    }

    struct Device::Impl
    {
        baseLib::performance::Diagnostics* performance;
        baseLib::WorkerWakeup wakeup;
        uint64_t diagnosticStale=0; // audio-thread only
        std::unique_ptr<Hardware> hardware;
        Queue<TimedAudioFrame, 8192> audio;
        Queue<MidiByte, 65536> midi;
        Queue<InputSample, 8192> input;
        std::array<std::atomic<uint8_t>, 16 * 128> controllers{};
        std::array<std::atomic<uint8_t>, 128> snapshotPatch{};
        std::array<std::atomic<uint8_t>, 16 * 128> snapshotControllers{};
        std::atomic<uint64_t> snapshotVersion{0};
        std::atomic<bool> snapshotRequested{true};
        std::atomic<size_t> leadFrames{DefaultLeadFrames};
        std::atomic<size_t> baseLeadFrames{DefaultLeadFrames};
        uint64_t lastSnapshotFrame = 0;
        std::mutex flashSnapshotMutex;
        std::vector<uint8_t> flashSnapshot;
        uint64_t publishedFlashRevision=~uint64_t(0);
        std::atomic<unsigned> realtimeBlock{256};
        std::atomic<uint8_t> snapshotBank{1},snapshotProgram{0};
        std::atomic<uint64_t> consumed{0}, underruns{0}, midiOverflows{0};
        std::atomic<bool> stop{false}, valid{false}, pauseRequested{false}, paused{false};
        std::atomic<bool> nonRealtime{false};
        std::mutex coldMutex, startMutex;
        std::condition_variable startCv;
        bool started = false;
        std::string failure;
        std::thread worker;
        uint64_t produced = 0;

        explicit Impl(const Rom& rom,baseLib::performance::Diagnostics* diagnostics)
            :performance(diagnostics)
        {
            for (auto& cc : controllers) cc.store(0xff, std::memory_order_relaxed);
            if (!rom.isValid()) { failure = rom.error(); return; }
            worker = std::thread([this, image = rom.data()]
            {
                dsp56k::ThreadTools::setCurrentThreadName("K-Station H8/DSP");
                dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::High);
                try
                {
                    hardware = std::make_unique<Hardware>(image);
                    if (!hardware->boot())
                    {
                        const auto debug = hardware->controller().debugState();
                        throw std::runtime_error("K-Station H8 boot timed out at PC " + std::to_string(debug.pc));
                    }
                    // Compile common polyphonic voice paths before opening audio.
                    // Restore the original sound through its native edit-buffer dump.
                    std::array<uint8_t,128> original{};hardware->controller().copyCurrentPatch(original.data());
                    const uint8_t dry[]{0xb0,91,0,0xb0,92,0,0xb0,93,0};hardware->enqueueMidi(dry,sizeof(dry));
                    for(uint8_t note:{36,43,48,55,60,67,72,79}){const uint8_t on[]{0x90,note,64};hardware->enqueueMidi(on,sizeof(on));}
                    for(unsigned n=0;n<8192;++n)hardware->renderSample();
                    for(uint8_t note:{36,43,48,55,60,67,72,79}){const uint8_t off[]{0x80,note,0};hardware->enqueueMidi(off,sizeof(off));}
                    const uint8_t silence[]{0xb0,120,0};hardware->enqueueMidi(silence,sizeof(silence));
                    for(unsigned n=0;n<45340;++n)hardware->renderSample();
                    std::array<uint8_t,142> restore{};std::copy_n(ProgramHeader,13,restore.begin());std::copy(original.begin(),original.end(),restore.begin()+13);restore.back()=0xf7;
                    hardware->enqueueMidi(restore.data(),restore.size());
                    // Match the plugin's Volume parameter default through the
                    // original MIDI path, without replacing DSP gain handling.
                    const uint8_t volume[]{0xb0, 7, 127};
                    hardware->enqueueMidi(volume, sizeof(volume));
                    controllers[7] = 127;
                    for (size_t i = 0; i < 4096; ++i) hardware->renderSample();
                    for (size_t i = 0; i < leadFrames.load(std::memory_order_relaxed); ++i)
                    {
                        audio.push(TimedAudioFrame{produced, hardware->renderSample()});
                        ++produced;
                    }
                    publishSnapshot();
                    valid.store(true, std::memory_order_release);
                    {
                        std::lock_guard lock(startMutex);
                        started = true;
                    }
                    startCv.notify_one();
                    dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest);
                    run();
                }
                catch (const std::exception& e)
                {
                    std::lock_guard lock(startMutex);
                    failure = e.what();
                    valid.store(false, std::memory_order_release);
                    started = true;
                    startCv.notify_one();
                }
            });
            std::unique_lock lock(startMutex);
            startCv.wait(lock, [this] { return started; });
        }

        ~Impl()
        {
            stop.store(true, std::memory_order_release);
            pauseRequested.store(false, std::memory_order_release);
            wakeup.notify(performance);
            if (worker.joinable()) worker.join();
        }

        void publishSnapshot()
        {
            baseLib::performance::Scope timing(performance,baseLib::performance::Metric::Snapshot);
            if(publishedFlashRevision!=hardware->controller().flashRevision())
            {
                std::unique_lock lock(flashSnapshotMutex,std::try_to_lock);
                if(lock.owns_lock()){hardware->controller().copyFlash(flashSnapshot);publishedFlashRevision=hardware->controller().flashRevision();}
            }
            const auto bank=hardware->controller().readMemory(0xcd82);
            const auto program=hardware->controller().readMemory(0xcd83);
            std::array<uint8_t,128> patch{};
            std::array<uint8_t,16 * 128> cc{};
            hardware->controller().copyCurrentPatch(patch.data());
            bool changed = snapshotVersion.load(std::memory_order_relaxed) == 0
                || bank!=snapshotBank.load(std::memory_order_relaxed) || program!=snapshotProgram.load(std::memory_order_relaxed);
            for (size_t i=0; i<patch.size(); ++i)
                changed |= patch[i] != snapshotPatch[i].load(std::memory_order_relaxed);
            for (size_t i=0; i<cc.size(); ++i)
            {
                cc[i] = controllers[i].load(std::memory_order_relaxed);
                changed |= cc[i] != snapshotControllers[i].load(std::memory_order_relaxed);
            }
            lastSnapshotFrame = produced;
            if (!changed) return;
            snapshotVersion.fetch_add(1, std::memory_order_acq_rel);
            snapshotBank.store(bank,std::memory_order_relaxed);snapshotProgram.store(program,std::memory_order_relaxed);
            for (size_t i=0; i<patch.size(); ++i) snapshotPatch[i].store(patch[i], std::memory_order_relaxed);
            for (size_t i=0; i<cc.size(); ++i) snapshotControllers[i].store(cc[i], std::memory_order_relaxed);
            snapshotVersion.fetch_add(1, std::memory_order_release);
        }

        void run()
        {
            hardware->setPerformanceDiagnostics(performance);
            baseLib::performance::WorkerSegment workTiming(performance);
            dsp56k::AudioWorkgroup::Member audioWorkgroup;
            unsigned configuredBlock=0;
            while (!stop.load(std::memory_order_acquire))
            {
                if (pauseRequested.load(std::memory_order_acquire))
                {
                    workTiming.flush();
                    paused.store(true, std::memory_order_release);
                    while (pauseRequested.load(std::memory_order_acquire) && !stop.load())
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    paused.store(false, std::memory_order_release);
                    continue;
                }
                const auto block=realtimeBlock.load(std::memory_order_relaxed);
                if(block!=configuredBlock){dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(int(DSP::SampleRate),int(block));configuredBlock=block;}
                audioWorkgroup.update();
                if (snapshotRequested.exchange(false, std::memory_order_acquire)
                    || produced - lastSnapshotFrame >= 1024) publishSnapshot();
                if (produced >= consumed.load(std::memory_order_acquire) + leadFrames.load(std::memory_order_relaxed))
                {
                    workTiming.flush();
                    baseLib::performance::Scope sleepTiming(performance,baseLib::performance::Metric::Sleep);
                    wakeup.waitWhile([&]{return !stop.load(std::memory_order_acquire) && !pauseRequested.load(std::memory_order_acquire)
                        && produced>=consumed.load(std::memory_order_acquire)+leadFrames.load(std::memory_order_relaxed);},performance);
                    continue;
                }
                MidiByte event{};
                workTiming.start();
                while (midi.peek(event) && event.due <= produced)
                {
                    // Missing factory slots are erased memory. Do not let a host
                    // Program Change load 128 bytes of FF as oscillator settings.
                    if (event.first && (event.value & 0xf0) == 0xc0)
                    {
                        MidiByte program{};
                        if (!midi.peekAt(1, program)) break;
                        const auto bank = hardware->controller().readMemory(0xcd82);
                        if (!hardware->controller().hasProgram(bank, program.value))
                        {
                            midi.pop(event);
                            midi.pop(program);
                            continue;
                        }
                    }
                    if (!hardware->enqueueMidi(&event.value, 1)) break;
                    midi.pop(event);
                }
                InputSample sample{};
                while (input.peek(sample) && sample.due <= produced)
                {
                    hardware->dsp().pushAudioInput(sample.value);
                    input.pop(sample);
                }
                const auto frame = hardware->renderSample();
                // A missed callback frame is already silence at the host.
                // Catch up guest time without replaying it late and silently
                // increasing the reported lead after every underrun.
                if (produced >= consumed.load(std::memory_order_acquire)
                    && !audio.push(TimedAudioFrame{produced, frame}))
                    throw std::runtime_error("K-Station worker audio queue overflow.");
                ++produced;
                // The initial plugin exposes MIDI input. Drain the original UART
                // output so a guest dump cannot block its transmitting firmware.
                uint8_t discarded;
                while (hardware->popMidiOut(discarded)) {}
                workTiming.frame();
            }
        }

        bool popAudioAt(uint64_t position, DSP::AudioFrame& frame)
        {
            TimedAudioFrame queued{};
            // One producer, one consumer. Discard at most the fixed queue
            // capacity, including a producer/callback underrun race. Never
            // consume a future frame or wait for guest execution here.
            for (size_t i=0; i<8192 && audio.peek(queued); ++i)
            {
                if (queued.due > position) return false;
                audio.pop(queued);
                if (queued.due == position) { frame=queued.frame; return true; }
                if(performance && performance->mode())++diagnosticStale;
            }
            return false;
        }

        bool pause()
        {
            if (!valid.load(std::memory_order_acquire)) return false;
            pauseRequested.store(true, std::memory_order_release);
            wakeup.notify(performance);
            while (!paused.load(std::memory_order_acquire))
            {
                if (!valid.load(std::memory_order_acquire))
                {
                    pauseRequested.store(false);
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            return true;
        }
        void resume()
        {
            pauseRequested.store(false, std::memory_order_release);
            // A cold caller must observe the previous pause ending before it
            // can request another one; otherwise a stale acknowledgement races
            // with the worker returning to guest execution.
            while (paused.load(std::memory_order_acquire) && valid.load(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    Device::Device(const synthLib::DeviceCreateParams& params,baseLib::performance::Diagnostics* performance) : synthLib::Device(params)
    {
        const auto rom = params.romData.empty() ? RomLoader::findROM() : Rom(params.romData, params.romName);
        m_impl = std::make_unique<Impl>(rom,performance);
    }
    Device::~Device() = default;
    bool Device::isValid() const { return m_impl->valid.load(std::memory_order_acquire); }
    std::string Device::error() const
    {
        std::lock_guard lock(m_impl->startMutex);
        return m_impl->failure;
    }
    float Device::getSamplerate() const { return static_cast<float>(DSP::SampleRate); }
    uint64_t Device::getDspClockHz() const { return DSP::ClockHz; }
    uint32_t Device::getInternalLatencyMidiToOutput() const { return static_cast<uint32_t>(m_impl->leadFrames.load(std::memory_order_relaxed)); }
    uint32_t Device::getInternalLatencyInputToOutput() const { return static_cast<uint32_t>(m_impl->leadFrames.load(std::memory_order_relaxed)); }
    uint64_t Device::getUnderrunCount() const { return m_impl->underruns.load(); }
    void Device::setBufferFloor(unsigned frames){m_impl->leadFrames.store(std::max(m_impl->baseLeadFrames.load(),size_t(std::min(4096u,frames))));m_impl->wakeup.notify(m_impl->performance);}
    void Device::setAudioBuffer(uint32_t block,float rate,unsigned floor){
        const auto burst=size_t(std::ceil(double(block)*DSP::SampleRate/rate));
        const auto lead=std::min(size_t(8064),std::max(MinimumLeadFrames,(burst+LeadMarginFrames+127)&~size_t(127)));
        m_impl->baseLeadFrames.store(lead);m_impl->realtimeBlock.store(unsigned(std::max(size_t(64),burst)));setBufferFloor(floor);
    }
    void Device::ensureAudioBlock(uint32_t block,float rate) noexcept{
        const auto burst=size_t(std::ceil(double(block)*DSP::SampleRate/rate));
        const auto needed=std::min(size_t(8064),std::max(MinimumLeadFrames,(burst+LeadMarginFrames+127)&~size_t(127)));
        auto lead=m_impl->leadFrames.load(std::memory_order_relaxed);
        for(unsigned attempt=0;attempt<4 && lead<needed;++attempt)if(m_impl->leadFrames.compare_exchange_weak(lead,needed)){m_impl->realtimeBlock.store(unsigned(std::max(size_t(64),burst)));break;}
    }
    uint64_t Device::getMidiOverflowCount() const { return m_impl->midiOverflows.load(); }
    const char* Device::workerWaitBackend() const {return m_impl->wakeup.backend();}
    void Device::setNonRealtime(bool enabled) noexcept { m_impl->nonRealtime.store(enabled, std::memory_order_release); }

    bool Device::prepareAudio(uint32_t maxHostBlock, float hostSamplerate)
    {
        if (!isValid() || !maxHostBlock || !std::isfinite(hostSamplerate) || hostSamplerate <= 0) return false;
        const double nativeBurst = std::ceil(double(maxHostBlock) * DSP::SampleRate / hostSamplerate);
        if (nativeBurst + LeadMarginFrames > 8192) return false;
        const size_t lead = std::max(MinimumLeadFrames,
            (static_cast<size_t>(nativeBurst) + LeadMarginFrames + 127) & ~size_t(127));
        m_impl->baseLeadFrames.store(lead);
        m_impl->realtimeBlock.store(unsigned(std::max(64.,nativeBurst)));
        std::lock_guard lock(m_impl->coldMutex);
        if (lead == m_impl->leadFrames.load(std::memory_order_relaxed)) return true;
        if (!m_impl->pause()) return false;
        try
        {
            const auto consumed = m_impl->consumed.load(std::memory_order_relaxed);
            const auto oldLead = m_impl->leadFrames.load(std::memory_order_relaxed);
            // Keep pending MIDI when the host changes its buffer while stopped.
            // Rebase scheduling coordinates together with the refilled audio.
            const auto pending = m_impl->midi.size();
            for (size_t i=0; i<pending; ++i)
            {
                MidiByte event{};
                m_impl->midi.pop(event);
                const auto relative = event.due > consumed + oldLead ? event.due - consumed - oldLead : 0;
                event.due = consumed + lead + relative;
                m_impl->midi.push(event);
            }
            m_impl->input.reset();
            m_impl->audio.reset();
            m_impl->produced = consumed;
            m_impl->leadFrames.store(lead, std::memory_order_relaxed);
            for (size_t i=0; i<lead; ++i)
            {
                m_impl->audio.push(TimedAudioFrame{m_impl->produced, m_impl->hardware->renderSample()});
                ++m_impl->produced;
            }
            m_impl->lastSnapshotFrame = m_impl->produced;
            m_impl->resume();
            return true;
        }
        catch (...) { m_impl->resume(); throw; }
    }

    bool Device::getControlSnapshot(std::array<uint8_t,128>& patch,
                                    std::array<uint8_t,16 * 128>& controllers,
                                    uint64_t& revision) const noexcept
    {
        m_impl->snapshotRequested.store(true, std::memory_order_release);
        for (unsigned attempt=0; attempt<2; ++attempt)
        {
            const auto before = m_impl->snapshotVersion.load(std::memory_order_acquire);
            if (!before || (before & 1)) continue;
            for (size_t i=0; i<patch.size(); ++i) patch[i] = m_impl->snapshotPatch[i].load(std::memory_order_relaxed);
            for (size_t i=0; i<controllers.size(); ++i)
                controllers[i] = m_impl->snapshotControllers[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (before == m_impl->snapshotVersion.load(std::memory_order_relaxed))
            { revision = before / 2; return true; }
        }
        return false;
    }

    void Device::process(const synthLib::TAudioInputs& inputs, const synthLib::TAudioOutputs& outputs,
                         size_t samples, const std::vector<synthLib::SMidiEvent>& midiIn,
                         std::vector<synthLib::SMidiEvent>& midiOut)
    {
        midiOut.clear();
        for (const auto& event : midiIn)
            if (event.type == synthLib::MidiEventType::Midi) sendMidi(event, midiOut);
        processAudio(inputs, outputs, samples);
    }

    void Device::processAudio(const synthLib::TAudioInputs& inputs, const synthLib::TAudioOutputs& outputs, size_t samples)
    {
        using namespace baseLib::performance;
        auto* performance=m_impl->performance;
        const auto epoch=performance?performance->epoch():0;
        const auto staleBefore=m_impl->diagnosticStale;
        uint64_t missing=0;
        if(epoch)performance->add(Metric::QueueDepth,epoch,m_impl->audio.size());
        auto consumed = m_impl->consumed.load(std::memory_order_relaxed);
        const bool offline = m_impl->nonRealtime.load(std::memory_order_acquire);
        const auto lead = m_impl->leadFrames.load(std::memory_order_relaxed);
        for (size_t i = 0; i < samples; ++i)
        {
            if (inputs[0]) m_impl->input.push(InputSample{consumed + lead, inputWord(inputs[0][i])});
            DSP::AudioFrame frame{};
            bool available = m_impl->popAudioAt(consumed, frame);
            if (!available && offline)
            {
                m_impl->wakeup.notify(performance);
                // A DAW bounce has no realtime deadline. Wait on the lock-free
                // queue so a fast host cannot turn worker lag into silent audio.
                // CPU/JIT execution stays on its single owner; the realtime
                // branch never enters this loop. No I/O, locks or sleeps here.
                while (isValid() && !m_impl->stop.load(std::memory_order_acquire))
                {
                    if (m_impl->popAudioAt(consumed, frame)) { available = true; break; }
                    std::atomic_signal_fence(std::memory_order_acquire);
                }
            }
            if (!available && isValid()) m_impl->underruns.fetch_add(1, std::memory_order_relaxed);
            if(epoch && !available)++missing;
            for (size_t channel = 0; channel < outputs.size(); ++channel)
                if (outputs[channel]) outputs[channel][i] = channel < 2 ? float(frame[channel]) / 8388608.0f : 0.0f;
            m_impl->consumed.store(++consumed, std::memory_order_release);
        }
        m_impl->wakeup.notify(performance);
        // A cold warm-up or bounce must refill before handing audio to a live host.
        if(offline)while(isValid() && !m_impl->stop.load() && m_impl->audio.size()<lead)std::this_thread::yield();
        if(epoch){performance->add(Metric::MissingFrames,epoch,missing);performance->add(Metric::StaleFrames,epoch,m_impl->diagnosticStale-staleBefore);}
    }

    bool Device::sendMidi(const synthLib::SMidiEvent& event, std::vector<synthLib::SMidiEvent>&)
    {
        if (!isValid()) return false;
        const auto due = m_impl->consumed.load(std::memory_order_relaxed) + m_impl->leadFrames.load(std::memory_order_relaxed) + event.offset;
        size_t count;
        uint8_t shortBytes[]{event.a, event.b, event.c};
        const uint8_t* bytes = shortBytes;
        uint8_t nrpnBytes[9]{};
        const bool internalNrpn = event.port == 254;
        if (internalNrpn)
        {
            // A reserved editor-only short event avoids allocating a SysEx
            // buffer when DAW automation is delivered on its audio thread.
            if (event.source != synthLib::MidiEventSource::Editor || !event.sysex.empty()
                || (event.a & 0xf0) != 0xb0 || event.b >= 128 || event.c >= 128) return false;
            for (size_t i=0; i<3; ++i) nrpnBytes[i*3] = event.a;
            nrpnBytes[1]=99; nrpnBytes[2]=0;
            nrpnBytes[4]=98; nrpnBytes[5]=event.b;
            nrpnBytes[7]=6; nrpnBytes[8]=event.c;
            bytes=nrpnBytes; count=9;
        }
        else if (!event.sysex.empty()) { bytes = event.sysex.data(); count = event.sysex.size(); }
        else
        {
            if (event.a < 0x80) return false;
            if (event.a >= 0xf8) count = 1;
            else if (event.a < 0xf0) count = (event.a & 0xe0) == 0xc0 ? 2 : 3;
            else switch (event.a) { case 0xf1: case 0xf3: count = 2; break; case 0xf2: count = 3; break; default: count = 1; }
        }
        const bool accepted = m_impl->midi.pushGenerated(count,
            [bytes, due](size_t i) { return MidiByte{due, bytes[i], i == 0}; });
        if (!accepted) m_impl->midiOverflows.fetch_add(1, std::memory_order_relaxed);
        else
        {
            if (!internalNrpn && event.sysex.empty() && (event.a & 0xf0) == 0xb0 && event.b < 128 && event.c < 128)
                m_impl->controllers[size_t(event.a & 15) * 128 + event.b].store(event.c, std::memory_order_relaxed);
            m_impl->snapshotRequested.store(true, std::memory_order_release);
        }
        return accepted;
    }

    void Device::readMidiOut(std::vector<synthLib::SMidiEvent>&) {}

    bool Device::enqueueMidiBytes(const uint8_t* bytes, size_t count, uint32_t offset)
    {
        if (!isValid() || !bytes || !count) return false;
        const auto due = m_impl->consumed.load(std::memory_order_relaxed)
            + m_impl->leadFrames.load(std::memory_order_relaxed) + offset;
        const bool accepted = m_impl->midi.pushGenerated(count,
            [bytes, due](size_t i) { return MidiByte{due, bytes[i], i == 0 || bytes[i] >= 0x80}; });
        if (!accepted) m_impl->midiOverflows.fetch_add(1, std::memory_order_relaxed);
        else
        {
            // Keep live performance controllers in session state, as sendMidi does.
            // Status-delimited messages only; SysEx payload never becomes a CC.
            uint8_t status = 0, number = 0;
            unsigned position = 0;
            for (size_t i = 0; i < count; ++i)
            {
                const auto byte = bytes[i];
                if (byte >= 0xf8) continue;
                if (byte >= 0x80) { status = byte; position = 0; continue; }
                if ((status & 0xf0) != 0xb0) continue;
                if (position++ == 0) number = byte;
                else
                {
                    m_impl->controllers[size_t(status & 15) * 128 + number].store(byte, std::memory_order_relaxed);
                    position = 0;
                }
            }
            m_impl->snapshotRequested.store(true, std::memory_order_release);
        }
        return accepted;
    }

#if SYNTHLIB_DEMO_MODE == 0
    bool Device::getState(std::vector<uint8_t>& state, synthLib::StateType)
    {
            std::array<uint8_t,128> patch{};
            std::array<uint8_t,16*128> controllers{};uint64_t revision=0;
            uint8_t bank=1,program=0;bool captured=false;
            for(unsigned attempt=0;attempt<64 && !captured;++attempt){
                if(getControlSnapshot(patch,controllers,revision)){
                    bank=m_impl->snapshotBank.load(std::memory_order_relaxed);program=m_impl->snapshotProgram.load(std::memory_order_relaxed);
                    std::atomic_thread_fence(std::memory_order_acquire);
                    captured=revision*2==m_impl->snapshotVersion.load(std::memory_order_relaxed);
                }
                if(!captured)std::this_thread::yield();
            }
            if(!captured)return false;
            const auto previousSize = state.size();
            state.insert(state.end(), std::begin(StateHeader), std::end(StateHeader));
            std::vector<uint8_t> flash;
            {std::lock_guard lock(m_impl->flashSnapshotMutex);flash=m_impl->flashSnapshot;}
            flash[0x1ff00] = bank;
            flash[0x1ff01] = program;
            // Session state contains presets/settings, never the external OS ROM.
            state.insert(state.end(), flash.begin() + UserFlashStart, flash.end());
            state.insert(state.end(), patch.begin(), patch.end());
            state.insert(state.end(),controllers.begin(),controllers.end());
            return state.size() - previousSize == StateSize;
    }

    bool Device::setState(const std::vector<uint8_t>& state, synthLib::StateType)
    {
        if (state.size() != StateSize || !std::equal(std::begin(StateHeader), std::end(StateHeader), state.begin())) return false;
        const auto patchOffset = sizeof(StateHeader) + UserFlashSize;
        if (!std::all_of(state.begin() + patchOffset, state.begin() + patchOffset + 128,
                         [](uint8_t byte) { return byte < 128; })) return false;
        std::lock_guard lock(m_impl->coldMutex);
        if (!m_impl->pause()) return false;
        try
        {
            const auto rom = getDeviceCreateParams().romData.empty() ? RomLoader::findROM()
                : Rom(getDeviceCreateParams().romData, getDeviceCreateParams().romName);
            if (!rom.isValid()) { m_impl->resume(); return false; }
            std::vector<uint8_t> flash(ControllerFlashSize, 0xff);
            std::copy(state.begin() + sizeof(StateHeader), state.begin() + patchOffset,
                      flash.begin() + UserFlashStart);
            // Before 0.3.2 the editor exposed delay ratio as 0..127, although
            // original 7E42 indexes a 13-entry table (patch byte 106). Recover
            // invalid old settings as 1/1 during cold restore, before boot can
            // load the selected flash patch. Valid programs remain unchanged.
            const auto repairRatio = [&flash](size_t offset)
            {
                if (std::all_of(flash.begin() + offset, flash.begin() + offset + 128,
                                [](uint8_t byte) { return byte < 128; })
                    && flash[offset + 106] > 12) flash[offset + 106] = 0;
            };
            repairRatio(0x20000);
            const auto bank = flash[0x1ff00], program = flash[0x1ff01];
            if (bank >= 1 && bank <= 4 && program < 100)
                repairRatio(0x26400 + ((size_t(bank) - 1) * 100 + program) * 128);
            auto replacement = std::make_unique<Hardware>(rom.data(), flash);
            if (!replacement->boot()) { m_impl->resume(); return false; }
            std::vector<uint8_t> restore(std::begin(ProgramHeader), std::end(ProgramHeader));
            restore.insert(restore.end(), state.begin() + patchOffset, state.begin() + patchOffset + 128);
            if (restore[sizeof(ProgramHeader) + 106] > 12)
                restore[sizeof(ProgramHeader) + 106] = 0;
            restore.push_back(0xf7);
            const auto ccOffset = patchOffset + 128;
            for (size_t i = 0; i < m_impl->controllers.size(); ++i)
                // Program settings are already in the canonical patch. A
                // stale CC from a previous preset must not overwrite them.
                // Restore only supported performance controls; never protocol
                // selectors/data entry or panic/reset commands.
                if (state[ccOffset + i] < 128 && (i % 128 == 1 || i % 128 == 2
                    || i % 128 == 7 || i % 128 == 64))
                {
                    restore.push_back(0xb0 | uint8_t(i / 128));
                    restore.push_back(uint8_t(i % 128));
                    restore.push_back(state[ccOffset + i]);
                }
            if (!replacement->enqueueMidi(restore.data(), restore.size())) { m_impl->resume(); return false; }
            // Restore passes through the original UART, including its serial byte
            // time, while audio is suspended. Never wait for host process().
            const auto settle = size_t(std::ceil(restore.size() * 10.0 * DSP::SampleRate / 31250.0)) + 2048;
            for (size_t i = 0; i < settle; ++i) replacement->renderSample();
            m_impl->hardware.swap(replacement);
            m_impl->hardware->setPerformanceDiagnostics(m_impl->performance);
            m_impl->publishedFlashRevision=~uint64_t(0);
            m_impl->audio.reset();
            m_impl->midi.reset();
            m_impl->input.reset();
            m_impl->produced = m_impl->consumed.load();
            for (size_t i=0; i<m_impl->controllers.size(); ++i)
                m_impl->controllers[i].store(state[ccOffset+i], std::memory_order_relaxed);
            for (size_t i = 0; i < m_impl->leadFrames.load(std::memory_order_relaxed); ++i)
            {
                m_impl->audio.push(TimedAudioFrame{m_impl->produced, m_impl->hardware->renderSample()});
                ++m_impl->produced;
            }
            m_impl->publishSnapshot();
            m_impl->resume();
            return true;
        }
        catch (...) { m_impl->resume(); throw; }
    }
#endif
}
