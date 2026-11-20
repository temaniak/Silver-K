#pragma once
#include "../../../controller/command/nativeArpeggiator.h"

namespace novation::controller::command {
// K-family melodic routing, plus KS chord extension. Each ROM rhythm
// entry has its own sounding and silent lengths.
struct KSArpPulseShape {
    uint8_t sounding=1,silent=1;
    static unsigned soundingGain(uint8_t value) noexcept {
        return value>=64?256:256-std::min(252u,unsigned(64-value)*4);
    }
    static unsigned tailGain(uint8_t value) noexcept {
        return value>=64?unsigned(value-64)*4:0;
    }
    uint16_t gate(uint8_t value) const noexcept {
        const unsigned total=unsigned(sounding)+silent;
        if(!total)return 0;
        const unsigned fraction=sounding*soundingGain(value)+silent*tailGain(value);
        return uint16_t(std::min(65535u,fraction*256/total));
    }
};

using KSArpRhythms=std::array<ArpRhythm,33>;

inline std::optional<KSArpRhythms> readKSArpRhythms(const uint8_t* image,size_t size) noexcept {
    constexpr size_t table=62704;
    constexpr uint32_t romBase=0xa00000;
    KSArpRhythms result{};
    if(!image || size<table+4*result.size())return {};
    for(size_t rhythm=0;rhythm<result.size();++rhythm) {
        const auto* entry=image+table+4*rhythm;
        uint32_t address=(uint32_t(entry[0])<<24)|(uint32_t(entry[1])<<16)|(uint32_t(entry[2])<<8)|entry[3];
        if(address>=romBase)address-=romBase;
        if(address>size || size-address<2)return {};
        const unsigned count=image[address];
        if(!count || size-address<2+2*count)return {};
        const auto* words=image+address+2;
        auto& parsed=result[rhythm];
        unsigned cursor=0,offset=0;
        // A sounding entry has a following duration and can absorb one
        // more rest. A rest at the loop seam also contributes to that gate.
        while(cursor<count) {
            const auto velocity=words[2*cursor],duration=words[2*cursor+1];
            ++cursor;
            if(!velocity) {offset+=duration;continue;}
            if(velocity>=128 || cursor==count || parsed.size==parsed.steps.size())return {};
            unsigned tail=words[2*cursor+1],advance=duration+tail;
            ++cursor;
            const unsigned peek=cursor%count;
            if(!words[2*peek]) {
                const auto rest=words[2*peek+1];
                tail+=rest;
                if(cursor<count) {++cursor;advance+=rest;}
            }
            if(!advance || offset+advance>UINT16_MAX)return {};
            parsed.steps[parsed.size++]={uint16_t(offset),duration,uint16_t(tail),velocity};
            offset+=advance;
        }
        if(!parsed.size || !offset)return {};
        parsed.units=uint16_t(offset);
    }
    return result;
}

inline std::optional<ArpSettings> readKSArp(const uint8_t* patch,size_t length,uint8_t channel,
                                           KSArpPulseShape shape={},const KSArpRhythms* rhythms=nullptr) noexcept {
    if(!patch || length<=167 || channel>15 || patch[87]>6 || patch[85]>15 || patch[86]>127 || patch[167]>127 ||
       (rhythms?patch[149]>=rhythms->size():patch[149]!=0))
        return {};
    const uint8_t flags=patch[88],route=flags&0x60;
    ArpSettings result{ArpPattern(patch[87]),patch[85],uint8_t((flags&3)+1),shape.gate(patch[86]),
        uint8_t(patch[167] && !rhythms?127:0),channel,bool(flags&4),bool(flags&8),bool(flags&0x10),!route || route==0x40,route!=0};
    if(rhythms) {
        result.rhythm=&(*rhythms)[patch[149]];
        result.soundingGain=uint16_t(KSArpPulseShape::soundingGain(patch[86]));
        result.tailGain=uint16_t(KSArpPulseShape::tailGain(patch[86]));
        result.rhythmVelocity=patch[167]!=0;
    }
    return result;
}
}
