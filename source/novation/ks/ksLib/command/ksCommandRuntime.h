#pragma once
#include "ksLib/command/ksProgramFlash.h"
#include "ksLib/command/ksArpPolicy.h"
#include "ksLib/command/ksDrumArpeggiator.h"

#include "ksLib/command/ksCommandRefresh.h"
#include "ksLib/command/ksControllerCheckpoint.h"
#include "ksLib/command/ksParameterIngress.h"
#include "ksLib/command/ksCommandVelocity.h"
#include "ksLib/command/ksPartConfigurationState.h"
#include "ksLib/ksPhysicalDspPair.h"
#include "../../../controller/musicalClock.h"
#include "../../../controller/command/commandAudioDevice.h"

#include <iosfwd>

#ifndef SILVER_KS_COMMAND_HOST
namespace p09 { struct Exercise; }
#endif
namespace novation::controller::command { struct DspStopReport; struct NativeEditorState; }

#if defined(_WIN32) && !defined(SILVER_KS_COMMAND_HOST)
#  ifdef silverKsP05Candidate_EXPORTS
#    define SILVER_KS_P05_API __declspec(dllexport)
#  else
#    define SILVER_KS_P05_API __declspec(dllimport)
#  endif
#else
#  define SILVER_KS_P05_API
#endif

