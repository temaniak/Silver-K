#include "kPolyNoteImage.h"
#include "kDspCoefficient.h"
#include <algorithm>
#include <cstdlib>

namespace novation::controller::command
{
    namespace
    {
        constexpr unsigned bipolarOffsets[]{0x0b,0x13,0x1b,0x34};
        constexpr int32_t signed8(uint8_t value) noexcept { return value<128?value:int32_t(value)-256; }
        constexpr int32_t signed16(uint16_t value) noexcept { return value<32768?value:int32_t(value)-65536; }
        constexpr int32_t bipolar(uint8_t raw) noexcept { return signed8(uint8_t(raw-64)); }
        constexpr uint32_t dspWord(uint32_t fixed) noexcept { return fixed>>8; }
        constexpr uint16_t doubledLevel(uint8_t raw) noexcept { return uint16_t((uint16_t(raw)<<8)|uint8_t(raw<<1)); }
        constexpr uint32_t squareLevel(uint8_t raw) noexcept
        {
            const uint32_t expanded=doubledLevel(raw);
            return dspWord((expanded*expanded)<<1);
        }
        constexpr uint32_t linearLevel(uint8_t raw) noexcept { return uint32_t(raw)<<14; }
        constexpr uint32_t scaledLevel(uint8_t raw) noexcept { return dspWord(uint32_t(raw)*512u*0x8102u); }

        constexpr uint32_t envelopeWord(uint8_t value,uint8_t slope,uint8_t threshold,uint16_t tuning,
                                        unsigned downshift=0) noexcept
        {
            const auto inverse=uint8_t(value^0x7f);
            const unsigned correction=inverse>threshold?unsigned(inverse-threshold)*256u:0u;
            return dspWord(kDspCoefficient(uint16_t(inverse*slope+correction+tuning))>>downshift);
        }

        constexpr uint8_t velocityProduct(uint8_t a,uint8_t b) noexcept { return uint8_t(uint16_t(a*b*2u)>>8); }
        uint8_t shapeVelocity(uint8_t raw,uint8_t exponent,uint8_t curve) noexcept
        {
            uint8_t value=raw;
            // Encoded exponent zero is the full unsigned-counter range.
            const unsigned powers=exponent?exponent:256;
            for(unsigned power=1;power<powers && value;++power)value=velocityProduct(raw,value);
            const auto inverse=uint8_t(value^0x7f);
            if(curve==1)return inverse;
            if(curve==2)return velocityProduct(value,value);
            if(curve==3)return velocityProduct(value,inverse);
            return value;
        }

        constexpr uint32_t velocityBlend(uint8_t velocity,uint8_t depth) noexcept
        {
            const auto amount=bipolar(depth);
            const auto input=amount<0?uint8_t(velocity^0x7f):velocity;
            const auto magnitude=signed8(uint8_t(amount<0?-amount:amount));
            const auto result=uint16_t(signed8(uint8_t(input+0x81))*magnitude*4+0x7f00);
            return uint32_t(result)<<8;
        }

        constexpr uint32_t oscillatorControl(uint8_t raw,unsigned index) noexcept
        {
            const auto value=uint8_t(raw-64);
            auto packed=uint16_t(uint16_t(value)<<8 | (value==63?255:0));
            if(index!=0)packed=uint16_t(packed<<1);
            const auto tail=index!=0?uint8_t((packed>>8)-uint8_t(packed)):uint8_t(packed);
            return uint32_t(packed)<<8 | tail;
        }

        constexpr int unisonSpread(uint8_t extra,uint8_t unit,uint8_t depth) noexcept
        {
            if(!extra)return 0;
            const int span=4*extra;
            const int divisor=2*(span+12);
            const int position=(8*unit+1-span)*256+divisor;
            const int product=signed8(depth)*(position/divisor);
            return signed8(uint8_t(uint16_t(product)>>8));
        }

        uint32_t oscillatorPitch(const KExpressionSource& s,const KVoiceTunings& t,unsigned lane,uint8_t unit) noexcept
        {
            const auto& p=s.patch;
            const int coarse=12*bipolar(p[0x87+lane])+uint8_t(p[7+8*lane]-64);
            const int key=uint8_t(p[0xc2]+p[8+8*lane])+t.finePitch-0xc0+
                unisonSpread(p[KUnisonOffset],unit,p[1]);
            const auto fine=uint16_t(uint32_t(signed16(uint16_t(key*8))*0x51eb)>>16);
            const int bend=signed16(uint16_t(t.pitchBend-0x100u))*bipolar(p[9+8*lane]);
            const auto total=uint16_t(t.oscillatorPitch+coarse*256+fine+bend);
            return dspWord(uint32_t(total)*0xaaabu);
        }

