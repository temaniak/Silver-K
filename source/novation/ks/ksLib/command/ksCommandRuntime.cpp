#include "ksCommandRuntime.h"
#include "ksLib/ksState.h"
#include "../../../controller/command/novationProgramSysEx.h"
#ifdef SILVER_P12_ACTION_TRACE
#include "../../../controller/command/p12ActionTrace.h"
#endif
#ifdef SILVER_P12_CPU_SCOPES
#include "../../../controller/command/p12CpuScopes.h"
#endif
#ifdef SILVER_P12_STATE_SIZE
#include "../../../controller/command/p12StateSize.h"
#endif

#include "ksLib/command/ksCommandCutoff.h"
#include "ksLib/command/ksCommandFeedback.h"
#include "ksLib/command/ksCommandProgram.h"
#include "ksLib/command/ksCommandNoteRelease.h"
#include "ksLib/command/ksCommandNoteStart.h"
#include "ksLib/command/ksCommandAllSoundOff.h"
#include "ksLib/command/ksArpPolicy.h"
#include "ksLib/command/ksPanicControl.h"
#include "ksLib/command/ksNoteModePolicy.h"
#include "ksLib/command/ksCommandPartFeedback.h"
#include "ksLib/command/ksCommandGlobalUpdate.h"
#include "ksLib/command/ksCommandVoiceUpdate.h"
#include "ksLib/command/ksCommandExpression.h"
#include "ksLib/command/ksCommandRouting.h"
#include "ksLib/command/ksDesktopMidiSelection.h"
#include "ksLib/command/ksDesktopDrumMap.h"
#include "ksLib/command/ksCommandExpressionUpdate.h"
#include "ksLib/command/ksCommandParameterEdit.h"
#ifdef SILVER_KS_COMMAND_HOST
#include "ksLib/command/ksLiveMixer.h"
#endif
#include "ksLib/command/ksCommandGlobalFallback.h"
#include "ksLib/command/ksCommandVoiceFeedback.h"
#include "ksLib/command/ksCommandClockState.h"
#include "ksLib/command/ksPartConfigurationState.h"
#include "ksLib/command/ksVoiceSelection.h"
#include "ksLib/command/ksVoiceStop.h"
#ifndef SILVER_KS_COMMAND_HOST
#include "ks_note_state_observation.h"
#endif
#ifndef SILVER_KS_NATIVE_COMMAND_ONLY
#include "ks_image_latency_model.h"
#endif
#include "ksLib/command/ksRefreshTiming.h"
#include "../../../controller/command/dspWordTransport.h"
#include "../../../controller/command/dspMusicalClock.h"
#include "../../../controller/command/dspGlobalSync.h"
#include "../../../controller/command/dspSessionStop.h"
#include "../../../controller/command/nativeEditorState.h"
#ifndef SILVER_KS_COMMAND_HOST
#include "p08_host_blocks.h"
#include "p09_lifecycle_exercise.h"
#endif
#ifdef SILVER_KS_P05_TRACE_DSP_USE
#include "ks_dsp_filter_use.h"
#include "ks_dsp_fade_use.h"
#endif

#include <algorithm>
#include <limits>
#include <ostream>
#include <exception>
#include <stdexcept>

namespace ks::research {
namespace {
using namespace novation::controller::command;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

uint64_t serialControllerCycle(uint64_t dspCycle) {
    return (dspCycle / 13) * 2 + ((dspCycle % 13) * 2 + 12) / 13;
}

struct Port {
    PhysicalDspPair& pair;
    std::ostream* wire;
    unsigned index;
    uint32_t word = 0;
    bool recordEnabled = true;
#ifdef SILVER_KS_P05_TRACE_DSP_USE
    bool filterTraceValid = true;
#endif

    void record(const char* event, unsigned offset, uint32_t value) {
        if (!wire || !recordEnabled)
            return;
        *wire << event << ',' << pair.controllerCycle() << ',' << pair.dsp(index).cycles() << ','
             << index << ',' << offset << ',' << value << '\n';
    }

    uint8_t hostRead(uint32_t offset) {
        const auto value = pair.dsp(index).hostRead(offset);
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        if (offset == 4) record("status", offset, value);
#endif
        if (offset >= 10)
            record("reply-byte", offset, value);
        return value;
    }

    bool hostWrite(uint32_t offset, uint8_t value) {
        auto& dsp = pair.dsp(index);
        const auto accepted = dsp.hostWrite(offset, value);
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        if (!accepted) record("refused-strobe", offset, value);
#endif
        if (offset == 0)
            record("flag", offset, value);
        if (wire && offset == 14 && accepted) {
            word = dsp.peripherals().getHDI08().rxData().front();
            record("accept", offset, word);
        }
        return accepted;
    }
};

#ifdef SILVER_KS_P05_TRACE_DSP_USE
thread_local Port* filterTracePort = nullptr;
thread_local Port* fadeTracePort = nullptr;
struct ExternalFilterTrace {
    DSP* endpoint = nullptr;
    void* context = nullptr;
    FilterTraceWriter writer = nullptr;
    bool valid = true;
};
thread_local ExternalFilterTrace externalFilterTrace;
thread_local ExternalFilterTrace externalFadeTrace;

bool bindFadeTrace(DSP* endpoint, void* context, FilterTraceWriter writer) noexcept {
    if (!endpoint) {
        const bool valid = externalFadeTrace.valid;
        externalFadeTrace = {};
        return valid;
    }
    if (!writer || fadeTracePort || externalFadeTrace.endpoint) return false;
    externalFadeTrace = {endpoint, context, writer, true};
    return true;
}

void recordFadeStore(dsp56k::DSP* dsp, unsigned pc) noexcept {
    auto* port = fadeTracePort;
    auto* endpoint = port ? &port->pair.dsp(0) : externalFadeTrace.endpoint;
    auto& valid = port ? port->filterTraceValid : externalFadeTrace.valid;
    if (!endpoint || !valid || &endpoint->core() != dsp) return;
    const auto store = fadeStore(*endpoint, pc);
    if (!store) return;
    if (!store->valid) { valid = false; return; }
    try {
        const auto* name = FadeStoreNames[unsigned(store->kind)];
        if (port) port->record(name, store->address, store->value);
        else valid = externalFadeTrace.writer(externalFadeTrace.context, name, store->address,
                                               store->value, endpoint->cycles());
    } catch (...) { valid = false; }
}

bool bindFilterTrace(DSP* endpoint, void* context, FilterTraceWriter writer) noexcept {
    if (!endpoint) {
        const bool valid = externalFilterTrace.valid;
        externalFilterTrace = {};
        return valid;
    }
    if (!writer || filterTracePort || externalFilterTrace.endpoint)
        return false;
    externalFilterTrace = {endpoint, context, writer, true};
    return true;
}

void recordFilterUse(dsp56k::DSP* dsp, unsigned pc) noexcept {
    auto* port = filterTracePort;
    auto* endpoint = port ? &port->pair.dsp(1) : externalFilterTrace.endpoint;
    auto& valid = port ? port->filterTraceValid : externalFilterTrace.valid;
    if (!endpoint || !valid || &endpoint->core() != dsp)
        return;
    const auto write = [&](const char* name, uint32_t address, uint32_t value) {
        if (port)
            port->record(name, address, value);
        else
            valid = externalFilterTrace.writer(externalFilterTrace.context, name, address, value, endpoint->cycles());
    };
    try {
        constexpr uint32_t cutoff = 0x005f;
        if (pc == 0x07f3) {
            if (endpoint->memory().get(dsp56k::MemArea_X, 0) == cutoff)
                write("x-store", cutoff, endpoint->memory().get(dsp56k::MemArea_X, cutoff));
            return;
        }
        const auto operand = filterUseOperand(*endpoint, pc);
        if (!operand)
            return;
        valid = operand->valid;
        constexpr const char* names[]{"cutoff-read", "runtime-store", "runtime-read"};
        if (operand->valid)
            write(names[unsigned(operand->kind)], operand->address, operand->value);
        if (operand->valid && operand->kind == FilterUseKind::RuntimeStore) {
            const auto transform = filterTransformOperand(*endpoint);
            valid = transform.valid;
            if (valid) {
                write("transform-input", operand->address, transform.input);
                write("transform-index", transform.tableAddress, transform.index);
                write("transform-table", transform.tableAddress, transform.tableValue);
            }
        }
    } catch (...) {
        valid = false;
    }
}
#endif
}

struct PhysicalCandidate::Runtime {
    static constexpr uint64_t UartByteClocks = 8000;
    // HDI strobes occupy the external three-cycle bus. Status polling waits
    // for the attached DSP and uses the same bus cadence.
    static constexpr ClockRate NativeRate{DSP::ClockHz, 2 * DSP::SerialSlotCycles};

    static constexpr size_t NoSysExSlot = CommandScheduler::SysExSlots;
    struct IncomingControl {
        uint64_t eligible;
        ControllerEvent event;
        NativeFrame at{};
        size_t sysExSlot = NoSysExSlot;
    };
    struct IncomingQueue {
        std::array<IncomingControl,CommandScheduler::Capacity> values{};
        size_t head=0,count=0;
        size_t size()const noexcept{return count;}
        IncomingControl& operator[](size_t i)noexcept{return values[(head+i)%values.size()];}
        const IncomingControl& operator[](size_t i)const noexcept{return values[(head+i)%values.size()];}
        IncomingControl& front()noexcept{return (*this)[0];}
        void push_back(IncomingControl value)noexcept{values[(head+count)%values.size()]=std::move(value);++count;}
        void pop_front()noexcept{head=(head+1)%values.size();--count;}
    };
    struct HostRateChange { BusCycle requestedAt; HostFrame anchor; ClockRate rate; };
    struct HostCutoff {
        HostFrame at;
        uint8_t channel;
        uint8_t value;
        std::optional<NativeFrame> due;
        bool arrived = false;
    };
    struct ActiveVoice {
        std::optional<VoiceAllocation> allocation;
        KSVoiceFeedbackState feedback{};
        uint64_t generation = 0;
        uint8_t pitchDistance = 0;
    };
    struct MusicalWork {
        struct Binding {
            uint64_t arpGeneration;
            VoiceAllocation voice;
            bool drum;
            NativeFrame nextStatus{},releaseDeadline{};
        };
        struct Start {ArpNote note;bool drum;};
        MusicalWork(NativeFrame at,std::ostream* pulses,std::ostream* notes,std::ostream* midi)
            : clock(NativeRate,at),origin(at),pulses(pulses),notes(notes),midi(midi) {}
        struct HostPosition {
            NativeFrame at;
            novation::controller::MusicalClock::Position value;
        };
        NativeMusicalClock clock;
        DspMusicalClock dspClock{Model::KS,NativeRate};
        DspGlobalSync phase{Model::KS};
        std::array<GlobalSyncKeys,5> phaseKeys;
        std::array<NativeArpeggiator,5> parts;
        std::array<KSDrumArpeggiator,5> drums{
            KSDrumArpeggiator{1}, KSDrumArpeggiator{2}, KSDrumArpeggiator{3},
            KSDrumArpeggiator{4}, KSDrumArpeggiator{5}
        };
        std::array<uint8_t,KSDrumGridEdit::Slots> drumPatterns{};
        std::array<std::optional<Binding>,KSMusicalState::VoiceCount> voices;
        std::bitset<KSMusicalState::VoiceCount> waitingGates;
        std::optional<Start> starting;
        std::optional<NativeFrame> reloadAt;
#ifndef SILVER_KS_COMMAND_HOST
        std::optional<p08::HostBlockPlayback> hostBlocks;
#endif
        kstation::Queue<HostPosition, 2048> hostPositions;
        std::optional<HostPosition> lastHostPosition;
        NativeFrame origin;
        std::ostream *pulses,*notes,*midi;
    };
    enum class RefreshStage { None, Signal, Auxiliary, ReleaseGate, Program, ProgramApply, Scene, InactivePartAck, Note,
                              VoiceStop, NoteAdmission, NoteInitialAdmission, ReleaseAdmission, SoundAdmission, SoundTransfer,
                              GlobalAdmission,GlobalTransfer,VoiceUpdateAdmission,VoiceUpdateTransfer,
                              ExpressionAdmission,ExpressionTransfer,Clock,Phase,LiveMixer };
    struct NoteCapacityDiagnostic {
        uint32_t frames;
        bool initial,occupiedOnly,active=false,completed=false;
        NativeFrame releaseAt{};
        uint64_t keys=0,generation=0,publication=0;
        std::array<DspTicket,DspTransactionQueue::EndpointCapacity> tickets{};
        bool release=false;
        VoiceSlot slot{};
        KSStorageSlot part=KSStorageSlot::Single;
    };
    struct SingleReload {
        ProgramSelection selection;
        KSVoiceImagePatch patch;
        KSRefreshPacket globals;
        NativeFrame started, nextPoll;
        bool fromBank;
        bool awaitingFence = false;
    };
    struct SceneReload {
        enum class Step { Fade, Stop, Effects, Route, Confirm, Rebuild };
        KSLayout layout;
        KSPerformanceRecord patches;
        ProgramSelection single;
        uint8_t performance, activeMask, mailboxMask;
        KSGlobalMaster master;
        NativeFrame started, nextPoll;
        Step step = Step::Fade;
        size_t part = 0;
        unsigned stoppedVoices = 0;
        uint8_t partMask = 0;
        bool programsOnly = false;

        bool ownsVoice(KSStorageSlot slot) const noexcept {
            return !programsOnly || (slot!=KSStorageSlot::Single && (partMask&(1u<<(size_t(slot)-1))));
        }
    };

    Runtime(PhysicalDspPair& pair, const PhysicalSeed& seed, std::ostream* packets, std::ostream* wire,
            std::ostream* timeline, FeedbackObservation feedbackObservation, HdiCadence cadence, KSProgramFlash* programs)
        : pair(pair), seed(seed), packets(packets), timeline(timeline), programs(programs),
          owner(session.coldReset(Model::KS, {}, native(), NativeRate, NativeRate, 0)),
          state(owner, seed.layout),routing(owner),expression(owner),parameters(owner),parameterIngress(owner,this->seed.parameterTables),
          drumBindings(owner),
          system(KSSystemSettings::fromPatch(owner,this->seed.patches[0])),
          clock(owner),editor(owner),configuration(owner),
          globalFallback(owner,this->seed.parameterTables,this->seed.parameterSelectors[0],this->seed.bankLatch),
          globals(owner,seed.layout,seed.globalTables,seed.globalMaster,seed.auxiliary),cutoff(state, seed.partChannels),
          refresh(KSRefreshClock::prepare(bus(), seed.tick, seed.lastAdmitted, seed.nextTickDelay)),
          deferred(seed.cooldown), feedbackObservation(feedbackObservation),
#ifdef SILVER_KS_NATIVE_COMMAND_ONLY
          hdiTiming{3, 3, 25000}, producerWordLimit(0),
#else
          hdiTiming(cadence != HdiCadence::BusMinimum ? HdiWordTiming{15, 27, 25000, 24, 18, 24}
                                                    : HdiWordTiming{3, 3, 25000}),
          producerWordLimit(cadence == HdiCadence::ProducerDeadlinesDiagnostic ? 100u :
                            cadence == HdiCadence::ProducerPrefixDiagnostic ? 57u : 0u),
#endif
          portA{pair, wire, 0}, portB{pair, wire, 1},
          hdiA(Endpoint::A, DspEndpoint::bind(portA)), hdiB(Endpoint::B, DspEndpoint::bind(portB)) {
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        require(!filterTracePort && !externalFilterTrace.endpoint && !fadeTracePort && !externalFadeTrace.endpoint,
                "Only one sequential DSP observation can be active");
#endif
        require(pair.startedBoth() && pair.serialRunning(), "KS candidate requires a prepared DSP pair");
#ifdef SILVER_KS_NATIVE_COMMAND_ONLY
        require(cadence == HdiCadence::BusMinimum, "Native controller requires actual HDI protocol timing");
#endif
        require(!producerWordLimit || seed.layout == KSLayout::Single,
                "P05 producer deadlines currently require the verified Single layout");
        require(bool(refresh), "Invalid prepared refresh clock");
        drumStore.session=owner;drumStore.generation=this->seed.drumStoreGeneration;
        drumStore.snapshotRecords=programs!=nullptr;
        for(size_t b=0;b<drumStore.banks.size();++b)
            drumStore.banks[b]={this->seed.drumRecords[b].data(),this->seed.drumRecords[b].size(),this->seed.drumBankRevisions[b]};
        bankLatches=this->seed.bankLatches;
        midiBanks.fill(255);
        require(desktopSelection.prepareCold(this->seed.desktopParameterSelectors),"Invalid prepared desktop selectors");
        for(size_t p=0;p<seed.partConfigurations.size();++p)
            require(configuration.prepareCold(KSStorageSlot(p),seed.partConfigurations[p],seed.partNoteDispatchModes[p]),"Invalid prepared applied part configuration");
        require(editor.select(seed.layout,seed.editorTarget),"Invalid prepared semantic editor selection");
        KSRoutingConfig routes;routes.layout=seed.layout;routes.activeMask=seed.activeMask;
        routes.parts[0]={seed.channel,configuration.read(KSStorageSlot::Single)->values.lowKey,configuration.read(KSStorageSlot::Single)->values.highKey};
        for(size_t index=0;index<seed.patches.size();++index) {
            require(globals.loadPart(KSStorageSlot(index),KSGlobalPart::fromPatch(seed.patches[index]),seed.patches[index][225]),"Invalid prepared globals");
            if(seed.layout==KSLayout::Single?index==0:index>0) {
                const auto applied=configuration.read(KSStorageSlot(index))->values;
                auto channel=seed.partChannels[index];
                if(seed.layout==KSLayout::Performance && !seed.partChannelRoutingImmediate)
                    channel=applied.channel<16?applied.channel:KSCommandCutoff::NoChannel;
                routes.parts[index]={channel,applied.lowKey,applied.highKey};this->seed.partChannels[index]=channel;
            }
        }
        require(routing.configure(routes),"Invalid prepared note routing");
        for (size_t index = 0; index < seed.patches.size(); ++index) {
            require(parameterIngress.prepareSelector(KSStorageSlot(index),seed.parameterSelectors[index]),"Invalid prepared parameter selector");
            const bool musical=seed.layout==KSLayout::Single?index==0:index>0;
            if(!musical && !(seed.layout==KSLayout::Performance && index==0))continue;
            const auto& patch = seed.patches[index];
            const auto selection = index==0?seed.singleSelection:ProgramSelection{patch[187],patch[188]};
            require(state.loadProgram(KSStorageSlot(index), selection, {patch[46], patch[150]}),
                    "Invalid prepared program");
            require(parameters.load(*state.storedDependency(KSStorageSlot(index),session.rateGeneration()),patch),"Invalid prepared parameter bank");
            parameterSources[index]=*state.storedDependency(KSStorageSlot(index),session.rateGeneration());
            metadata[index]=KSParameterMetadata::fromPatch(patch);
            if(!musical)continue;
            const auto& context=seed.partContexts[index];
            require(expression.prepare(KSStorageSlot(index),{context.wheelPitch,context.modulation},patch[177],seed.restartMono[index]),
                    "Invalid prepared expression");
        }
        state.takeDirtySlots();
        for (size_t index = 0; index < seed.keys.size(); ++index)
            if (state.keys(KSStorageSlot(index))) {
                require(state.prepareKeys(KSStorageSlot(index), seed.keys[index]), "Invalid prepared raw key state");
                require(state.prepareSustain(KSStorageSlot(index),seed.sustainDown[index],bool(seed.patches[index][88]&4)),
                        "Invalid prepared sustain state");
            }
        for (size_t index = 0; index < seed.patches.size(); ++index)
            if(seed.pendingMask & (1u<<index)) {
                if(seed.layout==KSLayout::Performance && index==0)require(state.requestStoredSingleRefresh(),"Invalid prepared stored refresh");
                else require(state.editCutoff(KSStorageSlot(index),seed.patches[index][46]),"Invalid prepared dirty part");
            }
        for (size_t index = 0; index < voices.size(); ++index) {
            const auto& prepared = seed.voices[index];
            if (!prepared.age)
                continue;
            auto& voice = voices[index];
            voice.allocation = state.assignVoice(VoiceSlot(index), prepared.part,
                                                prepared.note, prepared.velocity, prepared.age,prepared.noteOffExtra);
            require(bool(voice.allocation), "Invalid prepared voice");
            if(prepared.deferred) require(state.deferVoice(*voice.allocation),"Invalid prepared deferred voice");
            voice.feedback = {prepared.age, prepared.countdown};voice.pitchDistance=prepared.pitchDistance;
            const auto source=selectKsNoteAssignment(KSNoteAssignmentKind::Select,*state.dependency(prepared.part,session.rateGeneration()),
                *state.part(prepared.part)->program,prepared.note,drumStore);
            require(drumBindings.bind(state,*voice.allocation,source,drumStore),"Invalid prepared allocation program source");
            voice.generation = state.part(prepared.part)->revisions.program*8+(prepared.generation&7);
        }
        require(state.publish(native()), "Could not publish prepared KS state");
        if (packets)
            *packets << "bundle,stage,endpoint,address,word_index,word\n";
        if (wire) {
            *wire << "event,controller_cycle,dsp_cycle,endpoint,offset,value\n";
            observe(portA);
            observe(portB);
        }
        if (timeline)
            *timeline << "event,controller_cycle,native_frame,value\n";
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        filterTracePort = &portB;
        fadeTracePort = &portA;
#endif
    }

    ~Runtime() {
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        filterTracePort = nullptr;
        fadeTracePort = nullptr;
#endif
        session.stop(owner);
        detachObservers();
    }

    void detachObservers() {
        if (!portA.wire)
            return;
        for (unsigned index = 0; index < 2; ++index) {
            auto& host = pair.dsp(index).peripherals().getHDI08();
            host.setReadRxCallback({});
            host.setWriteTxCallback({});
        }
    }

    void observe(Port& port) {
        auto& host = pair.dsp(port.index).peripherals().getHDI08();
        host.setReadRxCallback([&port] { port.record("pop", 0, port.word); });
        host.setWriteTxCallback([&port, &host] { port.record("reply", 0, host.txData().front()); });
    }

    NativeFrame native() const { return {pair.dsp(1).audioFramesProduced()}; }
    BusCycle bus() const { return {pair.controllerCycle()}; }
    void event(const char* name, uint64_t value) {
#ifdef SILVER_P12_ACTION_TRACE
        P12ActionTrace::worker().nativeEvent(name, bus().ticks, native().ticks, value);
#endif
        if (timeline)
            *timeline << name << ',' << bus().ticks << ',' << native().ticks << ',' << value << '\n';
    }

    void enqueue(uint64_t inputCycle, ControllerEvent input) {
        require(!stopped,"KS input after stop");
        if (const auto* note = std::get_if<NoteOn>(&input.payload); note && !note->velocity)
            input.payload = NoteOff{note->channel, note->note, 0};
        const auto channel=KSCommandRouting::channelOf(input.payload);
        const bool realtime=musical && std::holds_alternative<MidiRealtime>(input.payload);
        require((realtime || (channel && *channel<16)) &&
                inputCycle >= bus().ticks && inputCycle >= lastInputCycle,
                "Invalid timestamped KS input");
        require(ingress.size() < CommandScheduler::Capacity, "KS diagnostic ingress is full");
        const auto firstByte = std::max(inputCycle, nextUart);
        const auto bytes = realtime ? 1u : std::holds_alternative<ProgramChange>(input.payload) ||
                           std::holds_alternative<ChannelPressure>(input.payload) ? 2u : 3u;
        require(firstByte <= std::numeric_limits<uint64_t>::max() - bytes * UartByteClocks,
                "Serial-equivalent input deadline overflow");
        const auto eligible = firstByte + (bytes - 1) * UartByteClocks;
        ingress.push_back({eligible, std::move(input)});
        ++queuedInputEvents;
        nextUart = eligible + UartByteClocks;
        lastInputCycle = inputCycle;
    }

    size_t enqueueMidiBytes(uint64_t inputCycle, const uint8_t* bytes, size_t size) {
        if (stopped)
            return 0;
        require(bytes && inputCycle >= bus().ticks && inputCycle >= lastInputCycle,"Invalid raw KS MIDI input");
        size_t consumed = 0;
        for (; consumed < size; ++consumed) {
            const auto eligible = std::max(inputCycle, nextUart);
            require(eligible <= std::numeric_limits<uint64_t>::max() - UartByteClocks,"KS MIDI byte timestamp overflow");
            const auto result = midiDecoder.push(bytes[consumed], [&](EventPayload payload) {
                return admitMidi(eligible,payload);
            });
            if (result == MidiDecoder::Result::Backpressure)
                break;
            if (result != MidiDecoder::Result::Consumed)
                event("midi_decode_rejected",unsigned(result));
            nextUart = eligible + UartByteClocks;
            lastInputCycle = inputCycle;
        }
        return consumed;
    }

    bool admitMidi(uint64_t eligible,EventPayload payload,uint8_t part=RoutedByChannel) {
        if (ingress.size() == CommandScheduler::Capacity)
            return false;
        if (const auto* note = std::get_if<NoteOn>(&payload); note && !note->velocity)
            payload = NoteOff{note->channel, note->note, 0};
        size_t slot = NoSysExSlot;
        if (auto* message = std::get_if<SysExBytes>(&payload)) {
            for (slot = 0; slot < usedSysEx.size() && usedSysEx[slot]; ++slot) {}
            if (slot == NoSysExSlot)
                return false;
            usedSysEx[slot] = true;
            std::copy_n(message->data,message->size,ingressSysEx[slot].begin());
            message->data = ingressSysEx[slot].data();
        }
        ingress.push_back({eligible,{payload,part},{},slot});
        ++queuedInputEvents;
        return true;
    }

