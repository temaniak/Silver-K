#pragma once

#include "controllerEvent.h"

#include <optional>
#include <string>
#include <vector>

namespace novation::controller::command {

inline constexpr size_t KUserFlashBytes = 0x20400, KSUserFlashBytes = 0x40400;

struct ControllerArchive {
    Model model;
    std::string resourceHash;
    std::vector<uint8_t> userFlash;
    std::vector<uint8_t> activeProgram;
    // Ordered control MIDI whose effects are still absent from activeProgram.
    std::vector<uint8_t> pendingControl;
    // Version two uses the final section for a model-owned KS checkpoint.
    std::vector<uint8_t> ksCheckpoint;
};

std::vector<uint8_t> encodeControllerArchive(const ControllerArchive&);
std::optional<ControllerArchive> decodeControllerArchive(const std::vector<uint8_t>&);
std::optional<ControllerArchive> decodeLegacyKArchive(const std::vector<uint8_t>&, std::string resourceHash);

}