        constexpr uint32_t bipolarWord(int amount,int16_t magnitude) noexcept
        {
            return dspWord(uint32_t(int32_t(magnitude)*(amount<0?-0x5555:0x5555))<<1);
        }

        constexpr uint32_t envelopeScale(uint32_t converted,uint8_t factor) noexcept
        {
            return uint32_t((uint64_t(converted)*factor)>>8);
        }

        struct PitchUpdate
        {
            std::array<uint32_t,4> words;
            uint8_t glideNote;
        };

        PitchUpdate pitchUpdate(const KExpressionSource& s,const KVoiceTunings& t,uint8_t note,
                                uint32_t previousPitch,bool legato) noexcept
        {
            const auto& p=s.patch;
            const auto transposedNote=uint8_t(note+t.partTranspose);
            const auto oldNote=uint8_t((previousPitch>>16)+60);
            const int transpose=bipolar(p[4]);
            const auto distance=uint8_t(transpose?std::abs(transpose):std::abs(int(transposedNote)-int(oldNote)));
            const bool autoGlide=p[KVoiceModeOffset]==uint8_t(KVoiceMode::MonoAutoglide);
            const auto inverse=uint8_t((autoGlide&&!legato?0:p[3])^0x7f);
            const auto glideInput=uint16_t(inverse*0x5du+(inverse>0x76?unsigned(inverse-0x76)*256u:0u)+t.portamento);
            const auto glide=kDspCoefficient(glideInput);
            const bool scaled=(p[5]&0x40) && distance;
            const auto glideNote=transpose?uint8_t(transposedNote+transpose):t.retainedGlideNote;
            return {{uint32_t(uint8_t(transposedNote+0xc4))<<16,
                     scaled?dspWord(std::max(0x200u,envelopeScale(glide,distance))):dspWord(glide>>5),
                     scaled?0:dspWord(glide),uint32_t(uint8_t(glideNote+0xc4))<<16},glideNote};
        }

        uint32_t noteTrigger(const KExpressionSource& s,bool legato) noexcept
        {
            const auto& p=s.patch;
            const uint32_t mode=uint32_t(legato?p[0x4e]&3:p[0x50]|3u)<<8;
            const uint32_t flags=legato?(p[0x50]&7u)|0xb8u:0xbfu;
            return (uint32_t(p[0x4f])<<16)|mode|flags|(p[4]!=64?0x40u:0u);
        }
    }

    bool validKPolyNoteInput(const KExpressionSource& s,
        const KVoiceTunings& t,VoiceSlot slot,uint8_t note,uint8_t velocity,uint32_t previousPitch,KVoiceStart start) noexcept
    {
        const auto& p=s.patch;
        if(size_t(slot)>=t.slotScales.size() || note>=128 || (!velocity&&!start.velocityIsShaped) ||
           velocity>=128 || previousPitch>0xffffff ||
           !supportsKNote(p[KVoiceModeOffset],p[KUnisonOffset],start.unit) || t.velocityCurve>3)return false;
        for(const auto offset:bipolarOffsets)if(std::abs(bipolar(p[offset]))>64)return false;
        return p[0x49]<=t.lfoRates.size() && p[0x4c]<=t.lfoRates.size();
    }

