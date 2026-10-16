#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ks {
inline constexpr unsigned PartCount=4, OutputCount=4;
using Program=std::array<uint8_t,256>;
struct State {
    std::array<Program,PartCount> parts{};
    bool performance=false;
    uint8_t globalChannel=0, activeMask=1;
};
inline std::array<uint8_t,5> singleSelection(const uint8_t* program,uint8_t channel){
    const auto bank=program[187];const bool drum=bank>=5 && bank<=8 && program[188]<50;
    return {uint8_t(0xb0|(channel&15)),32,uint8_t(drum?32+bank:33),uint8_t(0xc0|(channel&15)),uint8_t(drum?program[188]:0)};
}
// Canonical SysEx packing of the controller's unpacked editing mirrors.
// OS 2.1.02: unpack 0x6726, pack 0x687e, performance serialization 0x71ba.
inline void packMirrors(Program& p) {
    p[0]=(p[210]&7)|((p[209]&3)<<3)|((p[211]&3)<<5);
    p[88]=(p[214]&3)|((p[215]&1)<<2)|((p[212]&1)<<3)|((p[213]&1)<<4)|((p[216]&3)<<5);
    p[121]=(p[217]&7)|((p[218]&3)<<3);
    p[122]=(p[219]&3)|((p[220]&3)<<2)|((p[221]&3)<<4);
    p[127]=(p[222]&3)|((p[223]&3)<<2)|((p[224]&3)<<4)|((p[225]&1)<<6);
    p[126]=(p[226]&7)|((p[227]<<3)&0x78);
    std::fill(p.begin()+209,p.end(),0);
}
inline std::vector<uint8_t> stateMessage(const State& state) {
    std::vector<uint8_t> message{0xf0,0,0x20,0x29,1,0x42,0x7f,uint8_t(state.performance?2:0),0,0x11,2,0,0};
    for(unsigned part=0;part<(state.performance?PartCount:1);++part)
        message.insert(message.end(),state.parts[part].begin(),state.parts[part].end());
    message.push_back(0xf7);return message;
}
// Current-program M=0 applies synthesis bytes 0..178. Live part metadata is
// restored with the original addressed CC/NRPN, including in Single mode.
inline std::array<uint8_t,57> singleMetadata(const uint8_t* p,uint8_t channel) {
    std::array<uint8_t,57> bytes{};size_t n=0;const auto status=uint8_t(0xb0|(channel&15));
    auto cc=[&](uint8_t number,uint8_t value){bytes[n++]=status;bytes[n++]=number;bytes[n++]=value;};
    cc(7,p[179]);for(unsigned offset:{180,182,183,184,185,186}){cc(99,0);cc(98,uint8_t(offset-95));cc(6,p[offset]);}
    return bytes;
}
}
