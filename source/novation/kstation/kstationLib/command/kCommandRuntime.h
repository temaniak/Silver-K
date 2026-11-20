#pragma once

#include "kCommandSeed.h"
#include "kArpPolicy.h"
#include "kCommandVoiceFeedback.h"
#include "kCommandModWheelUpdate.h"
#include "kCommandNoteOff.h"
#include "kCommandNoteOn.h"
#include "kCommandLiveUpdate.h"
#include "kKeyState.h"
#include "kStationCommandBackend.h"
#include "kPolyVoiceSelection.h"
#include "kProgramFlash.h"
#include "../kPhysicalTimeline.h"
#include "../kstationQueue.h"
#include "../../../controller/command/dspMusicalClock.h"
#include "../../../controller/command/dspGlobalSync.h"
#include "../../../controller/command/dspSessionStop.h"
#include "../../../controller/command/nativeEditorState.h"
#include "../../../controller/command/novationProgramSysEx.h"
#include "../../../controller/command/commandAudioDevice.h"
#ifdef SILVER_P12_ACTION_TRACE
#include "../../../controller/command/p12ActionTrace.h"
#endif
#ifdef SILVER_P12_CPU_SCOPES
#include "../../../controller/command/p12CpuScopes.h"
#endif
#ifdef SILVER_P12_STATE_SIZE
#include "../../../controller/command/p12StateSize.h"
#endif

#include <stdexcept>

#ifndef SILVER_K_COMMAND_HOST
#include "p07_k_prepared_fixture.h"
#include "p08_host_blocks.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef SILVER_K_COLD_PROBE_ONLY
#include "p09_lifecycle_exercise.h"
#endif
#endif

namespace kstation::command::runtime {
using namespace novation::controller::command;
namespace kc = kstation::command;

inline void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// DSP clock / physical HDI clock ratio is 7, with 2304 DSP clocks per sample.
inline constexpr uint64_t ticksForFrames(uint64_t frames) { return (frames*2304+6)/7; }

#ifndef SILVER_K_COMMAND_HOST
namespace fs = std::filesystem;
using namespace kstation::command::research;

enum class DeliveryPolicy { Legacy, Serial, Priority };

struct ProbeOptions {
    DeliveryPolicy delivery=DeliveryPolicy::Legacy;
    std::optional<uint32_t> reuseFrame;
    std::optional<uint32_t> triggerFrame;
    bool mixed=false;
    bool adjacentRelease=false;
    bool referenceOnly=false;
    bool freshStart=false;
    bool cpuWindow=false,phaseReference=false;
    bool lifecycleTrace=false;
    bool decodedIngress=false;
    bool musical=false;
    uint32_t minimumGateFrames=0;
    uint32_t activationObservationDelay=0;
};

// MIDI at 31,250 baud in the local 16 MHz control clock. The request time
// marks availability of the first byte. Later messages share the serial line.
constexpr uint64_t MidiByteTicks=5120;

struct MidiByte { uint32_t frame; uint8_t value; };
struct HostPosition {
    uint32_t frame;
    novation::controller::MusicalClock::Position position;
};

inline std::vector<uint64_t> inputDeadlines(const std::vector<VerticalInput>& inputs,uint64_t origin,
                                     const ProbeOptions& options) {
    std::vector<uint64_t> deadlines;
    uint64_t nextByte=origin;
    for(const auto& input:inputs) {
        if(options.decodedIngress) {
            deadlines.push_back(origin+ticksForFrames(input.frame));
        } else if(options.delivery==DeliveryPolicy::Legacy) {
            deadlines.push_back(input.inputCycle+2*MidiByteTicks);
        } else {
            const auto firstByte=std::max(nextByte,origin+ticksForFrames(input.frame));
            deadlines.push_back(firstByte+2*MidiByteTicks);
            nextByte=firstByte+3*MidiByteTicks;
        }
    }
    return deadlines;
}

#endif

// A narrow native K window, with complete immutable musical commands and
// real DSP transactions.
class KCommandRuntime final {
    enum class Work { Idle,ModAdmission,ModTransfer,OffAdmission,OffTransfer,OnPitchAdmission,OnPitch,
                     OnImageAdmission,OnTransfer,Feedback,ReuseFeedback,Mailbox,Program,Live,Clock,Phase };
    struct MusicalWork {
#ifdef SILVER_K_COMMAND_HOST
        struct PendingPosition { NativeFrame at; novation::controller::MusicalClock::Position value; };
        kstation::Queue<PendingPosition, 2048> positions;
#endif
        explicit MusicalWork(NativeFrame at) :clock({112000000,2304},at) {}
        struct Binding {uint64_t arpGeneration;VoiceAllocation voice;};
        NativeMusicalClock clock;
        DspMusicalClock dspClock{Model::K,{112000000,2304}};
        DspGlobalSync phase{Model::K};
        GlobalSyncKeys phaseKeys;
        NativeArpeggiator arp;
        std::array<std::optional<Binding>,KMusicalState::VoiceCount> voices;
        std::optional<ArpNote> transferring;
        std::optional<NoteOn> phaseNote;
        std::optional<NativeFrame> programAt;
#ifndef SILVER_K_COMMAND_HOST
        size_t nextHost=0;
        std::ofstream pulses,notes,midi;
        std::ofstream hostLog;
        std::optional<p08::HostBlockPlayback> hostBlocks;
#endif
    };
public:
#ifdef SILVER_K_COMMAND_HOST
    KCommandRuntime(kc::PhysicalTimeline& physical, KCommandSeed seed, KProgramFlash& programs,
                    const std::vector<uint8_t>& flash, const KProgramTables& tables,
                    const KMidiParameters& parameters, ProgramSelection initial)
        : physical_(physical), seed_(seed), originFrame_(physical.frames()),
          backend_({originFrame_}, flash.data(), flash.size()), session_(backend_.session()),
          token_(backend_.token()), model_(backend_.model()), programTables_(tables), parameterMap_(parameters),
          port_(Endpoint::A, DspEndpoint::bind(*this)), writableFlash_(&programs) {
        initializeProgram(initial, true);
        applyClockPolicy(now(), source_.patch[0xc0]);
        initializeCold();
    }