    const KSUserDrumPattern& userDrumPattern(KSStorageSlot part) const noexcept {
        static constexpr KSUserDrumPattern empty;
        const auto p=size_t(part);
        const auto sound=state.part(part);
        if(!sound || !sound->program || sound->program->bank<5 || sound->program->bank>8)return empty;
        return seed.userDrums[p].patterns[musical->drumPatterns[p]];
    }

    bool pressDrum(KSStorageSlot part,NativeFrame at,uint8_t note,std::optional<uint8_t> pattern) noexcept {
        const auto p=size_t(part);
        auto& drum=musical->drums[p];
        const auto& bank=seed.userDrums[p];
        if(pattern && (*pattern!=musical->drumPatterns[p] || !drum.usesUserSequence())) {
            musical->drumPatterns[p]=*pattern;
            return drum.press(at,note,drum.userSequence(userDrumPattern(part),*pattern),bank.switchAfter);
        }
        return drum.press(at,note);
    }

    void configureArp(KSStorageSlot part,NativeFrame at) {
        const auto p=size_t(part);
        const auto& patch=seed.patches[p];
        if(patch[87]==7) {
            const uint8_t flags=patch[88],route=flags&0x60;
            auto& drum=musical->drums[p];
            const auto& bank=seed.userDrums[p];
            const bool user=bank.selected;
            auto& pattern=musical->drumPatterns[p];
            if (user && !bank.assignedKeysOnly) pattern=bank.selectedPattern;
            const auto sequence=user?drum.userSequence(userDrumPattern(part),pattern):
                seed.drumRhythms.sequence(patch[149]);
            require(musical->parts[p].configure(at,{}) &&
                    drum.configure(at,{sequence,seed.partChannels[p],bool(flags&4),bool(flags&8),bool(flags&16),
                                       !route || route==0x40,route!=0,bank.retrigger},user || drum.usesUserSequence()),
                    "KS drum configuration");
            return;
        }
        require(musical->drums[p].configure(at,{}),"KS drum handoff deadline");
        auto settings=readKSArp(patch.data(),patch.size(),seed.partChannels[p],{},&seed.arpRhythms);
        if(!settings) {
            require(!(patch[88]&4),"KS arp patch uses an unsupported enabled pattern");
            settings=ArpSettings{};
        }
        require(musical->parts[p].configure(at,*settings),"KS arp configuration deadline");
    }

    void applyDrumGridEdit(KSDrumGridEdit edit,NativeFrame at) {
        using Kind=KSDrumGridEdit::Kind;
        auto& bank=seed.userDrums[edit.slot];
        bank.apply(edit);
        if(!musical)return;
        const auto part=KSStorageSlot(edit.slot);
        auto& drum=musical->drums[edit.slot];
        if(edit.kind==Kind::Retrigger) {
            drum.setUserSequenceRetrigger(bank.retrigger);
        } else if(bank.selected && bank.assignedKeysOnly &&
           (edit.kind==Kind::AssignedKeys || edit.kind==Kind::KeyLow || edit.kind==Kind::KeyHigh)) {
            require(drum.keepAssignedKeys(at,bank.assignedNotes()),"KS assigned trigger keys");
        } else if(bank.selected && !bank.assignedKeysOnly && drum.enabled() &&
                  (edit.kind==Kind::SelectPattern || edit.kind==Kind::AssignedKeys)) {
            auto& pattern=musical->drumPatterns[edit.slot];
            if(pattern!=bank.selectedPattern) {
                pattern=bank.selectedPattern;
                require(drum.selectSequence(at,drum.userSequence(userDrumPattern(part),pattern),bank.switchAfter),
                        "KS manual sequence switch");
            }
        } else if(edit.kind==Kind::SwitchAfter && bank.selected) {
            require(drum.retimeSwitch(at,bank.switchAfter),"KS sequence switch timing");
        } else if(seed.partChannels[edit.slot]<16 && seed.patches[edit.slot][87]==7 &&
                  (edit.kind==Kind::SelectUser || (bank.selected && edit.pattern==musical->drumPatterns[edit.slot] &&
                   (edit.needsDrumMap() || edit.kind==Kind::Length)))) {
            configureArp(part,at);
        }
    }

    void clockSignal(MusicalClockSignal signal) {
        auto& work=*musical;
        if (work.pulses)
            *work.pulses<<signal.at.ticks-work.origin.ticks<<','<<signal.pulse<<','<<signal.epoch<<','
                   <<unsigned(signal.source)<<','<<signal.tick<<','<<signal.running<<','<<signal.framesPerPulse<<'\n';
        for(size_t p=0;p<work.parts.size();++p)if(state.keys(KSStorageSlot(p))) {
            auto delivery=signal;
            delivery.at=std::max(signal.at,sceneArpsAt[p]);
            require(work.parts[p].clock(delivery) && work.drums[p].clock(delivery),"KS arp missed a clock deadline");
        }
        observeDspClock(signal.freshMidiPeriod);
    }

    void observeDspClock(bool freshMidiPeriod=false) {
        musical->dspClock.observe(musical->clock,freshMidiPeriod);
    }

    bool serviceDspClock() {
        if(!musical || !musical->dspClock.pending())return false;
        const auto status=musical->dspClock.admit(session,owner,++operation,native());
        if(status==DspAdmission::Full)return false;
        require(status==DspAdmission::Accepted && musical->dspClock.ticket(),"KS DSP clock admission");
        stage=RefreshStage::Clock;
        startPort(*musical->dspClock.ticket());
        return true;
    }

    void completeDspClockWord() {
        auto& sync=musical->dspClock;
        require(sync.ticket() && sync.transferred(session,*sync.ticket()),"KS DSP clock transfer");
        if(const auto next=sync.ticket())startPort(*next);
        else {
            const auto words=*sync.applied();
            event("dsp_clock_phase",words.phase);
            event("dsp_clock_delay",words.delay);
            stage=RefreshStage::None;
        }
    }

    GlobalSyncMode syncMode() const noexcept { return globalSyncMode(system.globalSync()); }

    void resetGlobalPhase(KSStorageSlot part) {
        const auto& patch=seed.patches[size_t(part)];
        const auto fxPart=part==KSStorageSlot::Single?0:uint8_t(size_t(part)-1);
        const auto before=musical->phase.size();
        require(musical->phase.append({patch[122],patch[79],patch[78]},fxPart),"KS global phase intent");
        if(musical->phase.size()!=before)event("global_sync_requested",unsigned(syncMode()));
    }

    bool serviceGlobalSync() {
        if(!musical || !musical->phase.pending())return false;
        const auto status=musical->phase.admit(session,owner,++operation,native());
        require(status==DspAdmission::Accepted && musical->phase.ticket(),"KS global phase admission");
        stage=RefreshStage::Phase;startPort(*musical->phase.ticket());
        return true;
    }

    void completeGlobalPhase() {
        auto& phase=musical->phase;
        require(phase.ticket() && phase.transferred(session,*phase.ticket()),"KS global phase transfer");
        if(const auto next=phase.ticket())startPort(*next);
        else {
            event("global_sync_transferred",unsigned(syncMode()));
            stage=RefreshStage::None;
        }
    }

    void selectArpClock(NativeFrame at,uint16_t value) {
        constexpr std::array policies{ClockPolicy::Auto,ClockPolicy::Internal,ClockPolicy::ExternalMidi};
        require(value<policies.size(),"Unsupported KS clock source");
        if(const auto signal=musical->clock.setPolicy(at,policies[value])) {
            // Host timing is admitted before MIDI at the same sample. Apply
            // the captured position when Auto is selected. Publish only the
            // final reset so queued note-offs are drained after this handover.
            const auto& host=musical->lastHostPosition;
            if(policies[value]==ClockPolicy::Auto && host && host->value.available) {
                auto position=host->value;
                if(position.hasPpq && position.playing && host->at<at)
                    position.ppq+=double(at.ticks-host->at.ticks)*position.bpm/(60*DSP::SampleRate);
                if(applyHostPosition(at,position))return;
            }
            clockSignal(*signal);
        }
    }

    void enableArp(std::ostream* pulses,std::ostream* notes,std::ostream* midi,double hostSampleRate=0) {
        require(!musical && !ingress.size(),"KS arp must start at the prepared boundary");
        const auto at=native();
        musical=std::make_unique<MusicalWork>(at,pulses,notes,midi);
        musical->clock.setHostSampleRate(hostSampleRate);
        if (pulses) *pulses<<"frame,pulse,epoch,source,tick,running,frames_per_pulse\n";
        if (notes) *notes<<"deadline,dispatch,part,generation,note,velocity,on,internal,midi\n";
        if (midi) *midi<<"frame,status,note,velocity\n";
        for(size_t p=0;p<musical->parts.size();++p)if(state.keys(KSStorageSlot(p)))configureArp(KSStorageSlot(p),at);
        require(musical->clock.setTempo(at,1000000u*(64u+system.tempo)),"Invalid prepared KS tempo");
        selectArpClock(at,system.clockSource());
#ifdef SILVER_P12_STATE_SIZE
        p12StateSizes("initialized", "KS-controller", {
            {"mutable", "runtime", sizeof(Runtime)}, {"mutable", "clock_and_arp", sizeof(MusicalWork)},
            {"detail", "musical_state", sizeof(state)}, {"detail", "startup_seed_inline", sizeof(seed)},
            {"detail", "scheduler_session", sizeof(session)}, {"detail", "typed_midi_queue", sizeof(ingress)},
            {"detail", "sysex_slots", sizeof(ingressSysEx)}, {"detail", "arpeggio_host_positions", sizeof(musical->hostPositions)}});
#endif
    }

    void applyClockRequest(const KSClockRequest& request) {
        const auto at=request.at;
        EventPayload message;
        switch(request.kind) {
        case KSClockRequestKind::TempoSetting:
            require(musical->clock.setTempo(at,1000000u*(64u+request.value)),"KS tempo request deadline");
            observeDspClock();
            break;
        case KSClockRequestKind::SourceSetting:selectArpClock(at,request.value);break;
        case KSClockRequestKind::Pulse:message=MidiRealtime{0xf8};break;
        case KSClockRequestKind::Start:message=MidiRealtime{0xfa};break;
        case KSClockRequestKind::Continue:message=MidiRealtime{0xfb};break;
        case KSClockRequestKind::Stop:message=MidiRealtime{0xfc};break;
        case KSClockRequestKind::SongPosition:
            message=MidiCommon{0xf2,uint8_t(request.value&127),uint8_t(request.value>>7)};break;
        }
        if(const auto signal=musical->clock.midi(at,message)) {
            clockSignal(*signal);
            if(request.kind==KSClockRequestKind::Start && syncMode()==GlobalSyncMode::SongStart)
                for(size_t p=0;p<musical->phaseKeys.size();++p)
                    if(state.keys(KSStorageSlot(p)))resetGlobalPhase(KSStorageSlot(p));
        }
        require(clock.acknowledge(request),"KS clock receipt lost its pending request");
    }

    void playArp(KSStorageSlot part,ArpNote note,bool drum=false,bool forcedOff=false) {
        auto& work=*musical;
        if (work.notes)
            *work.notes<<note.at.ticks-work.origin.ticks<<','<<native().ticks-work.origin.ticks<<','<<size_t(part)<<','
                  <<note.generation<<','<<unsigned(note.note)<<','<<unsigned(note.velocity)<<','<<note.on<<','
                  <<note.internal<<','<<note.midiOut<<'\n';
        writeArpMidi(note);
        if(!note.internal)return;
        if(note.on) {
            dispatchPart=part;
            work.starting=MusicalWork::Start{note,drum};
            startNote({note.channel,note.note,note.velocity},false);
            if(allocatingNote)observeRefreshFeedback();
            else work.starting.reset();
            return;
        }
        std::array<VoiceAllocation,KSMusicalState::VoiceCount> owned;
        size_t count=0;
        for(auto& binding:work.voices) {
            if(!binding || binding->arpGeneration!=note.generation || binding->drum!=drum)continue;
            const auto voice=state.voice(binding->voice);
            if(!voice)binding.reset();
            else if(voice->part==part && voice->note==note.note) {
                if(drum && !forcedOff && state.noteOffExtra(voice->allocation).value_or(0)) {
                    binding.reset(); // The selected drum record owns the duration in DSP B.
                    continue;
                }
                if(KSCommandFeedback::voiceAction(voice->feedbackAge)==KSVoiceFeedbackAction::ReadPendingStatus) {
                    work.waitingGates.set(size_t(binding->voice.slot));
                    binding->releaseDeadline=note.at;
                    event("arp_gate_waiting_for_activation",size_t(binding->voice.slot));
                } else {
                    owned[count++]=binding->voice;
                    binding.reset();
                }
            }
        }
        if(count)startArpRelease(part,note.note,note.at,owned.data(),count);
    }

    void writeArpMidi(const ArpNote& note) {
        auto& work=*musical;
        if (note.midiOut) {
            const uint8_t bytes[]{uint8_t((note.on ? 0x90 : 0x80) | note.channel), note.note, note.velocity};
            midiSink.emit(bytes, sizeof(bytes));
        }
        if(note.midiOut && work.midi)*work.midi<<note.at.ticks-work.origin.ticks<<','<<(unsigned(note.on?0x90:0x80)|note.channel)
                               <<','<<unsigned(note.note)<<','<<unsigned(note.velocity)<<'\n';
    }

    void playDrum(KSStorageSlot part,KSDrumOutput output) {
        const auto note=output.note;
        if(!output.pressure) {
            playArp(part,note,true,output.forcedOff);
            return;
        }
        if(note.midiOut && musical->midi)
            *musical->midi<<note.at.ticks-musical->origin.ticks<<','<<(0xd0|unsigned(note.channel))<<','
                         <<unsigned(note.velocity)<<",0\n";
        if (note.midiOut) {
            const uint8_t bytes[]{uint8_t(0xd0 | note.channel), note.velocity};
            midiSink.emit(bytes, sizeof(bytes));
        }
        if(note.internal) {
            dispatchPart=part;
            const ScheduledEvent event{owner,note.at,0,{ChannelPressure{note.channel,note.velocity},
                                                       uint8_t(seed.layout==KSLayout::Single?0:size_t(part)-1)}};
            require(dispatchRouted(event,native()).status==DispatchDecision::Status::Applied,"KS drum pressure dispatch");
        }
    }

    void startArpRelease(KSStorageSlot part,uint8_t note,NativeFrame deadline,const VoiceAllocation* owned,size_t count) {
        require(!pendingRelease,"KS arp gate overtook another release");
        pendingRelease=KSCommandNoteRelease::prepareOwned(state,part,owned,count);
        require(bool(pendingRelease),"KS arp gate ownership");
        if(!pendingRelease->size()) {
            pendingRelease.reset();
            return;
        }
        arpReleaseDeadline=deadline;
        pendingReleaseValue=note;
        stage=RefreshStage::ReleaseAdmission;admitPendingRelease();
        if(stage==RefreshStage::ReleaseGate)startPort(releaseTickets[releaseIndex]);
    }

    bool serviceWaitingArpGate() {
        if(!musical || musical->waitingGates.none())return false;
        auto& work=*musical;
        for(size_t slot=0;slot<work.voices.size();++slot)if(work.waitingGates.test(slot)) {
            auto& binding=work.voices[slot];
            const auto voice=binding?state.voice(binding->voice):std::nullopt;
            if(voice && KSCommandFeedback::voiceAction(voice->feedbackAge)==KSVoiceFeedbackAction::ReadPendingStatus) {
                if(native()<binding->nextStatus)continue;
                feedbackForArpGate=true;
                readVoiceFeedback(slot);
                return true;
            }
            work.waitingGates.reset(slot);
            const auto deadline=binding?binding->releaseDeadline:NativeFrame{};
            binding.reset();
            if(voice && voice->feedbackAge) {
                event("arp_gate_activation_confirmed",slot);
                startArpRelease(voice->part,voice->note,deadline,&voice->allocation,1);
                return true;
            }
        }
        return false;
    }

    bool applyHostPosition(NativeFrame at, novation::controller::MusicalClock::Position position) {
        musical->lastHostPosition=MusicalWork::HostPosition{at,position};
        if (const auto signal = musical->clock.host(at, position)) {
            clockSignal(*signal);
#ifdef SILVER_KS_COMMAND_HOST
            if (position.playing && signal->reset && signal->source == ClockAuthority::Host &&
                syncMode() == GlobalSyncMode::SongStart)
                for (size_t part = 0; part < musical->phaseKeys.size(); ++part)
                    if (state.keys(KSStorageSlot(part)))
                        resetGlobalPhase(KSStorageSlot(part));
#endif
            return true;
        }
        observeDspClock();
        return false;
    }

    bool serviceArp(bool awaitingHostInput) {
        if(!musical)return false;
        auto& work=*musical;
        for(size_t p=0;p<work.parts.size();++p)if(const auto* next=work.parts[p].front()) {
            const auto note=*next;
            work.parts[p].pop();
            playArp(KSStorageSlot(p),note);
            return true;
        }
        for(size_t p=0;p<work.drums.size();++p)if(const auto next=work.drums[p].pop()) {
            playDrum(KSStorageSlot(p),*next);
            return true;
        }
        if(work.reloadAt) {
            configureArp(KSStorageSlot::Single,*work.reloadAt);
            work.reloadAt.reset();
        }
        if(const auto request=clock.front()) {
            applyClockRequest(*request);
            return true;
        }
        MusicalWork::HostPosition position{};
        const bool hasPosition = work.hostPositions.peek(position);
        auto host = hasPosition ? std::optional{position.at} : std::nullopt;
#ifndef SILVER_KS_COMMAND_HOST
        if (work.hostBlocks)
            host = work.hostBlocks->nextDeadline();
#endif
        const auto beforeInput=[&](std::optional<NativeFrame> due) {
            return due && !(native()<*due) && (!eligibleCount || *due<ingress.front().at) && (!host || *due<*host);
        };
        auto due=work.clock.nextDeadline();
        NativeArpeggiator* gate=nullptr;
        KSDrumArpeggiator* drum=nullptr;
        if(!beforeInput(due))due.reset();
        else if (awaitingHostInput && work.clock.source()==ClockAuthority::Host &&
                 work.clock.running() && *due==native())
            due.reset();
        for(auto& part:work.parts)if(const auto deadline=part.nextDeadline();beforeInput(deadline) &&
                                      (!due || !(*due<*deadline))) {
            due=deadline;gate=&part;
        }
        for(auto& part:work.drums)if(const auto deadline=part.nextDeadline();beforeInput(deadline) &&
                                      (!due || !(*due<*deadline))) {
            due=deadline;drum=&part;
        }
        if(!due) {
            if (hasPosition && !(native() < position.at) &&
                (!eligibleCount || !(ingress.front().at < position.at))) {
                work.hostPositions.pop(position);
                applyHostPosition(position.at, position.value);
                return true;
            }
#ifndef SILVER_KS_COMMAND_HOST
            if(!work.hostBlocks || !host || eligibleCount)return false;
            const auto rateGeneration=session.rateGeneration();
            return work.hostBlocks->dispatch(native(),[&](NativeFrame at,auto position) {
                if(session.rateGeneration()!=rateGeneration)rebaseParameterRate();
                applyHostPosition(at, position);
            },[&](NativeFrame at,const ControllerEvent& input) {
                require(ingress.size()==0,"KS host MIDI collided with the serial fixture");
                ingress.push_back({bus().ticks,input,at});
                ++eligibleCount;
                ++queuedInputEvents;
            });
#else
            return false;
#endif
        }
        if(drum)return drum->takeDue(*due);
        if(gate)return gate->takeDue(*due);
        const auto signal=work.clock.takeDue(*due);
        require(bool(signal),"KS clock deadline did not produce a signal");
        clockSignal(*signal);
        return true;
    }

    void recordPacket(const char* stageName, const KSRefreshPacket& packet) {
        if (!packets)
            return;
        for (size_t entry = 0; entry < packet.size(); ++entry) {
            const auto write = *packet.at(entry);
            for (size_t word = 0; word < write.count; ++word)
                *packets << bundle << ',' << stageName << ',' << unsigned(write.endpoint) << ',' << write.address
                        << ',' << word << ',' << write.words[word] << '\n';
        }
    }

    void startPort(DspTicket ticket) {
        const auto offer = session.transactions().offer(owner, ticket.endpoint, native());
        transport = ticket.endpoint == Endpoint::A ? &hdiA : &hdiB;
        DspWordAvailability availability;
#ifdef SILVER_KS_NATIVE_COMMAND_ONLY
        if (stage == RefreshStage::Note && paceNoteImage && noteInitialIndex + 2 == noteStart.initialCount()) {
            // Offset one is the first payload word. Allow the DSP to process
            // the voice setup before sending the envelope and auxiliary coefficients.
            static constexpr size_t firstEnvelopeWord = 1 + (0x0068 - KSVoiceImageResult::ImageAddress);
            static constexpr uint64_t voiceSetupCycles = PhysicalDspPair::ControllerClockHz / 1250; // 0.8 ms
            availability = {this, [](void* opaque, DspTicket, size_t offset, BusCycle previous) noexcept
                -> std::optional<BusCycle> {
                const auto& runtime = *static_cast<Runtime*>(opaque);
                const auto cycles = offset == firstEnvelopeWord ? voiceSetupCycles : runtime.hdiTiming.strobeSpacing;
                if (previous.ticks > std::numeric_limits<uint64_t>::max() - cycles) return std::nullopt;
                return BusCycle{previous.ticks + cycles};
            }};
        }
#else
        imageAvailability.reset();
        DspTransactionProgress progress;
        require(session.transactions().progress(ticket, progress), "Missing owned transfer metadata");
        if (producerWordLimit && ticket.endpoint == Endpoint::B && progress.address.mode == DspAddressMode::Write &&
            progress.wordCount == 101 && progress.address.address >= 0x2c &&
            (progress.address.address - 0x2c) % 101 == 0) {
            const auto index = (progress.address.address - 0x2c) / 101;
            require(size_t(index) < voices.size() && voices[index].allocation, "Invalid timed image voice");
            const auto assigned = state.voice(*voices[index].allocation);
            require(bool(assigned), "Timed image lost its allocation");
            const auto part = size_t(assigned->part);
            const auto& patch = seed.patches[part];
            imageAvailability = ImageAvailability{ticket, patch, seed.voices[index].context,
                {{patch[46], patch[150]}, assigned->velocity, seed.voiceTables.filter}};
            availability = {this, [](void* opaque, DspTicket current, size_t offset, BusCycle previous) noexcept
                -> std::optional<BusCycle> {
                auto& runtime = *static_cast<Runtime*>(opaque);
                const auto& source = runtime.imageAvailability;
                if (!source || current.sequence != source->ticket.sequence || current.session != source->ticket.session ||
                    !offset || offset > 100) return std::nullopt;
                const auto delay = offset > runtime.producerWordLimit ? std::nullopt :
                    p05::image_latency::firstStatus(unsigned(offset - 1), source->patch,
                        source->context, source->cutoff, runtime.seed.voiceTables);
                // The prefix experiment deliberately leaves 43 words
                // unmodelled. The full image experiment rejects missing laws.
                if (!delay && runtime.producerWordLimit == 100) return std::nullopt;
                const auto cycles = delay.value_or(unsigned(runtime.hdiTiming.strobeSpacing));
                if (cycles > std::numeric_limits<uint64_t>::max() - previous.ticks) return std::nullopt;
                if (delay) ++runtime.producerDeadlines;
                else ++runtime.unmodelledProducerWords;
                return BusCycle{previous.ticks + cycles};
            }};
        }
#endif
        DspPayloadValue payloadValue;
#ifdef SILVER_KS_COMMAND_HOST
        if (liveMixer.active() && stage != RefreshStage::LiveMixer)
            payloadValue = {this, [](void* opaque, Endpoint endpoint, uint16_t address, uint32_t target) noexcept {
                auto& runtime = *static_cast<Runtime*>(opaque);
                return runtime.liveMixer.preserve(endpoint, address, target, runtime.state);
            }};
#endif
        require(transport->beginAddressed(session, offer, native(), {bus().ticks + 3}, hdiTiming, availability, payloadValue),
                "Could not begin addressed KS transfer");
    }

#ifndef SILVER_KS_COMMAND_HOST
    p09::Work exerciseWork(DspTicket ticket) const noexcept {
        if (pendingNote && feedback && feedback->sequence==ticket.sequence)
            return p09::Work::Pitch;
        if (stage==RefreshStage::Note) {
            DspTransactionProgress progress;
            if (session.transactions().progress(ticket,progress) &&
                progress.address.address==0x2b+101*size_t(noteStart.slot()))
                return p09::Work::Trigger;
            return p09::Work::Image;
        }
        if (stage==RefreshStage::ReleaseGate || stage==RefreshStage::VoiceStop)
            return p09::Work::Release;
        if (singleReload || sceneReload)
            return p09::Work::Program;
        return p09::Work::Other;
    }
#endif

    void startRefresh() {
        DspTicket ticket;
        require(refresh->admit(session, owner, ++operation, bus(), native(), seed.activeMask, ticket) == DspAdmission::Accepted,
                "Could not admit periodic A8056 signal");
        signal = ticket;
        ++bundle;
        event("refresh_admitted", seed.activeMask);
        KSRefreshPacket packet;
        require(packet.appendWord(Endpoint::A, 0x8056, seed.activeMask), "Invalid periodic signal");
        recordPacket("periodic", packet);
        stage = RefreshStage::Signal;
        startPort(ticket);
    }

    void readPartFeedback(unsigned index) {
        const auto source=KSGlobalState::mailboxSource(seed.layout,KSPart(index));
        require(bool(source),"Invalid part feedback configuration source");
        if(seed.layout==KSLayout::Performance || index==0)preflightPartConfiguration(*source);
        require(!feedback && !transport && partFeedback.prepare(session,globals,KSPart(index)) &&
                partFeedback.admit(++operation,native())==DspAdmission::Accepted,"Could not own part feedback");
        feedback=*partFeedback.ticket();feedbackAddress=KSCommandFeedback::partStatusAddress(KSPart(index));
        event("feedback_requested",feedbackAddress);startPort(*feedback);
    }