namespace ks::research {

struct PreparedVoice {
    novation::controller::command::KSVoiceImageContext context;
    novation::controller::command::KSStorageSlot part;
    uint16_t age, countdown;
    uint8_t note, velocity, generation;
    bool deferred = false;
    uint8_t pitchDistance = 0, noteOffExtra = 0;
};

struct PhysicalSeed {
    std::array<novation::controller::command::KSVoiceImagePatch,
               novation::controller::command::KSMusicalState::StorageSlotCount> patches;
    novation::controller::command::KSVoiceImageTables voiceTables;
    novation::controller::command::KSGlobalMaster globalMaster;
    novation::controller::command::KSGlobalTables globalTables;
    novation::controller::command::KSCommandCutoff::Channels partChannels;
    novation::controller::command::KSLayout layout;
    novation::controller::command::KSStorageSlot trackingPart;
    uint64_t nextTickDelay;
    std::array<PreparedVoice, novation::controller::command::KSMusicalState::VoiceCount> voices;
    std::optional<novation::controller::command::VoiceSlot> tracked;
    uint16_t tick, lastAdmitted;
    uint8_t channel, activeMask, cooldown, auxiliary, pendingMask;
    bool panelAuxDirty;
    novation::controller::command::ProgramSelection singleSelection{};
    uint8_t performanceSelection = 0;
    std::array<uint8_t, 209> singleProgramRecord{};
    uint8_t transpose = 64, velocityCurve = 0, velocityPower = 0, partGeneration = 0, bankLatch = 0;
    bool hasSingleProgramRecord = false;
    std::array<novation::controller::command::KSKeyState::Projection,
               novation::controller::command::KSMusicalState::StorageSlotCount> keys{};
    std::array<bool, novation::controller::command::KSMusicalState::StorageSlotCount> sustainDown{};
    std::array<bool, novation::controller::command::KSMusicalState::StorageSlotCount> restartMono{};
    std::array<uint8_t, novation::controller::command::KSMusicalState::StorageSlotCount> lastPlayed{};
    uint8_t unisonSpread=0;
    novation::controller::command::KSStorageSlot activePart=novation::controller::command::KSStorageSlot::Single;
    std::array<novation::controller::command::KSVoiceImageContext,
               novation::controller::command::KSMusicalState::StorageSlotCount> partContexts{};
    novation::controller::command::KSParameterMapTables parameterTables{};
    std::array<uint8_t,novation::controller::command::KSMusicalState::StorageSlotCount> parameterSelectors{255,255,255,255,255};
    // Cold immutable program data, not firmware addresses. Runtime bindings
    // must point into the final owned seed, never a preparation temporary.
    std::array<std::array<novation::controller::command::KSVoiceImagePatch,50>,4> drumRecords{};
    uint64_t drumStoreGeneration=1;
    std::array<uint64_t,4> drumBankRevisions{1,1,1,1};
    std::array<uint8_t,novation::controller::command::KSMusicalState::StorageSlotCount> bankLatches{255,255,255,255,255};
    novation::controller::command::KSNoteVelocityTables velocityTables{};
    novation::controller::command::KSStorageSlot editorTarget=novation::controller::command::KSStorageSlot::Single;
    uint8_t editorSelectionMask=0;
    std::array<novation::controller::command::KSPartConfigurationValues,5> partConfigurations{};
    bool partChannelRoutingImmediate=true;
    std::array<uint8_t,5> partNoteDispatchModes{};
    std::array<uint8_t,16> desktopParameterSelectors{};
    novation::controller::command::KSArpRhythms arpRhythms{};
    novation::controller::command::KSDrumCatalog drumRhythms{};
    novation::controller::command::KSUserDrumPatterns userDrums{};
};

// EveryNativeFrame is a diagnostic intervention, not an accepted timing policy.
enum class FeedbackObservation { AtRefresh, DisabledControl, EveryNativeFrame };
// Research intervention for decomposing host I/O latency. This is not the
// external three-cycle bus minimum and is not an accepted timing model.
enum class HdiCadence { BusMinimum, HostAccessDiagnostic, ProducerPrefixDiagnostic, ProducerDeadlinesDiagnostic };

class PhysicalCandidate final {
public:
    SILVER_KS_P05_API PhysicalCandidate(PhysicalDspPair&, const PhysicalSeed&,
                                      novation::controller::command::KSProgramFlash&);
    SILVER_KS_P05_API PhysicalCandidate(PhysicalDspPair&, PhysicalSeed, std::ostream& packets,
                                      std::ostream& wire, std::ostream& timeline, FeedbackObservation,
                                      HdiCadence = HdiCadence::BusMinimum,
                                      novation::controller::command::KSProgramFlash* = nullptr);
    SILVER_KS_P05_API ~PhysicalCandidate();
    SILVER_KS_P05_API void enqueueCutoff(uint64_t inputCycle, uint8_t channel, uint8_t value);
    SILVER_KS_P05_API void enqueueRelease(uint64_t inputCycle, uint8_t note);
    SILVER_KS_P05_API void enqueueSustain(uint64_t inputCycle, uint8_t value);
    SILVER_KS_P05_API void enqueuePanic(uint64_t inputCycle, uint8_t controller, uint8_t value);
    SILVER_KS_P05_API void enqueueControl(uint64_t inputCycle,uint8_t channel,uint8_t controller,uint8_t value);
    SILVER_KS_P05_API void enqueuePitch(uint64_t inputCycle,uint8_t channel,uint16_t value);
    SILVER_KS_P05_API void enqueuePressure(uint64_t inputCycle,uint8_t channel,uint8_t value);
    SILVER_KS_P05_API void enqueueChannelNote(uint64_t inputCycle,uint8_t channel,uint8_t note,uint8_t velocity);
    SILVER_KS_P05_API void enableArpeggiator(std::ostream& clock,std::ostream& notes,std::ostream& midi);
    SILVER_KS_P05_API void enableArpeggiator(double hostSampleRate=0);
    SILVER_KS_P05_API bool enqueueHostPosition(novation::controller::MusicalClock::Position);
    SILVER_KS_P05_API bool enqueueControl(const novation::controller::command::ControllerEvent&);
    SILVER_KS_P05_API void setMidiSink(novation::controller::command::CommandMidiSink) noexcept;
#ifndef SILVER_KS_COMMAND_HOST
    SILVER_KS_P05_API void enableHostBlocks(bool variable,std::ostream& trace);
#endif
    SILVER_KS_P05_API void enqueueRealtime(uint64_t inputCycle,uint8_t status);
    SILVER_KS_P05_API void enqueueBank(uint64_t inputCycle, uint8_t bank);
    SILVER_KS_P05_API void enqueueProgram(uint64_t inputCycle, uint8_t program);
    SILVER_KS_P05_API void enqueueNote(uint64_t inputCycle, uint8_t note, uint8_t velocity);
    SILVER_KS_P05_API void enqueueSysEx(uint64_t inputCycle, novation::controller::command::SysExBytes);
    // Serial input owns each decoded frame until it reaches the controller.
    // A short return leaves the remaining bytes available for retry.
    SILVER_KS_P05_API size_t enqueueMidiBytes(uint64_t inputCycle, const uint8_t*, size_t);
    SILVER_KS_P05_API void captureSysEx(std::ostream&);
    SILVER_KS_P05_API novation::controller::command::ProgramSelection singleSelection() const;
    SILVER_KS_P05_API uint8_t performanceSelection() const;
    SILVER_KS_P05_API std::vector<uint8_t> pendingProgram() const;
    SILVER_KS_P05_API std::vector<uint8_t> checkpoint() const;
    SILVER_KS_P05_API bool checkpointReady() const noexcept;
    SILVER_KS_P05_API void restoreCheckpoint(const novation::controller::command::KSControllerCheckpoint&);
    SILVER_KS_P05_API std::array<uint8_t,21> globalSettings() const;
    SILVER_KS_P05_API std::vector<uint8_t> currentProgramDump() const;
    SILVER_KS_P05_API novation::controller::command::NativeEditorState editorState() const;
#ifdef SILVER_KS_COMMAND_HOST
    novation::controller::command::DspStopReport shutdown();
#else
    SILVER_KS_P05_API void exerciseLifecycle(p09::Exercise&);
    SILVER_KS_P05_API novation::controller::command::DspStopReport shutdown(p09::Exercise* = nullptr);
#endif
    // Research-only queue-capacity intervention. The held records never reach
    // DSP ports; real DSP/audio progress releases them after the given frames.
    SILVER_KS_P05_API void holdNextNoteAdmission(uint32_t frames, bool initialPhase, bool occupiedOnly=false);
    SILVER_KS_P05_API void holdNextVoiceRelease(uint32_t frames);
    SILVER_KS_P05_API void scheduleHostRate(uint64_t inputCycle, novation::controller::command::HostFrame at,
                                            novation::controller::command::ClockRate rate);
    SILVER_KS_P05_API void scheduleHostCutoff(novation::controller::command::HostFrame at, uint8_t channel,
                                             uint8_t value);
    SILVER_KS_P05_API PhysicalDspPair::AudioFrame render();
    SILVER_KS_P05_API void writeMetrics(std::ostream&) const;
    SILVER_KS_P05_API void writeMusicalState(std::ostream&) const;
    SILVER_KS_P05_API void writeProgramPatch(std::ostream&) const;
#ifndef SILVER_KS_COMMAND_HOST
    SILVER_KS_P05_API void writeNoteState(std::ostream&) const;
#endif
    SILVER_KS_P05_API void writeSustainState(std::ostream&) const;
    SILVER_KS_P05_API void writeStoragePatch(novation::controller::command::KSStorageSlot,std::ostream&) const;
    SILVER_KS_P05_API void writeEditorState(std::ostream&) const;
    SILVER_KS_P05_API void writePartConfiguration(std::ostream&) const;
    SILVER_KS_P05_API void writeDesktopSelectors(std::ostream&) const;

private:
    struct Runtime;
    std::unique_ptr<Runtime> runtime_;
};
}

#undef SILVER_KS_P05_API
