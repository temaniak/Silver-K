#include "kCommandStartup.h"

#include "kProgramFlash.h"
#include "../kstationRomLoader.h"
#include "../../../controller/command/novationProgramSysEx.h"
#include "baseLib/sha256.h"

#include <stdexcept>

namespace kstation::command {
using namespace novation::controller::command;

KCommandSeed KCommandState::seed(uint64_t physicalCycle) const {
    KCommandSeed value;
    value.patch = initial.expression.patch;
    value.modulationCurve = initial.expression.modulationCurve;
    value.controls = initial.expression.controls;
    value.channel = initial.channel;
    value.noteTunings = initial.tunings;
    value.heldNote = 255;
    value.cycle = physicalCycle;
    return value;
}

KCommandState readKCommandState(const Rom& rom, const std::vector<uint8_t>& archive) {
    if (!rom.isValid())
        throw std::runtime_error(rom.error());
    const auto& firmware = rom.data();
    const auto tables = readKProgramTables(firmware);
    KCommandState state{tables, readKColdProgram(firmware, tables), {1, 0},
                        baseLib::SHA256(firmware).toString(), std::vector<uint8_t>(0x40000, 0xff), {}};
    constexpr auto global = KProgramFlash::Globals;
    auto& initial = state.initial;
    std::copy(initial.record.begin(), initial.record.end(), state.flash.begin() + KStationCommandBackend::ProgramBase);
    std::copy_n(firmware.data() + 0xbb12, 23, state.flash.begin() + global);
    if (archive.empty())
        return state;

    auto saved = decodeControllerArchive(archive);
    if (!saved)
        saved = decodeLegacyKArchive(archive, state.resourceHash);
    if (!saved || saved->model != Model::K || saved->resourceHash != state.resourceHash)
        throw std::runtime_error("K saved-state integrity or resource mismatch");
    std::copy(saved->userFlash.begin(), saved->userFlash.end(), state.flash.begin() + KProgramFlash::UserOffset);
    state.selection = {state.flash[global], state.flash[global + 1]};
    initial.channel = state.flash[global + 4];
    if (!KStationCommandBackend::programOffset(state.selection) || initial.channel >= 16)
        throw std::runtime_error("K saved-state program or channel is invalid");
    std::copy_n(state.flash.data() + global, 23, initial.expression.patch.begin() + 0xb6);
    initial.expression = decodeKProgram(saved->activeProgram.data() + NovationProgramSysEx::HeaderSize, initial.expression);
    initial.tunings.partTranspose = initial.expression.patch[0xad];
    state.pending = std::move(saved->pendingControl);
    return state;
}

std::vector<uint8_t> saveKCommandState(const KCommandState& state, ProgramSelection selection,
                                       const KExpressionSource& source, std::vector<uint8_t> pending) {
    std::vector<uint8_t> flash(state.flash.begin() + KProgramFlash::UserOffset, state.flash.end());
    constexpr auto global = KProgramFlash::Globals - KProgramFlash::UserOffset;
    std::copy_n(source.patch.begin() + 0xb6, 23, flash.begin() + global);
    flash[global] = selection.bank;
    flash[global + 1] = selection.program;
    auto program = NovationProgramSysEx::encode(Model::K, 0, 0, 0, 0, source.patch.data(), KPackedProgramBytes);
    return encodeControllerArchive({Model::K, state.resourceHash, std::move(flash), std::move(program), std::move(pending)});
}
}
