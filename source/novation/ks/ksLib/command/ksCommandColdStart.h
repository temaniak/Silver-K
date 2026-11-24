#pragma once

#include "ksCommandGlobals.h"
#include "ksProgramFlash.h"

#include <vector>

namespace ks { class PhysicalDspPair; }

namespace novation::controller::command {

struct KSColdPrograms {
    KSLayout layout;
    ProgramSelection single;
    uint8_t performance, globalChannel, activeMask;
    std::array<KSVoiceImagePatch, 5> patches;
    std::optional<KSGlobalMaster> restoredMaster;

    KSGlobalMaster master() const noexcept {
        auto value=restoredMaster.value_or(KSGlobalMaster::fromPatch(patches[0],0));
        if (layout==KSLayout::Performance)value.performanceEffectStatus=patches[1][208];
        return value;
    }
};

KSColdPrograms readKsColdPrograms(const KSProgramFlash&);
bool restoreKsColdProgram(KSColdPrograms&, SysExBytes) noexcept;
std::array<uint8_t, 3> ksColdVoiceScale(const std::vector<uint8_t>& decodedOs, size_t slot);
void bootKsCommandDsps(ks::PhysicalDspPair&, const std::vector<uint8_t>& decodedOs,
                      const std::vector<uint8_t>& waveRom, const KSColdPrograms&, const KSGlobalTables&);

}