    void readVoiceFeedback(size_t index) {
        feedbackVoice=index;
        require(voices[index].allocation && voiceFeedback.prepare(session,state,*voices[index].allocation,voices[index].feedback) &&
            voiceFeedback.admit(++operation,native())==DspAdmission::Accepted,"Could not own correlated voice feedback");
        feedback=*voiceFeedback.ticket();feedbackAddress=KSCommandFeedback::voiceStatusAddress(VoiceSlot(index));
        event("feedback_requested",feedbackAddress);startPort(*feedback);
    }

    void observeRefreshFeedback(size_t first = 0) {
        // This diagnostic uses the existing refresh deadline as the sample
        // boundary. Each continuation is one correlated physical reply.
        for (size_t index = first; index < voices.size(); ++index) {
            const auto action = KSCommandFeedback::voiceAction(voices[index].feedback.age);
            if (action != KSVoiceFeedbackAction::Idle) {
                readVoiceFeedback(index);
                return;
            }
        }
        if (allocatingNote) {
            const auto note = *allocatingNote;
            allocatingNote.reset();
            allocateNote(note);
            return;
        }
        readPartFeedback(0);
    }

    void completeFeedback() {
        const auto ticket = *feedback;
        std::optional<DspReply> reply;
        const MusicalDispatch capture{&reply, [](void* context, CommandSession&, const ScheduledEvent& event,
                                                  NativeFrame) noexcept {
            const auto* value = std::get_if<DspReply>(&event.event.payload);
            if (!value)
                return DispatchDecision{DispatchDecision::Status::Fault, {}};
            *static_cast<std::optional<DspReply>*>(context) = *value;
            return DispatchDecision{};
        }};
        require(session.runUntil(native(), 1, capture).applied == 1 && reply && reply->endpoint == ticket.endpoint &&
                reply->transaction == ticket.sequence,
                "KS feedback reply lost its transaction owner");
        feedback.reset();
        event("feedback_observed", reply->word);
        if (pendingNote) {
            require(ticket.endpoint == Endpoint::B && feedbackAddress == 0x808c + 22 * *pendingNote,
                    "Note pitch reply has the wrong address");
            require(noteStart.acceptPitch(session,*reply), "Note pitch reply lost native startup ownership");
            const auto index = *pendingNote;
            const auto oldPitch = uint8_t(uint8_t(reply->word >> 16) + 60);
            const auto note = seed.voices[index].context.transposedNote;
            voices[index].pitchDistance = note < oldPitch ? uint8_t(oldPitch - note) : uint8_t(note - oldPitch);
            event("note_pitch_distance", voices[index].pitchDistance);
            pendingNote.reset();
            if(noteStart.suppressed()) {
                require(noteStart.finishSuppressed(session)==KSNoteStartProgress::Complete && noteStart.reset(),"Suppressed native note lost ownership");
                event("note_velocity_suppressed",index);completeNoteSequence();
            } else startNotePacket(index);
            return;
        }
        if(ticket.endpoint==Endpoint::A) {
            require(partFeedback.accept(*reply),"Part feedback lost its native owner");
            const auto result=partFeedback.result();require(result && partFeedback.reset(),"Could not retire part feedback");
            if(singleReload) {
                observeSingleReload(*result);
                return;
            }
            if(sceneReload) {
                observeScene(*result);
                return;
            }
            if(result->action==KSPartFeedbackAction::AcknowledgeAndRebuild) {
                const auto mailbox=KSPart(feedbackAddress-0x8041);
                if(seed.layout==KSLayout::Single && mailbox!=KSPart::One) {
                    KSRefreshPacket ack;
                    require(ack.appendWord(Endpoint::A,feedbackAddress,0),"KS inactive Single mailbox acknowledgement");
                    startPacket(RefreshStage::InactivePartAck,std::move(ack));
                    return;
                }
                const auto source=KSGlobalState::mailboxSource(seed.layout,mailbox);
                require(bool(source),"Invalid native part rebuild source");
                preflightPartConfiguration(*source);
                require(globalUpdate.prepare(session,globals,*source,mailbox),"Invalid native part rebuild source");
                event("part_feedback_rebuild",feedbackAddress);globalIsFeedback=true;feedbackConfigurationPending=true;feedbackConfigurationPart=*source;
                recordPacket("part-rebuild",globalUpdate.packet());stage=RefreshStage::GlobalAdmission;admitGlobalUpdate();return;
            }
            continuePartFeedback();return;
        }
        require(ticket.endpoint==Endpoint::B,"Unclassified correlated feedback endpoint");
        const auto progress=voiceFeedback.accept(*reply,native());
        if(progress==KSFeedbackProgress::Ready) {
            require(voiceFeedback.admit(++operation,native())==DspAdmission::Accepted,"Could not continue correlated voice age read");
            feedback=*voiceFeedback.ticket();feedbackAddress=uint16_t(0x808f+22*feedbackVoice);
            event("feedback_requested",feedbackAddress);startPort(*feedback);return;
        }
        require(progress==KSFeedbackProgress::Complete,"Native voice feedback did not complete");
        const auto result=voiceFeedback.result();require(result && voiceFeedback.reset(),"Voice feedback lost completed result");
        voices[feedbackVoice].feedback=*result;
        event(feedbackAddress==KSCommandFeedback::voiceStatusAddress(VoiceSlot(feedbackVoice))?"voice_pending_published":"voice_feedback_published",
              uint32_t(result->age)<<16 | result->countdown);
        if(feedbackForArpGate) {
            feedbackForArpGate=false;
            auto& binding=musical->voices[feedbackVoice];
            require(binding && musical->waitingGates.test(feedbackVoice),"KS arp feedback lost its gate");
            binding->nextStatus={native().ticks+1};
            return;
        }
        observeRefreshFeedback(feedbackVoice+1);return;
    }

    void continuePartFeedback() {
        if (feedbackAddress < 0x8044)
            readPartFeedback(feedbackAddress - 0x8041 + 1);
        else if (backgroundPartFeedback) {
            backgroundPartFeedback = false;
            lastPartPollFrame = native();
            event("part_poll_complete", 0);
        }
        else
            startRefresh();
    }

    void startPacket(RefreshStage nextStage, KSRefreshPacket packet) {
        stage = nextStage;
        const auto* name = "deferred";
        if (stage == RefreshStage::Auxiliary)
            name = "auxiliary";
        if (stage == RefreshStage::Program) name = "program";
        if (stage == RefreshStage::ProgramApply) name = "program-globals";
        if (stage == RefreshStage::Scene) name = "scene";
        if (stage == RefreshStage::InactivePartAck) name = "inactive-part-ack";
        if (stage == RefreshStage::Note) name = "note";
        if (stage == RefreshStage::VoiceStop) name = "voice-stop";
        recordPacket(name, packet);
        transfer.emplace(owner, std::move(packet));
        advancePacket();
    }

    bool startNextPart() {
        for(size_t p=0;p<updateParts.size();++p)if(updateParts.test(p)) {
            updateParts.reset(p);updatePart=KSStorageSlot(p);preflightPartConfiguration(updatePart);
            if(seed.layout==KSLayout::Performance && p==0) {
                require(globals.canPrepareRefresh(updatePart,true),"Stored configuration refresh capacity exhausted");
                activatePartConfiguration(updatePart);
                require(globals.requireStoredSingleRefresh() && globalUpdate.prepareStoredSingle(session,globals),"Could not own stored Single refresh");
                globalIsFeedback=false;recordPacket("stored-single-update",globalUpdate.packet());
                stage=RefreshStage::GlobalAdmission;admitGlobalUpdate();return true;
            }
            require(globals.canPrepareRefresh(updatePart),"Global configuration refresh capacity exhausted");
            rebaseParameters(updatePart);
            KSCommandVoiceUpdate::Contexts contexts{};
            for(size_t i=0;i<voices.size();++i) {
                auto context=seed.voices[i].context;
                if(const auto owner=state.voice(VoiceSlot(i));owner) {
                    const auto* source=expression.part(owner->part);require(source,"Missing owned expression");
                    context.wheelPitch=source->values.wheelPitch;context.modulation=source->values.modulation;
                }
                contexts[i]={context,uint8_t(voices[i].generation&7),seed.patches[p][1],voices[i].pitchDistance,0};
            }
            require(voiceUpdate.prepare(session,state,parameters,updatePart,seed.voiceTables,contexts,contextRevisions[p],drumBindings,drumStore,
                    updatePart==seed.trackingPart?seed.tracked:std::nullopt),"Could not own complete voice update");
            voiceUpdateApplied=false;
            recordPacket("voice-update",voiceUpdate.packet());stage=RefreshStage::VoiceUpdateAdmission;admitVoiceUpdate();return true;
        }
        return false;
    }

    void rebaseParameters(KSStorageSlot part,bool storedContext=false) {
        const auto current=storedContext?state.storedDependency(part,session.rateGeneration()):state.dependency(part,session.rateGeneration());const auto i=size_t(part);
        require(current && parameterSources[i] && parameters.rebase(*parameterSources[i],*current),"Could not rebase parameter source");
        parameterSources[i]=*current;
    }

    void rebaseParameterRate() {
        for(size_t i=0;i<parameterSources.size();++i)if(parameterSources[i]) {
            const auto part=KSStorageSlot(i);
            rebaseParameters(part,!state.keys(part));
        }
    }

    void preflightParameterContexts(bool allParts,KSStorageSlot selected,bool storedContext=false,size_t publications=1) {
        require(state.canPublish(native()),"Parameter publication exhausted");
        const auto previous=state.snapshot();
        require(!previous || previous->revision<=std::numeric_limits<uint64_t>::max()-publications,"Complete parameter publication capacity exhausted");
        for(size_t p=0;p<parameterSources.size();++p)if((state.keys(KSStorageSlot(p)) || (storedContext && p==size_t(selected))) && (allParts || p==size_t(selected))) {
            const auto dependency=storedContext?state.storedDependency(KSStorageSlot(p),session.rateGeneration()):state.dependency(KSStorageSlot(p),session.rateGeneration());
            require(dependency && parameterSources[p] && bool(parameters.observe(*parameterSources[p],state.part(KSStorageSlot(p))->parameters)) &&
                *dependency==*parameterSources[p] && (storedContext?state.canTouchStoredParameters(KSStorageSlot(p)):state.canTouchParameters(KSStorageSlot(p))) &&
                contextRevisions[p]!=std::numeric_limits<uint64_t>::max(),"Parameter context revision exhausted or stale");
        }
    }

    void refreshPartRoute(KSStorageSlot part) {
        if(!state.keys(part))return;
        const auto p=size_t(part);const auto applied=configuration.read(part);require(bool(applied),"Missing applied part route");
        auto route=metadata[p].route();route.low=applied->values.lowKey;route.high=applied->values.highKey;
        if(seed.layout==KSLayout::Single)route.channel=system.channel()<16?system.channel():KSCommandCutoff::NoChannel;
        else if(!seed.partChannelRoutingImmediate)route.channel=applied->values.channel<16?applied->values.channel:KSCommandCutoff::NoChannel;
        else route.channel=routing.config().parts[p].channel;
        auto routes=routing.config();const auto previous=routes.parts[p];
        if(previous.channel!=route.channel || previous.low!=route.low || previous.high!=route.high) {
            routes.parts[p]=route;require(routing.configure(routes),"Could not publish applied part routing");
            routeBatch.revision=routing.revision();
        }
        seed.partChannels[p]=route.channel;
    }
    void preflightPartConfiguration(KSStorageSlot part) {
        const auto p=size_t(part);const auto values=KSPartConfigurationValues::fromPatch(seed.patches[p]);
        require(contextRevisions[p]!=std::numeric_limits<uint64_t>::max() && routing.revision()!=std::numeric_limits<uint64_t>::max() &&
            configuration.canActivate(part,values,seed.patches[p][209],contextRevisions[p]),"Applied part configuration revision exhausted");
    }
    void activatePartConfiguration(KSStorageSlot part) {
        preflightPartConfiguration(part);const auto p=size_t(part);
        const auto before=configuration.read(part);const auto values=KSPartConfigurationValues::fromPatch(seed.patches[p]);
        require(configuration.activate(part,values,seed.patches[p][209],contextRevisions[p]),"Could not activate complete part configuration");
        if(!(before->values==values) || before->noteDispatchMode!=seed.patches[p][209])++contextRevisions[p];
        seed.partContexts[p].wheelFine=values.detune;
        for(size_t i=0;i<voices.size();++i)if(const auto voice=state.voice(VoiceSlot(i));voice && voice->part==part)
            seed.voices[i].context.wheelFine=values.detune;
        refreshPartRoute(part);event("part_configuration_activated",uint32_t(p)<<24 | uint32_t(values.velocityCurve)<<16 |
            uint32_t(values.transpose)<<8 | values.detune);
    }

    void preflightGlobalChannel(uint8_t channel,uint64_t epoch) {
        const auto current=configuration.read(KSStorageSlot::Single);require(bool(current),"Missing global channel configuration mirror");
        auto values=current->values;values.channel=channel;
        require(configuration.canActivate(KSStorageSlot::Single,values,epoch),"Global channel configuration mirror exhausted");
    }
    void updateGlobalChannel() {
        auto routes=routing.config();routes.parts[0].channel=system.channel()<16?system.channel():KSCommandCutoff::NoChannel;
        if(metadata[0].channel!=system.channel()) {
            require(metadata[0].revision!=std::numeric_limits<uint64_t>::max(),"Global channel mirror revision exhausted");
            metadata[0].channel=system.channel();++metadata[0].revision;
        }
        if(seed.layout==KSLayout::Single)seed.partChannels[0]=routes.parts[0].channel;
        auto applied=configuration.read(KSStorageSlot::Single)->values;applied.channel=system.channel();
        require(configuration.activate(KSStorageSlot::Single,applied,contextRevisions[0]),"Could not publish global channel configuration mirror");
        require(routing.configure(routes),"Could not publish global MIDI channel");routeBatch.revision=routing.revision();
        seed.channel=routes.parts[0].channel;seed.patches[0][181]=system.channel();
    }

    void reconcileEditor() {
        require(editor.select(seed.layout,KSCommandEditorSelection::reconciled(seed.layout,seed.editorSelectionMask)),"Editor selection revision exhausted");
    }

#ifdef SILVER_KS_COMMAND_HOST
    void queueLiveLevel(KSStorageSlot part, unsigned field, const KSVoiceImagePatch& before,
                        const KSVoiceImagePatch& after) noexcept {
        if (before[field] == after[field])
            return;
        const auto revision = state.part(part)->revisions.program;
        const auto at = native();
        if (field == KSLiveMixer::PartLevel) {
            liveMixer.route(part, *KSMusicalState::logicalPart(part), revision, after[180],
                            before[field], after[field], at);
            return;
        }
        const unsigned lane = field - KSLiveMixer::FirstVoiceLevel;
        for (size_t i = 0; i < KSMusicalState::VoiceCount; ++i) {
            const auto voice = state.voice(VoiceSlot(i));
            if (!voice || voice->part != part || !voice->feedbackAge ||
                (lane < KSLiveMixer::OscillatorCount && seed.voices[i].context.panelGate && after[222] != lane))
                continue;
            const auto binding = drumBindings.binding(voice->allocation);
            if (binding && binding->source.action != KSNoteAssignmentAction::PlayDrumRecord)
                liveMixer.voiceLevel(part, voice->allocation, revision, lane, before[field], after[field], at);
        }
    }
#endif

    void applyParameter(ControlChange input,bool normalizedAlias=false,const KSCommandGlobalFallback::PackedEdit* packed=nullptr) {
        if(!packed && !normalizedAlias && seed.layout==KSLayout::Performance &&
           seed.partChannelRoutingImmediate && input.controller==6) {
            const bool partEdit=routedInput && routedInput->part!=RoutedByChannel;
            const auto partSelector=parameterIngress.selector(dispatchPart);
            const auto selector=partEdit?partSelector:desktopSelection.selector(input.channel);
            // NRPN 86 updates the desktop routes without rebuilding a voice.
            // An editor edit owns one part. Raw MIDI still owns the channel.
            if(selector==86) {
                auto routes=routing.config();
                const auto channel=uint8_t(input.value&15);
                std::bitset<KSMusicalState::StorageSlotCount> changed;
                for(size_t p=1;p<routes.parts.size();++p) {
                    auto& route=routes.parts[p];
                    const bool recipient=partEdit?p==size_t(dispatchPart):route.channel==input.channel;
                    if(recipient && route.channel!=channel) {
                        route.channel=channel;
                        changed.set(p);
                    }
                }
                if(changed.any()) {
                    require(routing.configure(routes),"Could not publish desktop part channels");
                    routeBatch.revision=routing.revision();
                    for(size_t p=1;p<routes.parts.size();++p)if(changed[p]) {
                        seed.partChannels[p]=channel;
                        if(musical)configureArp(KSStorageSlot(p),routedAt);
                    }
                }
                event("desktop_part_channel_published",channel);return;
            }
            if(selector!=partSelector) {
                require(bool(selector),"Missing desktop sender selector");
                applyParameter({input.channel,98,*selector});
            }
        }
        const bool storedContext=packed!=nullptr;
        const auto part=packed?packed->editor.storage:dispatchPart;const auto p=size_t(part);rebaseParameters(part,storedContext);
        const auto dependency=storedContext?state.storedDependency(part,session.rateGeneration()):state.dependency(part,session.rateGeneration());
        const auto patch=projectKsOwnedPatch(parameters,globals,state,part,metadata[p],system,session.rateGeneration(),storedContext);
        const auto plan=packed?std::optional{packed->plan}:patch && dependency?parameterIngress.prepare(*dependency,*patch,input,false,normalizedAlias):std::nullopt;
        require(bool(plan),"Invalid native mapped parameter source");
        if(plan->action==KSParameterIngressAction::ReservedPackedBoundary) {event("parameter_reserved_boundary_rejected",input.value);return;}
        require(plan->action!=KSParameterIngressAction::ReservedControl,"Reserved MIDI control reached generic parameter edit");
        KSCommandParameterEdit edit;
        const auto prepared=edit.prepare(parameterIngress,parameters,globals,state,metadata[p],system,*plan,session.rateGeneration(),
            packed?&editor:nullptr,packed?std::optional{packed->editor}:std::nullopt);
        if(prepared==KSParameterEditStatus::Unsupported){require(!packed,"Unowned packed editor field");event("parameter_unowned_rejected",plan->field);return;}
        require(prepared==KSParameterEditStatus::Prepared,"Could not prepare complete mapped parameter edit");
        const auto policy=edit.policy();
        preflightParameterContexts(policy.globalPitch,part,storedContext,policy.globalPitch || packed?2:1);
        if(plan->action==KSParameterIngressAction::Patch)
            require(editor.canSelect(seed.layout,KSCommandEditorSelection::reconciled(seed.layout,seed.editorSelectionMask)),"Editor reconciliation exhausted");
        if(policy.clock)require(clock.available() && clock.projection().accepted!=std::numeric_limits<uint64_t>::max(),"Pending clock request capacity exhausted");
        const auto previousChannel=system.channel();
        if(policy.midiPolicy)require(metadata[0].revision<std::numeric_limits<uint64_t>::max()-1,"Global channel mirror capacity exhausted");
        if(edit.after()[242]!=previousChannel)preflightGlobalChannel(edit.after()[242],contextRevisions[0]+uint64_t(p==0));
        require((!(policy.routing || policy.midiPolicy) || routing.revision()!=std::numeric_limits<uint64_t>::max()) &&
                (!policy.expression || expression.part(part)->revision!=std::numeric_limits<uint64_t>::max()),"Parameter policy revision exhausted");
        bool liveLevel = false;
#ifdef SILVER_KS_COMMAND_HOST
        liveLevel = !storedContext && plan->action == KSParameterIngressAction::Patch && plan->editCount == 1 &&
                    KSLiveMixer::supports(plan->field) &&
                    (plan->field != KSLiveMixer::PartLevel || !pendingMixerRoute.test(p));
#endif
        require(edit.commit(native(), !liveLevel)==KSParameterEditStatus::Applied,"Could not commit complete mapped parameter edit");
        parameterSources[p]=*(storedContext?state.storedDependency(part,session.rateGeneration()):state.dependency(part,session.rateGeneration()));
        if(plan->action==KSParameterIngressAction::Selector || plan->action==KSParameterIngressAction::Ignore) {
            event(plan->action==KSParameterIngressAction::Selector?"parameter_selector":"parameter_ignored",input.value);return;
        }
#ifdef SILVER_KS_COMMAND_HOST
        if (!storedContext && (*patch)[180] != edit.after()[180]) {
            pendingMixerRoute.set(p);
            liveMixer.cancelRoute(*KSMusicalState::logicalPart(part));
        }
        if (liveLevel)
            queueLiveLevel(part, plan->field, *patch, edit.after());
#endif
        seed.patches[p]=edit.after();
        if(policy.routing) {
            if(seed.layout==KSLayout::Single){metadata[p].channel=system.channel();seed.patches[p][181]=system.channel();}
            refreshPartRoute(part);
        }
        if(policy.expression)require(expression.setPolicy(part,metadata[p].expressionController,seed.restartMono[p]),"Could not apply expression policy");
        seed.velocityPower=system.velocityPower();
        for(size_t i=0;i<voices.size();++i) {
            const auto voice=state.voice(VoiceSlot(i));
            if(voice && voice->part==part) {
                // Transpose is captured by voice population; fine tuning is
                // sampled from the activated configuration, not desired185.
                seed.voices[i].context.wheelFine=configuration.read(part)->values.detune;
            }
            if(policy.globalPitch)seed.voices[i].context.globalFine=system.globalFine();
        }
        seed.partContexts[p].wheelFine=configuration.read(part)->values.detune;
        if(policy.globalPitch)for(size_t q=0;q<seed.partContexts.size();++q) {
            seed.partContexts[q].globalFine=system.globalFine();
            if(q!=p && state.keys(KSStorageSlot(q))) {
                require(state.touchParameters(KSStorageSlot(q),false),"Global tuning revision exhausted");rebaseParameters(KSStorageSlot(q));
                ++contextRevisions[q];
            }
        }
        ++contextRevisions[p];
        if (!liveLevel) auxDirty=true;
        if(policy.globalPitch)require(state.publish(native()),"Could not publish global tuning dependencies");
        if(system.channel()!=previousChannel)updateGlobalChannel();
        if(plan->action==KSParameterIngressAction::Patch && !packed)reconcileEditor();
        event("parameter_published",uint32_t(plan->field)<<8 | plan->value);
        if(musical && ((plan->field>=85 && plan->field<=88) || plan->field==149 || plan->field==167))
            configureArp(part,routedAt);
        if(policy.clock) {
            require(clock.parameter(owner,musical?routedAt:native(),plan->field,plan->value)==KSClockAdmission::Accepted,
                    "Could not own clock policy request");
            event("clock_policy_published",system.tempo);
        }
        if(policy.midiPolicy)event("midi_policy_published",system.midiSendMode());
    }

    void admitVoiceUpdate() {
        const auto result=voiceUpdate.admit(++operation,native(),contextRevisions[size_t(updatePart)]);
        if(result==DspAdmission::Full)return;
        require(result==DspAdmission::Accepted,"Native voice update admission failed");
        if(!voiceUpdateApplied) {
            for(size_t i=0;i<voices.size();++i)if(const auto glide=voiceUpdate.glide(VoiceSlot(i));glide && glide->changedDistanceAndPrevious) {
                voices[i].pitchDistance=glide->distance;
                for(auto& voice:seed.voices)voice.context.previousNote=glide->previousNote;
                for(auto& context:seed.partContexts)context.previousNote=glide->previousNote;
            }
            voiceUpdateApplied=true;
        }
        if(voiceUpdate.ticket()){stage=RefreshStage::VoiceUpdateTransfer;startPort(*voiceUpdate.ticket());}
        else finishVoiceUpdate();
    }
    void finishVoiceUpdate() {
        const auto progress=voiceUpdate.progress(contextRevisions[size_t(updatePart)]);
        if(progress==KSRefreshProgress::Ready){stage=RefreshStage::VoiceUpdateAdmission;admitVoiceUpdate();return;}
        require(progress==KSRefreshProgress::Complete,"Native voice update did not finish");
        require(voiceUpdate.reset(),"Could not retire complete voice update");
        activatePartConfiguration(updatePart);
        require(globals.requireRefresh(updatePart) && globalUpdate.prepare(session,globals,updatePart),
                "Could not own mandatory full globals sequence");
        globalIsFeedback=false;recordPacket("global-update",globalUpdate.packet());stage=RefreshStage::GlobalAdmission;admitGlobalUpdate();
    }
    void admitGlobalUpdate() {
        const auto result=globalUpdate.admit(++operation,native());if(result==DspAdmission::Full)return;
        require(result==DspAdmission::Accepted,"Native global update admission failed");
        stage=RefreshStage::GlobalTransfer;startPort(*globalUpdate.ticket());
    }
    void finishGlobalUpdate() {
        const auto progress=globalUpdate.progress();
        if(progress==KSRefreshProgress::Ready){
            if(feedbackConfigurationPending){activatePartConfiguration(feedbackConfigurationPart);feedbackConfigurationPending=false;}
            stage=RefreshStage::GlobalAdmission;admitGlobalUpdate();return;
        }
        require(progress==KSRefreshProgress::Complete,"Native globals intent did not finish");
#ifdef SILVER_KS_COMMAND_HOST
        if (const auto* source = globalUpdate.source(); source && !source->storedSingle)
            pendingMixerRoute.reset(size_t(source->source));
#endif
        require(globalUpdate.reset(),"Native globals owner did not reset");
        stage=RefreshStage::None;
        if(globalIsFeedback){event("part_feedback_rebuild_accepted",feedbackAddress);globalIsFeedback=false;continuePartFeedback();return;}
        if(startNextPart())return;
        require(deferred.finish(),"Deferred complete owner lost its batch");event("bundle_complete",1);
    }

