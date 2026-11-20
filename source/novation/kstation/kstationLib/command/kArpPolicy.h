#pragma once
#include "../../../controller/command/nativeArpeggiator.h"
#include "kExpressionPacket.h"

namespace novation::controller::command {
inline std::optional<ArpSettings> readKArp(const KExpressionSource& source,uint8_t channel) noexcept {
    const auto& p=source.patch;
    if(p[0x57]>5 || p[0x55]>15 || p[0x56]>127 || channel>15)return {};
    const uint8_t flags=p[0x58],route=flags&0x60;
    const unsigned gate=std::min(255u,13u+2u*((312u*p[0x56])/256u));
    return ArpSettings{ArpPattern(p[0x57]),p[0x55],uint8_t((flags&3)+1),uint16_t(gate*256),
        0,channel,bool(flags&4),bool(flags&8),bool(flags&0x10),!route || route==0x40,route!=0};
}
}