    bool enqueueHostPosition(novation::controller::MusicalClock::Position position) noexcept {
        return !stopped_ && arp_->positions.push({now(), position});
    }
    bool enqueueMidiByte(uint8_t value) noexcept {
        return !stopped_ && hostMidi_.push({now(), value});
    }
    bool enqueueControl(const ControllerEvent& event) noexcept {
        return !stopped_ && hostMidi_.push({now(), 0, false, event});
    }
    void setMidiSink(CommandMidiSink sink) noexcept { midiSink_ = sink; }
#ifdef SILVER_P12_STATE_SIZE
    void reportStateSize() const noexcept {
        p12StateSizes("initialized", "K-controller", {
            {"mutable", "runtime", sizeof(KCommandRuntime)}, {"mutable", "clock_and_arp", arp_ ? sizeof(MusicalWork) : 0},
            {"detail", "host_midi", sizeof(hostMidi_)}, {"detail", "scheduler_session", sizeof(CommandSession)},
            {"detail", "musical_state", sizeof(KMusicalState)}, {"detail", "arpeggio_host_positions", sizeof(MusicalWork::positions)}});
    }
#endif
    bool checkpointReady() const noexcept {
        return !hostMidi_.size() && !arp_->positions.size() && session_.midiAtMessageBoundary() &&
            !session_.snapshot().queued && (work_ != Work::Live || !liveFromInput_);
    }
    std::vector<uint8_t> archiveMidi() const {
        auto bytes = pendingProgram();
        const auto parameters = model_.part().parameters;
        const auto channel = uint8_t(0xb0 | seed_.channel);
        if (parameters.volume != 63)
            bytes.insert(bytes.end(), {channel, 7, parameters.volume});
        if (source_.controls[0])
            bytes.insert(bytes.end(), {channel, 1, source_.controls[0]});
        if (source_.controls[2])
            bytes.insert(bytes.end(), {channel, 2, source_.controls[2]});
        return bytes;
    }

private:
    struct PendingByte {
        NativeFrame at;
        uint8_t value;
        bool raw = true;
        ControllerEvent control{};
    };
public:
#else
    KCommandRuntime(kc::PhysicalTimeline& physical,KCommandSeed seed,const std::vector<VerticalInput>& inputs,
                 const fs::path& directory,ProbeOptions options,const std::vector<MidiByte>& midi,
                 const std::vector<uint8_t>& flash,const KProgramTables& programTables,const KMidiParameters& parameterMap,
                 const std::vector<HostPosition>& host,std::optional<bool> variableBlocks,ProgramSelection initial={1,0})
        :physical_(physical),seed_(seed),inputs_(inputs),options_(options),deadlines_(inputDeadlines(inputs,physical.cycle(),options)),
         originFrame_(physical.frames()),midi_(midi),backend_({originFrame_},flash.data(),flash.size()),
         session_(backend_.session()),token_(backend_.token()),model_(backend_.model()),programTables_(programTables),parameterMap_(parameterMap),
         port_(Endpoint::A,DspEndpoint::bind(*this)),host_(host) {
        if(!options_.cpuWindow) {
            events_.exceptions(std::ios::badbit|std::ios::failbit);packets_.exceptions(std::ios::badbit|std::ios::failbit);
            events_.open(directory/"candidate-events.csv");packets_.open(directory/"candidate-packets.csv");
            events_<<"event,controller_cycle,native_frame,value\n";packets_<<"bundle,endpoint,address,word\n";
        }
        initializeProgram(initial, options_.musical);
        if (arp_) {
            for(auto* file:{&arp_->pulses,&arp_->notes,&arp_->midi})file->exceptions(std::ios::badbit|std::ios::failbit);
            arp_->pulses.open(directory/"candidate-clock.csv");
            arp_->pulses<<"frame,pulse,epoch,source,tick,running,frames_per_pulse\n";
            arp_->notes.open(directory/"candidate-arp.csv");
            arp_->notes<<"deadline,dispatch,generation,note,velocity,on,internal,midi\n";
            arp_->midi.open(directory/"candidate-midi-out.csv");
            arp_->midi<<"frame,status,note,velocity\n";
            if(variableBlocks) {
                arp_->hostLog.exceptions(std::ios::badbit|std::ios::failbit);
                arp_->hostLog.open(directory/"candidate-host-blocks.csv");
                arp_->hostBlocks.emplace(session_,token_,NativeFrame{originFrame_},*variableBlocks,arp_->hostLog);
            }
        }
        if(decodedInput())std::cout<<"research_backend="<<backend_.identity()<<" model=K\n";
        if(options_.reuseFrame) {
            require(seed.patch[KVoiceModeOffset]==2,"K reuse probe requires Poly2");
            reuseTrace_.open(directory/"candidate-reuse.csv");
            reuseTrace_.exceptions(std::ios::badbit|std::ios::failbit);
            reuseTrace_<<"requested_frame,start_frame,completed_frame,start_cycle,completed_cycle,cached_slot,fresh_slot,queries\n";
        }
#ifdef SILVER_K_P07_TRACE_DSP_USE
        stateTrace_.open(directory/"candidate-voice-states.csv");
        stateTrace_.exceptions(std::ios::badbit|std::ios::failbit);
        stateTrace_<<"controller_cycle,native_frame,slot,age,countdown\n";
        selectionTrace_.open(directory/"candidate-selection.csv");
        selectionTrace_.exceptions(std::ios::badbit|std::ios::failbit);
        selectionTrace_<<"controller_cycle,native_frame,selected_slot,reason\n";
        sampleState();
#endif
    }
#endif
    ~KCommandRuntime(){
        if(activeTicket_)port_.cancel(session_);
        backend_.abortProgram();
        session_.stop(token_);
    }
    uint8_t hostRead(uint32_t offset){return physical_.read(offset);}
    bool hostWrite(uint32_t offset,uint8_t value){return physical_.write(offset,value);}
    NativeFrame now()const{return {physical_.frames()};}
#if defined(SILVER_K_COLD_PROBE_ONLY) || defined(SILVER_K_COMMAND_HOST)
    void initializeCold() { startLive({KLiveKind::Rebuild}, false); }
    bool coldReady() const noexcept { return work_ != Work::Live; }
#ifdef SILVER_K_COLD_PROBE_ONLY
    void exerciseLifecycle(p09::Exercise& exercise) {
        exercise_ = &exercise;
        exercise.checkPrevious(session_, token_, now());
        if (exercise.previous && exercise.previous->voice)
            require(!model_.voice(*exercise.previous->voice), "K accepted a voice from a retired owner");
    }
#endif
    NativeEditorState editorState() const {
        require(!stopped_,"K editor state belongs to a stopped owner");
        const auto view = model_.snapshot();
        require(bool(view), "K editor projection is unavailable");
        const auto& desired = view->state.part;
        NativeEditorState panel;
        panel.session = token_;
        panel.frame = view->frame;
        panel.revision = view->revision;
        panel.rateGeneration = session_.rateGeneration();
        panel.loadingProgram = backend_.pendingProgram().has_value();
        panel.globalChannel = seed_.channel;
        panel.activeMask = 1;
        panel.selectedPart = 0;
        auto& part = panel.parts[0];
        part.program = *desired.program;
        part.revisions = desired.revisions;
        part.channel = seed_.channel;
        part.cutoff = desired.parameters.cutoff;
        part.volume = desired.parameters.volume;
        part.parameterBytes = KPackedProgramBytes;
        std::copy_n(source_.patch.begin(), KPackedProgramBytes, part.parameters.begin());
        return panel;
    }
#ifdef SILVER_K_COMMAND_HOST
    DspStopReport shutdown() {
#else
    DspStopReport shutdown(p09::Exercise* exercise = nullptr) {
#endif
        if (stopped_)
            return {DspDrain::Idle,true};
        auto budget = 320000u;
#ifdef SILVER_K_COLD_PROBE_ONLY
        if (exercise) {
            if (exercise->exhaustDrain)
                budget = 0;
            if (exercise->fillIngress) {
                const auto due=NativeFrame{now().ticks+10000};
                auto status=AdmissionStatus::Accepted;
                for(size_t i=0;i<=CommandScheduler::Capacity && status==AdmissionStatus::Accepted;++i)
                    status=session_.submit(token_,due,{NoteOn{0,99,127}}).status;
                exercise->ingressBackpressure=status==AdmissionStatus::Full &&
                    session_.snapshot().queued==CommandScheduler::Capacity;
            }
            exercise->retired.session = token_;
            exercise->retired.hostBlock = session_.hostBlock(token_, {}, 64);
            exercise->retired.ticket = exercise->interruptedTransfer
                ? std::optional{exercise->interruptedTransfer->ticket} : activeTicket_;
            exercise->retired.voice = exercise->interruptedVoice;
            for (size_t slot=0;!exercise->retired.voice && slot<KMusicalState::VoiceCount;++slot)
                if (const auto voice=model_.voice(VoiceSlot(slot)))
                    exercise->retired.voice=voice->allocation;
            exercise->queuedAtStop = midi_.size() - nextByte_ + session_.snapshot().queued;
        } else
#endif
        {
            physical_.observe(nullptr,nullptr);
        }
        const auto report = stopDspSession(session_, token_, &port_, {physical_.cycle()}, budget,
            [&](BusCycle at) { physical_.advanceTo(at.ticks); physical_.discardAudio(); },
            [&] { return now(); }, [&] { return physical_.dsp().peripherals().getHDI08().hasRXData(); });
        activeTicket_.reset();
        stopped_ = true;
        backend_.abortProgram();
        activations_.fill(std::nullopt);
        physical_.reset(true);
#ifdef SILVER_K_COLD_PROBE_ONLY
        if (exercise) {
            exercise->stop = report;
            exercise->stoppedRejected = session_.submit(token_, now(), {NoteOn{0,99,127}}).status == AdmissionStatus::Stopped &&
                (!exercise->retired.ticket ||
                 session_.receiveReply(*exercise->retired.ticket, now(), 0x123456).status == AdmissionStatus::Stopped);
            auto& hdi=physical_.dsp().peripherals().getHDI08();
            exercise->endpointReset = !physical_.running() && !physical_.bootFinished() &&
                !hdi.hasRXData() && !hdi.hasTX();
        }
#endif
        return report;
    }
    std::vector<uint8_t> pendingProgram() const {
        const auto pending=backend_.pendingProgram();
        if(!pending)return {};
        if(pending->fromBank)
            return {uint8_t(0xb0|seed_.channel),32,pending->selection.bank,uint8_t(0xc0|seed_.channel),pending->selection.program};
        return NovationProgramSysEx::encode(Model::K,0,0,0,0,pending->record.data(),pending->record.size());
    }
#ifdef SILVER_K_COLD_PROBE_ONLY
    void captureSysEx(std::ostream& output,KProgramFlash& flash) noexcept { sysExOut_=&output; writableFlash_=&flash; }
#endif
    ProgramSelection selection() const { return *model_.part().program; }
    const KExpressionSource& program() const noexcept { return source_; }
#endif
    void event([[maybe_unused]] const char* name, [[maybe_unused]] uint64_t value) {
#ifdef SILVER_P12_ACTION_TRACE
        P12ActionTrace::worker().nativeEvent(name, physical_.cycle(), physical_.frames(), value);
#endif
#ifndef SILVER_K_COMMAND_HOST
        if(!options_.cpuWindow)events_<<name<<','<<physical_.cycle()<<','<<physical_.frames()<<','<<value<<'\n';
#endif
    }

    kstation::DSP::AudioFrame render() {
#if defined(SILVER_K_COLD_PROBE_ONLY) || defined(SILVER_K_COMMAND_HOST)
        require(!stopped_,"K controller audio requested after stop");
#endif
        kstation::DSP::AudioFrame frame{};
        while(!physical_.popAudio(frame)) {
#ifdef SILVER_P12_CPU_SCOPES
            p12::CpuScope controllerCpu(p12::CpuRegion::Controller);
#endif
            ready();
#ifdef SILVER_K_COMMAND_HOST
            uint64_t next = physical_.cycle() + ticksForFrames(1);
            if (const auto bus = port_.nextBusAccess(); bus && bus->ticks > physical_.cycle())
                next = std::min(next, bus->ticks);
#else
            // Historical captures retain their sampling grid. The native
            // benchmark uses the next audio demand and actual bus deadlines.
            uint64_t next=physical_.cycle()+(options_.cpuWindow?ticksForFrames(1):128);
            if(const auto bus=port_.nextBusAccess();bus && bus->ticks>physical_.cycle())next=std::min(next,bus->ticks);
            if(work_==Work::Idle && nextInput_<inputs_.size()) {
                const auto due=deadlines_[nextInput_];if(due>physical_.cycle())next=std::min(next,due);
            }
            if(work_==Work::Idle && nextByte_<midi_.size()) {
                const auto due=seed_.cycle+ticksForFrames(midi_[nextByte_].frame);
                if(due>physical_.cycle())next=std::min(next,due);
            }
#endif
            if(arp_ && work_==Work::Idle) {
                // Host blocks already carry native frame deadlines. Observe
                // them at the frame boundary shared by both partitions.
#ifdef SILVER_K_COMMAND_HOST
                const auto hostWake = nextHostFrame();
#else
                const auto hostWake=arp_->hostBlocks?std::nullopt:nextHostFrame();
#endif
                for(const auto due:{arp_->clock.nextDeadline(),arp_->arp.nextDeadline(),hostWake})if(due && now()<*due) {
                    const auto cycle=seed_.cycle+ticksForFrames(due->ticks-originFrame_);
                    if(cycle>physical_.cycle())next=std::min(next,cycle);
                }
            }
            require(next>physical_.cycle(),"K physical clock failed to advance");
#ifdef SILVER_P12_CPU_SCOPES
            controllerCpu.finish();
            p12::CpuScope dspCpu(p12::CpuRegion::DspA);
#endif
            physical_.advanceTo(next);
        }
#ifdef SILVER_P12_CPU_SCOPES
        p12::CpuScope controllerCpu(p12::CpuRegion::Controller);
#endif
        ready();return frame;
    }
#ifndef SILVER_K_COMMAND_HOST
    size_t consumedMidi() const noexcept {return nextByte_;}
    void finish(const fs::path& directory) {
        require(completed_==(decodedInput()?dispatchedMidi_:inputs_.size()) && nextByte_==midi_.size() &&
                !session_.snapshot().queued && (work_==Work::Idle || work_==Work::Feedback || work_==Work::Mailbox),
                "K native window has pending musical work");
        require(!options_.reuseFrame || reuseComplete_,"K native window did not complete the reuse query");
        require(!arp_ || (!arp_->arp.front() && !arp_->arp.nextDeadline() && !arp_->transferring &&
                !arp_->phase.pending() && !arp_->phaseNote && (!arp_->hostBlocks || arp_->hostBlocks->finished()) &&
                          !arp_->dspClock.pending() && arp_->nextHost==host_.size()),
                "K native window has pending arp notes");
        for(size_t i=0;i<KMusicalState::VoiceCount;++i) {
            const auto voice=model_.voice(VoiceSlot(i));
            require(!voice || (!model_.activationPending(voice->allocation) && !model_.releasePending(voice->allocation)),
                    "K native window ended with an unconfirmed activation or release");
        }
        std::ofstream file(directory/"candidate-final-state.csv");file<<"key,value\n";
        const auto keys=keys_.physicalCount();
        file<<"controller_cycle,"<<physical_.cycle()<<"\nchannel,"<<unsigned(seed_.channel)<<"\nsustain,"<<(model_.sustainDown()?16:0)<<"\ninhibited,0\nheld_note,"<<(keys_.down(60)?60:255)
            <<"\nphysical60,"<<keys_.down(60)<<"\nheld60,"<<keys_.down(60)<<"\nkey_count,"<<keys<<"\nkey_order_count,"<<keys<<"\nraw_velocity60,100\n";
        for(size_t i=0;i<3;++i)file<<"control_"<<i<<','<<unsigned(source_.controls[i])<<'\n';
        if(decodedInput()) {
            const auto part=model_.part();
            file<<"bank,"<<unsigned(part.program->bank)<<"\nprogram,"<<unsigned(part.program->program)
                <<"\nprogram_revision,"<<part.revisions.program<<"\namp_attack,"<<unsigned(source_.patch[KStationCommandBackend::AmpAttack])
                <<"\ncutoff,"<<unsigned(part.parameters.cutoff)
                <<"\nvoice_limit,"<<model_.activeVoiceLimit()
                <<"\nbank_latch,"<<unsigned(backend_.bankLatch().value_or(0xff))<<"\nmidi_bytes,"<<nextByte_<<'\n';
            std::ofstream patch(directory/"candidate-final-patch.bin",std::ios::binary);
            patch.write(reinterpret_cast<const char*>(source_.patch.data()),source_.patch.size());
            require(bool(patch),"K native program output failed");
        }
        for(size_t i=0;i<8;++i) {
            const auto voice=model_.voice(VoiceSlot(i));const auto feedback=voice?model_.feedback(voice->allocation):std::nullopt;
            const auto p="voice_"+std::to_string(i)+"_";
            file<<p<<"age,"<<(feedback?feedback->age:0)<<'\n'<<p<<"countdown,"<<(feedback?feedback->countdown:seed_.voices[i].countdown)<<'\n'
                <<p<<"note,"<<unsigned(voice?voice->note:seed_.voices[i].note)<<'\n'<<p<<"velocity,"<<unsigned(voice?voice->velocity:seed_.voices[i].velocity)<<'\n';
        }
        std::ofstream metrics(directory/"candidate-metrics.csv");const auto m=port_.metrics();
        metrics<<"metric,count\ninput_events,"<<(decodedInput()?dispatchedMidi_:inputs_.size())<<"\ncompleted_events,"<<completed_<<"\nmodulation_bundles,"<<bundle_
            <<"\nfeedback_observations,"<<feedbackCount_<<"\naccepted_words,"<<m.acceptedWords<<"\nread_replies,"<<m.replies
            <<"\nrefused_commits,"<<m.refusedCommits<<"\ntimeouts,"<<m.timeouts<<"\nfaults,"<<m.faults<<"\npending_background_read,"<<bool(activeTicket_)
            <<"\nruntime_bytes,"<<sizeof(KCommandRuntime)<<"\nmodel_bytes,"<<sizeof(KMusicalState)<<"\nexpression_owner_bytes,"<<sizeof(KCommandModWheelUpdate)
            <<"\nrelease_owner_bytes,"<<sizeof(KCommandNoteOff)<<'\n';
    }
#endif
private:
    void initializeProgram(ProgramSelection initial, bool musical) {
        require(physical_.cycle()==seed_.cycle && !seed_.sustain && !seed_.inhibited &&
                (seed_.heldNote==60 || seed_.heldNote==255),"Invalid K command seed");
        KParameters parameters{seed_.patch[46], seed_.controls[0]};
#ifdef SILVER_K_COMMAND_HOST
        parameters.volume = 63;
#endif
        require(model_.loadProgram(initial,parameters),"K native program source");
        source_.patch=seed_.patch;source_.modulationCurve=seed_.modulationCurve;source_.controls=seed_.controls;
        if(musical) {
            arp_=std::make_unique<MusicalWork>(now());
            const auto settings=readKArp(source_,seed_.channel);
            require(settings && arp_->arp.configure(now(),*settings) &&
                    arp_->clock.setTempo(now(),1000000u*(64u+source_.patch[0x54])),"Invalid prepared K clock or arp");
        }
        tunings_=seed_.noteTunings;
        if(seed_.heldNote==60)keys_.press(60,100);
        for(size_t i=0;i<8;++i)if(seed_.voices[i].age) {
            const auto& v=seed_.voices[i];require(v.part==0 && v.note==60 && !v.deferred,"Unsupported K prepared voice cohort");
            require(bool(model_.assignVoice(VoiceSlot(i),v.note,v.velocity,v.age,v.countdown)),"K native allocation source");
        }
        require(model_.publish(now()),"K initial snapshot");nextPollFrame_=physical_.frames()+64;
    }

    std::array<KPolyVoiceState,8> allocationState() const {
        std::array<KPolyVoiceState,8> states{};
        for(size_t i=0;i<states.size();++i) {
            const auto voice=model_.voice(VoiceSlot(i));
            if(voice) {
                const auto feedback=model_.feedback(voice->allocation);
                if(feedback)states[i]={feedback->age,feedback->countdown};
            }
        }
        return states;
    }

#ifndef SILVER_K_COMMAND_HOST
    void beginReuseQuery() {
        reuseStartCycle_=physical_.cycle();
        reuseStartFrame_=physical_.frames()-originFrame_;
        cachedSlot_=selectKPolyVoice(allocationState(),model_.activeVoiceLimit()).slot;
        reuseActive_=true;
        scanReuse();
    }

#endif
    void scanReuse() {
        // Poly may stop at the first free slot. A retrigger can reuse a later
        // voice of the same pitch, so its refresh must cross earlier holes.
        const bool retrigger=source_.patch[KVoiceModeOffset]!=uint8_t(KVoiceMode::Poly);
        while(reuseCursor_<model_.activeVoiceLimit()) {
            const auto voice=model_.voice(VoiceSlot(reuseCursor_));
            if(!voice || !voice->feedbackAge) {
                if(!retrigger)break;
                ++reuseCursor_;
                continue;
            }
            if(model_.activationPending(voice->allocation)) {
                ++reuseCursor_;
                continue;
            }
            startFeedback(*voice,Work::ReuseFeedback);
            return;
        }
#ifndef SILVER_K_COMMAND_HOST
        const auto choice=selectKPolyVoice(allocationState(),model_.activeVoiceLimit());
#endif
        if(pendingStart_) {
            const auto start=*pendingStart_;
            pendingStart_.reset();
            reuseActive_=false;
            event("note_on_fresh_reads",reuseQueries_);
            startGroup(start.note,start.resume);
            admitOnPitch();
            return;
        }
#ifndef SILVER_K_COMMAND_HOST
        reuseTrace_<<*options_.reuseFrame<<','<<reuseStartFrame_<<','<<physical_.frames()-originFrame_<<','
                   <<reuseStartCycle_<<','<<physical_.cycle()<<','<<size_t(cachedSlot_)<<','<<size_t(choice.slot)<<','<<reuseQueries_<<'\n';
        reuseActive_=false;
        reuseComplete_=true;
#else
        throw std::logic_error("Missing K voice start during reuse query");
#endif
    }

#ifdef SILVER_K_P07_TRACE_DSP_USE
    void sampleState() {
        std::array<KPolyVoiceState,8> states{};bool changed=!haveState_;
        for(size_t i=0;i<states.size();++i) {
            const auto voice=model_.voice(VoiceSlot(i));
            const auto feedback=voice?model_.feedback(voice->allocation):std::nullopt;
            states[i]={feedback?feedback->age:seed_.voices[i].age,feedback?feedback->countdown:seed_.voices[i].countdown};
            const auto state=(uint32_t(states[i].age)<<16)|states[i].countdown;
            if(!haveState_ || state!=lastState_[i]) {
                stateTrace_<<physical_.cycle()<<','<<(physical_.frames()-originFrame_)<<','<<i<<','<<states[i].age<<','<<states[i].countdown<<'\n';
                changed=true;
            }
            lastState_[i]=state;
        }
        if(changed) {
            const auto choice=selectKPolyVoice(states,model_.activeVoiceLimit());
            selectionTrace_<<physical_.cycle()<<','<<(physical_.frames()-originFrame_)<<','<<size_t(choice.slot)<<','<<unsigned(choice.reason)<<'\n';
        }
        haveState_=true;
    }
#endif
    void startPort(DspTicket ticket) {
        require(!activeTicket_,"K native endpoint already active");activeTicket_=ticket;
        const auto offer=session_.transactions().offer(token_,Endpoint::A,now());
        auto firstAccess=physical_.cycle()+3;
#ifndef SILVER_K_COMMAND_HOST
        if(options_.minimumGateFrames && work_==Work::OffTransfer) {
            // Test-only gate hold measured from the trigger's HDI acceptance.
            require(lastTriggerTransfer_!=0,"K gate hold requires a completed trigger");
            firstAccess=std::max(firstAccess,lastTriggerTransfer_+ticksForFrames(options_.minimumGateFrames));
        }
        if(options_.triggerFrame && work_==Work::OnTransfer) {
            DspTransactionProgress transfer;
            require(session_.transactions().progress(ticket,transfer),"K probe transfer missing");
            const auto triggerAddress=KFeedbackProtocol::triggerAddress(on_.selection().slot);
            if(transfer.address.address==triggerAddress) {
                // Fixture only: release the completed note image's trigger at
                // a chosen bus boundary. The DSP still chooses when to consume it.
                const auto target=seed_.cycle+ticksForFrames(*options_.triggerFrame);
                require(firstAccess<=target,"K trigger fixture reached the bus boundary too late");
                firstAccess=target;
                event("note_trigger_bus_boundary",target);
            }
        }
#endif
        require(offer.status==DspOfferStatus::Ready && offer.ticket.sequence==ticket.sequence &&
            port_.beginAddressed(session_,offer,now(),{firstAccess},{3,3,1600000}),"K native HDI admission");
    }
#ifdef SILVER_K_COLD_PROBE_ONLY
    p09::Work exerciseWork(DspTicket ticket) const noexcept {
        switch(work_) {
        case Work::OnPitch: return p09::Work::Pitch;
        case Work::OnTransfer: {
            DspTransactionProgress progress;
            if(session_.transactions().progress(ticket,progress) &&
               progress.address.address==KFeedbackProtocol::triggerAddress(on_.selection().slot))
                return p09::Work::Trigger;
            return p09::Work::Image;
        }
        case Work::OffTransfer: return p09::Work::Release;
        case Work::Program: return p09::Work::Program;
        default: return p09::Work::Other;
        }
    }
#endif
    void applyClockSignal(MusicalClockSignal signal) {
#ifndef SILVER_K_COMMAND_HOST
        arp_->pulses<<signal.at.ticks-originFrame_<<','<<signal.pulse<<','<<signal.epoch<<','
                    <<unsigned(signal.source)<<','<<signal.tick<<','<<signal.running<<','<<signal.framesPerPulse<<'\n';
#endif
        require(arp_->arp.clock(signal),"K arp missed a clock deadline");
        observeDspClock(signal.freshMidiPeriod);
    }

    void observeDspClock(bool freshMidiPeriod=false) {
        arp_->dspClock.observe(arp_->clock,freshMidiPeriod);
    }

    bool serviceDspClock() {
        if(!arp_ || !arp_->dspClock.pending())return false;
        const auto status=arp_->dspClock.admit(session_,token_,++operation_,now());
        if(status==DspAdmission::Full)return false;
        require(status==DspAdmission::Accepted && arp_->dspClock.ticket(),"K DSP clock admission");
        work_=Work::Clock;
        startPort(*arp_->dspClock.ticket());
        return true;
    }

    void completeDspClockWord(DspTicket ticket) {
        require(arp_->dspClock.transferred(session_,ticket),"K DSP clock transfer");
        if(const auto next=arp_->dspClock.ticket())startPort(*next);
        else {
            const auto words=*arp_->dspClock.applied();
            event("dsp_clock_phase",words.phase);
            event("dsp_clock_delay",words.delay);
            work_=Work::Idle;
        }
    }

    GlobalSyncMode syncMode() const noexcept {return globalSyncMode(source_.patch[0xc7]);}

    void resetGlobalPhase() {
        const auto& patch=source_.patch;
        require(arp_->phase.append({patch[0x7a],patch[0x4f],patch[0x4e]}),"K global phase intent");
        if(arp_->phase.pending())event("global_sync_requested",unsigned(syncMode()));
    }

    bool serviceGlobalSync() {
        if(!arp_ || !arp_->phase.pending())return false;
        const auto status=arp_->phase.admit(session_,token_,++operation_,now());
        require(status==DspAdmission::Accepted && arp_->phase.ticket(),"K global phase admission");
        work_=Work::Phase;startPort(*arp_->phase.ticket());
        return true;
    }

    void completeGlobalPhase(DspTicket ticket) {
        require(arp_->phase.transferred(session_,ticket),"K global phase transfer");
        if(const auto next=arp_->phase.ticket())startPort(*next);
        else {
            event("global_sync_transferred",unsigned(syncMode()));
            work_=Work::Idle;
            if(arp_->phaseNote) {
                const auto note=*arp_->phaseNote;arp_->phaseNote.reset();
                requestStart(note,false);
                if(work_==Work::OnPitchAdmission)admitOnPitch();
            }
        }
    }

    std::optional<NativeFrame> nextHostFrame() const noexcept {
#ifdef SILVER_K_COMMAND_HOST
        MusicalWork::PendingPosition position{};
        return arp_->positions.peek(position) ? std::optional{position.at} : std::nullopt;
#else
        if(arp_->hostBlocks)return arp_->hostBlocks->nextDeadline();
        return arp_->nextHost<host_.size()?std::optional{NativeFrame{originFrame_+host_[arp_->nextHost].frame}}:std::nullopt;
#endif
    }

    std::optional<NativeFrame> nextMidiFrame() const noexcept {
#ifdef SILVER_K_COMMAND_HOST
        PendingByte byte{};
        return hostMidi_.peek(byte) ? std::optional{byte.at} : std::nullopt;
#else
        return nextByte_ < midi_.size() ? std::optional{NativeFrame{originFrame_ + midi_[nextByte_].frame}} : std::nullopt;
#endif
    }

    bool decodedInput() const noexcept {
#ifdef SILVER_K_COMMAND_HOST
        return true;
#else
        return !midi_.empty() || (arp_ && arp_->hostBlocks);
#endif
    }

    bool serviceArp() {
        if(!arp_)return false;
        if(const auto* next=arp_->arp.front()) {
            const auto note=*next;
            arp_->arp.pop();
#ifndef SILVER_K_COMMAND_HOST
            arp_->notes<<note.at.ticks-originFrame_<<','<<now().ticks-originFrame_<<','<<note.generation<<','
                       <<unsigned(note.note)<<','<<unsigned(note.velocity)<<','<<note.on<<','<<note.internal<<','<<note.midiOut<<'\n';
            if(note.midiOut)arp_->midi<<note.at.ticks-originFrame_<<','<<(unsigned(note.on?0x90:0x80)|note.channel)
                                    <<','<<unsigned(note.note)<<','<<unsigned(note.velocity)<<'\n';
#else
            if (note.midiOut) {
                const uint8_t bytes[]{uint8_t((note.on ? 0x90 : 0x80) | note.channel), note.note, note.velocity};
                midiSink_.emit(bytes, sizeof(bytes));
            }
#endif
            if(!note.internal)return true;
            if(note.on) {
                arp_->transferring=note;
                requestStart({note.channel,note.note,note.velocity},false);
                return true;
            }
            std::array<VoiceAllocation,KMusicalState::VoiceCount> owned;
            size_t count=0;
            for(auto& binding:arp_->voices) {
                if(!binding || binding->arpGeneration!=note.generation)continue;
                const auto voice=model_.voice(binding->voice.slot);
                if(!voice || !(voice->allocation==binding->voice))binding.reset();
                else if(voice->note==note.note) {
                    owned[count++]=binding->voice;
                    binding.reset();
                }
            }
            if(count) {
                arp_->transferring=note;
                require(off_.prepareOwned(session_,model_,owned.data(),count),"K arp gate ownership");
                work_=Work::OffAdmission;admitOff();
            }
            return true;
        }
        if(arp_->programAt) {
            const auto at=*arp_->programAt;
            const auto settings=readKArp(source_,seed_.channel);
            require(settings && arp_->arp.configure(at,*settings) &&
                    arp_->clock.setTempo(at,1000000u*(64u+source_.patch[0x54])),"K arp program publication");
            observeDspClock();
            arp_->programAt.reset();
        }
        const auto host=nextHostFrame();
        const auto midi = nextMidiFrame();
        const auto beforeInput=[&](std::optional<NativeFrame> at) {
            return at && !(now()<*at) && (!host || *at<*host) &&
                   (!midi || *at < *midi);
        };
        const auto clock=arp_->clock.nextDeadline(),gate=arp_->arp.nextDeadline();
        if(beforeInput(gate) && (!clock || !(*clock<*gate)))
            return arp_->arp.gateDue(*gate);
        if(beforeInput(clock)) {
            const auto signal=arp_->clock.takeDue(*clock);
            require(bool(signal),"K clock deadline did not produce a signal");
            applyClockSignal(*signal);
            return true;
        }
        if(host && !(now()<*host) && (!midi || !(*midi < *host))) {
#ifdef SILVER_K_COMMAND_HOST
            MusicalWork::PendingPosition position{};
            arp_->positions.pop(position);
            const auto signal = arp_->clock.host(*host, position.value);
            if (signal) {
                applyClockSignal(*signal);
                if (signal->reset && signal->running && signal->source == ClockAuthority::Host &&
                    syncMode() == GlobalSyncMode::SongStart)
                    resetGlobalPhase();
            } else {
                observeDspClock();
            }
#else
            if(arp_->hostBlocks) {
                return arp_->hostBlocks->dispatch(now(),[&](NativeFrame at,auto position) {
                    if(const auto signal=arp_->clock.host(at,position))applyClockSignal(*signal);
                    else observeDspClock();
                },[&](NativeFrame at,const ControllerEvent& input) {
                    require(session_.submit(token_,at,input).status==AdmissionStatus::Accepted,"K host MIDI admission");
                    dispatchScheduled();
                });
            }
            if(const auto signal=arp_->clock.host(*host,host_[arp_->nextHost].position))applyClockSignal(*signal);
            else observeDspClock();
            ++arp_->nextHost;
#endif
            return true;
        }
        return false;
    }

    void programStarted(uint8_t program, NativeFrame requested) {
        if(arp_) {
            require(arp_->arp.clear(requested),"K arp program barrier");
            arp_->programAt=requested;
        }
        work_=Work::Program;
        event("program_started",program);
    }

    bool dispatchMeta(const EventPayload& payload,NativeFrame at,NativeFrame requested) {
        if(const auto* realtime=std::get_if<MidiRealtime>(&payload)) {
            event("midi_realtime",realtime->status);
            if(arp_)if(const auto signal=arp_->clock.midi(requested,payload)) {
                applyClockSignal(*signal);
                if(realtime->status==0xfa && syncMode()==GlobalSyncMode::SongStart)resetGlobalPhase();
            }
            return true;
        }
        if(const auto* program=std::get_if<ProgramChange>(&payload)) {
            if(program->channel!=seed_.channel) {
                event("midi_foreign_channel",program->channel);
                return true;
            }
#ifdef SILVER_P12_ACTION_TRACE
            event("program_dispatched",program->program);
#endif
            const auto status=backend_.beginProgram(program->program,source_,tunings_,programTables_,at);
            using R=KStationCommandBackend::ProgramResult;
            switch(status) {
            case R::Started:programStarted(program->program,requested);break;
            case R::MissingCurrentBank:event("program_missing_current",program->program);break;
            case R::MissingDestination:event("program_missing_destination",program->program);break;
            case R::Unsupported:event("program_unsupported",program->program);break;
            }
            return true;
        }
        if(const auto* cc=std::get_if<ControlChange>(&payload)) {
            if(cc->channel!=seed_.channel) {
                event("midi_foreign_channel",cc->channel);
                return true;
            }
            if(cc->controller==32) {
                event(backend_.selectBank(cc->value)?"bank_latched":"bank_ignored",cc->value);
                return true;
            }
        }
        if(const auto* common=std::get_if<MidiCommon>(&payload)) {
            if(arp_ && common->status==0xf2) {
                if(const auto signal=arp_->clock.midi(requested,payload))applyClockSignal(*signal);
                return true;
            }
            event("midi_common_unsupported",common->status);
            return true;
        }
        if(const auto* sysEx=std::get_if<SysExBytes>(&payload)) {
#ifdef SILVER_P12_ACTION_TRACE
            event("midi_sysex_dispatched",sysEx->size);
#endif
            if(writableFlash_) dispatchSysEx(*sysEx,at,requested);
            else event("midi_sysex_unsupported",sysEx->size);
            return true;
        }
        return false;
    }

    void dispatchSysEx(SysExBytes bytes, NativeFrame at, NativeFrame requested) {
        const auto frame=NovationProgramSysEx::decode(Model::K,seed_.channel,bytes);
        if(!frame) {
            if(NovationProgramSysEx::identityRequest(seed_.channel,bytes))event("midi_identity_unavailable",0);
            else if(NovationProgramSysEx::addressed(Model::K,seed_.channel,bytes) &&
                    !NovationProgramSysEx::knownType(Model::K,bytes.data[7]))
                event(NovationProgramSysEx::firmwareType(bytes.data[7])?"midi_firmware_update_unsupported":"midi_sysex_unsupported",
                      bytes.data[7]);
            else event("midi_sysex_rejected",bytes.size);
            return;
        }
        const auto selected=*model_.part().program;
        if(frame->type==0) {
            if(backend_.beginProgramRecord(selected,frame->data,source_,tunings_,programTables_,at)==
               KStationCommandBackend::ProgramResult::Started) programStarted(selected.program,requested);
            else event("midi_sysex_rejected",frame->type);
            return;
        }
        const auto bank=frame->control?frame->bank:selected.bank;
        if(frame->type==1 || frame->type==2 || frame->type==3) {
            const auto stored=frame->type==3 ? writableFlash_->storeGlobals(frame->data) :
                writableFlash_->storeProgram({bank,frame->program},frame->data,frame->type==2);
            if(stored && frame->type==3) {
                std::copy_n(frame->data,23,source_.patch.begin()+0xb6);
                seed_.channel=frame->data[4];
            }
            event(stored?"midi_sysex_stored":"midi_sysex_write_rejected",frame->type);
            return;
        }
        const uint8_t* data=nullptr;
        uint8_t replyBank=0;
        std::array<uint8_t,KProgramFlash::GlobalBytes> globals;
        switch(frame->type) {
        case 0x40: data=source_.patch.data(); break;
        case 0x41: data=backend_.programRecord({bank,frame->program});replyBank=bank;break;
        case 0x42:
            if (!(frame->program&1) && frame->program<99 &&
                backend_.programRecord({bank,uint8_t(frame->program+1)}))
                data=backend_.programRecord({bank,frame->program});
            replyBank=bank;break;
        case 0x43:
            std::transform(writableFlash_->globalPage(),writableFlash_->globalPage()+globals.size(),globals.begin(),
                           [](uint8_t value){return uint8_t(value&127);});
            data=globals.data();break;
        default:event("midi_sysex_unsupported",frame->type);return;
        }
        if(!data){event("midi_sysex_missing",frame->type);return;}
        const auto replyType=uint8_t(frame->type-0x40);
        const bool bankReply=replyType==1 || replyType==2;
        const auto reply=NovationProgramSysEx::encode(Model::K,replyType,bankReply?frame->control:0,replyBank,
            bankReply?frame->program:0,data,NovationProgramSysEx::payloadBytes(Model::K,replyType));
#ifndef SILVER_K_COMMAND_HOST
        if (sysExOut_)
            sysExOut_->write(reinterpret_cast<const char*>(reply.data()),std::streamsize(reply.size()));
#else
        midiSink_.emit(reply.data(), reply.size());
#endif
        event("midi_sysex_reply",replyType);
    }

    void prepareRelease(uint8_t note,uint8_t excludedSlots=0) {
        require(note<128,"K release pitch");
        if(isKMono(source_.patch[KVoiceModeOffset]) && monoNote_==note) {
            if(const auto previous=keys_.nearestHeld(note)) {
                keys_.release(note,model_.sustainDown()&&!sustainBypass());
                event("key_released",note);
                event("mono_return",previous->note);
                requestStart({seed_.channel,previous->note,previous->velocity},true);
                return;
            }
        }
        require(off_.prepare(session_,model_,note,sustainBypass(),excludedSlots),"K native release intent");
        releasedKey_=note;
        work_=Work::OffAdmission;event("release_intent",note);admitOff();
    }

    void completeLive(std::optional<DspReply> reply) {
        require(live_.finish(reply,now(),source_,tunings_),"K live parameter publication");
        if(live_.resetsKeys()) {
            keys_.clear();
            monoNote_.reset();
        }
        event("live_applied",uint8_t(liveEdit_.kind));
        require(live_.reset(),"K live update reset");
        if(liveFromInput_)++completed_;
        work_=Work::Idle;
    }
    void startLive(KLiveEdit edit,bool fromInput=true) {
        liveEdit_=edit;liveFromInput_=fromInput;
        if(!live_.prepare(session_,model_,source_,tunings_,programTables_,edit,now())) {
            require(fromInput && edit.kind==KLiveKind::Patch,"K live edit preparation failed");
            event("parameter_value_unsupported",edit.offset);
            ++completed_;
            return;
        }
        event("live_begin",uint8_t(edit.kind));
        event("live_write_count",live_.size());
        if(!live_.size())completeLive(std::nullopt);
        else work_=Work::Live;
    }
    void applyClockPolicy(NativeFrame at, uint8_t value) {
        constexpr std::array policies{ClockPolicy::Auto,ClockPolicy::Internal,ClockPolicy::ExternalMidi};
        if (value >= policies.size())
            return;
        if (const auto signal = arp_->clock.setPolicy(at, policies[value]))
            applyClockSignal(*signal);
        source_.patch[0xc0] = value;
    }

    void applyParameter(std::optional<KParameterValue> edit,NativeFrame requested) {
        if(!edit) {
            event("parameter_unmapped",0);++completed_;return;
        }
        const auto offset=edit->offset;
        if(arp_ && offset==0xc7) {
            if(edit->value<=2)source_.patch[offset]=edit->value;
            else event("parameter_value_unsupported",offset);
            ++completed_;
            return;
        }
        if(arp_ && (offset==0xc0 || (offset>=0x54 && offset<=0x58))) {
            if(offset==0xc0 && edit->value<3) {
                applyClockPolicy(requested, edit->value);
                event("clock_policy_applied",edit->value);
            } else if(offset!=0xc0) {
                const auto next=editKProgram(source_,*edit);
                if(const auto settings=readKArp(next,seed_.channel)) {
                    require(arp_->arp.configure(requested,*settings) &&
                            arp_->clock.setTempo(requested,1000000u*(64u+next.patch[0x54])),"K arp parameter deadline");
                    source_=next;
                    observeDspClock();
                    event("arp_parameter_applied",offset);
                } else event("parameter_value_unsupported",offset);
            } else event("parameter_value_unsupported",offset);
            ++completed_;
            return;
        }
        // MIDI setup, clock/arp settings and UI configuration have their
        // own package owners. Keep them distinguishable from sound edits.
        const bool musicalOffset=offset<128 || (offset>=0xc2 && offset<=0xc6);
        if(!musicalOffset || (offset>=0x54 && offset<=0x58)) {
            event("parameter_other_package",offset);++completed_;return;
        }
        if((offset==0x49 || offset==0x4c) && edit->value>tunings_.lfoRates.size()) {
            event("parameter_value_unsupported",offset);++completed_;return;
        }
        event("parameter_offset",offset);
        startLive({KLiveKind::Patch,edit->value,offset});
    }

    DispatchDecision dispatch(const ScheduledEvent& input,NativeFrame at) {
        if(decodedInput()) {
            ++dispatchedMidi_;
            if(dispatchMeta(input.event.payload,at,input.requestedTime)) {
                if(work_!=Work::Program)++completed_;
                return {};
            }
        }
        if(const auto* cc=std::get_if<ControlChange>(&input.event.payload)) {
            require(cc->channel==seed_.channel && cc->value<128,"Unsupported K vertical control");
            if(cc->controller==64) {
                if(arp_)require(arp_->arp.pedal(input.requestedTime,cc->value>=64),"K arp sustain deadline");
                require(off_.preparePedal(session_,model_,cc->value,sustainBypass()),"K native sustain intent");
                releasedKey_.reset();
                work_=Work::OffAdmission;event("pedal_intent",cc->value);admitOff();
                return {};
            }
            if(cc->controller!=1) {
                switch(cc->controller) {
                case 2:startLive({KLiveKind::Breath,cc->value});return {};
                case 7:startLive({KLiveKind::Volume,cc->value});return {};
                case 98:nrpn_=cc->value;event("nrpn_selected",cc->value);++completed_;return {};
                case 99:case 100:case 101:case 121:
                    event("midi_control_noop",cc->controller);++completed_;return {};
                case 6:applyParameter(nrpn_?parameterMap_.nrpn(*nrpn_,cc->value,source_):std::nullopt,input.requestedTime);return {};
                case 120:
                    if(!cc->value) {
                        clearMusicalKeys(input.requestedTime);
                        startLive({KLiveKind::Silence});
                    }
                    else {event("midi_control_noop",cc->controller);++completed_;}
                    return {};
                case 123:case 124:case 125:case 126:case 127:
                    if(!cc->value || cc->controller==126) {
                        clearMusicalKeys(input.requestedTime);
                        require(off_.prepareAll(session_,model_),"K all-notes-off intent");
                        releasedKey_.reset();allNotesOff_=true;
                        work_=Work::OffAdmission;admitOff();
                    } else {event("midi_control_noop",cc->controller);++completed_;}
                    return {};
                default:applyParameter(parameterMap_.control(cc->controller,cc->value),input.requestedTime);return {};
                }
            }
            auto source=source_;source.controls[0]=cc->value;
            std::array<KExpressionVoice,8> voices{};size_t count=0;
            for(size_t i=0;i<8;++i)if(const auto voice=model_.voice(VoiceSlot(i)))voices[count++]={voice->allocation,voice->feedbackAge};
            const auto packet=compileKModWheelPacket(token_,source,voices.data(),count);
            require(packet && mod_.prepare(session_,model_,*packet,cc->value),"K native immutable modulation source");
            ++bundle_;
#ifndef SILVER_K_COMMAND_HOST
            if(!options_.cpuWindow)for(size_t i=0;i<packet->size();++i){const auto* w=packet->at(i);packets_<<bundle_<<','<<unsigned(w->endpoint)<<','<<w->address<<','<<w->word<<'\n';}
#endif
            pendingWheel_=cc->value;work_=Work::ModAdmission;event("modulation_intent",cc->value);admitMod();
        } else if(const auto* note=std::get_if<NoteOff>(&input.event.payload)) {
            if(decodedInput() && note->channel!=seed_.channel) {
                event("midi_foreign_channel",note->channel);++completed_;return {};
            }
            require(note->channel==seed_.channel,"K native release channel");
            releaseKey(note->note,input.requestedTime);
        } else if(const auto* note=std::get_if<NoteOn>(&input.event.payload)) {
            if(decodedInput() && note->channel!=seed_.channel) {
                event("midi_foreign_channel",note->channel);++completed_;return {};
            }
            if(!note->velocity) {
                releaseKey(note->note,input.requestedTime);
                return {};
            }
            require(note->channel==seed_.channel && note->note<128,"K NoteOn channel or pitch");
            if(arp_ && arp_->phaseKeys.press(note->note,syncMode()))resetGlobalPhase();
            if(arp_ && arp_->arp.enabled()) {
                require(arp_->arp.press(input.requestedTime,note->note,note->velocity),"K arp held key deadline");
                ++completed_;return {};
            }
            if(arp_ && arp_->phase.pending())arp_->phaseNote=*note;
            else requestStart(*note,false);
        } else if(const auto* pressure=std::get_if<ChannelPressure>(&input.event.payload)) {
            if(pressure->channel==seed_.channel)startLive({KLiveKind::Pressure,pressure->pressure});
            else {event("midi_foreign_channel",pressure->channel);++completed_;}
        } else if(const auto* bend=std::get_if<PitchBend>(&input.event.payload)) {
            if(bend->channel==seed_.channel)startLive({KLiveKind::Pitch,bend->value});
            else {event("midi_foreign_channel",bend->channel);++completed_;}
        } else if(const auto* pressure=std::get_if<PolyPressure>(&input.event.payload)) {
            event("midi_poly_pressure_ignored",pressure->note);++completed_;
        } else throw std::runtime_error("Unsupported K vertical input");
        return {};
    }

    void clearMusicalKeys(NativeFrame at) {
        if(arp_) {
            arp_->phaseKeys.clear();
            require(arp_->arp.clear(at),"K arp panic deadline");
        }
    }

    void releaseKey(uint8_t note,NativeFrame at) {
        uint8_t excludedSlots=0;
        if(arp_) {
            arp_->phaseKeys.release(note);
            if(arp_->arp.enabled()) {
                require(arp_->arp.release(at,note),"K arp key release deadline");
                if(!keys_.down(note)){++completed_;return;}
            }
            for(const auto& binding:arp_->voices)if(binding) {
                const auto voice=model_.voice(binding->voice.slot);
                if(voice && voice->allocation==binding->voice)
                    excludedSlots|=uint8_t(1u<<size_t(binding->voice.slot));
            }
        }
        prepareRelease(note,excludedSlots);
    }

    void requestStart(NoteOn note,bool resume) {
#ifndef SILVER_K_COMMAND_HOST
        if (!options_.freshStart) {
            startGroup(note, resume);
            return;
        }
#endif
        pendingStart_=RequestedStart{note,resume};
        reuseActive_=true;
        reuseCursor_=reuseQueries_=0;
    }

    void startGroup(NoteOn note,bool resume) {
        group_={note,resume};
        const auto units=std::min(size_t(source_.patch[KUnisonOffset])+1,model_.activeVoiceLimit());
        require(units!=0 && units<=8,"K invalid voice group");
        start_={uint8_t(units-1),0,isKMono(source_.patch[KVoiceModeOffset]) && (resume || keys_.anyHeld())};
        prepareStart();
    }

    void prepareStart() {
        const auto& note=group_.note;
        require(on_.prepare(session_,model_,source_,tunings_,note.note,note.velocity,start_),"K native NoteOn intent");
        event("note_on_note",note.note);
        event("note_on_slot",size_t(on_.selection().slot));
        event("note_on_steal",on_.selection().reason==KPolyVoiceSelectionReason::Oldest);
        if(on_.selection().reason==KPolyVoiceSelectionReason::SameNote)
            event(isKMono(source_.patch[KVoiceModeOffset])?"mono_reuse_slot":"note_on_retrigger_slot",size_t(on_.selection().slot));
        if(source_.patch[KUnisonOffset])event("unison_unit",start_.unit);
        if(on_.selection().reason==KPolyVoiceSelectionReason::Oldest) {
            const auto victim=model_.voice(on_.selection().slot);
            require(bool(victim),"K voice stealing without a live victim");
            event("note_on_victim",victim->note);
        }
        work_=Work::OnPitchAdmission;
    }

    void admitOnPitch() {
        const auto status=on_.admitPitch(++operation_,now());
        if(status==DspAdmission::Full)return;
        require(status==DspAdmission::Accepted && on_.ticket(),"K NoteOn pitch query admission");
        work_=Work::OnPitch;
        startPort(*on_.ticket());
    }

    void admitOnImage() {
        const auto status=on_.admitImage(++operation_,now());
        if(status==DspAdmission::Full)return;
        require(status==DspAdmission::Accepted && on_.ticket(),"K NoteOn image admission");
        if(!group_.resume && !start_.excludedSlots && !(arp_ && arp_->transferring))
            keys_.press(group_.note.note,group_.note.velocity);
        if(isKMono(source_.patch[KVoiceModeOffset]))monoNote_=group_.note.note;
        tunings_.retainedGlideNote=on_.glideNote();
        work_=Work::OnTransfer;
        startPort(*on_.ticket());
    }

    void progressOn() {
        const auto status=on_.progress();
        if(status==KWriteProgress::Complete) {
            lastTriggerTransfer_=physical_.cycle();
            const auto slot=on_.selection().slot;
            const auto voice=model_.voice(slot);
            const auto trigger=physical_.watchVoiceTrigger(uint8_t(size_t(slot)),on_.trigger());
            require(voice && trigger,"K physical activation watch could not bind the accepted trigger");
            std::optional<NativeFrame> arpDeadline;
            if(arp_ && arp_->transferring) {
                arp_->voices[size_t(slot)]=MusicalWork::Binding{arp_->transferring->generation,voice->allocation};
                arpDeadline=arp_->transferring->at;
                event("arp_gate_on_latency_frames",now().ticks-arpDeadline->ticks);
            }
            auto& watch=activations_[size_t(slot)];
            if(watch)event("activation_watch_replaced_slot",size_t(slot));
            auto observeAt = lastTriggerTransfer_;
#ifndef SILVER_K_COMMAND_HOST
            observeAt += ticksForFrames(options_.activationObservationDelay);
#endif
            watch=WatchedActivation{voice->allocation,*trigger,observeAt,arpDeadline};
            event("activation_watch_slot",size_t(slot));
#ifdef SILVER_K_COLD_PROBE_ONLY
            if(exercise_ && model_.activationPending(voice->allocation))
                exercise_->at(p09::Work::Activation,p09::Boundary::Logical,Endpoint::A,nullptr,voice->allocation);
#endif
            event("note_on_complete",group_.note.note);
            require(on_.reset(),"K NoteOn owner reset");
            if(start_.unit) {
                start_.excludedSlots=uint8_t(start_.excludedSlots | (1u<<size_t(slot)));
                --start_.unit;
                prepareStart();
                return;
            }
            if(arp_ && arp_->transferring)arp_->transferring.reset();
            else ++completed_;
            work_=Work::Idle;
        } else {
            require(status==KWriteProgress::Ready && on_.ticket(),"K NoteOn image progress fault");
            startPort(*on_.ticket());
        }
    }
    void admitMod() {
        const auto status=mod_.admit(++operation_,now());if(status==DspAdmission::Full)return;
        require(status==DspAdmission::Accepted,"K modulation reservation rejected");
        if(mod_.next()==0){source_.controls[0]=pendingWheel_;event("modulation_source_committed",pendingWheel_);}
        work_=Work::ModTransfer;require(bool(mod_.ticket()),"K modulation missing transfer");startPort(*mod_.ticket());
    }
    void progressMod() {
        const auto status=mod_.progress();
        if(status==KWriteProgress::Complete){require(mod_.reset(),"K modulation owner reset");++completed_;work_=Work::Idle;event("modulation_complete",completed_);}
        else {require(status==KWriteProgress::Ready,"K modulation progress fault");work_=Work::ModAdmission;admitMod();}
    }
    bool sustainBypass() const { return (source_.patch[0x58]&4)!=0; }

    void observeActivations() {
        for(auto& watch:activations_) {
            if(!watch)continue;
            if(!model_.activationPending(watch->allocation)) {
                watch.reset();
                continue;
            }
            if(physical_.cycle()<watch->observeAt || !physical_.voiceActivated(watch->trigger))continue;
            require(model_.confirmActivation(watch->allocation,now()),"K native activation confirmation rejected");
            event("activation_confirmed_slot",size_t(watch->allocation.slot));
            if(watch->arpDeadline)
                event("arp_activation_latency_frames",now().ticks-watch->arpDeadline->ticks);
            watch.reset();
        }
    }

    bool startConfirmedReleases() {
        if(!model_.hasReadyRelease())return false;
        require(off_.preparePending(session_,model_),"K pending release preparation");
        pendingOff_=true;work_=Work::OffAdmission;admitOff();
        return true;
    }

    void admitOff() {
        const auto status=off_.admit(++operation_,now());if(status==DspAdmission::Full)return;
        require(status==DspAdmission::Accepted,"K release batch reservation rejected");
        if(!pendingOff_ && !(arp_ && arp_->transferring)) {
            if(allNotesOff_) {
                keys_.clear();monoNote_.reset();allNotesOff_=false;
                event("all_notes_off",0);
            } else if(releasedKey_) {
                keys_.release(*releasedKey_,model_.sustainDown()&&!sustainBypass());
                event("key_released",*releasedKey_);
            } else {
                if(!model_.sustainDown())keys_.liftPedal();
                event("pedal_applied",model_.sustainDown());
            }
        }
#ifndef SILVER_K_COMMAND_HOST
        if(options_.lifecycleTrace) {
            for(size_t i=0;i<off_.size();++i)event("gate_queued_slot",size_t(off_.allocation(i)->slot));
            for(size_t i=0;i<KMusicalState::VoiceCount;++i) {
                const auto voice=model_.voice(VoiceSlot(i));
                if(voice && model_.releasePending(voice->allocation))event("release_waiting_slot",i);
            }
        }
#endif
        work_=Work::OffTransfer;
#ifdef SILVER_K_P07_TRACE_DSP_USE
        sampleState();
#endif
        if(const auto ticket=off_.ticket())startPort(*ticket);else progressOff();
    }
    void progressOff() {
        const auto status=off_.progress();
        if(status==KWriteProgress::Complete){
            require(off_.reset(),"K release owner reset");
            if(pendingOff_)event("deferred_gate_complete",0);
            else if(arp_ && arp_->transferring)arp_->transferring.reset();
            else ++completed_;
            pendingOff_=false;work_=Work::Idle;event("release_complete",completed_);
        }
        else {require(status==KWriteProgress::Ready || status==KWriteProgress::WaitingForTransfer,"K release progress fault");require(bool(off_.ticket()),"K release lost next gate");startPort(*off_.ticket());}
    }
    void startFeedback(const KMusicalState::Voices::Voice& voice,Work work) {
        require(feedback_.prepare(session_,model_,voice.allocation) &&
                feedback_.admit(++operation_,now())==DspAdmission::Accepted,"K feedback reservation");
        event(work==Work::ReuseFeedback?"reuse_feedback_slot":"feedback_slot",size_t(voice.allocation.slot));
        work_=work;
        startPort(*feedback_.ticket());
    }

    void scanFeedback() {
        while(pollCursor_<8) {
            const auto slot=VoiceSlot(pollCursor_++);const auto voice=model_.voice(slot);
            if(!voice || !voice->feedbackAge || model_.activationPending(voice->allocation))continue;
            startFeedback(*voice,Work::Feedback);
            return;
        }
        pollActive_=false;pollCursor_=0;
        const auto draft=KFeedbackProtocol::mailboxRead(now());DspTicket ticket;
        require(session_.transactions().submit(token_,++operation_,&draft,1,&ticket,[]()noexcept{})==DspAdmission::Accepted,"K mailbox read admission");
#ifdef SILVER_P12_ACTION_TRACE
        event("mailbox_requested",0x80802b);
#endif
        work_=Work::Mailbox;startPort(ticket);
    }
    void pumpProgram() {
        require(!backend_.programTimedOut(now()),"K DSP program transition timed out");
        if(!backend_.nextProgramTransfer(now()))return;
        DspTicket ticket;
        require(backend_.submitProgramTransfer(++operation_,now(),ticket)==DspAdmission::Accepted,
                "K program transfer admission");
        startPort(ticket);
    }
    void pump() {
#ifdef SILVER_K_COLD_PROBE_ONLY
        const auto before=exercise_ ? port_.metrics() : HdiWordMetrics{};
        const auto ticketAtIo=activeTicket_;
        if(exercise_ && ticketAtIo && !session_.transactions().started(*ticketAtIo))
            p09::beforeIo(*exercise_,session_,*ticketAtIo,exerciseWork(*ticketAtIo));
#endif
        const auto status=port_.pump(session_,{physical_.cycle()},now());
#ifdef SILVER_K_COLD_PROBE_ONLY
        if(exercise_ && ticketAtIo)
            p09::afterIo(*exercise_,session_,port_,*ticketAtIo,exerciseWork(*ticketAtIo),before,status);
#endif
        if(status==HdiWordStatus::ReplyQueued) {
            struct Delivery{DspReply reply{};NativeFrame at{};}delivery;
            const MusicalDispatch handler{&delivery,[](void* p,CommandSession&,const ScheduledEvent& event,NativeFrame)noexcept {
                const auto* reply=std::get_if<DspReply>(&event.event.payload);
                if(!reply)return DispatchDecision{DispatchDecision::Status::Fault,{}};
                *static_cast<Delivery*>(p)={*reply,event.requestedTime};return DispatchDecision{};
            }};
            require(session_.runUntil(now(),1,handler).applied==1,"K native feedback reply dispatch failed");
            if(work_==Work::OnPitch) {
                require(on_.acceptPitch(delivery.reply,now()),"K NoteOn pitch reply or image rejected");
                event("note_on_pitch_reply",delivery.reply.word);
                activeTicket_.reset();
                work_=Work::OnImageAdmission;
                admitOnImage();
                return;
            }
            if(work_==Work::Program) {
                require(bool(activeTicket_),"K program reply without active ticket");
                event("program_mailbox",delivery.reply.word);
                const auto status=backend_.acceptProgramReply(*activeTicket_,delivery.reply,now(),source_);
                activeTicket_.reset();
                require(status!=KStationCommandBackend::ReloadProgress::Fault,"K program reply rejected");
                if(status==KStationCommandBackend::ReloadProgress::DspReady)event("program_dsp_ready",delivery.reply.word);
                if(status==KStationCommandBackend::ReloadProgress::Applied) {
                    activations_.fill(std::nullopt);
                    if(arp_) {
                        arp_->voices.fill(std::nullopt);
                        arp_->phaseKeys.programApplied(syncMode());
                    }
                    keys_.detachProgram();
                    monoNote_.reset();
                    for(auto& voice:seed_.voices)voice={};
                    const auto part=model_.part();
                    tunings_.partTranspose=source_.patch[0xad];
                    event("program_applied",uint64_t(part.program->bank)*256+part.program->program);
                    event("program_revision",part.revisions.program);
                    event("program_voice_limit",model_.activeVoiceLimit());
#ifdef SILVER_K_P07_TRACE_DSP_USE
                    sampleState();
#endif
                    work_=Work::Idle;
                    ++completed_;
                }
                return;
            }
            if(work_==Work::Live) {
                activeTicket_.reset();
                completeLive(delivery.reply);
                return;
            }
            if(work_==Work::Mailbox) {
                const auto value=KFeedbackProtocol::mailboxResult(delivery.reply.word);
                require(activeTicket_ && delivery.reply.transaction==activeTicket_->sequence && value &&
                    session_.transactions().retire(*activeTicket_),"K mailbox reply correlation");
                event("mailbox_status",value->status);
                if(value->action==KMailboxAction::AcknowledgeAndRebuild) {
                    activeTicket_.reset();
                    startLive({KLiveKind::Rebuild},false);
                    return;
                }
            }else {
                require((work_==Work::Feedback || work_==Work::ReuseFeedback) &&
                        feedback_.accept(delivery.reply,now())==KFeedbackProgress::Complete,"K native feedback reply rejected");
                event("feedback_word",delivery.reply.word);++feedbackCount_;require(feedback_.reset(),"K feedback owner reset");
                if(work_==Work::ReuseFeedback) {
                    ++reuseQueries_;
                    const auto updated=model_.voice(VoiceSlot(reuseCursor_));
                    if(updated && updated->feedbackAge)++reuseCursor_;
                }
#ifdef SILVER_K_P07_TRACE_DSP_USE
                sampleState();
#endif
            }
            activeTicket_.reset();work_=Work::Idle;
        } else if(status==HdiWordStatus::WordAccepted) {
            DspTransactionProgress p;require(activeTicket_ && session_.transactions().progress(*activeTicket_,p),"K transfer progress missing");
            if(p.status!=DspStatus::Transferred)return;
            const auto ticket=*activeTicket_;
            activeTicket_.reset();
            if(work_==Work::ModTransfer)progressMod();
            else if(work_==Work::OffTransfer)progressOff();
            else if(work_==Work::OnTransfer)progressOn();
            else if(work_==Work::Program)
                require(backend_.finishProgramTransfer(ticket,now())==KStationCommandBackend::ReloadProgress::Pending,
                        "K native program transfer incomplete");
            else if(work_==Work::Live)
                require(live_.transferred()==KWriteProgress::Ready,"K native live edit transfer failed");
            else if(work_==Work::Clock)completeDspClockWord(ticket);
            else if(work_==Work::Phase)completeGlobalPhase(ticket);
            else require(false,"K unexpected data transfer");
        } else require(status==HdiWordStatus::Progress || status==HdiWordStatus::WaitingForBus || status==HdiWordStatus::WaitingForReadiness,
            "K native physical transport fault");
    }

    void dispatchScheduled() {
        struct Call { KCommandRuntime& window; std::exception_ptr failure; } call{*this,{}};
        const MusicalDispatch handler{&call,[](void* p,CommandSession&,const ScheduledEvent& e,NativeFrame at)noexcept {
            auto& call=*static_cast<Call*>(p);
            try { return call.window.dispatch(e,at); }
            catch (...) { call.failure=std::current_exception(); return DispatchDecision{DispatchDecision::Status::Fault,{}}; }
        }};
        const auto result=session_.runUntil(now(),1,handler);
        if(call.failure)std::rethrow_exception(call.failure);
        require(result.applied==1 && result.status!=PumpStatus::Fault,"K native musical dispatch failed");
#ifdef SILVER_P12_ACTION_TRACE
        event("scheduled_musical_event",result.applied);
#endif
        // Physical reply admission follows the completion of musical dispatch.
        if(reuseActive_)scanReuse();
        else if(work_==Work::OnPitchAdmission)admitOnPitch();
    }

    bool dispatchMidiBytes() {
        unsigned budget=32;
        while (budget--) {
#ifdef SILVER_K_COMMAND_HOST
            PendingByte byte{};
            if (!hostMidi_.peek(byte)) return false;
            if (!byte.raw) {
                const auto result = session_.submit(token_, byte.at, byte.control);
                if (result.status == AdmissionStatus::Full)
                    return false;
                require(result.status == AdmissionStatus::Accepted, "K rejected a prepared host control");
                hostMidi_.pop(byte);
                dispatchScheduled();
                return true;
            }
            const auto admitted = backend_.pushMidiByte(byte.at, byte.value);
            if (!admitted.consumed) return false;
            hostMidi_.pop(byte);
#else
            if (nextByte_ == midi_.size()) return false;
            const auto& byte=midi_[nextByte_];
            if(physical_.cycle()<seed_.cycle+ticksForFrames(byte.frame) || now().ticks<originFrame_+byte.frame)return false;
            const auto admitted=backend_.pushMidiByte({originFrame_+byte.frame},byte.value);
            require(admitted.consumed,"K research MIDI stream backpressure");
            ++nextByte_;
#endif
            if(admitted.decode!=MidiDecoder::Result::Consumed)event("midi_decode_rejected",unsigned(admitted.decode));
            if(admitted.admission.sequence) {
                dispatchScheduled();
                return true;
            }
        }
        return false;
    }

    void ready() {
#ifdef SILVER_P12_ACTION_TRACE
        P12ActionTrace::worker().readyCheck();
#endif
        if(activeTicket_) {
            pump();
            if(activeTicket_)return;
#ifndef SILVER_K_COMMAND_HOST
            if(options_.delivery!=DeliveryPolicy::Priority && !reuseActive_)return;
#endif
        }
        if(work_==Work::ModAdmission){admitMod();return;}
        if(work_==Work::OffAdmission){admitOff();return;}
        if(work_==Work::OnPitchAdmission){admitOnPitch();return;}
        if(work_==Work::OnImageAdmission){admitOnImage();return;}
        if(work_==Work::Program){pumpProgram();return;}
        if(work_==Work::Live) {
            const auto status=live_.submit(++operation_,now());
            if(status==DspAdmission::Full)return;
            require(status==DspAdmission::Accepted && live_.ticket(),"K live transfer admission");
            startPort(*live_.ticket());return;
        }
        require(work_==Work::Idle,"K native operation has no transfer");
        observeActivations();
        if(startConfirmedReleases())return;
        if(reuseActive_) {scanReuse();return;}
        if(serviceGlobalSync())return;
        while(serviceArp()) {
            if(work_==Work::OnPitchAdmission)admitOnPitch();
#ifndef SILVER_K_COMMAND_HOST
            if(!arp_->hostBlocks || work_!=Work::Idle || reuseActive_ || arp_->phase.pending())return;
#else
            if(work_!=Work::Idle || reuseActive_ || arp_->phase.pending())return;
#endif
        }
        if(dispatchMidiBytes())return;
        if(serviceDspClock())return;
#ifndef SILVER_K_COMMAND_HOST
        if(nextInput_<inputs_.size() && deadlines_[nextInput_]<=physical_.cycle()) {
            if(options_.delivery!=DeliveryPolicy::Legacy)event("midi_eligible",deadlines_[nextInput_]);
            const auto& input=inputs_[nextInput_++];ControllerEvent event;
            if(input.kind==InputKind::ModWheel)event.payload=ControlChange{seed_.channel,1,input.value};
            else if(input.kind==InputKind::Sustain)event.payload=ControlChange{seed_.channel,64,input.value};
            else if(input.kind==InputKind::NoteOn)event.payload=NoteOn{seed_.channel,input.value,input.velocity};
            else event.payload=NoteOff{seed_.channel,input.kind==InputKind::Release?uint8_t(60):input.value,0};
            require(session_.submit(token_,now(),event).status==AdmissionStatus::Accepted,"K native complete-event admission");
            dispatchScheduled();
            return;
        }
        if(options_.reuseFrame && !reuseComplete_ && physical_.frames()-originFrame_>=*options_.reuseFrame) {
            require(nextInput_==inputs_.size() && !keys_.down(60),"K reuse probe requires completed release and no later input");
            beginReuseQuery();
            return;
        }
        // Keep new background reads out of an experimental three-frame guard
        // before known musical work. An in-flight physical reply drains normally.
        if(options_.delivery==DeliveryPolicy::Priority && nextInput_<inputs_.size() &&
           deadlines_[nextInput_]-physical_.cycle()<=ticksForFrames(3))return;
#endif
        if(physical_.frames()>=nextPollFrame_) {
            nextPollFrame_+=((physical_.frames()-nextPollFrame_)/64+1)*64;pollActive_=true;
        }
        if(pollActive_)scanFeedback();
    }
    kc::PhysicalTimeline& physical_;
    KCommandSeed seed_;
#ifndef SILVER_K_COMMAND_HOST
    const std::vector<VerticalInput>& inputs_;
    ProbeOptions options_;
    std::vector<uint64_t> deadlines_;
#endif
    uint64_t originFrame_;
#ifdef SILVER_K_COMMAND_HOST
    kstation::Queue<PendingByte, 8192> hostMidi_;
#else
    const std::vector<MidiByte>& midi_;
#endif
    KStationCommandBackend backend_;
    CommandSession& session_;Session token_;KMusicalState& model_;
    KProgramTables programTables_;
    const KMidiParameters& parameterMap_;
    DspWordTransport port_;
#ifndef SILVER_K_COMMAND_HOST
    const std::vector<HostPosition>& host_;
#endif
    KExpressionSource source_{};
    KVoiceTunings tunings_{};
    KCommandModWheelUpdate mod_;KCommandNoteOff off_;KCommandNoteOn on_;KCommandVoiceFeedback feedback_;
    KCommandLiveUpdate live_;
    KLiveEdit liveEdit_{KLiveKind::Patch};
    std::optional<uint8_t> nrpn_;
    std::unique_ptr<MusicalWork> arp_;
    KKeyState keys_;
    struct RequestedStart {NoteOn note{};bool resume=false;};
    std::optional<RequestedStart> pendingStart_;
    RequestedStart group_;
    KVoiceStart start_;
    std::optional<uint8_t> monoNote_;
    std::optional<uint8_t> releasedKey_;
    struct WatchedActivation {
        VoiceAllocation allocation;
        kc::PhysicalTimeline::VoiceTrigger trigger;
        uint64_t observeAt;
        std::optional<NativeFrame> arpDeadline;
    };
    std::array<std::optional<WatchedActivation>,KMusicalState::VoiceCount> activations_;
    bool pendingOff_=false,allNotesOff_=false,liveFromInput_=false;
#ifndef SILVER_K_COMMAND_HOST
    std::ofstream events_,packets_;
    std::ostream* sysExOut_=nullptr;
#else
    CommandMidiSink midiSink_;
#endif
    Work work_=Work::Idle;
    std::optional<DspTicket> activeTicket_;
#ifdef SILVER_K_COLD_PROBE_ONLY
    p09::Exercise* exercise_=nullptr;
#endif
#if defined(SILVER_K_COLD_PROBE_ONLY) || defined(SILVER_K_COMMAND_HOST)
    bool stopped_=false;
#endif
    KProgramFlash* writableFlash_=nullptr;
    uint64_t operation_=0,bundle_=0,completed_=0,nextPollFrame_=0,feedbackCount_=0;
    uint64_t lastTriggerTransfer_=0;
    size_t pollCursor_=0,dispatchedMidi_=0;
    uint8_t pendingWheel_=0;
    bool pollActive_=false;
#ifndef SILVER_K_COMMAND_HOST
    size_t nextInput_=0,nextByte_=0;
    std::ofstream reuseTrace_;
    VoiceSlot cachedSlot_{};
    uint64_t reuseStartCycle_=0,reuseStartFrame_=0;
    bool reuseComplete_=false;
#endif
    size_t reuseCursor_=0,reuseQueries_=0;
    bool reuseActive_=false;
#ifdef SILVER_K_P07_TRACE_DSP_USE
    std::ofstream stateTrace_,selectionTrace_;
    std::array<uint32_t,8> lastState_{};
    bool haveState_=false;
#endif
};
}
