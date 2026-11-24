#pragma once
#include <cstdint>
#include <optional>
namespace novation::controller::command {
enum class KSPanicAction { Ignore, AllNotesOff, AllSoundOff };
inline std::optional<KSPanicAction> ksPanicControl(uint8_t controller,uint8_t value)noexcept {
    if(value>=128)return std::nullopt;
    if(controller==120)return value?KSPanicAction::Ignore:KSPanicAction::AllSoundOff;
    if(controller>=123 && controller<=127)
        return controller==126 || !value?KSPanicAction::AllNotesOff:KSPanicAction::Ignore;
    return std::nullopt;
}
}
