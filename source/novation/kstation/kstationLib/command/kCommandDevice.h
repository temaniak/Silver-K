#pragma once

#include "../../../controller/command/commandAudioDevice.h"

namespace kstation {
class Rom;

class KCommandDevice final : public novation::controller::command::CommandAudioDevice {
public:
    KCommandDevice(Format, Rom, std::vector<uint8_t> archive = {});
};
}
