#include "ksCommandStartup.h"

#include "ksCommandColdStart.h"
#include "ksCommandExpression.h"
#include "../ksRom.h"
#include "../../../controller/command/controllerArchive.h"
#include "baseLib/sha256.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace ks {
namespace {
using namespace novation::controller::command;
using research::PhysicalSeed;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

PhysicalSeed makeSeed(const KSColdPrograms& programs, const KSProgramFlash& flash,
                      const std::vector<uint8_t>& firmware, const KSGlobalTables& globals,
                      const std::optional<KSControllerCheckpoint>& checkpoint) {
    const auto voices = decodeKsVoiceImageTables(firmware.data(), firmware.size());
    const auto parameters = decodeKsParameterMapTables(firmware.data(), firmware.size());
    const auto velocity = decodeKsNoteVelocityTables(firmware.data(), firmware.size());
    const auto arp = readKSArpRhythms(firmware.data(), firmware.size());
    const auto drums = readKSDrumCatalog(firmware.data(), firmware.size());
    require(voices && parameters && velocity && arp && drums, "KS cold command tables are invalid");
    PhysicalSeed seed{};
    seed.voiceTables = *voices;
    seed.parameterTables = *parameters;
    seed.velocityTables = *velocity;
    seed.arpRhythms = *arp;
    seed.drumRhythms = *drums;
    seed.globalTables = globals;
    seed.patches = programs.patches;
    seed.layout = programs.layout;
    seed.channel = programs.globalChannel;
    seed.activeMask = programs.activeMask;
    seed.singleSelection = programs.single;
    seed.performanceSelection = programs.performance;
    seed.globalMaster = programs.master();
    seed.auxiliary = seed.patches[0][225];
    seed.nextTickDelay = 25000;
    seed.bankLatch = 255;
    seed.partChannels.fill(KSCommandCutoff::NoChannel);
    if (seed.layout == KSLayout::Single) {
        seed.partChannels[0] = seed.channel;
    } else {
        seed.activePart = KSStorageSlot(1);
        seed.trackingPart = seed.activePart;
        seed.editorTarget = seed.activePart;
        seed.globalMaster.previousEffectStatus = seed.globalMaster.performanceEffectStatus;
        for (size_t part = 1; part < seed.patches.size(); ++part)
            seed.partChannels[part] = seed.patches[part][181];
    }
    for (size_t part = 0; part < seed.patches.size(); ++part) {
        const auto& patch = seed.patches[part];
        seed.partConfigurations[part] = KSPartConfigurationValues::fromPatch(patch);
        seed.partNoteDispatchModes[part] = patch[209];
        seed.partContexts[part].wheelFine = seed.partConfigurations[part].detune;
        seed.partContexts[part].wheelPitch = KSExpressionValues{}.wheelPitch;
        seed.partContexts[part].globalFine = seed.patches[0][244];
    }
    for (size_t slot = 0; slot < seed.voices.size(); ++slot)
        seed.voices[slot].context.oscillatorAmplitude = ksColdVoiceScale(firmware, slot);
    for (size_t bank = 0; bank < seed.drumRecords.size(); ++bank) {
        for (size_t program = 0; program < seed.drumRecords[bank].size(); ++program) {
            auto patch = flash.single({uint8_t(5 + bank), uint8_t(program)});
            if (patch) {
                expandKsParameterFields(*patch);
                seed.drumRecords[bank][program] = *patch;
            } else seed.drumRecords[bank][program].fill(0xff);
        }
    }
    if (checkpoint) {
        seed.editorSelectionMask=checkpoint->editorMask;
        seed.editorTarget=KSStorageSlot(checkpoint->editorPart);
        seed.parameterSelectors=checkpoint->partSelectors;
        seed.desktopParameterSelectors=checkpoint->desktopSelectors;
        if (seed.layout==KSLayout::Performance)
            std::copy(checkpoint->performanceChannels.begin(),checkpoint->performanceChannels.end(),seed.partChannels.begin()+1);
    }
    return seed;
}

}

KSCommandState readKSCommandState(std::vector<uint8_t> bytes, const std::vector<uint8_t>& wave) {
    KSCommandState state;
    state.resourceHash = baseLib::SHA256(wave).toString();
    if (auto saved = decodeControllerArchive(bytes)) {
        require(saved->model == Model::KS && saved->resourceHash == state.resourceHash,
                "KS saved-state resource mismatch");
        if (!saved->ksCheckpoint.empty()) {
            state.checkpoint = decodeKSControllerCheckpoint(saved->ksCheckpoint);
            require(bool(state.checkpoint), "Invalid native KS control checkpoint");
        }
        state.flash = std::move(saved->userFlash);
        state.activeProgram = std::move(saved->activeProgram);
        state.pendingControl = std::move(saved->pendingControl);
    } else {
        constexpr std::string_view archiveMagic = "SKCMD";
        require(bytes.size() < archiveMagic.size() ||
                !std::equal(archiveMagic.begin(), archiveMagic.end(), bytes.begin()), "Invalid KS controller archive");
        state.flash = std::move(bytes);
    }
    return state;
}

PhysicalSeed bootKSCommandController(PhysicalDspPair& pair, const Rom& rom, const std::vector<uint8_t>& wave,
                                     const KSCommandState& state, const KSProgramFlash& flash, SysExBytes legacy) {
    require(rom.isValid(), rom.error().c_str());
    auto programs = readKsColdPrograms(flash);
    if (state.restored())
        require(restoreKsColdProgram(programs, {state.activeProgram.data(), state.activeProgram.size()}),
                "KS saved current program is invalid");
    if (legacy.data)
        require(restoreKsColdProgram(programs, legacy), "KS legacy current program is invalid");
    if (const auto& checkpoint = state.checkpoint) {
        const bool multi = programs.layout == KSLayout::Performance;
        require(bool(checkpoint->storedSingle) == multi && (multi || !checkpoint->editorPart), "Invalid saved KS layout");
        if (checkpoint->storedSingle) {
            programs.patches[0] = *checkpoint->storedSingle;
            programs.restoredMaster = KSGlobalMaster::fromPatch(programs.patches[1], 0);
        }
    }
    const auto tables = decodeKsGlobalTables(rom.data().data(), rom.data().size());
    require(bool(tables), "KS cold global tables are invalid");
    const auto seed = makeSeed(programs, flash, rom.data(), *tables, state.checkpoint);
    bootKsCommandDsps(pair, rom.data(), wave, programs, *tables);
    return seed;
}

std::vector<uint8_t> saveKSCommandState(const std::string& resourceHash, const KSProgramFlash& flash,
                                        const research::PhysicalCandidate& controller, const std::vector<uint8_t>& pending,
                                        const std::vector<uint8_t>& checkpoint) {
    auto current = controller.currentProgramDump();
    const auto selection = controller.singleSelection();
    constexpr auto global = KSProgramFlash::Globals;
    std::vector<uint8_t> userFlash(flash.userData(), flash.userData() + KSProgramFlash::UserBytes);
    const auto settings = controller.globalSettings();
    std::copy(settings.begin(), settings.end(), userFlash.begin() + global);
    userFlash[global] = 1;
    userFlash[global + 1] = controller.performanceSelection();
    userFlash[global + 2] = selection.bank;
    userFlash[global + 3] = selection.program;
    userFlash[global + 4] = current[7] == 2;
    return encodeControllerArchive({Model::KS, resourceHash, std::move(userFlash), std::move(current), pending, checkpoint});
}
}
