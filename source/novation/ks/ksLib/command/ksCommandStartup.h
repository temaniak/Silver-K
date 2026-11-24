#pragma once

#include "ksCommandRuntime.h"

#include <string>

namespace ks {
class Rom;

struct KSCommandState {
    std::vector<uint8_t> flash;
    std::string resourceHash;
    std::vector<uint8_t> activeProgram, pendingControl;
    std::optional<novation::controller::command::KSControllerCheckpoint> checkpoint;

    bool restored() const noexcept { return !activeProgram.empty(); }
};

KSCommandState readKSCommandState(std::vector<uint8_t> flashOrArchive, const std::vector<uint8_t>& wave);
research::PhysicalSeed bootKSCommandController(
    PhysicalDspPair&, const Rom&, const std::vector<uint8_t>& wave,
    const KSCommandState&, const novation::controller::command::KSProgramFlash&,
    novation::controller::command::SysExBytes legacyProgram = {});
std::vector<uint8_t> saveKSCommandState(
    const std::string& resourceHash, const novation::controller::command::KSProgramFlash&,
    const research::PhysicalCandidate&, const std::vector<uint8_t>& pending,
    const std::vector<uint8_t>& checkpoint);
}