    void selectDeferred() {
        const auto selected = deferred.preview(state);
        if (selected != KSDeferredSelection::Single && selected != KSDeferredSelection::Performance) {
            require(deferred.commit(state, selected), "Unsupported deferred selection");
            stage = RefreshStage::None;
            return;
        }
        const auto dirty = state.dirtySlots();
        updateParts=dirty;
        require(deferred.commit(state, selected), "Deferred selection changed before commit");
        event("dirty_taken", uint32_t(dirty.to_ulong()));
        require(startNextPart(), "Admitted update has no part packet");
    }

    void advancePacket() {
        const auto progress = transfer->progress(session);
        if (progress == KSRefreshProgress::Ready) {
            require(transfer->admit(session, ++operation, native()) == DspAdmission::Accepted,
                    "Could not admit addressed KS update word");
            startPort(*transfer->ticket());
            return;
        }
        require(progress == KSRefreshProgress::Complete, "KS transfer ended before packet completion");
        transfer.reset();
        if (stage == RefreshStage::InactivePartAck) {
            const auto mailbox=KSPart(feedbackAddress-KSCommandFeedback::partStatusAddress(KSPart::One));
            require(globals.acknowledgeInactiveSingleMailbox(mailbox),"KS inactive mailbox receipt lost its owner");
            event("inactive_part_acknowledged",feedbackAddress);
            stage=RefreshStage::None;
            continuePartFeedback();
            return;
        }
        if (stage == RefreshStage::Scene) {
            advanceScene();
            return;
        }
        if (singleReload && (stage == RefreshStage::Program || stage == RefreshStage::ProgramApply)) {
            singleReload->awaitingFence = stage == RefreshStage::ProgramApply;
            singleReload->nextPoll = native();
            stage = RefreshStage::None;
            return;
        }
        if (stage == RefreshStage::Program) {
            require(globals.acknowledgeAuxiliary(auxiliaryRevision,auxiliaryValue),"Program auxiliary receipt lost its source");
            event("program_packet_accepted", 1);
            if(musical)musical->phaseKeys[0].programApplied(syncMode());
            stage = RefreshStage::None;
            return;
        }
        if (stage == RefreshStage::Auxiliary) {
            require(globals.acknowledgeAuxiliary(auxiliaryRevision,auxiliaryValue),"Auxiliary receipt lost its source");
            selectDeferred();
            return;
        }
        if (stage == RefreshStage::LiveMixer) {
            stage = RefreshStage::None;
            return;
        }
        require(false,"Unexpected generic packet continuation");
    }

    void advanceSignal() {
        require(session.transactions().retire(*signal), "Could not retire periodic KS signal");
        signal.reset();
#ifdef SILVER_P12_ACTION_TRACE
        event("refresh_transferred",0x8056);
#endif
        if (auxDirty) {
            auxDirty = false;
            KSRefreshPacket packet;
            const auto source=editor.projection();require(bool(source),"Missing auxiliary editor source");
            const auto selected=globals.selectAuxiliary(editor,*source);
            require(selected!=KSGlobalEdit::Invalid && selected!=KSGlobalEdit::Exhausted,"Invalid selected auxiliary source");
            auxiliaryValue=globals.auxiliary();auxiliaryRevision=globals.auxiliaryRevision();
            require(appendKsAuxiliary(packet, auxiliaryValue), "Invalid prepared panel auxiliary value");
            startPacket(RefreshStage::Auxiliary, packet);
        } else {
            selectDeferred();
        }
    }

    DispatchDecision releaseNote(const ScheduledEvent& event, NativeFrame) {
        const auto* note = std::get_if<NoteOff>(&event.event.payload);
        const auto* pedal=std::get_if<ControlChange>(&event.event.payload);
        const auto panic=pedal?ksPanicControl(pedal->controller,pedal->value):std::nullopt;
        const auto part=dispatchPart;const auto p=size_t(part);
        if ((!note && (!pedal || (pedal->controller!=64 && !panic))) || event.session != owner ||
            (note?note->channel:pedal->channel)!=seed.partChannels[p])
            return {DispatchDecision::Status::Fault, {}};
        if(panic==KSPanicAction::Ignore){this->event("panic_ignored",pedal->controller);return {};}
        if(musical) {
            auto& arp=musical->parts[p];
            auto& drum=musical->drums[p];
            if(note) {
                musical->phaseKeys[p].release(note->note);
                if(arp.enabled())
                    require(arp.release(event.requestedTime,note->note),"KS arp key release deadline");
                if(drum.enabled())
                    require(drum.releaseKey(event.requestedTime,note->note),"KS drum key release deadline");
                if((arp.enabled() || drum.enabled()) &&
                   !state.keys(part)->projection().physical[note->note])return {};
            }
            if(pedal && pedal->controller==64) {
                require(arp.pedal(event.requestedTime,pedal->value>=64),"KS arp sustain deadline");
                if(drum.enabled())
                    require(drum.pedal(event.requestedTime,pedal->value>=64),"KS drum sustain deadline");
            }
            if(panic==KSPanicAction::AllSoundOff || panic==KSPanicAction::AllNotesOff)
            {
                musical->phaseKeys[p].clear();
                require(arp.clear(event.requestedTime) && drum.clear(event.requestedTime),
                        "KS arp panic deadline");
            }
        }
        if(panic==KSPanicAction::AllSoundOff) {
            require(soundOff.prepare(session,state),"Could not prepare complete all-sound-off");
            stage=RefreshStage::SoundAdmission;admitSoundOff();return {};
        }
        std::optional<KSMonoReplacement> replacement;
        if(note) {
            auto keys=*state.keys(part);require(keys.release(note->note),"Invalid projected mono key release");
            replacement=ksMonoReplacement(keys.projection(),note->note,seed.lastPlayed[p],KSVoiceMode(configuration.read(part)->noteDispatchMode));
        }
        uint16_t excludedSlots=0;
        if(note && musical)for(const auto& binding:musical->voices)
            if(binding && state.voice(binding->voice))excludedSlots|=uint16_t(1u<<size_t(binding->voice.slot));
        auto release = note?(replacement?KSCommandNoteRelease::prepareKeysOnly(state,part,note->note):
                                       KSCommandNoteRelease::prepare(state,part,note->note,excludedSlots)):
                       panic==KSPanicAction::AllNotesOff?KSCommandNoteRelease::prepareAllNotes(state,part):
                           KSCommandNoteRelease::prepareSustain(state,part,pedal->value);
        if (!release || pendingRelease)
            return {DispatchDecision::Status::Fault, {}};
        pendingRelease=*release;pendingReleasePedal=pedal && pedal->controller==64;
        if(replacement)replacementNote=NoteOn{seed.partChannels[p],replacement->note,replacement->velocity};
        pendingAllNotes=panic==KSPanicAction::AllNotesOff;pendingReleaseValue=note?note->note:pendingAllNotes?pedal->controller:pedal->value;
        stage=RefreshStage::ReleaseAdmission;admitPendingRelease();
        return {};
    }

    void admitPendingRelease() {
        require(bool(pendingRelease),"Missing complete release intent");
        if(pendingRelease->size() && noteCapacity && noteCapacity->release && !noteCapacity->active && !noteCapacity->completed)
            fillNoteCapacity("voice_release_capacity_held",pendingRelease->release(0).allocation.slot);
        const auto result=pendingRelease->admit(session,++operation,native());
        if(result==DspAdmission::Full){event("voice_release_full",pendingRelease->size());return;}
        if(result!=DspAdmission::Accepted){session.stop(owner);require(false,"Native release admission failed");}
        const auto& release=pendingRelease;
        releaseCount = release->size();
        if(!pendingReleasePedal && release->deferredCount()) event("note_deferred_voices",release->deferredCount());
        releaseIndex = 0;
        for (size_t i = 0; i < releaseCount; ++i) {
            const auto& item = release->release(i);
            auto& voice = voices[size_t(item.allocation.slot)];
            voice.feedback = {release->releasedAge(i), 0xffff};
            releaseTickets[i] = *release->ticket(i);
            releaseAddresses[i] = item.address;
        }
        event(arpReleaseDeadline?"arp_gate_published":pendingAllNotes?"all_notes_off_applied":
            pendingReleasePedal?"sustain_published":"note_off_applied",pendingReleaseValue);
        if(!releaseCount)arpReleaseDeadline.reset();
        pendingRelease.reset();stage=releaseCount?RefreshStage::ReleaseGate:RefreshStage::None;
        if(replacementNote){const auto next=*replacementNote;replacementNote.reset();startNote(next,false);}
    }

    void admitSoundOff() {
        if(noteCapacity && noteCapacity->release && !noteCapacity->active && !noteCapacity->completed && soundOff.size())
            fillNoteCapacity("voice_release_capacity_held",soundOff.slot());
        const auto result=soundOff.admit(++operation,native());
        if(result==DspAdmission::Full){event("all_sound_off_full",2);return;}
        require(result==DspAdmission::Accepted,"Complete all-sound-off admission failed");
        if(!soundPublished) {
            for(size_t i=0;i<voices.size();++i) {
                const auto model=state.voice(VoiceSlot(i));
                if(model && model->feedbackAge)voices[i].feedback={model->feedbackAge,0xffff};
            }
            soundPublished=true;event("all_sound_off_applied",soundOff.size());
        }
        soundIndex=0;
        if(!soundOff.size()){require(soundOff.reset(),"Empty all-sound-off reset");soundPublished=false;stage=RefreshStage::None;}
        else stage=RefreshStage::SoundTransfer;
    }

    KSRefreshPacket fadeSingle(const KSVoiceImagePatch& patch) {
        KSRefreshPacket packet;
        require(packet.appendWord(Endpoint::A, 0x8041, 1) && appendKsAuxiliary(packet, patch[225]),
                "Could not encode program fade and auxiliary reset");
        for (size_t index = 0; index < voices.size(); ++index) {
            auto& voice = voices[index];
            if (!voice.allocation || !voice.feedback.age) continue;
            require(packet.appendWord(Endpoint::B, uint16_t(0x72 + 101 * index), 0x100000) &&
                    packet.appendWord(Endpoint::B, KSCommandFeedback::voiceStatusAddress(VoiceSlot(index)), 0),
                    "Could not encode program voice release");
            const auto age = uint16_t((voice.feedback.age & 0x3fff) | 1);
            require(state.observeVoiceFeedback(*voice.allocation, age), "Program lost its voice owner");
            voice.feedback = {age, 0xffff};
        }
        return packet;
    }

    void commitSingle(ProgramSelection selected, const KSVoiceImagePatch& patch) {
        seed.patches[0] = patch;
        seed.singleSelection = selected;
        seed.partGeneration = uint8_t(seed.partGeneration + 8);
        require(state.clearKeys(KSStorageSlot::Single), "Program lost its key state");
        require(state.setSustainBypass(KSStorageSlot::Single,bool(patch[88]&4)),"Program lost its sustain policy");
        seed.restartMono[0]=true;
        require(state.loadProgram(KSStorageSlot::Single,seed.singleSelection,{patch[46],patch[150]},KSProgramRefresh::OwnedReload),
                "Could not commit selected program source");
        const auto source=state.dependency(KSStorageSlot::Single,session.rateGeneration());
        require(source && parameters.load(*source,patch) && globals.loadPart(KSStorageSlot::Single,KSGlobalPart::fromPatch(patch),patch[225]) &&
            expression.setPolicy(KSStorageSlot::Single,patch[177],true),"Program lost its typed parameter owners");
        parameterSources[0]=*source;
        require(drumBindings.rebindEditablePart(state,KSStorageSlot::Single,session.rateGeneration()),"Could not rebind editable program tails");
        metadata[0]=KSParameterMetadata::fromPatch(patch);
        KSGlobalEditSet globalEdit;globalEdit.master=KSGlobalMaster::fromPatch(patch,globals.observedEffectStatus());
        globalEdit.auxiliary=patch[225];require(globals.apply(globalEdit)!=KSGlobalEdit::Invalid,"Invalid program globals");
        for (auto& voice : seed.voices) voice.context.panelGate = false;
        seed.auxiliary = patch[225];
        seed.globalMaster = KSGlobalMaster::fromPatch(patch, seed.globalMaster.previousEffectStatus);
        require(state.publish(native()), "Could not publish selected program");
        auxDirty = true;
        reconcileEditor();auxiliaryValue=patch[225];auxiliaryRevision=globals.auxiliaryRevision();
        event("program_published", 256u * selected.bank + selected.program);
        event("program_cutoff_published", patch[46]);
    }

    void changeProgram(const ProgramChange& input) {
        require(seed.layout == KSLayout::Single && input.channel == seed.channel && seed.bankLatch == 33 &&
                input.program == 1 && seed.hasSingleProgramRecord, "Unsupported P05 program selection");
        const auto patch = decodeKsSingleProgram(seed.patches[0], seed.singleProgramRecord);
        require(patch[209] == 2 && patch[210] == 0 && patch[3] == 0 && patch[4] == 64,
                "P05 program requires poly, no unison, neutral detune and zero glide");
        auto packet = fadeSingle(patch);
        commitSingle({1, input.program}, patch);
        startPacket(RefreshStage::Program, std::move(packet));
    }

    bool beginSingleReload(ProgramSelection selection, const KSVoiceImagePatch& patch, NativeFrame requested, bool fromBank) {
        if(!canLoadPart(KSStorageSlot::Single,patch))return false;
        KSRefreshPacket effects;
        const auto master = KSGlobalMaster::fromPatch(patch, globals.observedEffectStatus());
        if (!effects.appendWord(Endpoint::A, KSCommandFeedback::partStatusAddress(KSPart::One), 0) ||
            !appendKsGlobals(effects, KSGlobalPart::fromPatch(patch), master, seed.globalTables))
            return false;
        if (musical) {
            require(musical->parts[0].clear(requested) && musical->drums[0].clear(requested), "KS arp program barrier");
            musical->voices.fill(std::nullopt);
            musical->waitingGates.reset();
            musical->reloadAt = requested;
        }
#ifdef SILVER_KS_COMMAND_HOST
        liveMixer.cancelPart(KSStorageSlot::Single);
        pendingMixerRoute.reset(size_t(KSStorageSlot::Single));
#endif
        singleReload.emplace(SingleReload{selection, patch, effects, native(), native(), fromBank});
        event("program_started", uint32_t(selection.bank) * 256 + selection.program);
        startPacket(RefreshStage::Program, fadeSingle(patch));
        return true;
    }

    void observeSingleReload(KSPartFeedbackResult receipt) {
        require(feedbackAddress == KSCommandFeedback::partStatusAddress(KSPart::One), "KS program reply address");
        auto& job = *singleReload;
        if (!job.awaitingFence && receipt.action == KSPartFeedbackAction::AcknowledgeAndRebuild) {
            event("program_dsp_ready", receipt.status);
            startPacket(RefreshStage::ProgramApply, job.globals);
        } else if (job.awaitingFence && receipt.status == 0) {
            commitSingle(job.selection, job.patch);
            commitDesktopPatchDrums(1);
            preflightPartConfiguration(KSStorageSlot::Single);
            activatePartConfiguration(KSStorageSlot::Single);
            require(globals.acknowledgeAuxiliary(auxiliaryRevision, auxiliaryValue), "KS program auxiliary fence");
            auxDirty = false;
            if (musical) musical->phaseKeys[0].programApplied(syncMode());
            singleReload.reset();
            finishDesktopSoundLoad(KSDesktopLoadStatus::Applied);
        } else {
            job.nextPoll = {native().ticks + 4};
        }
    }

    static KSVoiceImagePatch performancePatch(const KSVoiceImagePatch& stored,const KSVoiceImagePatch& record) {
        std::array<uint8_t,209> packed;
        std::copy_n(record.begin(),packed.size(),packed.begin());
        return decodeKsSingleProgram(stored,packed);
    }

    bool canLoadPart(KSStorageSlot part,const KSVoiceImagePatch& patch) const {
        const auto revision=contextRevisions[size_t(part)];
        return revision!=std::numeric_limits<uint64_t>::max() && parameters.canLoad(part,patch) &&
            configuration.canActivate(part,KSPartConfigurationValues::fromPatch(patch),patch[209],revision);
    }

    bool canLoadScenePart(KSLayout layout,size_t index,const KSVoiceImagePatch& patch,const KSGlobalMaster& master) const {
        const bool multi=layout==KSLayout::Performance;
        KSRefreshPacket verification;
        return canLoadPart(KSStorageSlot(index+multi),patch) &&
            appendKsGlobals(verification,KSGlobalPart::fromPatch(patch),master,seed.globalTables,
                            multi?std::optional{KSPart(index)}:std::nullopt);
    }

    bool beginScene(KSLayout layout,const KSPerformanceRecord& records,ProgramSelection single,uint8_t performance,
                    NativeFrame requested) {
        const bool multi = layout == KSLayout::Performance;
        SceneReload next{layout,{},single,performance,uint8_t(multi ? records.front()[190]&15 : 1),
                        seed.activeMask,{},native(),native()};
        next.partMask=multi?15:1;
        next.master=KSGlobalMaster::fromPatch(multi?seed.patches[0]:records[0],globals.observedEffectStatus());
        if(multi)next.master.performanceEffectStatus=records[0][208];
        for(size_t i=0;i<(multi?4u:1u);++i) {
            const auto part=i+multi;
            next.patches[i]=performancePatch(seed.patches[part],records[i]);
            if(!canLoadScenePart(layout,i,next.patches[i],next.master))
                return false;
        }
        startScene(std::move(next),requested);
        return true;
    }

    void startScene(SceneReload next,NativeFrame requested) {
#ifdef SILVER_KS_COMMAND_HOST
        for (size_t p = 0; p < KSMusicalState::StorageSlotCount; ++p) {
            const auto part = KSStorageSlot(p);
            if (next.ownsVoice(part) || next.layout != seed.layout) {
                liveMixer.cancelPart(part);
                pendingMixerRoute.reset(p);
            }
        }
#endif
        clearSceneArps(std::max(requested,native()),next);
        KSRefreshPacket fade;
        for(unsigned i=0;i<4;++i)if(next.mailboxMask&(1u<<i))
            require(fade.appendWord(Endpoint::A,KSCommandFeedback::partStatusAddress(KSPart(i)),1),"KS scene fade capacity");
        const bool multi=next.layout==KSLayout::Performance;
        if(next.programsOnly)event("part_reload_started",next.partMask);
        else event("scene_started",uint32_t(multi)<<8|(multi?next.performance:next.single.program));
        sceneReload.emplace(std::move(next));
        if(fade.size())startPacket(RefreshStage::Scene,std::move(fade));
        else stopSceneVoices();
    }

    void clearSceneArps(NativeFrame at,const SceneReload& reload) {
        if(!musical)return;
        for(size_t p=0;p<musical->parts.size();++p) {
            if(!reload.ownsVoice(KSStorageSlot(p)))continue;
            auto& arp=musical->parts[p];
            auto& drum=musical->drums[p];
            require(arp.clear(at) && drum.clear(at),"KS scene arp reset");
            while(const auto* note=arp.front()) {
                writeArpMidi(*note);
                arp.pop();
            }
            while(const auto drumNote=drum.pop())writeArpMidi(drumNote->note);
            require(arp.configure(at,{}) && drum.configure(at,{}),"KS scene arp disable");
        }
        for(size_t i=0;i<musical->voices.size();++i) {
            const auto voice=state.voice(VoiceSlot(i));
            if(!voice || reload.ownsVoice(voice->part)) {
                musical->voices[i].reset();
                musical->waitingGates.reset(i);
            }
        }
    }

    void stopSceneVoices() {
        auto& job=*sceneReload;
        job.step=SceneReload::Step::Stop;
        event(job.programsOnly?"part_reload_dsp_faded":"scene_dsp_faded",job.programsOnly?job.partMask:seed.activeMask);
        KSRefreshPacket stops;
        for(size_t slot=0;slot<voices.size();++slot) {
            const auto voice=state.voice(VoiceSlot(slot));
            if(!voice || !job.ownsVoice(voice->part))continue;
            const auto stop=encodeKsVoiceStop(VoiceSlot(slot));
            require(bool(stop),"KS scene voice stop rejected");
            for(size_t i=0;i<stop->size();++i) {
                const auto write=*stop->at(i);
                require(stops.append(write.endpoint,write.address,write.words,write.count),"KS scene voice stop capacity");
            }
            ++job.stoppedVoices;
        }
        if(stops.size())startPacket(RefreshStage::Scene,std::move(stops));
        else advanceScene();
    }

    void startSceneEffects() {
        auto& job=*sceneReload;
        const bool multi=job.layout==KSLayout::Performance;
        KSRefreshPacket packet;
        require(packet.appendWord(Endpoint::A,KSCommandFeedback::partStatusAddress(KSPart(job.part)),0) &&
                appendKsGlobals(packet,KSGlobalPart::fromPatch(job.patches[job.part]),job.master,seed.globalTables,
                                multi?std::optional{KSPart(job.part)}:std::nullopt),"KS scene effects rejected after preflight");
        job.master.previousEffectStatus=multi?job.master.performanceEffectStatus:0;
        startPacket(RefreshStage::Scene,std::move(packet));
    }

    void confirmScene(SceneReload& job,uint8_t mask) {
        job.step=SceneReload::Step::Confirm;
        job.mailboxMask=mask;
        job.nextPoll=native();
    }

    void advanceScene() {
        auto& job=*sceneReload;
        stage=RefreshStage::None;
        switch(job.step) {
        case SceneReload::Step::Fade:
            job.nextPoll=native();
            return;
        case SceneReload::Step::Stop:
            event(job.programsOnly?"part_reload_voices_stopped":"scene_voices_stopped",job.stoppedVoices);
            job.step=SceneReload::Step::Effects;
            while(!(job.partMask&(1u<<job.part)))++job.part;
            startSceneEffects();
            return;
        case SceneReload::Step::Effects: {
            do { ++job.part; } while(job.part<4 && !(job.partMask&(1u<<job.part)));
            if(job.part<4) {
                startSceneEffects();
                return;
            }
            if(job.programsOnly) {
                confirmScene(job,job.partMask);
                return;
            }
            KSRefreshPacket routingPacket;
            const auto oldOnly=uint8_t(seed.activeMask & ~uint8_t(job.layout==KSLayout::Performance?15:1));
            for(unsigned i=0;i<4;++i)if(oldOnly&(1u<<i))
                require(routingPacket.appendWord(Endpoint::A,KSCommandFeedback::partStatusAddress(KSPart(i)),0),"KS scene old mailbox ack");
            require(appendKsAuxiliary(routingPacket,job.patches.front()[225]) &&
                    routingPacket.appendWord(Endpoint::A,0x8056,job.activeMask),"KS scene routing packet");
            job.step=SceneReload::Step::Route;
            startPacket(RefreshStage::Scene,std::move(routingPacket));
            return;
        }
        case SceneReload::Step::Route:
            confirmScene(job,job.layout==KSLayout::Performance?15:uint8_t(seed.activeMask | 1));
            return;
        case SceneReload::Step::Rebuild:
            confirmScene(job,job.mailboxMask);
            return;
        case SceneReload::Step::Confirm:
            throw std::logic_error("KS scene confirmation received a transfer");
        }
    }

    void observeScene(KSPartFeedbackResult receipt) {
        auto& job=*sceneReload;
        const auto part=unsigned(feedbackAddress-KSCommandFeedback::partStatusAddress(KSPart::One));
        const bool fading=job.step==SceneReload::Step::Fade;
        if(!fading && receipt.action==KSPartFeedbackAction::AcknowledgeAndRebuild) {
            // The route update can produce another fade receipt while the
            // original FX writes are being applied. It is a new DSP request.
            event("scene_confirmation_rebuild",part);
            job.step=SceneReload::Step::Rebuild;
            job.part=part;
            if(job.partMask&(1u<<part))startSceneEffects();
            else {
                KSRefreshPacket ack;
                require(ack.appendWord(Endpoint::A,feedbackAddress,0),"KS retiring scene mailbox acknowledgement");
                startPacket(RefreshStage::Scene,std::move(ack));
            }
            return;
        }
        const auto expected=fading?3:0;
        if(receipt.status==expected) {
            job.mailboxMask=uint8_t(job.mailboxMask & ~(1u<<part));
            if(job.programsOnly)event(fading?"part_reload_faded":"part_reload_confirmed",part);
            else event(fading?"scene_part_faded":"scene_part_confirmed",part);
        }
        if(job.mailboxMask) {
            job.nextPoll={native().ticks+4};
        } else if(fading) {
            stopSceneVoices();
        } else {
            const bool multi=job.layout==KSLayout::Performance;
            if (desktopSoundLoad && desktopSoundLoad->extras) {
                uint8_t slots=1;
                if(multi) {
                    if(const auto* preset=std::get_if<KSDesktopPreset>(&desktopSoundLoad->sound))
                        slots=preset->wholePerformance?uint8_t(30):uint8_t(2u<<preset->part);
                    else slots=uint8_t(2u<<std::get<KSDesktopDrumMap>(desktopSoundLoad->sound).part);
                }
                commitDesktopPatchDrums(slots);
            }
            if(job.programsOnly)commitPartReload(job);
            else commitScene(job);
            sceneReload.reset();
            finishDesktopSoundLoad(KSDesktopLoadStatus::Applied);
        }
    }

