#pragma once

#include "kProgramImage.h"

namespace novation::controller::command {
struct KParameterValue {uint8_t offset,value;};

// The only firmware inputs here are the two MIDI lookup tables. No live
// controller memory, register state or execution phase is consulted.
class KMidiParameters final {
    struct PackedField {
        uint8_t offset=0xff,mask=0,shift=0;
    };
    struct Multiplex {
        uint8_t selectorShift=0,limit=0;
        std::array<PackedField,64> fields{};
    };
public:
    bool bind(const uint8_t* firmware,size_t bytes) noexcept {
        constexpr size_t mapping=0xbcc6,selectors=0xbdb2;
        if(!firmware || bytes<selectors+16)return false;
        std::copy_n(firmware+mapping,ordinary_.size(),ordinary_.begin());
        for(size_t i=0;i<packed_.size();++i) {
            const auto d=firmware+selectors+4*i;
            const uint32_t cursor=(uint32_t(d[0])<<24)|(uint32_t(d[1])<<16)|(uint32_t(d[2])<<8)|d[3];
            if(cursor>bytes-4)return false;
            auto& group=packed_[i];
            const auto header=firmware+cursor;
            group.selectorShift=header[0];group.limit=header[1];
            if(group.selectorShift>7 || group.limit>2*group.fields.size() || bytes-cursor-4<2*((group.limit+1)/2))return false;
            for(size_t j=0;j<(size_t(group.limit)+1)/2;++j) {
                const unsigned shift=header[4+2*j];
                const auto offset=header[5+2*j];
                if(shift>7 || offset>=KPackedProgramBytes)return false;
                group.fields[j]={offset,uint8_t(header[2]<<shift),uint8_t(shift)};
            }
        }
        return true;
    }

    std::optional<KParameterValue> control(uint8_t number,uint8_t value) const noexcept
    {return number<128?mapped(number,value):std::nullopt;}

    std::optional<KParameterValue> nrpn(uint8_t number,uint8_t value,const KExpressionSource& source) const noexcept {
        if(number<=50)return mapped(128+number,value);
        if(number<104 || number>107 || value>=128)return {};
        const auto& group=packed_[number-104];
        const unsigned selector=value>>group.selectorShift;
        // The exclusive limit is not a packed field. The reference's
        // boundary path does not modify the sounding program.
        if(selector>=group.limit)return {};
        const auto& field=group.fields[selector/2];
        if(field.offset>=KPackedProgramBytes)return {};
        return KParameterValue{field.offset,uint8_t((source.patch[field.offset]&~field.mask)|((value<<field.shift)&field.mask))};
    }
private:
    std::optional<KParameterValue> mapped(size_t index,uint8_t value) const noexcept {
        const auto offset=ordinary_[index];
        if(offset==0xff || value>=128)return {};
        return KParameterValue{offset,value};
    }
    std::array<uint8_t,179> ordinary_{};
    std::array<Multiplex,4> packed_{};
};

inline KExpressionSource editKProgram(const KExpressionSource& source,KParameterValue edit) noexcept {
    auto next=source;
    next.patch[edit.offset]=edit.value;
    return edit.offset<KPackedProgramBytes?decodeKProgram(next.patch.data(),source):next;
}
}
