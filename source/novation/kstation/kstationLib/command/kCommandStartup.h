#pragma once

#include "kCommandColdStart.h"
#include "kCommandSeed.h"
#include "../../../controller/command/controllerArchive.h"

namespace kstation {
class Rom;
}

namespace kstation::command {

struct KCommandState {
    novation::controller::command::KProgramTables tables;
    novation::controller::command::KColdProgram initial;
    novation::controller::command::ProgramSelection selection{1, 0};
    std::string resourceHash;
    std::vector<uint8_t> flash, pending;

    KCommandSeed seed(uint64_t physicalCycle) const;
};

KCommandState readKCommandState(const Rom&, const std::vector<uint8_t>& archive);
std::vector<uint8_t> saveKCommandState(
    const KCommandState&, novation::controller::command::ProgramSelection,
    const novation::controller::command::KExpressionSource&, std::vector<uint8_t> pending);
}