    void retireSceneVoices(const SceneReload& job) {
        for(size_t i=0;i<voices.size();++i) {
            const auto voice=state.voice(VoiceSlot(i));
            if(voice && job.ownsVoice(voice->part)) {
                require(drumBindings.retire(voice->allocation) && state.retireVoice(voice->allocation),"KS scene voice retirement");
                if(seed.tracked==VoiceSlot(i))seed.tracked.reset();
                voices[i]={};
            }
        }
        if(!job.programsOnly) {
            voices.fill({});
            seed.tracked.reset();
        }
    }

    KSPartConfigurationValues installScenePart(KSStorageSlot part,ProgramSelection selection,const KSVoiceImagePatch& patch) {
        const auto p=size_t(part);
        seed.patches[p]=patch;
        require(state.loadProgram(part,selection,{patch[46],patch[150]},KSProgramRefresh::OwnedReload) &&
                state.clearKeys(part) && state.setSustainDown(part,false) &&
                state.setSustainBypass(part,bool(patch[88]&4)) &&
                globals.loadPart(part,KSGlobalPart::fromPatch(patch),patch[225]),"KS scene part ownership");
        const auto dependency=state.storedDependency(part,session.rateGeneration());
        require(dependency && parameters.load(*dependency,patch),"KS scene parameter image");
        parameterSources[p]=*dependency;
        metadata[p]=KSParameterMetadata::fromPatch(patch);
        const auto controls=KSPartConfigurationValues::fromPatch(patch);
        require(configuration.activate(part,controls,patch[209],++contextRevisions[p]),"KS scene part configuration");
        seed.partContexts[p].wheelFine=controls.detune;
        require(expression.part(part)?expression.setPolicy(part,patch[177],true):
                expression.prepare(part,{},patch[177],true),"KS scene expression");
        seed.lastPlayed[p]=0;
        seed.restartMono[p]=true;
        return controls;
    }

    void activateSceneArp(KSStorageSlot part,NativeFrame at) {
        if(!musical || !state.keys(part))return;
        const auto index=size_t(part);
        sceneArpsAt[index]=at;
        musical->phaseKeys[index].programApplied(syncMode());
        if(seed.partChannels[index]<16)configureArp(part,at);
    }

    void commitPartReload(const SceneReload& job) {
        const auto at=native();
        retireSceneVoices(job);
        std::bitset<5> refreshed;
        for(size_t i=0;i<job.patches.size();++i)if(job.partMask&(1u<<i)) {
            const auto part=KSStorageSlot(i+1);
            const auto& patch=job.patches[i];
            installScenePart(part,{patch[187],patch[188]},patch);
            refreshed.set(i+1);
        }
        require(globals.acknowledgeScene(refreshed,job.master.performanceEffectStatus,job.partMask) &&
                state.publish(at),"KS part-program observed publication");
        lastSceneCommit=at;
        for(size_t i=0;i<job.patches.size();++i)if(job.partMask&(1u<<i)) {
            activateSceneArp(KSStorageSlot(i+1),at);
            event("part_program_published",uint32_t(i+1)<<16 | uint32_t(job.patches[i][187])<<8 | job.patches[i][188]);
        }
    }

    void commitScene(const SceneReload& job) {
        const auto at=native();
        const bool layoutChanged=seed.layout!=job.layout;
        retireSceneVoices(job);
        for(size_t p=0;p<seed.patches.size();++p)if(state.keys(KSStorageSlot(p)))
            require(state.clearKeys(KSStorageSlot(p)) && state.setSustainDown(KSStorageSlot(p),false),"KS scene old keys");
        state.takeDirtySlots();
        require(state.setLayout(job.layout) && globals.changeLayout(job.layout),"KS scene layout publication");
        seed.layout=job.layout;
        const bool multi=seed.layout==KSLayout::Performance;
        seed.singleSelection=job.single;
        seed.performanceSelection=job.performance;
        seed.activeMask=job.activeMask;
        seed.activePart=seed.trackingPart=multi?KSStorageSlot::Multi1:KSStorageSlot::Single;
        seed.editorSelectionMask=multi?uint8_t(1):uint8_t(0);
        reconcileEditor();
        seed.partChannels.fill(KSCommandCutoff::NoChannel);
        KSRoutingConfig routes{seed.layout,seed.activeMask,{}};
        routes.parts[0].channel=seed.channel;
        std::bitset<5> refreshed;
        for(size_t i=0;i<(multi?4u:1u);++i) {
            const auto p=i+multi;
            const auto part=KSStorageSlot(p);
            const auto& patch=job.patches[i];
            const auto selection=multi?ProgramSelection{patch[187],patch[188]}:job.single;
            const auto controls=installScenePart(part,selection,patch);
            seed.partChannels[p]=multi?uint8_t(std::min<unsigned>(controls.channel,16)):seed.channel;
            routes.parts[p]={seed.partChannels[p],controls.lowKey,controls.highKey};
            refreshed.set(p);
        }
        if(multi && layoutChanged) {
            const auto& stored=seed.patches[0];
            require(state.loadProgram(KSStorageSlot::Single,job.single,{stored[46],stored[150]},KSProgramRefresh::OwnedReload),
                    "KS stored Single layout barrier");
            const auto dependency=state.storedDependency(KSStorageSlot::Single,session.rateGeneration());
            require(dependency && parameters.load(*dependency,stored),"KS stored Single after scene change");
            parameterSources[0]=*dependency;
        }
        require(routing.configure(routes),"KS scene MIDI routing");
        routeBatch.revision=routing.revision();
        seed.globalMaster=job.master;
        seed.auxiliary=job.patches.front()[225];
        KSGlobalEditSet edit;
        edit.master=job.master;
        edit.auxiliary=seed.auxiliary;
        const auto result=globals.apply(edit);
        require(result!=KSGlobalEdit::Invalid && result!=KSGlobalEdit::Exhausted &&
                globals.acknowledgeAuxiliary(globals.auxiliaryRevision(),seed.auxiliary) &&
                globals.acknowledgeScene(refreshed,multi?job.master.performanceEffectStatus:0) &&
                state.publish(at),"KS scene observed publication");
        auxDirty=false;
        for(size_t p=0;p<seed.patches.size();++p)activateSceneArp(KSStorageSlot(p),at);
        lastSceneCommit=at;
        event("scene_published",uint32_t(multi)<<8|(multi?job.performance:job.single.program));
    }

    ks::State currentSound() const {
        ks::State snapshot;
        snapshot.performance=seed.layout==KSLayout::Performance;
        snapshot.globalChannel=system.channel();
        snapshot.activeMask=seed.activeMask;
        const auto count=snapshot.performance?4u:1u;
        for(size_t i=0;i<count;++i) {
            const auto part=KSStorageSlot(i+snapshot.performance);
            const auto patch=projectKsOwnedPatch(parameters,globals,state,part,metadata[size_t(part)],system,
                                                session.rateGeneration(),true);
            require(bool(patch),"KS current program projection");
            snapshot.parts[i]=*patch;
            const auto source=state.part(part);
            if(source && source->program && source->program->bank>=5 && source->program->bank<=8) {
                snapshot.parts[i][187]=source->program->bank;
                snapshot.parts[i][188]=source->program->program;
            }
            // Publish and save the MIDI channel currently used by the desktop.
            const auto channel=routing.config().parts[size_t(part)].channel;
            if(snapshot.performance && seed.partChannelRoutingImmediate && channel<KSPartRoute::NoChannel)
                snapshot.parts[i][181]=channel;
        }
        if(snapshot.performance)snapshot.parts[0][190]=seed.activeMask;
        return snapshot;
    }

    std::vector<uint8_t> currentProgramDump() const {
        auto snapshot=currentSound();
        for (auto& patch : snapshot.parts)
            ks::packMirrors(patch);
        return ks::stateMessage(snapshot);
    }

    std::optional<KSMusicalState::Part> desktopDrumPart(KSStorageSlot slot) const noexcept {
        if(!programs || (slot==KSStorageSlot::Single)!=(seed.layout==KSLayout::Single))
            return {};
        const auto part=state.part(slot);
        if(!part || !part->program || part->program->bank<5 || part->program->bank>8 || part->program->program>=49)
            return {};
        return part;
    }

    void editDesktopDrumVoice(KSDrumVoiceEdit edit) noexcept {
        const auto part=desktopDrumPart(KSStorageSlot(edit.slot));
        if(!part || part->revisions.program!=edit.programRevision || part->program->bank!=edit.bank)
            return;
        auto& bank=drumStore.banks[edit.bank-5];
        auto& patch=seed.drumRecords[edit.bank-5][edit.program];
        const auto value=uint8_t((patch[edit.field]&~edit.mask)|edit.bits);
        if(value==patch[edit.field] || bank.revision==UINT64_MAX ||
           !programs->editDrumVoice({edit.bank,edit.program},edit.field,value))
            return;
        patch[edit.field]=value;
        expandKsParameterFields(patch);
        ++bank.revision;
    }

    bool desktopDrumControl(const EventPayload& input) noexcept {
        if(const auto* mode=std::get_if<KSDrumEditMode>(&input)) {
            desktopDrumSlot=mode->slot;
            return true;
        }
        if(const auto* edit=std::get_if<KSDrumVoiceEdit>(&input)) {
            editDesktopDrumVoice(*edit);
            return true;
        }
        return false;
    }

    NativeEditorState editorState() const {
        require(!stopped,"KS editor state belongs to a stopped owner");
        const auto view=state.snapshot();
        require(bool(view),"KS editor projection is unavailable");
        const auto sound=currentSound();
        NativeEditorState panel;
        panel.session=owner;
        panel.frame=view->frame;
        panel.revision=view->revision;
        panel.rateGeneration=session.rateGeneration();
        panel.model=Model::KS;
        panel.performance=sound.performance;
        panel.loadingProgram=singleReload.has_value() || sceneReload.has_value();
        panel.performanceProgram=seed.performanceSelection;
        panel.globalChannel=sound.globalChannel;
        panel.activeMask=sound.activeMask;
        const auto selected=size_t(editor.projection()->storage);
        panel.selectedPart=sound.performance ? (selected ? uint8_t(selected-1) : RoutedByChannel) : 0;
        if (sound.performance)
            panel.performanceName=editorProgramName(sound.parts[0].data()+192);
        for (size_t i=0;i<(sound.performance?4u:1u);++i) {
            const auto storage=i+sound.performance;
            const auto& source=view->state.slots[storage];
            auto& part=panel.parts[i];
            part.program=source.program.value_or(ProgramSelection{});
            part.revisions=source.revisions;
            part.channel=routing.config().parts[storage].channel;
            part.cutoff=source.parameters.cutoff;
            part.volume=sound.parts[i][179];
            part.name=editorProgramName(sound.parts[i].data()+151);
            part.parameters=sound.parts[i];
            part.parameterBytes=uint16_t(part.parameters.size());
            if(musical && (sound.activeMask & (1u << i)))
                panel.drumPlayheads[i]=musical->drums[storage].playhead();
        }
        if(desktopDrumSlot!=KSDrumEditMode::Off) {
            if(const auto part=desktopDrumPart(KSStorageSlot(desktopDrumSlot))) {
                const auto& selected=desktopDrumKeys[desktopDrumSlot];
                auto& target=panel.drumEditor;
                target.slot=desktopDrumSlot;
                target.program=selected.revision==part->revisions.program?
                    selected.program:std::min(part->program->program,uint8_t(48));
                const auto& patch=seed.drumRecords[part->program->bank-5][target.program];
                std::copy_n(patch.begin(),target.voice.size(),target.voice.begin());
            }
        }
        return panel;
    }

#ifndef SILVER_KS_COMMAND_HOST
    void exerciseLifecycle(p09::Exercise& exercise) {
        lifecycleExercise=&exercise;
        exercise.checkPrevious(session,owner,native());
        if (exercise.previous && exercise.previous->voice)
            require(!state.voice(*exercise.previous->voice),"KS accepted a voice from a retired owner");
    }
#endif

    DspStopReport shutdown(
#ifndef SILVER_KS_COMMAND_HOST
        p09::Exercise* exercise
#endif
    ) {
        if (stopped)
            return {DspDrain::Idle,true};
        auto budget = PhysicalDspPair::ControllerClockHz / 50;
#ifndef SILVER_KS_COMMAND_HOST
        if (exercise) {
            if (exercise->exhaustDrain)
                budget = 0;
            if (exercise->fillIngress) {
                const auto remaining=CommandScheduler::Capacity-ingress.size();
                std::array<uint8_t,CommandScheduler::Capacity+1> burst;
                burst.fill(0xf8);
                exercise->ingressBackpressure=enqueueMidiBytes(std::max(bus().ticks,lastInputCycle),
                    burst.data(),remaining+1)==remaining && ingress.size()==CommandScheduler::Capacity;
            }
            exercise->retired.session=owner;
            exercise->retired.hostBlock=session.hostBlock(owner,{},64);
            if (exercise->interruptedTransfer)
                exercise->retired.ticket=exercise->interruptedTransfer->ticket;
            else if (transport)
                exercise->retired.ticket=transport->activeTicket();
            exercise->retired.voice=exercise->interruptedVoice;
            for (const auto& voice : voices)
                if (!exercise->retired.voice && voice.allocation)
                    exercise->retired.voice=voice.allocation;
            exercise->queuedAtStop=ingress.size()+session.snapshot().queued+bool(routedInput);
        } else
#endif
        {
            portA.recordEnabled=portB.recordEnabled=false;
        }
        const auto rxPending=[&] {
            return pair.dsp(0).peripherals().getHDI08().hasRXData() || pair.dsp(1).peripherals().getHDI08().hasRXData();
        };
        const auto report=stopDspSession(session,owner,transport,bus(),budget,
            [&](BusCycle at) { pair.advanceTo(at.ticks); pair.discardAudio(); }, [&] { return native(); }, rxPending);
        stopped=true;
        transport=nullptr;
        while (ingress.size())
            ingress.pop_front();
        routedInput.reset();
        eligibleCount=0;
        usedSysEx.fill(false);
        midiDecoder.reset();
        detachObservers();
        pair.resetForStop();
#ifndef SILVER_KS_COMMAND_HOST
        if (exercise) {
            constexpr uint8_t note[]{0x90,99,127};
            exercise->stop=report;
            exercise->stoppedRejected=enqueueMidiBytes(bus().ticks,note,sizeof(note))==0 &&
                session.submit(owner,native(),{NoteOn{0,99,127}}).status==AdmissionStatus::Stopped &&
                (!exercise->retired.ticket ||
                 session.receiveReply(*exercise->retired.ticket,native(),0x123456).status==AdmissionStatus::Stopped);
            exercise->endpointReset=!pair.startedBoth() && !pair.serialRunning() && !pair.audioDepth() && !rxPending() &&
                !pair.dsp(0).bootFinished() && !pair.dsp(1).bootFinished() &&
                !pair.dsp(0).peripherals().getHDI08().hasTX() && !pair.dsp(1).peripherals().getHDI08().hasTX();
        }
#endif
        return report;
    }

    bool checkpointReady() const noexcept {
        if (!programs || desktopSoundLoad || routedInput || clock.projection().pending ||
            (musical && musical->hostPositions.size()))
            return false;
        for (size_t i = 0; i < ingress.size(); ++i)
            if (ingress[i].event.part != RoutedByChannel ||
                std::holds_alternative<KSDesktopSoundCommand>(ingress[i].event.payload) ||
                std::holds_alternative<KSDrumVoiceEdit>(ingress[i].event.payload))
                return false;
        return true;
    }

    std::vector<uint8_t> checkpoint() const {
        require(checkpointReady(),"KS checkpoint outside an owned control boundary");
        KSControllerCheckpoint saved;
        const auto now=bus().ticks;
        saved.nextByteDelay=nextUart>now?nextUart-now:0;
        saved.decoder=midiDecoder.checkpoint();
        saved.banks=midiBanks;
        for (uint8_t channel=0;channel<16;++channel)
            saved.desktopSelectors[channel]=*desktopSelection.selector(channel);
        for (size_t i=0;i<saved.partSelectors.size();++i)
            saved.partSelectors[i]=*parameterIngress.selector(KSStorageSlot(i));
        saved.globalSelector=globalFallback.selector();
        saved.editorMask=seed.editorSelectionMask;
        saved.editorPart=uint8_t(editor.projection()->storage);
        if (seed.layout==KSLayout::Performance) {
            for (size_t i=0;i<saved.performanceChannels.size();++i)
                saved.performanceChannels[i]=routing.config().parts[i+1].channel;
            saved.storedSingle=seed.patches[0];
            (*saved.storedSingle)[84]=system.tempo;
            std::copy(system.settings.begin(),system.settings.end(),saved.storedSingle->begin()+233);
        }
        if (sceneReload) {
            const auto& scene=*sceneReload;
            saved.reload=KSReloadIntent{scene.programsOnly?KSReloadKind::Parts:KSReloadKind::Scene,
                scene.layout,scene.single,scene.performance,scene.activeMask,scene.partMask,scene.patches,scene.master};
        } else if (singleReload) {
            const auto& single=*singleReload;
            saved.reload=KSReloadIntent{KSReloadKind::Single,KSLayout::Single,single.selection,seed.performanceSelection,1,1,
                {single.patch},KSGlobalMaster::fromPatch(single.patch,0)};
        }
        for (size_t i=0;i<ingress.size();++i) {
            const auto& input=ingress[i];
            // The grid is saved by Silver-K, independently of the serial
            // MIDI checkpoint.
            if (std::holds_alternative<KSDrumGridEdit>(input.event.payload) ||
                std::holds_alternative<KSDrumEditMode>(input.event.payload))
                continue;
            require(input.event.part==RoutedByChannel,"KS checkpoint encountered a routed input");
            saved.midi.push_back({input.eligible>now?input.eligible-now:0,encodeKSQueuedMidi(input.event.payload)});
        }
        const auto defaultSelector=[](uint8_t b) { return b==255; };
        if (!saved.reload && !saved.storedSingle && saved.midi.empty() && !saved.nextByteDelay &&
            !saved.decoder.runningStatus && !saved.decoder.pendingStatus && !saved.decoder.discardingSysEx &&
            saved.decoder.sysExPrefix.empty() && !saved.editorMask && saved.globalSelector==255 &&
            std::all_of(saved.banks.begin(),saved.banks.end(),defaultSelector) &&
            std::all_of(saved.partSelectors.begin(),saved.partSelectors.end(),defaultSelector) &&
            std::none_of(saved.desktopSelectors.begin(),saved.desktopSelectors.end(),[](uint8_t b) { return b!=0; }))
            return {};
        auto encoded=encodeKSControllerCheckpoint(saved);
        require(!encoded.empty(),"Invalid native KS checkpoint");
        return encoded;
    }

    void restoreCheckpoint(const KSControllerCheckpoint& saved) {
        const auto now=bus().ticks;
        require(programs && !queuedInputEvents && !routedInput && !sceneReload && !singleReload &&
                stage==RefreshStage::None && !feedback,
                "KS checkpoint requires a fresh control owner");
        require(saved.nextByteDelay<=std::numeric_limits<uint64_t>::max()-now && ksCheckpointDetail::valid(saved),
                "Invalid saved KS checkpoint");
#ifdef SILVER_P12_ACTION_TRACE
        event("checkpoint_epoch", saved.nextByteDelay);
#endif
        if (saved.reload)restoreReload(*saved.reload);
        require(globalFallback.restoreColdSelector(saved.globalSelector),"Invalid saved KS global selector");
        require(midiDecoder.restore(saved.decoder),"Invalid saved KS MIDI decoder state");
        midiBanks=saved.banks;
        nextUart=now+saved.nextByteDelay;
        lastInputCycle=now;
        for (const auto& input : saved.midi) {
            const auto payload=decodeKSQueuedMidi(input.message);
            require(payload && input.delay<=saved.nextByteDelay && admitMidi(now+input.delay,*payload),
                    "Invalid saved KS MIDI queue");
        }
        event("checkpoint_queue_restored",saved.midi.size());
    }

    void restoreReload(const KSReloadIntent& intent) {
        if (intent.kind==KSReloadKind::Single) {
            require(seed.layout==KSLayout::Single &&
                    beginSingleReload(intent.single,intent.patches[0],native(),false),"Invalid saved KS Single reload");
            return;
        }
        const bool parts=intent.kind==KSReloadKind::Parts;
        if (parts)
            require(seed.layout==KSLayout::Performance && intent.performance==seed.performanceSelection &&
                    intent.activeMask==seed.activeMask && intent.single.bank==seed.singleSelection.bank &&
                    intent.single.program==seed.singleSelection.program,"Saved KS part reload has the wrong Performance");
        SceneReload scene{intent.layout,intent.patches,intent.single,intent.performance,intent.activeMask,
                          parts?intent.partMask:seed.activeMask,intent.master,native(),native()};
        scene.partMask=intent.partMask;
        scene.programsOnly=parts;
        scene.master.previousEffectStatus=globals.observedEffectStatus();
        for (size_t i=0;i<scene.patches.size();++i)if (scene.partMask&(1u<<i))
            require(canLoadScenePart(scene.layout,i,scene.patches[i],scene.master),"Invalid saved KS program parameters");
        startScene(std::move(scene),native());
    }

    void beginPartPrograms(KSRoutedParts routes,uint8_t bank,uint8_t program,NativeFrame at) {
        if(!routes.count && routes.global==KSGlobalRoute::Program) {
            const auto selected=editor.projection();
            if(selected && selected->storage!=KSStorageSlot::Single &&
                (seed.activeMask&(1u<<(size_t(selected->storage)-1))))
                routes.parts[routes.count++]=selected->storage;
        }
        SceneReload next{KSLayout::Performance,{},seed.singleSelection,seed.performanceSelection,seed.activeMask,
                         0,globals.desiredMaster(),native(),native()};
        next.programsOnly=true;
        next.master.previousEffectStatus=globals.observedEffectStatus();
        for(size_t i=0;i<routes.count;++i) {
            const auto part=routes.parts[i];
            const auto index=size_t(part);
            const auto current=state.part(part);
            require(current && current->program,"KS Performance part is missing its program");
            const ProgramSelection selection{bank==255?current->program->bank:bank,program};
            const auto address=uint32_t(index)<<16 | uint32_t(selection.bank)<<8 | selection.program;
            const auto record=programs->single(selection);
            if(!record) {
                event("part_program_missing",address);
                continue;
            }
            const auto& previous=seed.patches[index];
            auto patch=performancePatch(previous,*record);
            // The catalog supplies the sound. Mixer, MIDI, keyboard ranges,
            // and Performance metadata belong to the existing Part.
            std::copy(previous.begin()+179,previous.begin()+209,patch.begin()+179);
            patch[187]=selection.bank;
            patch[188]=selection.program;
            if(!canLoadScenePart(next.layout,index-1,patch,next.master)) {
                event("part_program_unsupported",address);
                continue;
            }
            next.patches[index-1]=patch;
            next.partMask|=uint8_t(1u<<(index-1));
        }
        if(next.partMask) {
            next.mailboxMask=next.partMask;
            startScene(std::move(next),at);
        }
    }

    void publishDesktopLoad(KSDesktopSoundLoad& request,KSDesktopLoadStatus status) noexcept {
        if(status==KSDesktopLoadStatus::Applied && std::holds_alternative<KSDesktopPartSwitch>(request.sound))
            request.appliedRevision=state.snapshot()->revision;
        if(request.extras)request.extras->status.store(status,std::memory_order_release);
        request.status.store(status,std::memory_order_release);
    }

    void commitDesktopPatchDrums(uint8_t slots) noexcept {
        if(!desktopSoundLoad || !desktopSoundLoad->extras)return;
        auto& extras=*desktopSoundLoad->extras;
        const auto* preset=std::get_if<KSDesktopPreset>(&desktopSoundLoad->sound);
        const bool whole=preset && preset->wholePerformance;
        for(size_t slot=0;slot<seed.userDrums.size();++slot)if(slots&(1u<<slot)) {
            auto& bank=seed.userDrums[slot];
            bank=extras.sequences[whole?slot:0];
            if(musical)musical->drumPatterns[slot]=bank.selectedPattern;
        }
        extras.appliedSlots=slots;
    }

    void replaceDesktopDrumBank(uint8_t bank,const KSDrumBank& records) noexcept {
        programs->storeDrumBank(bank,records);
        auto& expanded=seed.drumRecords[bank-5];
        expanded=records;
        for(auto& item:expanded)expandKsParameterFields(item);
        ++drumStore.banks[bank-5].revision;
    }

    void finishDesktopSoundLoad(KSDesktopLoadStatus status) noexcept {
        if(auto* request=std::exchange(desktopSoundLoad,nullptr)) publishDesktopLoad(*request,status);
    }

    bool beginDesktopPartSwitch(KSDesktopPartSwitch edit,NativeFrame at) {
        if(!programs || seed.layout!=KSLayout::Performance || edit.part>=ks::PartCount) return false;
        const auto bit=uint8_t(1u<<edit.part);
        const auto mask=uint8_t(edit.enabled?seed.activeMask|bit:seed.activeMask&~bit);
        if(mask==seed.activeMask) return true;
        // Read each edit against the current scene at its place in the input queue.
        auto records=currentSound().parts;
        records[0][190]=mask;
        return beginScene(seed.layout,records,seed.singleSelection,seed.performanceSelection,at);
    }

