#pragma once

#include "kDspCoefficient.h"
#include "kPolyNoteImage.h"
#include "../kstationPackedProgramFields.h"
#include <stdexcept>
#include <vector>

namespace novation::controller::command {
inline constexpr size_t KPackedProgramBytes=128;

inline KExpressionSource decodeKProgram(const uint8_t* record,const KExpressionSource& current) noexcept {
    auto decoded=current;
    std::copy_n(record,KPackedProgramBytes,decoded.patch.begin());
    uint8_t fieldByte=0;
    unsigned previous=KPackedProgramBytes;
    for(const auto& field:kstation::PackedProgramFields) {
        if(field.packed!=previous)fieldByte=record[field.packed];
        previous=field.packed;
        fieldByte=uint8_t(fieldByte>>field.shift);
        const auto masked=uint8_t(fieldByte&field.mask);
        decoded.patch[field.decoded]=uint8_t(masked+field.bias);
        if(field.direct)fieldByte=masked;
    }
    const auto transpose=record[0x7e];
    decoded.patch[0xad]=uint8_t((transpose>>3)-((transpose&0x40)?16:0));
    return decoded;
}

struct KProgramTables {
    uint16_t clock=0;
    std::array<std::array<uint16_t,10>,6> reverb{};
    std::array<std::array<uint16_t,2>,64> delayRatios{};
    std::array<uint32_t,34> syncRates{}, delaySyncRates{};
};

inline KProgramTables readKProgramTables(const std::vector<uint8_t>& firmware) {
    const auto word = [](const std::vector<uint8_t>& data, size_t offset) {
        return uint16_t(data.at(offset) * 256u + data.at(offset + 1));
    };
    KProgramTables tables;
    tables.clock = word(firmware, 0xbf98);
    if (!tables.clock)
        throw std::runtime_error("K program tables require a DSP clock");
    const auto syncClock = (33554u * tables.clock * 2u) >> 16;
    const auto delayClock = uint16_t(0x3e800000u / tables.clock);
    for (size_t i = 0; i < tables.syncRates.size(); ++i) {
        const auto coefficient = word(firmware, 0xa812 + 2 * i);
        const auto divisor = uint16_t(coefficient << 4);
        tables.syncRates[i] = divisor ? uint32_t((uint64_t(delayClock) << 16) / divisor) * 2u : 0;
        tables.delaySyncRates[i] = uint16_t(coefficient << 5) * syncClock;
    }
    for (size_t i = 0; i < tables.reverb.size(); ++i)
        for (size_t j = 0; j < tables.reverb[i].size(); ++j)
            tables.reverb[i][j] = word(firmware, 0xc078 + 20 * i + 2 * j);
    for (size_t i = 0; i < tables.delayRatios.size(); ++i)
        for (size_t j = 0; j < tables.delayRatios[i].size(); ++j)
            tables.delayRatios[i][j] = word(firmware, 0xc0f0 + 4 * i + 2 * j);
    return tables;
}

struct KProgramGlobals {
    static constexpr uint16_t FirstFxAddress=0x03ba;
    std::array<uint32_t,49> fx{}; // 03ba..03ea
    std::array<uint32_t,2> routing{};
    std::array<uint32_t,2> pan{};
    uint32_t voiceCount=8;
    uint32_t unison=0,split=0,splitAux=0,level=0;
};

inline uint32_t kProgramVoiceCount(const KExpressionSource& source) noexcept
{return source.patch[0x59]?6:8;}

inline KProgramGlobals compileKProgramGlobals(const KExpressionSource& source,const KVoiceTunings& tunings,
                                             const KProgramTables& tables) noexcept {
    const auto& p=source.patch;
    const auto clock=tables.clock;
    const auto q8=[](uint8_t v) {return uint32_t(v)<<16;};
    const auto biased=[](uint8_t v) {return uint8_t(v-64);};
    const auto doubled=[](uint8_t v) {return uint16_t(uint16_t(v)<<9);};
    const auto scale=[&](uint8_t v) {return (uint32_t(doubled(v))*0x8102u)>>8;};
    const auto complement=[](uint8_t v) {
        v^=0x7f;
        return uint32_t(v)*0x10000u+uint32_t(uint8_t(v<<1))*256u;
    };
    KProgramGlobals result;
    auto& fx=result.fx;
    size_t next=0;
    const auto emit=[&](uint32_t word) {fx[next++]=word&0xffffff;};
    const auto rate=[&](uint8_t free,uint8_t sync) {
        const auto numerator=clock?uint16_t(0x028a1ea0u/clock):0;
        emit(sync?0:(uint32_t(numerator)*free*free)>>8);
        emit(sync && sync<=tables.syncRates.size()?tables.syncRates[sync-1]>>8:0);
    };

    // Distortion and expression depths, followed by the EQ oscillator and filter.
    emit(scale(p[0x61]));
    const auto distortionWheel=biased(p[0x62]);
    const auto wheelWord=uint16_t((uint16_t(distortionWheel)<<8)|(distortionWheel==63?255:0));
    const auto doubledWheel=uint16_t(wheelWord<<1);
    emit((uint32_t(doubledWheel)<<8)|uint8_t((doubledWheel>>8)-uint8_t(doubledWheel)));
    const auto compensation=uint16_t(uint16_t(uint8_t(p[0x63]+0x80)>>1)<<8);
    emit(uint32_t(compensation-(compensation>>4))<<8);
    for(size_t offset=0x51;offset<=0x53;++offset)emit(uint32_t(doubled(biased(p[offset])))<<8);
    rate(p[0x5e],p[0x5f]);
    emit(uint32_t(p[0x60])<<15);
    const auto eqFrequency=uint32_t(p[0x5d])*256u*0xcccdu;
    const auto eqHigh=uint16_t((eqFrequency>>16)-0x3f50u+tunings.filterPitch);
    emit((uint32_t(eqHigh)<<8)|((eqFrequency>>8)&0xffu));
    emit(uint32_t(doubled(biased(p[0x5c])))<<8);
    rate(p[0x70],p[0x71]);
    const auto chorusMode=p[0x92];
    emit(chorusMode);
    const uint32_t low=chorusMode?uint32_t(p[0x73])*255u/2u<<16:
        ((0x1feu*clock)>>16)*2u*p[0x73];
    const uint32_t high=chorusMode?uint32_t(p[0x74])*255u<<16:
        ((0x200u*clock)>>16)*4u*p[0x74];
    const auto minimum=low+(chorusMode?0x1000000u:0x800u);
    const auto maximum=(chorusMode?0x7fff0000u:0x30000u)-minimum;
    emit(low>>8);
    emit(std::min(std::max(high,minimum),maximum)>>8);
    const auto chorusFeedback=doubled(biased(p[0x72]));
    const int32_t signedFeedback=chorusFeedback<0x8000?chorusFeedback:int32_t(chorusFeedback)-0x10000;
    emit((uint32_t(signedFeedback*0x7333)<<1)>>8);
    emit(q8(p[0x6e]));
    emit(uint32_t(doubled(biased(p[0x6f])))<<8);
    emit(chorusMode);

    // Reverb send, decay and twelve delay-line coefficients.
    emit(q8(p[0x6b]));
    emit(uint32_t(doubled(biased(p[0x6c])))<<8);
    const auto& reverb=tables.reverb[std::min<uint8_t>(p[0x91],5)];
    const auto decay=uint16_t((reverb[0]&0xff00)+uint8_t(reverb[0])*p[0x6d]);
    emit(uint32_t(decay)<<8);
    emit(0x599a00);
    const auto target=uint16_t(decay*2);
    uint32_t root=0;
    for(unsigned bit=128;bit;bit>>=1) {
        const auto candidate=root+bit;
        if(candidate*candidate<=target)root=candidate;
    }
    emit((0x599au*(0x7fffu+128u*root))>>8);
    uint16_t line=reverb[1];
    for(size_t i=0;i<12;++i) {
        if(i>=6)line=reverb[i-2];
        const auto doubledLine=uint16_t(line<<1);
        emit((uint32_t(doubledLine)*clock)>>16);
        line=uint16_t(((uint32_t(doubledLine)*reverb[2])>>16)+reverb[3]);
    }

    // Stereo delay, followed by the pan oscillator and position.
    const auto delaySync=p[0x67];
    const auto delayTime=(uint32_t(uint8_t(p[0x66]<<1))*0xafu*clock)>>16;
    emit(delaySync?0:std::clamp(delayTime,4u,0x7ff8u));
    emit(delaySync && delaySync<=tables.delaySyncRates.size()?tables.delaySyncRates[delaySync-1]>>8:0);
    for(auto ratio:tables.delayRatios[p[0x6a]&63u])emit(uint32_t(ratio)<<7);
    emit(q8(p[0x64]));
    emit(uint32_t(doubled(biased(p[0x65])))<<8);
    emit(q8(p[0x68]));
    emit(complement(p[0x69]));
    rate(p[0x76],p[0x77]);
    emit(scale(p[0x78]));
    const auto position=biased(p[0x75]);
    auto panWord=uint16_t(position)<<8;
    const auto positiveTail=uint8_t(position<<2);
    if(position<128 && positiveTail)panWord|=positiveTail|3;
    panWord=uint16_t(panWord<<1);
    emit((uint32_t(panWord)<<8)|uint8_t(panWord));

    result.routing={(p[0x7b]&0x40)?0x7fff00u:0u,
                    (p[0x7b]&0x20)?uint32_t(p[0xc6]*0xe6u+0x666u)<<8:0u};
    const auto outputPan=kDspCoefficient(uint16_t(uint8_t(p[0xc5]+0x68)*0xaau))>>8;
    result.pan[p[0xc4]?1:0]=outputPan;
    result.voiceCount=kProgramVoiceCount(source);
    result.unison=scale(p[0x59]);
    if(p[0x7b]&8)result.splitAux=q8(p[0x5b]);
    else result.split=q8(p[0x5b]);
    result.level=complement(p[0x5a]);
    return result;
}
}
