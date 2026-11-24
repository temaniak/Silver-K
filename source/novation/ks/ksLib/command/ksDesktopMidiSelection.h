#pragma once

#include "../../../controller/command/controllerEvent.h"
#include <array>
#include <optional>

namespace novation::controller::command {
// The desktop MIDI boundary retains an external selector for each channel.
// It is independent of selectors inside the routed musical parts. Supply only
// the original raw control here, before any expression/controller aliasing.
class KSDesktopMidiSelection final {
public:
    bool prepareCold(const std::array<uint8_t,16>& selectors)noexcept {
        for(auto value:selectors)if(value>=128)return false;
        selectors_=selectors;return true;
    }
    bool accept(ControlChange raw)noexcept {
        if(raw.channel>=selectors_.size() || raw.controller>=128 || raw.value>=128)return false;
        if(raw.controller==98)selectors_[raw.channel]=raw.value;
        return true;
    }
    std::optional<uint8_t> selector(uint8_t channel)const noexcept {
        return channel<selectors_.size()?std::optional{selectors_[channel]}:std::nullopt;
    }
private:
    std::array<uint8_t,16> selectors_{};
};
}