    std::optional<KPolyNoteImage> compileKPolyNoteImage(const KExpressionSource& s,
        const KVoiceTunings& t,VoiceSlot slot,uint8_t note,uint8_t velocity,uint32_t previousPitch,KVoiceStart start) noexcept
    {
        if(!validKPolyNoteInput(s,t,slot,note,velocity,previousPitch,start))return {};
        const auto& p=s.patch;
        KPolyNoteImage image;
        image.velocity=start.velocityIsShaped?velocity:shapeVelocity(velocity,uint8_t(p[0xc3]+2),t.velocityCurve);
        size_t count=0;
        const auto append=[&](uint32_t value) {
            if(count<image.words.size())image.words[count]=value&0xffffffu;
            ++count;
        };
        const auto modulation=compileKModulationDepths(s);
        const auto appendModulation=[&](unsigned lane){
            const auto amount=bipolar(p[bipolarOffsets[lane]]);
            append(bipolarWord(amount,s.modulationCurve[size_t(std::abs(amount))]));
            append(modulation[lane]);
            const unsigned base=lane==3?0x30:0x1f;
            for(unsigned i=0;i<3;++i)append(bipolarWord(1,int16_t(bipolar(p[base+i])*256)));
        };
        for(unsigned lane=0;lane<3;++lane) {
            append(p[0x83+lane]);
            constexpr unsigned controls[]{0x0c,0x0e,0x0d};
            for(unsigned i=0;i<3;++i)append(oscillatorControl(p[8*lane+controls[i]],i));
            append(oscillatorPitch(s,t,lane,start.unit));
            appendModulation(lane);
            append(uint32_t(signed8(t.slotScales[size_t(slot)][lane])*(p[2]>>2)));
        }
        for(const auto offset:{0x2a,0x27,0x26,0x25,0x28,0x29})append(linearLevel(p[offset]));
        append(uint32_t(uint16_t(0x7fff-2u*p[0x2d]*p[0x2c]))<<8);
        append(uint32_t(uint16_t((p[0x2c]^0x7f)*0xb5u))<<8);
        append(kFilterCutoffWord(p[KCutoffPatchOffset],t.filterPitch));
        appendModulation(3);

        append(scaledLevel(p[0x2f]));
        append(uint32_t(p[0x2b])<<16);
        append(p[0x82]);
        append(squareLevel(p[0x38]));
        const int fifth=bipolar(p[0x39]);
        append(uint32_t(uint16_t((fifth<0?-1:1)*fifth*fifth*8))<<8);

        const auto attack=[&](unsigned offset){return envelopeWord(p[offset],0x6b,0x77,t.attack);};
        const auto envelope=[&](unsigned offset){return envelopeWord(p[offset],0x6b,0x7f,t.envelope);};
        append(velocityBlend(image.velocity,p[0x3a]));
        append(attack(0x3b));
        append(envelope(0x3c));
        append(velocityBlend(image.velocity,p[0x3d]));
        append(attack(0x3e));
        append(envelope(0x3f));
        append(envelope(0x41));
        append(squareLevel(p[0x40]));
        const auto tracking=uint8_t((p[0x7d]?uint8_t(p[0x7d]-p[KUnisonOffset]):0x40)+0x74);
        append(dspWord(kDspCoefficient(uint16_t(tracking*0xaau))));
        append(velocityBlend(image.velocity,p[0x42]));
        append(attack(0x43));
        append(envelope(0x44));
        append(envelope(0x46));
        append(scaledLevel(p[0x45]));

        for(unsigned lfo=0;lfo<2;++lfo) {
            const auto mode=p[0x49+3*lfo];
            if(mode) {
                append(0);
                append(dspWord(t.lfoRates[mode-1]));
            } else {
                const auto rate=p[0x48+3*lfo];
                const auto converted=kDspCoefficient(uint16_t((uint32_t(rate)*128u*0xaaabu>>16)+0x4700u));
                append(dspWord(envelopeScale(converted,uint8_t(std::min(rate,uint8_t(32))*4u))<<1));
                append(0);
            }
            const auto shape=p[0x8b+lfo];
            append(uint8_t((shape^3)+1));
            append(shape==2?0:0x400000);
            append(envelopeWord(p[0x4a+3*lfo],0x50,0x73,t.lfo,1));
        }

        const auto pitch=pitchUpdate(s,t,note,previousPitch,start.legato);
        for(const auto word:pitch.words)append(word);
        image.glideNote=pitch.glideNote;
        if(count!=image.words.size())return {};

        image.mode=p[0x8a];
        image.level=scaledLevel(p[0x2b]);
        image.trigger=noteTrigger(s,start.legato);
        return image;
    }

    std::optional<KRepeatNoteUpdate> compileKRepeatNoteUpdate(const KExpressionSource& s,
        const KVoiceTunings& t,uint8_t note,uint8_t velocity,uint32_t previousPitch,KVoiceStart start) noexcept
    {
        const auto& p=s.patch;
        if(!supportsKNote(p[KVoiceModeOffset],p[KUnisonOffset],start.unit) ||
           p[KVoiceModeOffset]==uint8_t(KVoiceMode::Poly) ||
           note>=128 || !velocity || velocity>=128 || previousPitch>0xffffff || t.velocityCurve>3)return {};
        const auto shaped=shapeVelocity(velocity,uint8_t(p[0xc3]+2),t.velocityCurve);
        const auto pitch=pitchUpdate(s,t,note,previousPitch,start.legato);
        const auto trigger=noteTrigger(s,start.legato || p[KVoiceModeOffset]==uint8_t(KVoiceMode::PolyRetrigger));
        return KRepeatNoteUpdate{{velocityBlend(shaped,p[0x3a]),velocityBlend(shaped,p[0x3d]),velocityBlend(shaped,p[0x42])},
                                 pitch.words,trigger,shaped,pitch.glideNote};
    }

    std::array<uint32_t,3> compileKOscillatorPitches(const KExpressionSource& source,const KVoiceTunings& t,
                                                   uint8_t unit) noexcept
    { return {oscillatorPitch(source,t,0,unit),oscillatorPitch(source,t,1,unit),oscillatorPitch(source,t,2,unit)}; }
}