    bool beginDesktopPreset(const KSDesktopPreset& request,const KSDesktopPatchExtras* extras,NativeFrame at) {
        if(!programs || request.part>=ks::PartCount)return false;
        if(request.wholePerformance) {
            uint8_t replace=0;
            if(extras)for(size_t i=0;i<extras->drumMaps.size();++i)if(const auto& map=extras->drumMaps[i]) {
                const auto bank=uint8_t(i+5);
                if(std::memcmp(programs->drumBankData(bank),map->data(),sizeof(KSDrumBank))==0)continue;
                if(drumStore.banks[i].revision==UINT64_MAX || !programs->canStoreDrumBank(bank,*map))return false;
                replace|=uint8_t(1u<<i);
            }
            if(!beginScene(KSLayout::Performance,request.records,seed.singleSelection,seed.performanceSelection,at))return false;
            if(extras)for(size_t i=0;i<extras->drumMaps.size();++i)if(replace&(1u<<i))
                replaceDesktopDrumBank(uint8_t(i+5),*extras->drumMaps[i]);
            return true;
        }

        const bool multi=seed.layout==KSLayout::Performance;
        if(!multi && request.part)return false;
        auto patch=currentSound().parts[request.part];
        std::copy_n(request.records.front().begin(),179,patch.begin());
        expandKsParameterFields(patch);
        auto selection=multi?ProgramSelection{patch[187],patch[188]}:seed.singleSelection;
        if(selection.bank>=5 && selection.bank<=8) {
            selection={1,0};
            patch[187]=selection.bank;
            patch[188]=selection.program;
        }
        if(!multi)return beginSingleReload(selection,patch,at,false);

        // Take the current part at admission so consecutive requests cannot
        // overwrite each other's programs or concurrent edits to other parts.
        const auto mask=uint8_t(1u<<request.part);
        SceneReload next{KSLayout::Performance,{},seed.singleSelection,seed.performanceSelection,
                         seed.activeMask,mask,globals.desiredMaster(),native(),native()};
        next.master.previousEffectStatus=globals.observedEffectStatus();
        if(!canLoadScenePart(next.layout,request.part,patch,next.master))return false;
        next.patches[request.part]=patch;
        next.partMask=mask;
        next.programsOnly=true;
        startScene(std::move(next),at);
        return true;
    }

    bool beginDesktopDrumLoad(const KSDesktopDrumMap& request,bool nativePatch,NativeFrame at) {
        auto selection=request.selection;
        const bool multi=seed.layout==KSLayout::Performance;
        if(!programs || (!nativePatch && multi!=request.performance) || request.part>=4 ||
           selection.bank<5 || selection.bank>8 || selection.program>=50)
            return false;
        if(nativePatch && multi && request.complete &&
           std::memcmp(programs->drumBankData(selection.bank),request.records.data(),sizeof(KSDrumBank))!=0) {
            // Loading a portable part must preserve the Drum Maps used by
            // its neighbours, even if the file was saved at the same address.
            std::bitset<4> occupied;
            for(size_t i=0;i<ks::PartCount;++i)if(i!=request.part) {
                const auto part=state.part(KSStorageSlot(i+1));
                if(part && part->program && part->program->bank>=5 && part->program->bank<=8)
                    occupied.set(part->program->bank-5);
            }
            if(occupied.test(selection.bank-5))
                for(uint8_t i=0;i<4;++i)if(!occupied.test(i)) {
                    selection.bank=uint8_t(i+5);
                    break;
                }
        }
        const auto* current=programs->drumBankData(selection.bank);
        const auto& record=request.records[request.complete?selection.program:0];
        if(!request.complete && std::memcmp(current+size_t(selection.program)*record.size(),
                                             record.data(),record.size())!=0)
            return false;
        const bool replace=request.complete && std::memcmp(current,request.records.data(),sizeof(KSDrumBank))!=0;
        auto& bank=drumStore.banks[selection.bank-5];
        if(replace && (bank.revision==UINT64_MAX || !programs->canStoreDrumBank(selection.bank,request.records)))
            return false;
        if(multi) {
            auto records=currentSound().parts;
            auto& target=records[request.part];
            std::copy_n(record.begin(),179,target.begin());
            if(request.complete) {
                const auto& effects=request.records[49];
                std::copy_n(effects.begin()+84,41,target.begin()+84);
                target[126]=effects[126];
            }
            target[187]=selection.bank;
            target[188]=selection.program;
            if(!beginScene(seed.layout,records,seed.singleSelection,seed.performanceSelection,at))
                return false;
        } else if(!beginSingleReload(selection,performancePatch(seed.patches[0],record),at,true)) {
            return false;
        }
        if(replace)replaceDesktopDrumBank(selection.bank,request.records);
        return true;
    }

    bool handleCatalogMidi(const ControllerEvent& input, NativeFrame at) {
        const auto* control=std::get_if<ControlChange>(&input.payload);
        const auto* program=std::get_if<ProgramChange>(&input.payload);
        if(!program && (!control || control->controller!=32))return false;
        const auto channel=program?program->channel:control->channel;
        const auto routes=routing.route(owner,input);
        require(bool(routes),"Invalid KS catalog MIDI route");
        if(!routes->count && routes->global==KSGlobalRoute::None)return true;
        auto& latch=midiBanks[channel];
        const bool multi=seed.layout==KSLayout::Performance;
        if(control) {
            latch=control->value;
            event("bank_latched",latch);
            return true;
        }
        const auto selected=program->program;
        const auto bank=std::exchange(latch,uint8_t(255));
        if(bank==65 || (multi && bank==255 && routes->global==KSGlobalRoute::Program)) {
            const auto performance=programs->performance(selected);
            if(!performance)event("performance_missing",selected);
            else if(!beginScene(KSLayout::Performance,*performance,seed.singleSelection,selected,at))
                event("performance_unsupported",selected);
            return true;
        }
        if(multi && !(bank>=33 && bank<=40)) {
            beginPartPrograms(*routes,bank,selected,at);
            return true;
        }
        const auto singleBank=bank==255?seed.singleSelection.bank:uint8_t(bank>=33 && bank<=40?bank-32:bank);
        const ProgramSelection selection{singleBank,selected};
        const auto record=programs->single(selection);
        const auto address=uint32_t(singleBank)*256+selected;
        if(!record)event("program_missing",address);
        else if(multi) {
            KSPerformanceRecord records{};
            records.front()=*record;
            if(!beginScene(KSLayout::Single,records,selection,seed.performanceSelection,at))
                event("program_unsupported",address);
        } else if(!beginSingleReload(selection,performancePatch(seed.patches[0],*record),at,true))
            event("program_unsupported",address);
        return true;
    }

    void applySysEx(SysExBytes bytes, NativeFrame requested) {
        const auto frame=NovationProgramSysEx::decode(Model::KS,seed.channel,bytes);
        if(!frame) {
            if(NovationProgramSysEx::identityRequest(seed.channel,bytes))event("midi_identity_unavailable",0);
            else if(NovationProgramSysEx::addressed(Model::KS,seed.channel,bytes) &&
                    !NovationProgramSysEx::knownType(Model::KS,bytes.data[7]))
                event(NovationProgramSysEx::firmwareType(bytes.data[7])?"midi_firmware_update_unsupported":"midi_sysex_unsupported",
                      bytes.data[7]);
            else event("midi_sysex_rejected",bytes.size);
            return;
        }
        requested=std::max(requested,lastSceneCommit);
        if(frame->type==0 && seed.layout==KSLayout::Single) {
            auto patch=seed.patches[0];
            std::copy_n(frame->data,179,patch.begin());
            expandKsParameterFields(patch);
            if(!beginSingleReload(seed.singleSelection,patch,requested,false))event("midi_sysex_rejected",frame->type);
            return;
        }
        // The complete dump can install all four parts and enter Performance
        // in one scene, without recalling a stored Performance as a bridge.
        if(frame->type==2) {
            KSPerformanceRecord records;
            for(size_t i=0;i<records.size();++i)
                std::copy_n(frame->data+i*records[i].size(),records[i].size(),records[i].begin());
            if(!beginScene(KSLayout::Performance,records,seed.singleSelection,seed.performanceSelection,requested))
                event("midi_sysex_rejected",frame->type);
            return;
        }
        const auto bank=frame->control?frame->bank:seed.singleSelection.bank;
        if(programs && (frame->type==1 || frame->type==3 || frame->type==4)) {
            const auto stored=frame->type==1 ? storeSysExSingle({bank,frame->program},frame->data) :
                              frame->type==3 ? programs->storePerformance(frame->program,frame->data) :
                                               storeSysExGlobals(frame->data,requested);
            event(stored?"midi_sysex_stored":"midi_sysex_write_rejected",frame->type);
            return;
        }
        std::vector<uint8_t> reply;
        if((frame->type==0x40 && seed.layout==KSLayout::Single) ||
           (frame->type==0x42 && seed.layout==KSLayout::Performance)) {
            reply=currentProgramDump();
        } else if(frame->type==0x41 && programs) {
            const auto patch=programs->single({bank,frame->program});
            if(!patch){event("midi_sysex_missing",frame->type);return;}
            reply=NovationProgramSysEx::encode(Model::KS,1,frame->control,bank,frame->program,patch->data(),patch->size());
        } else if(frame->type==0x43 && programs) {
            const auto performance=programs->performance(frame->program);
            if(!performance){event("midi_sysex_missing",frame->type);return;}
            std::array<uint8_t,1024> data;
            for(size_t i=0;i<performance->size();++i)
                std::copy((*performance)[i].begin(),(*performance)[i].end(),data.begin()+i*(*performance)[i].size());
            reply=NovationProgramSysEx::encode(Model::KS,3,frame->control,1,frame->program,data.data(),data.size());
        } else if(frame->type==0x44 && programs) {
            std::array<uint8_t,256> page;
            std::transform(programs->globalPage(),programs->globalPage()+page.size(),page.begin(),
                           [](uint8_t value){return uint8_t(value&127);});
            reply=NovationProgramSysEx::encode(Model::KS,4,0,0,0,page.data(),page.size());
        } else {
            event("midi_sysex_unsupported",frame->type);
            return;
        }
        if(sysExOut_)sysExOut_->write(reinterpret_cast<const char*>(reply.data()),std::streamsize(reply.size()));
        midiSink.emit(reply.data(), reply.size());
        event("midi_sysex_reply",reply[7]);
    }

    bool storeSysExSingle(ProgramSelection selection,const uint8_t* data) {
        if(selection.bank<=4)return programs->storeSingle(selection,data);
        if(selection.bank>8)return false;
        const auto index=selection.bank-5;
        auto& drum=drumStore.banks[index];
        if(drum.revision==std::numeric_limits<uint64_t>::max() || !programs->storeSingle(selection,data))return false;
        auto& patch=seed.drumRecords[index][selection.program];
        std::copy_n(data,patch.size(),patch.begin());
        expandKsParameterFields(patch);
        ++drum.revision;
        return true;
    }

    bool storeSysExGlobals(const uint8_t* data,NativeFrame at) {
        const auto channel=system.channel();
        const auto previousClock=system.clockSource();
        const auto previousFine=system.globalFine();
        if(data[8]>=16 || data[9]>2 || system.revision==std::numeric_limits<uint64_t>::max())return false;
        if(data[8]!=channel)preflightGlobalChannel(data[8],contextRevisions[0]);
        if(!programs->storeGlobals(data))return false;
        std::copy_n(data,21,system.settings.begin()+1);
        ++system.revision;
        if(system.channel()!=channel)updateGlobalChannel();
        seed.velocityPower=system.velocityPower();
        if(system.globalFine()!=previousFine) {
            for(size_t i=0;i<seed.partContexts.size();++i) {
                const auto part=KSStorageSlot(i);
                seed.partContexts[i].globalFine=system.globalFine();
                if(!state.keys(part))continue;
                require(state.touchParameters(part,false),"KS Global Dump pitch revision");
                rebaseParameters(part);
                ++contextRevisions[i];
            }
            for(auto& voice:seed.voices)voice.context.globalFine=system.globalFine();
            require(state.publish(native()),"KS Global Dump pitch publication");
        }
        if(musical && system.clockSource()!=previousClock)
            require(clock.parameter(owner,at,243,system.clockSource())==KSClockAdmission::Accepted,"KS Global Dump clock policy");
        return true;
    }

    void startNote(const NoteOn& note,bool publishKey=true) {
        notePart=dispatchPart;const auto p=size_t(notePart);
        require(note.channel == seed.partChannels[p] && note.velocity, "Unsupported native note route");
        require(!allocatingNote && !pendingNote && !noteStart.reserved(), "Native note already owns allocation");
        const auto source=selectKsNoteAssignment(KSNoteAssignmentKind::Select,*state.dependency(notePart,session.rateGeneration()),
            *state.part(notePart)->program,note.note,drumStore);
        if(source.action==KSNoteAssignmentAction::IgnoreNote) {
            require((!publishKey || state.pressKey(notePart,note.note,note.velocity)) && state.publish(native()),"Ignored drum note lost key state");
            event("drum_note_ignored",note.note);return;
        }
        require(source.playable(),"Missing native note program source");selectedNote=source;
        const auto& patch=source.record?*source.record:seed.patches[p];
        sequenceNote=note;noteLayer=ksUnisonExtraVoices(patch[0]);notePublishesKey=publishKey;
        allocatingNote = note;
    }

    void allocateNote(const NoteOn& note) {
        const auto p=size_t(notePart);
        const auto shaped = ksNoteVelocity(note.velocity,configuration.read(notePart)->values.velocityCurve,seed.velocityPower,seed.velocityTables);
        require(bool(shaped), "Unsupported P05 velocity curve");
        KSVoiceSelectionInput input;
        input.mode = ksVoiceMode(selectedNote->record?(*selectedNote->record)[0]:seed.patches[p][0]);
        input.newMonoVoice=seed.restartMono[p];
        require(state.part(notePart)->revisions.program<=std::numeric_limits<uint64_t>::max()/8,"Voice program group exhausted");
        input.owner = {notePart, state.part(notePart)->revisions.program*8+noteLayer};
        input.note = note.note;
        for (size_t i = 0; i < voices.size(); ++i) {
            const auto owned = state.voice(VoiceSlot(i));
            input.voices[i] = {{owned ? owned->part : KSStorageSlot::Single, voices[i].generation},
                              owned ? owned->note : uint8_t(0), voices[i].feedback.age, voices[i].feedback.countdown};
        }
        const auto selected = selectKsVoice(input);
        require(bool(selected),"Invalid native voice allocation policy");
        selectedGroup=input.owner.group;
        const auto index = size_t(selected->slot);
        event("note_velocity_resolved",uint32_t(p)<<24 | uint32_t(configuration.read(notePart)->values.velocityCurve)<<16 |
            uint32_t(note.velocity)<<8 | uint32_t(shaped->suppressed?128:shaped->value));
        event("voice_selected", uint32_t(index) | (uint32_t(selected->stopBeforeReuse) << 8));
        require(noteStart.prepare(session,state,notePart,VoiceSlot(index),note.note,note.velocity,shaped->suppressed?128:shaped->value,
                                  selected->stopBeforeReuse,selected->allocation,notePublishesKey,
                                  selectedNote->record?(*selectedNote->record)[173]:seed.patches[p][173],shaped->suppressed), "Could not prepare native Note On intent");
        stage=RefreshStage::NoteAdmission;
        admitNoteStart();
    }

    void admitNoteStart() {
        holdNoteCapacity(false);
        const auto result=noteStart.admit(session,++operation,native());
        if (result==DspAdmission::Full) { event("note_start_full",noteStart.stopCount()+1);return; }
        require(result==DspAdmission::Accepted,"Native Note On admission failed");
        if (noteStart.stopCount()) {
            const auto packet=encodeKsVoiceStop(noteStart.slot());
            require(bool(packet),"Invalid native stop packet");recordPacket("voice-stop",*packet);
            stage=RefreshStage::VoiceStop;noteStopIndex=0;startPort(*noteStart.stopTicket(0));
        } else beginNotePitch();
    }

    void beginNotePitch() {
        const auto allocation=noteStart.allocation();
        const auto owned=allocation?state.voice(*allocation):std::nullopt;
        require(bool(owned),"Native Note On lost its committed allocation");
        const auto index=size_t(allocation->slot);
        voices[index] = {allocation, {0x8000, 0xffff}, selectedGroup, 0};
        if(musical) {
            if(musical->waitingGates.test(index)) {
                musical->waitingGates.reset(index);
                event("arp_gate_replaced",index);
            }
            auto& binding=musical->voices[index];
            binding.reset();
            if(musical->starting && !noteStart.suppressed())
                binding=MusicalWork::Binding{musical->starting->note.generation,*allocation,musical->starting->drum};
        }
        require(selectedNote && drumBindings.bind(state,*allocation,*selectedNote,drumStore),"Could not bind allocation program source");
        const auto p=size_t(owned->part);seed.restartMono[p]=false;seed.lastPlayed[p]=owned->note;notePublishesKey=false;
        require(expression.setPolicy(owned->part,seed.patches[p][177],false),"Could not clear startup expression suppression");
        const auto amplitudes=seed.voices[index].context.oscillatorAmplitude;
        seed.voices[index].context=seed.partContexts[p];seed.voices[index].context.oscillatorAmplitude=amplitudes;
        const auto* controls=expression.part(owned->part);require(controls,"Missing expression source");
        seed.voices[index].context.wheelPitch=controls->values.wheelPitch;seed.voices[index].context.modulation=controls->values.modulation;
        seed.voices[index].context.transposedNote = uint8_t(owned->note + configuration.read(owned->part)->values.transpose - 64);
        seed.voices[index].context.panelGate = false;
        event("note_on_published", (uint32_t(index) << 16) | (uint32_t(owned->note) << 8) | owned->velocity);
        stage=RefreshStage::None;
        pendingNote = index;
        feedback=*noteStart.pitchTicket();feedbackAddress=uint16_t(0x808c+22*index);
        event("feedback_requested",feedbackAddress);startPort(*feedback);
    }

    void completeNoteSequence() {
        stage=RefreshStage::None;
        if(noteLayer){--noteLayer;allocatingNote=sequenceNote;observeRefreshFeedback();}
        else {
            sequenceNote.reset();selectedNote.reset();
            if(musical)musical->starting.reset();
        }
    }

    void startNotePacket(size_t index) {
        const auto part=state.voice(*voices[index].allocation)->part;const auto p=size_t(part);
        const auto source=observeKsAllocatedVoiceSource(state,parameters,drumBindings,drumStore,*voices[index].allocation,
            seed.voiceTables.filter,session.rateGeneration());
        require(bool(source),"Could not observe allocation program source");
        KSParameterVoiceContext extended{seed.voices[index].context,uint8_t(voices[index].generation&7),source->patch[1],
                                         voices[index].pitchDistance,0};
        noteTracks=part==seed.activePart && !source->immutableRecord();
        if(source->immutableRecord())seed.tracked.reset();
        require(noteStart.prepareInitial(session,*source,seed.voiceTables,seed.voices[index].context,parameters,drumBindings,drumStore,&extended,noteTracks),
                "Unsupported native initial note image");
#ifdef SILVER_KS_NATIVE_COMMAND_ONLY
        // This timing applies to direct, fixed-phase starts in Single mode.
        const auto& patch = source->patch;
        paceNoteImage = part == KSStorageSlot::Single && noteStart.initialImage() && !source->immutableRecord() &&
            (!musical || !musical->starting) && ksUnisonExtraVoices(patch[0]) == 0 && patch[2] == 0 && patch[147] != 0;
#endif
        stage=RefreshStage::NoteInitialAdmission;admitNoteInitial();
    }

    void admitNoteInitial() {
        holdNoteCapacity(true);
        const auto result=noteStart.admitInitial(session,++operation,native());
        if (result==DspAdmission::Full) {event("note_initial_full",noteStart.initialCount());return;}
        require(result==DspAdmission::Accepted,"Native initial image admission failed");
        const auto index=size_t(noteStart.slot());
        KSRefreshPacket packet;
        require(noteStart.appendInitial(packet),"Could not record owned initial note");
        const auto* glide=noteStart.glide();require(glide,"Initial note lost glide result");
        if(glide->changedDistanceAndPrevious) {
            voices[index].pitchDistance=glide->distance;
            for(auto& voice:seed.voices)voice.context.previousNote=glide->previousNote;
                for(auto& context:seed.partContexts)context.previousNote=glide->previousNote;
        }
        if(noteTracks){seed.tracked=VoiceSlot(index);seed.trackingPart=notePart;}
        else if(seed.tracked==VoiceSlot(index))seed.tracked.reset();
        recordPacket("note",packet);stage=RefreshStage::Note;noteInitialIndex=0;
        startPort(*noteStart.initialTicket(0));
    }

    void holdNoteCapacity(bool initial) {
        if (!noteCapacity || noteCapacity->release || noteCapacity->initial!=initial || noteCapacity->active || noteCapacity->completed ||
            (noteCapacity->occupiedOnly && !noteStart.stopCount())) return;
        fillNoteCapacity(initial?"note_initial_capacity_held":"note_start_capacity_held",noteStart.slot());
    }

    void fillNoteCapacity(const char* kind,VoiceSlot slot) {
        require(session.transactions().available(Endpoint::B)==DspTransactionQueue::EndpointCapacity,
                "Capacity intervention requires an otherwise empty endpoint");
        const uint32_t placeholder=0;
        DspTransactionDraft draft;draft.model=Model::KS;draft.endpoint=Endpoint::B;draft.words=&placeholder;draft.wordCount=1;
        for(auto& ticket:noteCapacity->tickets)
            require(session.transactions().submit(owner,++operation,&draft,1,&ticket,[]() noexcept {})==DspAdmission::Accepted,
                    "Could not hold diagnostic queue records");
        noteCapacity->active=true;noteCapacity->releaseAt={native().ticks+noteCapacity->frames};
        noteCapacity->part=noteCapacity->release?dispatchPart:notePart;
        noteCapacity->keys=state.keys(noteCapacity->part)->revision();
        noteCapacity->generation=*state.voiceGeneration(slot);noteCapacity->slot=slot;
        noteCapacity->publication=state.snapshot()->revision;
        event(kind,noteCapacity->frames);
    }

    void releaseNoteCapacity() {
        if (!noteCapacity || !noteCapacity->active || native()<noteCapacity->releaseAt) return;
        require(state.keys(noteCapacity->part)->revision()==noteCapacity->keys &&
                *state.voiceGeneration(noteCapacity->slot)==noteCapacity->generation &&
                state.snapshot()->revision==noteCapacity->publication,"Full queue changed musical state");
        for(auto ticket:noteCapacity->tickets)
            require(session.transactions().cancel(ticket)==DspCancellation::Cancelled &&
                    session.transactions().retire(ticket),"Diagnostic queue record reached a physical port");
        noteCapacity->active=false;noteCapacity->completed=true;
        event("note_capacity_state_unchanged",1);event("note_capacity_released",noteCapacity->frames);
    }

    DispatchDecision dispatchRouted(const ScheduledEvent& original,NativeFrame at) {
        auto input=original;const auto p=size_t(dispatchPart);
        KSExpressionUpdate decoded;
        preflightParameterContexts(false,dispatchPart);
            const auto expressionResult=expression.apply(state,dispatchPart,input.event.payload,decoded);
            if(expressionResult==KSExpressionApply::Applied) {
                rebaseParameters(dispatchPart);++contextRevisions[p];require(state.publish(native()),"Expression source publication failed");
                const auto sources=observeKsAllocatedVoiceSources(state,parameters,drumBindings,drumStore,seed.voiceTables.filter,
                    session.rateGeneration(),dispatchPart);
                require(bool(sources),"Could not observe allocation expression sources");
                KSCommandVoiceUpdate::Contexts contexts{};
                for(size_t i=0;i<voices.size();++i)if((*sources)[i])
                    contexts[i]={seed.voices[i].context,uint8_t(voices[i].generation&7),(*sources)[i]->patch[1],voices[i].pitchDistance,0};
                const auto* source=expression.part(dispatchPart);
                auto packet=encodeKsAllocatedExpressionPacket(owner,seed.layout,dispatchPart,decoded.field,source->values,
                    source->refreshSuppressed,seed.voiceTables,*sources,contexts);
                require(packet && expressionUpdate.prepare(session,state,expression,dispatchPart,source->revision,*packet),
                        "Could not own complete expression update");
                event("expression_published",unsigned(decoded.field));recordPacket("expression",*packet);
                stage=RefreshStage::ExpressionAdmission;admitExpression();return {};
            }
            if(expressionResult==KSExpressionApply::Ignored)return {};
            require(expressionResult!=KSExpressionApply::Busy && expressionResult!=KSExpressionApply::Invalid &&
                    expressionResult!=KSExpressionApply::RevisionExhausted,"Invalid native expression publication");
            if(expressionResult==KSExpressionApply::ForwardControl)input.event.payload=decoded.forwarded;
        if(const auto* value=std::get_if<NoteOn>(&input.event.payload)) {
            const auto& bank=seed.userDrums[p];
            const auto editPart=desktopDrumSlot==p?desktopDrumPart(dispatchPart):std::nullopt;
            if(editPart) {
                if(value->note<KSDrumGridEdit::FirstNote || value->note>KSDrumGridEdit::LastNote)return {};
                desktopDrumKeys[p]={editPart->revisions.program,uint8_t(value->note-KSDrumGridEdit::FirstNote)};
            }
            const bool drum=!editPart && musical && musical->drums[p].enabled();
            const auto pattern=drum && bank.selected?bank.patternForKey(value->note):std::nullopt;
            if(drum && bank.selected && bank.assignedKeysOnly && !pattern)return {};
            if(musical && musical->phaseKeys[p].press(value->note,syncMode()))resetGlobalPhase(dispatchPart);
            if(drum)
                require(pressDrum(dispatchPart,original.requestedTime,value->note,pattern),"KS drum trigger deadline");
            else if(!editPart && musical && musical->parts[p].enabled())
                require(musical->parts[p].press(original.requestedTime,value->note,value->velocity),"KS arp held key deadline");
            else startNote(*value);
        }
        else if(const auto* value=std::get_if<ProgramChange>(&input.event.payload)) {
            if(seed.layout==KSLayout::Single && seed.bankLatch==33 && value->program==1 && seed.hasSingleProgramRecord) {
                if(musical) {
                    require(musical->parts[0].clear(original.requestedTime) &&
                            musical->drums[0].clear(original.requestedTime),"KS arp program barrier");
                    musical->voices.fill(std::nullopt);
                    musical->waitingGates.reset();
                    musical->reloadAt=original.requestedTime;
                }
                changeProgram(*value);
            }
            else {
                require(!programRequests[p] && programRequestRevision!=std::numeric_limits<uint64_t>::max(),"Unconsumed native program request");
                programRequests[p]=KSGlobalProgramRequest{owner,++programRequestRevision,dispatchPart,bankLatches[p],value->program};
                bankLatches[p]=255;if(dispatchPart==KSStorageSlot::Single)seed.bankLatch=255;event("program_requested",uint32_t(p)<<16|value->program);
            }
        } else if(const auto* value=std::get_if<ControlChange>(&input.event.payload)) {
            if(value->controller==64 || ksPanicControl(value->controller,value->value))return releaseNote(input,at);
            if(value->controller==121){event("reset_controllers_ignored",value->value);return {};}
            if(value->controller==32) {
                bankLatches[p]=value->value;
                if(dispatchPart==KSStorageSlot::Single)seed.bankLatch=value->value;
                event("bank_latched",value->value);
            } else if(value->controller==105) {
                const auto result=cutoff.applyPart(dispatchPart,value->value,at);
                if(result.status==DispatchDecision::Status::Fault)return result;
                rebaseParameters(dispatchPart);auxDirty=true;seed.patches[p][46]=value->value;reconcileEditor();
                event("cutoff_published",value->value);
                if(seed.layout==KSLayout::Performance)event("cutoff_storage_slot",uint8_t(dispatchPart));
            } else applyParameter(*value,decoded.remapped);
        } else if(std::holds_alternative<NoteOff>(input.event.payload))return releaseNote(input,at);
        return {};
    }

