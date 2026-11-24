#pragma once

#include "kProgramImage.h"

#include <vector>

namespace kstation::command { class PhysicalTimeline; }

namespace novation::controller::command {

struct KColdProgram {
    std::array<uint8_t, KPackedProgramBytes> record;
    KExpressionSource expression;
    KVoiceTunings tunings;
    uint8_t channel;
};

KColdProgram readKColdProgram(const std::vector<uint8_t>& decodedOs, const KProgramTables&);
void bootKCommandDsp(kstation::command::PhysicalTimeline&, const std::vector<uint8_t>& decodedOs);

}
