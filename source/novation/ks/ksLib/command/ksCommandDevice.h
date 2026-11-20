#pragma once

#include "../../../controller/command/commandAudioDevice.h"

namespace ks {
class Rom;

class KSCommandDevice final : public novation::controller::command::CommandAudioDevice {
public:
    KSCommandDevice(Format, Rom, std::vector<uint8_t> wave, std::vector<uint8_t> flashOrState);
};
}