    void applyIngress() {
#ifdef SILVER_P12_ACTION_TRACE
        event("scheduled_musical_event",1);
#endif
        if(!routedInput) {
            const auto input=ingress.front().event;
#ifdef SILVER_P12_ACTION_TRACE
            auto& trace = P12ActionTrace::worker();
            if (trace.restoring()) {
                const auto bytes = encodeKSQueuedMidi(input.payload);
                trace.dispatch(bus().ticks, ingress.front().eligible, bytes.data(), bytes.size());
            }
#endif
            routedAt=musical?ingress.front().at:native();
            if(const auto* sysex=std::get_if<SysExBytes>(&input.payload)) {
                applySysEx(*sysex,routedAt);
                usedSysEx[ingress.front().sysExSlot]=false;
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            if(std::holds_alternative<MidiRealtime>(input.payload) || std::holds_alternative<MidiCommon>(input.payload)) {
                if(musical) {
                    // An earlier ordered tempo/source edit may have advanced
                    // the clock at a reload fence past this byte's arrival.
                    const auto at=std::max(routedAt,clock.lastAdmission().value_or(routedAt));
                    const auto admitted=clock.admit(owner,at,input.payload);
                    require(admitted==KSClockAdmission::Accepted || admitted==KSClockAdmission::Unhandled,
                            "Could not admit native KS MIDI clock");
                }
                if(programs) {
                    if(const auto* realtime=std::get_if<MidiRealtime>(&input.payload))event("midi_realtime",realtime->status);
                }
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            routedAt=std::max(routedAt,lastSceneCommit);
            if(const auto* load=std::get_if<KSDesktopSoundCommand>(&input.payload)) {
                auto* request=load->request;
                const auto accepted=std::visit([&](const auto& sound) {
                    using Sound=std::decay_t<decltype(sound)>;
                    if constexpr (std::is_same_v<Sound,KSDesktopPreset>) return beginDesktopPreset(sound,request->extras.get(),routedAt);
                    else if constexpr (std::is_same_v<Sound,KSDesktopDrumMap>) return beginDesktopDrumLoad(sound,bool(request->extras),routedAt);
                    else return beginDesktopPartSwitch(sound,routedAt);
                },request->sound);
                if(!accepted)
                    publishDesktopLoad(*request,KSDesktopLoadStatus::Rejected);
                else if(singleReload || sceneReload)
                    desktopSoundLoad=request;
                else
                    publishDesktopLoad(*request,KSDesktopLoadStatus::Applied);
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            if(const auto* edit=std::get_if<KSDrumGridEdit>(&input.payload)) {
                applyDrumGridEdit(*edit,routedAt);
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            if(desktopDrumControl(input.payload)) {
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            if(programs && handleCatalogMidi(input,routedAt)) {
                ingress.pop_front();--eligibleCount;++completedInputEvents;
                return;
            }
            if(const auto* control=std::get_if<ControlChange>(&input.payload);
               control && seed.partChannelRoutingImmediate && input.part==RoutedByChannel)
                require(desktopSelection.accept(*control),"Invalid desktop MIDI selection");
            const auto routed=routing.route(owner,input);
            require(bool(routed),"Invalid native routed event");
            routedInput=input;routeBatch=*routed;routeIndex=0;
            ingress.pop_front();--eligibleCount;
        }
        require(routing.current(owner,routeBatch),"Routing changed during an owned input");
        while(routeIndex<routeBatch.count && stage==RefreshStage::None && !feedback) {
            auto input=*routedInput;dispatchPart=routeBatch.parts[routeIndex++];
            const auto p=size_t(dispatchPart);input.part=uint8_t(seed.layout==KSLayout::Single?0:p-1);
            if(const auto* note=std::get_if<NoteOn>(&input.payload);note && !routing.noteInRange(dispatchPart,note->note))continue;
            if(const auto* note=std::get_if<NoteOff>(&input.payload);note && !routing.noteInRange(dispatchPart,note->note))continue;
            struct RoutedCall { Runtime& runtime; std::exception_ptr failure; } call{*this,{}};
            const MusicalDispatch dispatcher{&call,[](void* context,CommandSession&,const ScheduledEvent& event,NativeFrame at)noexcept {
                auto& call=*static_cast<RoutedCall*>(context);
                try{return call.runtime.dispatchRouted(event,at);}
                catch(...){call.failure=std::current_exception();return DispatchDecision{DispatchDecision::Status::Fault,{}};}
            }};
            require(session.submit(owner,musical?routedAt:native(),input).status==AdmissionStatus::Accepted,
                    "KS session refused complete routed event");
            const auto pumped=session.runUntil(native(),1,dispatcher);
            if(call.failure)std::rethrow_exception(call.failure);
            require(pumped.applied==1 && pumped.status!=PumpStatus::Fault,"Native routed event failed");
            if(musical && musical->phase.pending())return;
            if(allocatingNote){observeRefreshFeedback();return;}
            if(stage==RefreshStage::ReleaseGate)startPort(releaseTickets[releaseIndex]);
            if(stage==RefreshStage::SoundTransfer)startPort(*soundOff.ticket(0));
            if(stage!=RefreshStage::None)return;
        }
        if(routeIndex==routeBatch.count) {
            if(routeBatch.global==KSGlobalRoute::MirroredControl) {
                const auto input=std::get<ControlChange>(routedInput->payload);
                const auto control=decodeKsGlobalFallbackControl(seed.parameterTables,globalFallback.selector(),input);
                if(control.action==KSGlobalFallbackAction::ContextRequired) {
                    const auto target=editor.projection();require(bool(target),"Missing global packed editor target");
                    const auto p=size_t(target->storage);rebaseParameters(target->storage,true);
                    const auto dependency=state.storedDependency(target->storage,session.rateGeneration());
                    const auto patch=projectKsOwnedPatch(parameters,globals,state,target->storage,metadata[p],system,session.rateGeneration(),true);
                    const auto packed=patch && dependency?globalFallback.preparePacked(editor,parameterIngress,*dependency,*patch,input):std::nullopt;
                    require(packed && globalFallback.currentPacked(*packed,editor,parameterIngress,*dependency,*patch),"Invalid native packed editor source");
                    require(globals.canRequireStoredSingleRefresh(),"Stored Single refresh revision exhausted");
                    applyParameter(input,false,&*packed);
                    require(globalFallback.commitPacked(*packed),"Could not retire complete global packed edit");
                    reconcileEditor();
                    require(state.requestStoredSingleRefresh() && state.publish(native()),"Could not publish complete global packed request");
                    event("global_packed_applied",uint32_t(p)<<16 | control.parameter);
                } else {
                // Preflight the complete cross-part source barrier before any
                // desired global/system owner is changed.
                preflightParameterContexts(true,KSStorageSlot::Single);
                require(routing.revision()!=std::numeric_limits<uint64_t>::max(),"Global route revision exhausted");
                const auto previousChannel=system.channel();
                require(metadata[0].revision!=std::numeric_limits<uint64_t>::max(),"Global channel mirror revision exhausted");
                const auto decoded=decodeKsGlobalFallbackControl(seed.parameterTables,globalFallback.selector(),std::get<ControlChange>(routedInput->payload));
                if(decoded.action==KSGlobalFallbackAction::GlobalSetting && decoded.field==242 && decoded.value!=previousChannel)
                    preflightGlobalChannel(decoded.value,contextRevisions[0]);
                if(decoded.action==KSGlobalFallbackAction::GlobalSetting && (decoded.field==84 || decoded.field==243))
                    require(clock.available() && clock.projection().accepted!=std::numeric_limits<uint64_t>::max(),"Global clock request capacity exhausted");
                const auto result=globalFallback.applyControl(globals,system,std::get<ControlChange>(routedInput->payload));
                require(result.status!=KSGlobalFallbackStatus::Invalid && result.status!=KSGlobalFallbackStatus::Exhausted,
                        "Invalid native global control fallback");
                if(result.status==KSGlobalFallbackStatus::ContextRequired || result.status==KSGlobalFallbackStatus::ReservedPackedBoundary)
                    event("global_parameter_context_rejected",result.control.parameter);
                else event("global_control_applied",result.control.field);
                if(result.auxiliaryNotify)auxDirty=true;
                if(result.globalDirty || result.globalPitch)for(size_t p=1;p<5;++p)if(state.keys(KSStorageSlot(p))) {
                    require(state.touchParameters(KSStorageSlot(p),false),"Global fallback parameter revision exhausted");rebaseParameters(KSStorageSlot(p));++contextRevisions[p];
                    if(result.globalPitch)seed.partContexts[p].globalFine=system.globalFine();
                }
                if(result.globalDirty || result.globalPitch)require(state.publish(native()),"Global fallback publication exhausted");
                if(system.channel()!=previousChannel)updateGlobalChannel();
                if(result.globalPitch)for(auto& voice:seed.voices)voice.context.globalFine=system.globalFine();
                if(result.velocity)seed.velocityPower=system.velocityPower();
                if(result.clock) {
                    require(clock.parameter(owner,musical?routedAt:native(),result.control.field,result.control.value)==KSClockAdmission::Accepted,
                            "Could not own global clock policy request");
                    event("clock_policy_published",system.tempo);
                }
                }
            } else if(routeBatch.global==KSGlobalRoute::Program) {
                require(!programRequests[0],"Unconsumed global program request");
                const auto request=globalFallback.program(owner,seed.layout,std::get<ProgramChange>(routedInput->payload));
                if(request){require(!programRequests[0],"Unconsumed global program request");programRequests[0]=*request;event("global_program_requested",request->program);}
            }
            routedInput.reset();++completedInputEvents;
        }
    }

    void admitExpression() {
        const auto result=expressionUpdate.admit(++operation,native());if(result==DspAdmission::Full)return;
        require(result==DspAdmission::Accepted,"Expression physical admission failed");
        if(expressionUpdate.ticket()){stage=RefreshStage::ExpressionTransfer;startPort(*expressionUpdate.ticket());}
        else finishExpression();
    }
    void finishExpression() {
        const auto progress=expressionUpdate.progress();
        if(progress==KSExpressionUpdateProgress::Ready){stage=RefreshStage::ExpressionAdmission;admitExpression();return;}
        require(progress==KSExpressionUpdateProgress::Complete && expressionUpdate.reset(),"Expression intent did not finish");
        stage=RefreshStage::None;event("expression_complete",1);
    }

    void readyWork(bool awaitingHostInput=false) {
#ifdef SILVER_P12_ACTION_TRACE
        P12ActionTrace::worker().readyCheck();
#endif
        if (hostRate && !hostRateRequested && !(bus() < hostRate->requestedAt)) {
            hostRateRequested = true;
            event("host_rate_requested", session.rateGeneration());
            event("host_rate_waits_for_packet", stage == RefreshStage::VoiceUpdateTransfer);
            if (hostCutoff) {
                const auto block = session.hostBlock(owner, hostRate->anchor, 1);
                const auto anchor = block ? session.hostDeadline(*block, 0) : std::nullopt;
                const auto map = anchor ? TimeMap<TimeDomain::Host, TimeDomain::Native>::make(
                    hostRate->anchor, *anchor, hostRate->rate, NativeRate) : std::nullopt;
                hostCutoff->due = map ? map->map(hostCutoff->at) : std::nullopt;
                require(hostCutoff->due && !(*hostCutoff->due < native()), "Invalid pending host cutoff deadline");
            }
        }
        if (hostCutoff && !hostCutoff->arrived && hostCutoff->due && !(native() < *hostCutoff->due)) {
            require(native() == *hostCutoff->due, "Host cutoff missed its native deadline");
            enqueue(bus().ticks, {ControlChange{hostCutoff->channel, 105, hostCutoff->value}});
            event("host_cutoff_arrived", session.rateGeneration());
            hostCutoff->arrived = true;
            if (!hostRateRequested) {
                event("host_cutoff_admitted", session.rateGeneration());
                hostCutoff.reset();
            }
        }
        while (eligibleCount < ingress.size() && ingress[eligibleCount].eligible <= bus().ticks) {
            ingress[eligibleCount].at=native();
            const auto& input = ingress[eligibleCount].event.payload;
            if (const auto* cc = std::get_if<ControlChange>(&input)) {
                if(ksPanicControl(cc->controller,cc->value)) {
                    event("panic_eligible",cc->controller);
                } else if (cc->controller==64) {
                    event("sustain_eligible",cc->value);
                } else if (cc->controller == 32) {
                    event("bank_eligible", cc->value);
                } else if (const auto routed=routing.route(owner,ingress[eligibleCount].event);routed && routed->count) {
                    event("cc105_eligible", cc->value);
                    if (feedback)
                        event("cc105_during_feedback", feedbackAddress);
                } else {
                    event("cc105_off_channel_eligible", cc->channel);
                }
            } else if (const auto* release = std::get_if<NoteOff>(&input)) {
                event("release_eligible", release->note);
            } else if (const auto* note = std::get_if<NoteOn>(&input)) {
                event("note_eligible", note->note);
            } else if(const auto* program=std::get_if<ProgramChange>(&input)) {
                event("program_eligible",program->program);
            } else if(const auto* sysex=std::get_if<SysExBytes>(&input)) {
                event("midi_sysex_eligible",sysex->size);
            } else if(const auto* realtime=std::get_if<MidiRealtime>(&input);realtime && programs) {
                event("midi_realtime_eligible",realtime->status);
            } else {
                event("expression_eligible",1);
            }
            ++eligibleCount;
        }
        if (transport && transport->nextBusAccess() == bus()) {
#ifndef SILVER_KS_COMMAND_HOST
            const auto ticket=lifecycleExercise ? transport->activeTicket() : std::nullopt;
            const auto work=ticket ? exerciseWork(*ticket) : p09::Work::Other;
            const auto before=ticket ? transport->metrics() : HdiWordMetrics{};
            if (ticket && !session.transactions().started(*ticket))
                p09::beforeIo(*lifecycleExercise,session,*ticket,work);
#endif
            const auto status = transport->pump(session, bus(), native());
#ifndef SILVER_KS_COMMAND_HOST
            if (ticket)
                p09::afterIo(*lifecycleExercise,session,*transport,*ticket,work,before,status);
#endif
            if (status == HdiWordStatus::WordAccepted) {
                transport = nullptr;
                require(!feedback && stage != RefreshStage::None, "Unexpected KS write completion");
                if (stage == RefreshStage::Signal)
                    advanceSignal();
                else if(stage==RefreshStage::VoiceUpdateTransfer)finishVoiceUpdate();
                else if(stage==RefreshStage::GlobalTransfer)finishGlobalUpdate();
                else if(stage==RefreshStage::ExpressionTransfer)finishExpression();
                else if(stage==RefreshStage::Clock)completeDspClockWord();
                else if(stage==RefreshStage::Phase)completeGlobalPhase();
                else if (stage==RefreshStage::VoiceStop) {
                    if (++noteStopIndex<noteStart.stopCount()) startPort(*noteStart.stopTicket(noteStopIndex));
                    else {
                        require(noteStart.finishStop(session,native())==KSNoteStartProgress::Complete,
                                "Physical stop did not commit native replacement");
                        event("voice_stop_accepted",size_t(noteStart.slot()));beginNotePitch();
                    }
                } else if (stage==RefreshStage::Note) {
                    if (++noteInitialIndex<noteStart.initialCount()) startPort(*noteStart.initialTicket(noteInitialIndex));
                    else {
                        require(noteStart.finishInitial(session)==KSNoteStartProgress::Complete && noteStart.reset(),
                                "Native initial transfer did not complete");
                        if(musical && musical->starting)
                            event("arp_gate_on_latency_frames",native().ticks-musical->starting->note.at.ticks);
                        event("note_gate_accepted",1);completeNoteSequence();
                    }
                }
                else if(stage==RefreshStage::SoundTransfer) {
                    if(++soundIndex<2)startPort(*soundOff.ticket(soundIndex));
                    else {
                        ++releases;event("gate_off_accepted",0x8085+22*size_t(soundOff.slot()));
                        const auto progress=soundOff.progress();
                        if(progress==KSAllSoundProgress::Complete) {
                            require(soundOff.reset(),"Completed all-sound-off retained owner");
                            soundPublished=false;stage=RefreshStage::None;
                        } else {
                            require(progress==KSAllSoundProgress::Ready,"All-sound-off transfer failed");
                            stage=RefreshStage::SoundAdmission;admitSoundOff();
                            if(stage==RefreshStage::SoundTransfer)startPort(*soundOff.ticket(0));
                        }
                    }
                }
                else if (stage == RefreshStage::ReleaseGate) {
                    require(releaseIndex < releaseCount && session.transactions().retire(releaseTickets[releaseIndex]),
                            "KS gate-off lost its batch owner");
                    ++releases;
                    event("gate_off_accepted", releaseAddresses[releaseIndex]);
                    if(arpReleaseDeadline)
                        event("arp_gate_off_latency_frames",native().ticks-arpReleaseDeadline->ticks);
                    if (++releaseIndex < releaseCount)
                        startPort(releaseTickets[releaseIndex]);
                    else {
                        releaseCount = releaseIndex = 0;
                        arpReleaseDeadline.reset();
                        stage = RefreshStage::None;
                    }
                } else
                    advancePacket();
            } else if (status == HdiWordStatus::ReplyQueued) {
                transport = nullptr;
                require(bool(feedback), "Unexpected KS reply during a write");
                completeFeedback();
            } else {
                require(status == HdiWordStatus::Progress || status == HdiWordStatus::WaitingForReadiness,
                        "Physical KS HDI transaction stalled");
            }
        }
        releaseNoteCapacity();
        if(stage==RefreshStage::ExpressionAdmission && session.transactions().available(Endpoint::A) &&
            session.transactions().available(Endpoint::B))admitExpression();
        if(stage==RefreshStage::VoiceUpdateAdmission && session.transactions().available(Endpoint::B))admitVoiceUpdate();
        if(stage==RefreshStage::GlobalAdmission) {
            const auto* source=globalUpdate.source();require(source,"Missing pending global source");
            if(session.transactions().available(Endpoint::A) && session.transactions().available(Endpoint::B))admitGlobalUpdate();
        }
        if(stage==RefreshStage::SoundAdmission) {
            if(session.snapshot().lifecycle!=Lifecycle::Running || session.transactions().poisoned(Endpoint::B)) {
                soundOff.abort();require(false,"All-sound-off lost session/endpoint while waiting for capacity");
            }
            if(session.transactions().available(Endpoint::B)>=2) {
                admitSoundOff();if(stage==RefreshStage::SoundTransfer)startPort(*soundOff.ticket(0));
            }
        }
        if (stage==RefreshStage::NoteAdmission || stage==RefreshStage::NoteInitialAdmission || stage==RefreshStage::ReleaseAdmission) {
            if (session.snapshot().lifecycle!=Lifecycle::Running || session.transactions().poisoned(Endpoint::B)) {
                if(stage==RefreshStage::ReleaseAdmission)session.stop(owner);else noteStart.abort(session);
                require(false,"Native startup lost its session/endpoint while waiting for capacity");
            }
        }
        if (stage==RefreshStage::NoteAdmission && session.transactions().available(Endpoint::B)>=noteStart.stopCount()+1)
            admitNoteStart();
        if (stage==RefreshStage::NoteInitialAdmission && session.transactions().available(Endpoint::B)>=noteStart.initialCount())
            admitNoteInitial();
        if(stage==RefreshStage::ReleaseAdmission && pendingRelease &&
           session.transactions().available(Endpoint::B)>=pendingRelease->size()) {
            admitPendingRelease();
            if(stage==RefreshStage::ReleaseGate)startPort(releaseTickets[releaseIndex]);
        }
        if (stage != RefreshStage::None || feedback)
            return;
        if (sceneReload) {
            require(native().ticks-sceneReload->started.ticks < 45340,"KS scene DSP confirmation timed out");
            if (!(native() < sceneReload->nextPoll)) {
                for(unsigned part=0;part<4;++part)if(sceneReload->mailboxMask&(1u<<part)) {
                    readPartFeedback(part);
                    break;
                }
            }
            return;
        }
        if (singleReload) {
            require(native().ticks - singleReload->started.ticks < 45340, "KS program DSP confirmation timed out");
            if (!(native() < singleReload->nextPoll))
                readPartFeedback(0);
            return;
        }
        if (!applyHostRate())
            return;
        if (serviceGlobalSync())
            return;
        if (refresh->nextDeadline(bus()) == bus()) {
            if (feedbackObservation != FeedbackObservation::DisabledControl)
                observeRefreshFeedback();
            else
                startRefresh();
            return;
        }
        while (stage==RefreshStage::None && !feedback) {
            if(serviceGlobalSync())continue;
            if(allocatingNote){observeRefreshFeedback();continue;}
            if(!routedInput && serviceArp(awaitingHostInput))continue;
            if(!routedInput && !eligibleCount) {
                if(serviceWaitingArpGate())continue;
                if(serviceDspClock())continue;
                break;
            }
            applyIngress();
        }
#ifdef SILVER_KS_COMMAND_HOST
        if (stage == RefreshStage::None && !feedback && liveMixer.due(native())) {
            KSRefreshPacket packet;
            liveMixer.append(packet, state, native());
            if (packet.size())
                startPacket(RefreshStage::LiveMixer, std::move(packet));
        }
#endif
        // Diagnostic: sample the four part mailboxes after real audio-frame
        // progress, with no catch-up backlog. A correlated reply or a complete
        // acknowledgement/rebuild owns the port until the batch finishes.
        // This probes the lost second fade request without borrowing a legacy
        // foreground cadence or reading DSP memory to make musical decisions.
        if (feedbackObservation == FeedbackObservation::EveryNativeFrame &&
            stage == RefreshStage::None && !feedback && lastPartPollFrame < native()) {
            backgroundPartFeedback = true;
            event("part_poll_started", 0);
            readPartFeedback(0);
        }
    }

    bool applyHostRate() {
        if (!hostRateRequested)
            return true;
        // The last accepted write can still occupy the DSP host port.
        // Resume at a real ESAI boundary if it has not been consumed yet.
        for (unsigned endpoint = 0; endpoint < 2; ++endpoint)
            if (pair.dsp(endpoint).peripherals().getHDI08().hasRXData())
                return false;
        const auto oldBlock = session.hostBlock(owner, hostRate->anchor, 1);
        require(oldBlock && session.rebaseHost(owner, hostRate->anchor, hostRate->rate, 0) &&
                !session.hostDeadline(*oldBlock, 0), "Could not retire the previous host rate segment");
        rebaseParameterRate();
        event("host_rate_committed", session.rateGeneration());
        if (hostCutoff) {
            const auto block = session.hostBlock(owner, hostCutoff->at, 1);
            require(block && hostCutoff->due && session.hostDeadline(*block, 0) == hostCutoff->due,
                    "Host cutoff deadline changed across the barrier");
            event("host_cutoff_mapped", hostCutoff->due->ticks);
            if (hostCutoff->arrived) {
                event("host_cutoff_admitted", session.rateGeneration());
                hostCutoff.reset();
            }
        }
        hostRate.reset();
        hostRateRequested = false;
        return true;
    }

    uint64_t nextBoundary() const {
        auto next = serialControllerCycle(pair.nextSerialDspCycle());
        if (eligibleCount < ingress.size())
            next = std::min(next, ingress[eligibleCount].eligible);
        if (transport)
            next = std::min(next, transport->nextBusAccess()->ticks);
        if (hostRate && !hostRateRequested)
            next = std::min(next, hostRate->requestedAt.ticks);
        if (stage == RefreshStage::None && !feedback && !hostRateRequested && !singleReload && !sceneReload)
            next = std::min(next, refresh->nextDeadline(bus())->ticks);
        return next;
    }

    PhysicalDspPair::AudioFrame render() {
        require(!stopped,"KS controller audio requested after stop");
        PhysicalDspPair::AudioFrame frame{};
        while (!pair.popAudio(frame)) {
#ifdef SILVER_P12_CPU_SCOPES
            p12::CpuScope controllerCpu(p12::CpuRegion::Controller);
#endif
            readyWork();
            const auto next = nextBoundary();
            require(next > bus().ticks, "Detached physical candidate deadline did not advance");
#ifdef SILVER_P12_CPU_SCOPES
            controllerCpu.finish();
#endif
            pair.advanceTo(next);
        }
#ifdef SILVER_P12_CPU_SCOPES
        p12::CpuScope controllerCpu(p12::CpuRegion::Controller);
#endif
#ifdef SILVER_KS_COMMAND_HOST
        // Host MIDI can still map to this native frame before the resampler
        // requests the next one. Let it arrive before emitting a Host tick.
        readyWork(true);
#else
        readyWork();
#endif
        return frame;
    }

    void writeMetrics(std::ostream& out) const {
        if(musical) {
#ifndef SILVER_KS_COMMAND_HOST
            require(!musical->hostBlocks || musical->hostBlocks->finished(), "KS diagnostic host playback is incomplete");
#endif
            require(!musical->starting && !arpReleaseDeadline && musical->waitingGates.none() &&
                    !clock.projection().pending && !musical->dspClock.pending() && !musical->phase.pending() &&
                    !musical->hostPositions.size(),
                    "KS native window has pending arp work");
            for(const auto& arp:musical->parts)
                require(!arp.front() && !arp.nextDeadline() && !arp.keyCount(),"KS native window ended with pending arp keys or gates");
            for(const auto& drum:musical->drums)
                require(drum.idle(),"KS native window ended with pending drum notes");
        }
#ifdef SILVER_KS_P05_TRACE_DSP_USE
        require(portA.filterTraceValid && portB.filterTraceValid, "Incomplete candidate DSP instruction trace");
#endif
        const auto counts = cutoff.metrics();
        out << "metric,count\ncc105_edits," << counts.edits << "\ncutoff_calculations," << counts.directCalculations
            << "\ncutoff_cache_hits," << counts.reusedCalculations << "\naddressed_operations," << operation
            << "\nrelease_gates," << releases << "\nproducer_deadlines," << producerDeadlines
            << "\nunmodelled_producer_words," << unmodelledProducerWords << '\n';
        if(musical)
            out << "drum_sequences," << seed.drumRhythms.tracks.size()
                << "\ndrum_sequence_events," << seed.drumRhythms.size
                << "\norigin_native_frame," << musical->origin.ticks << '\n';
        out << "runtime_bytes," << sizeof(Runtime) << "\nseed_bytes," << sizeof(PhysicalSeed)
            << "\ningress_capacity," << CommandScheduler::Capacity << "\ningress_bytes," << sizeof(IncomingQueue)
            << "\npending_clock_requests," << clock.projection().pending
            << "\nclock_requests_accepted," << clock.projection().accepted
            << "\nclock_requests_acknowledged," << clock.projection().acknowledged
            << "\ninput_events_queued," << queuedInputEvents << "\ninput_events_completed," << completedInputEvents
            << "\npending_input_events," << ingress.size()+size_t(bool(routedInput)) << '\n';
        const auto emit = [&](char endpoint, HdiWordMetrics metrics) {
            out << endpoint << "_status_reads," << metrics.statusReads
                << '\n' << endpoint << "_accepted_words," << metrics.acceptedWords
                << '\n' << endpoint << "_replies," << metrics.replies
                << '\n' << endpoint << "_readiness_transitions," << metrics.readinessTransitions
                << '\n' << endpoint << "_blocked_cycles," << metrics.blockedCycles
                << '\n' << endpoint << "_max_pending_cycles," << metrics.maxPendingAge << '\n';
        };
        emit('A', hdiA.metrics());
        emit('B', hdiB.metrics());
    }

    void writeMusicalState(std::ostream& out) const {
        const auto snapshot = state.snapshot();
        require(bool(snapshot), "KS musical state has no publication");
        out << "kind,slot,value,note,velocity\n";
        for (size_t slot = 0; slot < snapshot->state.slots.size(); ++slot) {
            const auto& part = snapshot->state.slots[slot];
            if (part.program && state.keys(KSStorageSlot(slot)))
                out << "cutoff," << slot << ',' << unsigned(part.parameters.cutoff) << ",,\n";
        }
        for (size_t slot = 0; slot < snapshot->state.voices.size(); ++slot) {
            const auto& voice = snapshot->state.voices[slot];
            if (voice && voice->feedbackAge)
                out << "voice," << slot << ',' << unsigned(voice->part) << ','
                    << unsigned(voice->note) << ',' << unsigned(voice->velocity) << '\n';
        }
        if (seed.hasSingleProgramRecord || programs) {
            out << "program,0," << unsigned(seed.singleSelection.bank) << ',' << unsigned(seed.singleSelection.program) << ",\n";
            out << "generation,0," << unsigned(seed.partGeneration) << ",,\n";
            for (size_t note = 0; note < KSKeyState::NoteCount; ++note)
                if (snapshot->state.keys[0].physical.test(note)) out << "down,0," << note << ",,\n";
        }
    }

    PhysicalDspPair& pair;
    PhysicalSeed seed;
    std::ostream* packets;
    std::ostream* timeline;
    KSProgramFlash* const programs;
    KSDesktopSoundLoad* desktopSoundLoad=nullptr;
    std::ostream* sysExOut_=nullptr;
    CommandMidiSink midiSink;
    CommandSession session;
    Session owner;
    KSMusicalState state;
    KSCommandRouting routing;
    KSDesktopMidiSelection desktopSelection;
    KSCommandExpression expression;
    KSVoiceParameterBank parameters;
    KSParameterIngress parameterIngress;
    KSDrumProgramStore drumStore{};
    struct DesktopDrumKey { uint64_t revision=0; uint8_t program=0; };
    std::array<DesktopDrumKey,KSMusicalState::StorageSlotCount> desktopDrumKeys{};
    uint8_t desktopDrumSlot=KSDrumEditMode::Off;
    KSDrumVoiceBindings drumBindings;
    std::array<KSParameterMetadata,5> metadata{};
    KSSystemSettings system;
    KSCommandClockState clock;
    KSCommandEditorSelection editor;
    KSPartConfigurationState configuration;
    bool feedbackConfigurationPending=false;
    KSStorageSlot feedbackConfigurationPart=KSStorageSlot::Single;
    KSCommandGlobalFallback globalFallback;
    std::optional<SingleReload> singleReload;
    std::optional<SceneReload> sceneReload;
    NativeFrame lastSceneCommit{};
    std::array<NativeFrame,5> sceneArpsAt{};
    std::array<uint8_t,16> midiBanks{};
    std::array<std::optional<KSGlobalProgramRequest>,5> programRequests{};
    uint64_t programRequestRevision=0;
    KSGlobalState globals;
    KSCommandPartFeedback partFeedback;
    KSCommandVoiceFeedback voiceFeedback;
    KSCommandGlobalUpdate globalUpdate;
    KSCommandVoiceUpdate voiceUpdate;
    KSCommandExpressionUpdate expressionUpdate;
#ifdef SILVER_KS_COMMAND_HOST
    KSLiveMixer liveMixer;
    std::bitset<KSMusicalState::StorageSlotCount> pendingMixerRoute;
#endif
    std::array<std::optional<ParameterDependency<KSStorageSlot>>,5> parameterSources{};
    std::array<uint64_t,5> contextRevisions{};
    KSMusicalState::DirtySlots updateParts;
    KSStorageSlot updatePart=KSStorageSlot::Single;
    bool globalIsFeedback=false,voiceUpdateApplied=false;
    KSCommandCutoff cutoff;
    std::optional<KSRefreshClock> refresh;
    KSDeferredRefresh deferred;
    FeedbackObservation feedbackObservation;
    const HdiWordTiming hdiTiming;
    const unsigned producerWordLimit;
#ifndef SILVER_KS_NATIVE_COMMAND_ONLY
    struct ImageAvailability {
        DspTicket ticket;
        KSVoiceImagePatch patch;
        KSVoiceImageContext context;
        KSCutoffInputs cutoff;
    };
    std::optional<ImageAvailability> imageAvailability;
#endif
    uint64_t producerDeadlines = 0, unmodelledProducerWords = 0;
    NativeFrame lastPartPollFrame{};
    bool backgroundPartFeedback = false;
    std::array<ActiveVoice, std::tuple_size_v<decltype(PhysicalSeed::voices)>> voices{};
    Port portA, portB;
    DspWordTransport hdiA, hdiB;
    DspWordTransport* transport = nullptr;
#ifndef SILVER_KS_COMMAND_HOST
    p09::Exercise* lifecycleExercise = nullptr;
#endif
    bool stopped = false;
    std::optional<DspTicket> signal;
    std::optional<DspTicket> feedback;
    std::array<DspTicket, KSMusicalState::VoiceCount> releaseTickets{};
    std::array<uint16_t, KSMusicalState::VoiceCount> releaseAddresses{};
    size_t releaseCount = 0, releaseIndex = 0;
    uint16_t feedbackAddress = 0;
    size_t feedbackVoice = 0;
    std::optional<size_t> pendingNote;
    std::optional<NoteOn> allocatingNote;
    std::optional<NoteOn> sequenceNote,replacementNote;
    std::optional<KSNoteAssignment> selectedNote;
    uint8_t noteLayer=0;
    uint64_t selectedGroup=0;
    bool notePublishesKey=true;
    bool noteTracks=true;
#ifdef SILVER_KS_NATIVE_COMMAND_ONLY
    bool paceNoteImage = false;
#endif
    KSStorageSlot dispatchPart=KSStorageSlot::Single,notePart=KSStorageSlot::Single;
    KSCommandNoteStart noteStart;
    size_t noteStopIndex=0,noteInitialIndex=0;
    std::optional<NoteCapacityDiagnostic> noteCapacity;
    std::optional<KSCommandNoteRelease> pendingRelease;
    KSCommandAllSoundOff soundOff;
    size_t soundIndex=0;
    bool soundPublished=false,pendingAllNotes=false;
    bool pendingReleasePedal=false,feedbackForArpGate=false;
    std::optional<NativeFrame> arpReleaseDeadline;
    uint8_t pendingReleaseValue=0;
    std::optional<KSRefreshTransfer> transfer;
    std::optional<ControllerEvent> routedInput;
    NativeFrame routedAt{};
    std::unique_ptr<MusicalWork> musical;
    KSRoutedParts routeBatch{};size_t routeIndex=0;
    std::array<uint8_t,5> bankLatches{};
    IncomingQueue ingress;
    MidiDecoder midiDecoder;
    std::array<std::array<uint8_t,MaxSysExBytes>, CommandScheduler::SysExSlots> ingressSysEx{};
    std::array<bool, CommandScheduler::SysExSlots> usedSysEx{};
    std::optional<HostRateChange> hostRate;
    std::optional<HostCutoff> hostCutoff;
    size_t eligibleCount = 0;
    RefreshStage stage = RefreshStage::None;
    bool auxDirty = seed.panelAuxDirty;
    uint8_t auxiliaryValue=0;
    uint64_t auxiliaryRevision=0;
    bool hostRateRequested = false;
    uint64_t releases = 0;
    uint64_t queuedInputEvents=0,completedInputEvents=0;
    uint64_t bundle = 0, operation = 0, nextUart = 0, lastInputCycle = 0;
};

PhysicalCandidate::PhysicalCandidate(PhysicalDspPair& pair, const PhysicalSeed& seed, KSProgramFlash& programs)
    : runtime_(std::make_unique<Runtime>(pair, seed, nullptr, nullptr, nullptr,
                                       FeedbackObservation::AtRefresh, HdiCadence::BusMinimum, &programs)) {}

PhysicalCandidate::PhysicalCandidate(PhysicalDspPair& pair, PhysicalSeed seed, std::ostream& packets,
    std::ostream& wire, std::ostream& timeline, FeedbackObservation feedbackObservation, HdiCadence cadence, KSProgramFlash* programs)
    : runtime_(std::make_unique<Runtime>(pair, seed, &packets, &wire, &timeline, feedbackObservation, cadence, programs)) {}

PhysicalCandidate::~PhysicalCandidate() = default;

void PhysicalCandidate::enqueueCutoff(uint64_t inputCycle, uint8_t channel, uint8_t value) {
    require(channel < 16 && value < 128, "Invalid CC105 channel or value");
    runtime_->enqueue(inputCycle, {ControlChange{channel, 105, value}});
}

void PhysicalCandidate::enqueueRelease(uint64_t inputCycle, uint8_t note) {
    require(note < 128, "Invalid KS release note");
    runtime_->enqueue(inputCycle, {NoteOff{runtime_->seed.channel, note, 0}});
}

void PhysicalCandidate::enqueueSustain(uint64_t inputCycle,uint8_t value) {
    require(value<128,"Invalid sustain value");
    runtime_->enqueue(inputCycle,{ControlChange{runtime_->seed.channel,64,value}});
}

void PhysicalCandidate::enqueuePanic(uint64_t inputCycle,uint8_t controller,uint8_t value) {
    require(bool(ksPanicControl(controller,value)),"Invalid native panic control");
    runtime_->enqueue(inputCycle,{ControlChange{runtime_->seed.channel,controller,value}});
}

void PhysicalCandidate::enqueueBank(uint64_t inputCycle, uint8_t bank) {
    require(bank < 128, "Invalid bank");
    runtime_->enqueue(inputCycle, {ControlChange{runtime_->seed.channel, 32, bank}});
}

void PhysicalCandidate::enqueueProgram(uint64_t inputCycle, uint8_t program) {
    require(program < 128, "Invalid program");
    runtime_->enqueue(inputCycle, {ProgramChange{runtime_->seed.channel, program}});
}

void PhysicalCandidate::enqueueNote(uint64_t inputCycle, uint8_t note, uint8_t velocity) {
    require(note < 128 && velocity < 128, "Invalid native Note On");
    runtime_->enqueue(inputCycle, {NoteOn{runtime_->seed.channel, note, velocity}});
}

void PhysicalCandidate::enqueueSysEx(uint64_t inputCycle, SysExBytes message) {
    require(runtime_->enqueueMidiBytes(inputCycle,message.data,message.size)==message.size,"KS SysEx ingress backpressure");
}

size_t PhysicalCandidate::enqueueMidiBytes(uint64_t inputCycle, const uint8_t* data, size_t size) {
    return runtime_->enqueueMidiBytes(inputCycle,data,size);
}

void PhysicalCandidate::captureSysEx(std::ostream& out) {
    runtime_->sysExOut_=&out;
}

ProgramSelection PhysicalCandidate::singleSelection() const { return runtime_->seed.singleSelection; }
uint8_t PhysicalCandidate::performanceSelection() const { return runtime_->seed.performanceSelection; }
std::vector<uint8_t> PhysicalCandidate::checkpoint() const { return runtime_->checkpoint(); }
bool PhysicalCandidate::checkpointReady() const noexcept { return runtime_->checkpointReady(); }
void PhysicalCandidate::restoreCheckpoint(const KSControllerCheckpoint& saved) { runtime_->restoreCheckpoint(saved); }
NativeEditorState PhysicalCandidate::editorState() const { return runtime_->editorState(); }
#ifdef SILVER_KS_COMMAND_HOST
DspStopReport PhysicalCandidate::shutdown() { return runtime_->shutdown(); }
#else
void PhysicalCandidate::exerciseLifecycle(p09::Exercise& exercise) { runtime_->exerciseLifecycle(exercise); }
DspStopReport PhysicalCandidate::shutdown(p09::Exercise* exercise) { return runtime_->shutdown(exercise); }
#endif

std::vector<uint8_t> PhysicalCandidate::pendingProgram() const {
    const auto& pending=runtime_->singleReload;
    if(!pending)return {};
    if(pending->fromBank) {
        const auto channel=runtime_->seed.channel;
        return {uint8_t(0xb0|channel),32,pending->selection.bank,uint8_t(0xc0|channel),pending->selection.program};
    }
    auto patch=pending->patch;
    ks::packMirrors(patch);
    return NovationProgramSysEx::encode(Model::KS,0,0,0,0,patch.data(),patch.size());
}

std::array<uint8_t,21> PhysicalCandidate::globalSettings() const {
    std::array<uint8_t,21> settings;
    std::copy_n(runtime_->system.settings.begin() + 1,settings.size(),settings.begin());
    return settings;
}

std::vector<uint8_t> PhysicalCandidate::currentProgramDump() const { return runtime_->currentProgramDump(); }

void PhysicalCandidate::enqueueControl(uint64_t cycle,uint8_t channel,uint8_t controller,uint8_t value) {
    require(channel<16 && controller<128 && value<128,"Invalid native control");
    runtime_->enqueue(cycle,{ControlChange{channel,controller,value}});
}
void PhysicalCandidate::enqueuePitch(uint64_t cycle,uint8_t channel,uint16_t value) {
    require(channel<16 && value<16384,"Invalid native pitch bend");runtime_->enqueue(cycle,{PitchBend{channel,value}});
}
void PhysicalCandidate::enqueuePressure(uint64_t cycle,uint8_t channel,uint8_t value) {
    require(channel<16 && value<128,"Invalid native pressure");runtime_->enqueue(cycle,{ChannelPressure{channel,value}});
}
void PhysicalCandidate::enqueueChannelNote(uint64_t cycle,uint8_t channel,uint8_t note,uint8_t velocity) {
    require(channel<16 && note<128 && velocity<128,"Invalid native channel note");runtime_->enqueue(cycle,{NoteOn{channel,note,velocity}});
}

void PhysicalCandidate::enableArpeggiator(std::ostream& clock,std::ostream& notes,std::ostream& midi) {
    runtime_->enableArp(&clock,&notes,&midi);
}
void PhysicalCandidate::enableArpeggiator(double hostSampleRate) {
    runtime_->enableArp(nullptr,nullptr,nullptr,hostSampleRate);
}
bool PhysicalCandidate::enqueueHostPosition(novation::controller::MusicalClock::Position position) {
    return !runtime_->stopped && runtime_->musical &&
        runtime_->musical->hostPositions.push({runtime_->native(), position});
}
bool PhysicalCandidate::enqueueControl(const ControllerEvent& event) {
    if (runtime_->stopped)
        return false;
    if (const auto* edit=std::get_if<KSDrumGridEdit>(&event.payload))
        if (!edit->valid() || event.part!=RoutedByChannel)
            return false;
    if (const auto* load=std::get_if<KSDesktopSoundCommand>(&event.payload))
        if (!load->request || event.part!=RoutedByChannel)
            return false;
    if (const auto* mode=std::get_if<KSDrumEditMode>(&event.payload))
        if (!mode->valid() || event.part!=RoutedByChannel)
            return false;
    if (const auto* edit=std::get_if<KSDrumVoiceEdit>(&event.payload))
        if (!edit->valid() || event.part!=RoutedByChannel)
            return false;
    if (event.part != RoutedByChannel &&
        event.part >= (runtime_->seed.layout == KSLayout::Single ? 1u : 4u))
        return true;
    return runtime_->admitMidi(std::max(runtime_->bus().ticks, runtime_->nextUart), event.payload, event.part);
}
void PhysicalCandidate::setMidiSink(CommandMidiSink sink) noexcept { runtime_->midiSink = sink; }

#ifndef SILVER_KS_COMMAND_HOST
void PhysicalCandidate::enableHostBlocks(bool variable,std::ostream& trace) {
    auto& runtime=*runtime_;
    require(runtime.musical && !runtime.musical->hostBlocks && !runtime.ingress.size(),
            "KS host blocks require a fresh native musical window");
    runtime.musical->hostBlocks.emplace(runtime.session,runtime.owner,runtime.musical->origin,variable,trace);
}
#endif

void PhysicalCandidate::enqueueRealtime(uint64_t cycle,uint8_t status) {
    require(status>=0xf8,"Invalid native KS realtime");
    runtime_->enqueue(cycle,{MidiRealtime{status}});
}

void PhysicalCandidate::holdNextNoteAdmission(uint32_t frames,bool initial,bool occupiedOnly) {
    require(frames && frames<=4096 && !runtime_->noteCapacity,"Invalid note capacity intervention");
    runtime_->noteCapacity=Runtime::NoteCapacityDiagnostic{frames,initial,occupiedOnly};
}
void PhysicalCandidate::holdNextVoiceRelease(uint32_t frames) {
    holdNextNoteAdmission(frames,false);runtime_->noteCapacity->release=true;
}

void PhysicalCandidate::scheduleHostRate(uint64_t inputCycle, HostFrame at, ClockRate rate) {
    require(inputCycle > runtime_->bus().ticks && !runtime_->hostRate && !runtime_->hostCutoff &&
            rate.numerator && rate.denominator,
            "Invalid KS host rate change");
    runtime_->hostRate = Runtime::HostRateChange{{inputCycle}, at, rate};
}

void PhysicalCandidate::scheduleHostCutoff(HostFrame at, uint8_t channel, uint8_t value) {
    require(runtime_->hostRate && runtime_->hostRate->anchor < at && !runtime_->hostCutoff && channel < 16 && value < 128,
            "Invalid KS host cutoff request");
    runtime_->hostCutoff = Runtime::HostCutoff{at, channel, value};
}

PhysicalDspPair::AudioFrame PhysicalCandidate::render() { return runtime_->render(); }

void PhysicalCandidate::writeMetrics(std::ostream& out) const { runtime_->writeMetrics(out); }
void PhysicalCandidate::writeMusicalState(std::ostream& out) const { runtime_->writeMusicalState(out); }
#ifndef SILVER_KS_COMMAND_HOST
void PhysicalCandidate::writeNoteState(std::ostream& out) const {
    out << "kind,part,index,note,velocity\n";
    for (size_t slot = 0; slot < runtime_->seed.partChannels.size(); ++slot)
        if (const auto* keys = runtime_->state.keys(novation::controller::command::KSStorageSlot(slot)))
            writeNoteStateRows(out, slot, keys->projection());
}
#endif
void PhysicalCandidate::writeSustainState(std::ostream& out) const {
    const auto snapshot=runtime_->state.snapshot();require(bool(snapshot),"Missing sustain snapshot");
    out<<"kind,part,slot,value\n";
    for(size_t part=0;part<runtime_->seed.partChannels.size();++part)
        if(runtime_->state.keys(KSStorageSlot(part))) {
            const auto& sustain=snapshot->state.sustain[part];
            out<<"down,"<<part<<",0,"<<sustain.down<<'\n'<<"bypass,"<<part<<",0,"<<sustain.bypass<<'\n';
        }
    for(size_t slot=0;slot<snapshot->state.voices.size();++slot) {
        const auto& voice=snapshot->state.voices[slot];const auto& pending=snapshot->state.deferred[slot];
        if(voice && voice->feedbackAge && pending && *pending==voice->allocation)
            out<<"deferred,"<<size_t(voice->part)<<','<<slot<<",1\n";
    }
}
void PhysicalCandidate::writeProgramPatch(std::ostream& out) const {
    const auto& patch = runtime_->seed.patches[0];
    out.write(reinterpret_cast<const char*>(patch.data()), patch.size());
}
void PhysicalCandidate::writeStoragePatch(KSStorageSlot part,std::ostream& out)const {
    require(size_t(part)<runtime_->seed.patches.size(),"Invalid storage projection");
    const auto p=size_t(part);
    const auto patch=projectKsOwnedPatch(runtime_->parameters,runtime_->globals,runtime_->state,part,runtime_->metadata[p],
        runtime_->system,runtime_->session.rateGeneration(),true);
    const auto& value=patch?*patch:runtime_->seed.patches[p];
    out.write(reinterpret_cast<const char*>(value.data()),value.size());
}
void PhysicalCandidate::writeEditorState(std::ostream& out)const {
    const auto source=runtime_->editor.projection();require(bool(source),"Missing editor projection");
    out<<"field,value\nlayout_performance,"<<unsigned(runtime_->seed.layout==KSLayout::Performance)<<"\neditor_target,"<<size_t(source->storage)<<"\neditor_selection_mask,"<<unsigned(runtime_->seed.editorSelectionMask)
        <<"\nglobal_channel,"<<unsigned(runtime_->system.channel())<<"\nglobal_setting_channel,"<<unsigned(runtime_->system.channel())
        <<"\nstored_single_aux,"<<unsigned(runtime_->globals.storedAuxiliary(KSStorageSlot::Single).value_or(0))
        <<"\npart1_aux,"<<unsigned(runtime_->globals.storedAuxiliary(KSStorageSlot::Multi1).value_or(0))<<"\nopaque_global233,"<<unsigned(runtime_->system.opaque233())<<'\n';
}

void PhysicalCandidate::writePartConfiguration(std::ostream& out)const {
    out<<"part,level,output,channel,lowKey,highKey,transpose,detune,velocityCurve,noteDispatchMode\n";
    for(size_t p=0;p<5;++p) {
        const auto value=runtime_->configuration.read(KSStorageSlot(p));require(bool(value),"Missing applied configuration projection");
        out<<p;for(auto byte:value->values.bytes())out<<','<<unsigned(byte);out<<','<<unsigned(value->noteDispatchMode)<<'\n';
    }
}

void PhysicalCandidate::writeDesktopSelectors(std::ostream& out)const {
    out<<"channel,selector\n";
    for(uint8_t channel=0;channel<16;++channel)out<<unsigned(channel)<<','<<unsigned(*runtime_->desktopSelection.selector(channel))<<'\n';
}

}

#ifdef SILVER_KS_P05_TRACE_DSP_USE
extern "C" void silverKsObserveXStore(dsp56k::DSP* dsp) noexcept { ks::research::recordFilterUse(dsp, 0x07f3); }
extern "C" void silverKsObserveFilterUse(dsp56k::DSP* dsp, unsigned pc) noexcept {
    ks::research::recordFilterUse(dsp, pc);
    ks::research::recordFadeStore(dsp, pc);
}
extern "C" bool silverKsBindFilterUseTrace(ks::DSP* dsp, void* context, ks::research::FilterTraceWriter writer) noexcept {
    return ks::research::bindFilterTrace(dsp, context, writer);
}
extern "C" bool silverKsBindFadeUseTrace(ks::DSP* dsp, void* context, ks::research::FilterTraceWriter writer) noexcept {
    return ks::research::bindFadeTrace(dsp, context, writer);
}
#endif
