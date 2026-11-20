#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
namespace ks::parameter {
// Each formula lists its complete numerical dependencies. No address, patch,
// part/voice assignment, table generation or rate is inferred from a key.
// Callers still observe every source (including table/base words) live.
enum class Formula:uint8_t{CurveProduct,Volume,ModulationIndex,FilterPrefix,FilterWord,GlobalShape,HeaderPart,HeaderMode,VoiceScale,TimeInput,GateMask,RateWord,ClockInput,PitchOctave,PitchCoarse,PitchSpread,PitchFine,PitchWheel,LfoRateInput,LfoWeighted,LfoFlags,GlobalDivision,GlobalTimeScale,GlobalRangeFirst,GlobalRangeSecond,GlobalMatrixShape,GlobalMatrixGain,GlobalRecurrence,GlobalMatrixCoefficient,GlobalClamp,GlobalMaster,GlobalEffect,Count};
using DependencyKey=std::array<uint32_t,8>;
struct DependencyValue{uint32_t first=0,second=0;uint8_t flags=0;};
constexpr int dependencySign8(uint8_t n){return n<128?int(n):int(n)-256;}
constexpr int dependencySign16(uint16_t n){return n<32768?int(n):int(n)-65536;}
inline DependencyValue calculate(Formula f,const DependencyKey&k){
 switch(f){
 case Formula::CurveProduct:{const auto factor=k[0]?-21845:21845;return {uint32_t(uint32_t(dependencySign16(uint16_t(k[1]))*factor)<<1)>>8,0,0};}
 case Formula::Volume:{const auto amount=uint8_t(k[0]),level=uint8_t(k[1]);const auto volume=uint16_t(unsigned(amount)*level*2);return {uint32_t(uint16_t(0x7fff-volume))<<8,uint32_t(uint16_t(unsigned(uint8_t(level^127))*181))<<8,0};}
 case Formula::ModulationIndex:{uint16_t sum=0;for(unsigned n=0;n<3;++n)sum=uint16_t(sum+uint16_t(dependencySign8(uint8_t(k[1+2*n]-64))*dependencySign8(uint8_t(k[2+2*n]))));const auto scaled=uint16_t(uint32_t(dependencySign16(sum)*512)>>16);const auto combined=uint16_t(uint16_t(dependencySign8(uint8_t(k[0]-64)))+scaled);const bool negative=combined&0x8000;auto magnitude=negative?uint16_t(0-combined):combined;const bool clamp=magnitude>64;if(clamp)magnitude=64;return {uint32_t(magnitude)*2,0,uint8_t(unsigned(negative)|(unsigned(clamp)<<1))};}
 case Formula::FilterPrefix:{const auto modulation=uint16_t(unsigned(uint16_t(dependencySign8(uint8_t(k[0]-64))*dependencySign8(uint8_t(k[1]-64))))*2);auto cutoff=uint16_t((unsigned(uint8_t(k[2]))<<7)+modulation);const bool zero=cutoff&0x8000;if(zero)cutoff=0;const bool maximum=cutoff>0x3f80;if(maximum)cutoff=0x3f80;const auto converted=uint32_t(uint16_t(unsigned(cutoff)*2))*0xaaab-0x48001400u;return {converted,0,uint8_t(unsigned(zero)|(unsigned(maximum)<<1))};}
 case Formula::FilterWord:return {uint32_t(k[0]+(uint32_t(uint16_t(k[1]))<<16))>>8,0,0};
 case Formula::GlobalShape:{const auto kind=k[0];const auto raw=uint8_t(k[1]);if(kind!=1){auto shaped=uint16_t(unsigned(kind==2?uint8_t(raw-64):raw)<<8);shaped=kind==0?uint16_t(shaped>>1):uint16_t(unsigned(shaped)*2);return {uint32_t(shaped)<<8,0,0};}const auto product=uint32_t(unsigned(raw)<<8)*0xcccd;auto captured=uint16_t(uint16_t(product>>16)-0x3f50);captured=uint16_t(captured+uint16_t(k[2]));const bool clamp=captured&0x8000;if(clamp)captured=0x7fff;return {(uint32_t(captured)<<8)|uint8_t(product>>8),product,uint8_t(clamp)};}
 case Formula::HeaderPart:{auto n=uint8_t(k[1]-k[2]);const bool clamp=n&128;if(clamp)n=0;return {k[0]==0?uint8_t(unsigned(n)*3+0x67):n,0,uint8_t(clamp)};}
 case Formula::HeaderMode:return {uint32_t((((uint8_t(k[0])>>6)&1)^1)*3+3),0,0};
 case Formula::VoiceScale:return {uint32_t(dependencySign8(uint8_t(k[0]))*256*int(uint8_t(k[1])>>2))>>8,0,0};
 case Formula::TimeInput:{const auto inverted=uint8_t(k[0])^127;return {uint16_t(unsigned(inverted)*uint8_t(k[1])+(inverted>uint8_t(k[2])?unsigned(uint8_t(inverted-uint8_t(k[2])))<<8:0)+uint16_t(k[3])),0,0};}
 case Formula::GateMask:{uint32_t mask=0;if(uint8_t(k[0])&&uint8_t(k[1])==uint8_t(k[2])){constexpr unsigned required[]{2,2,1,1,0,0,0};for(unsigned n=0;n<7;++n)if(uint8_t(k[3])!=required[n])mask|=1u<<n;}return {mask,0,0};}
 case Formula::RateWord:return {uint32_t(uint16_t(unsigned(uint8_t(k[0]))<<9))*0x8102,0,0};
 case Formula::ClockInput:return {uint16_t(unsigned(uint8_t(uint8_t(k[0])-(uint8_t(k[1])&7)-94))*170),0,0};
 case Formula::PitchOctave:{const auto octave=uint8_t(((uint8_t(k[1])>>(2*unsigned(k[2])))&3)-1);const auto delta=uint8_t(dependencySign8(octave)*12);return {uint16_t((unsigned(uint8_t((uint16_t(k[0])>>8)+delta))<<8)|(uint16_t(k[0])&255)),0,0};}
 case Formula::PitchCoarse:return {uint16_t((unsigned(uint8_t((uint16_t(k[0])>>8)+uint8_t(k[1]-64)))<<8)|(uint16_t(k[0])&255)),0,0};
 case Formula::PitchSpread:{const auto unison=uint8_t((uint8_t(k[1])&7)*4);const auto counter=uint8_t(((uint8_t(k[2])&7)*8+1)-unison);const auto divisor=uint8_t((unison+12)*2);const auto numerator=dependencySign16(uint16_t((unsigned(counter)<<8)|divisor));const auto quotient=uint8_t(numerator/dependencySign8(divisor));const auto product=uint16_t(dependencySign8(uint8_t(k[3]))*dependencySign8(quotient));return {uint16_t(uint16_t(k[0])+dependencySign8(uint8_t(product>>8))),0,0};}
 case Formula::PitchFine:{const auto word=uint16_t(unsigned(uint16_t(k[1]))*8);const auto product=uint32_t(dependencySign16(word)*20971);return {uint16_t(uint16_t(k[0])+(product>>16)),0,0};}
 case Formula::PitchWheel:{const auto product=uint32_t(dependencySign8(uint8_t(k[1]-64))*dependencySign16(uint16_t(k[2]-256)));return {uint32_t(uint16_t(uint16_t(k[0])+product))*0xaaab>>8,0,0};}
 case Formula::LfoRateInput:{const auto product=uint32_t(uint16_t(unsigned(uint8_t(k[0]))<<7))*0xaaab;return {uint16_t(uint16_t(product>>16)+0x4700+uint16_t(uint16_t(k[1])-0x4470)),0,0};}
 case Formula::LfoWeighted:{const auto rate=uint8_t(k[0])>32?32:uint8_t(k[0]);const auto weight=uint32_t(rate*4)<<8;const auto upper=uint32_t(k[1]>>16)*weight;const auto lower=uint32_t(uint16_t(k[1]))*weight;return {uint32_t(upper+(lower>>16))<<1,0,0};}
 case Formula::LfoFlags:{const auto mode=uint8_t(k[1]);return {(uint8_t(k[0])?0x40000000u:0u)+((mode==4||mode==5?1u:mode==2?2u:0u)<<8),0,0};}
 case Formula::GlobalDivision:{constexpr uint32_t n=0x28a1ea0;const auto d=uint16_t(k[0]);const uint32_t q=d?n/d:0;return {((n-q*d)<<16)|uint16_t(q),0,0};}
 case Formula::GlobalTimeScale:{const auto squared=uint16_t(unsigned(uint8_t(k[1]))*uint8_t(k[1]));return {uint32_t(uint16_t(k[0]))*squared,squared,0};}
 case Formula::GlobalRangeFirst:{const auto product=uint32_t(k[0]?uint8_t(k[2])*255u:0x1feu*uint16_t(k[1]));const auto scale=uint16_t(uint16_t(product>>16)<<1);return {k[0]?uint32_t(uint16_t(product>>1))<<16:uint32_t(scale)*uint8_t(k[2]),product,0};}
 case Formula::GlobalRangeSecond:{const auto product=uint32_t(k[0]?uint8_t(k[2])*255u:0x200u*uint16_t(k[1]));const auto scale=uint16_t(uint16_t(product>>16)<<2);auto candidate=k[0]?product<<16:uint32_t(scale)*uint8_t(k[2]);const bool low=candidate<k[3];if(low)candidate=k[3];const auto upper=uint32_t(k[0]?0x7fff0000u:0x30000u)-k[3];const bool high=candidate>upper;if(high)candidate=upper;return {candidate,upper,uint8_t(unsigned(low)|(unsigned(high)<<1))};}
 case Formula::GlobalMatrixShape:{const auto product=uint16_t(unsigned(uint8_t(k[0]))*uint8_t(k[1]));return {uint16_t((unsigned(uint8_t((product>>8)+uint8_t(k[2])))<<8)|uint8_t(product)),0,0};}
 case Formula::GlobalMatrixGain:{const auto gain=uint16_t(unsigned(uint8_t(k[0]))*128+0x7fff);return {0x599au*gain,gain,0};}
 case Formula::GlobalRecurrence:{const auto product=uint32_t(uint16_t(uint16_t(k[0])<<1))*uint16_t(k[1]);return {uint16_t(uint16_t(product>>16)+uint16_t(k[2])),product,0};}
 case Formula::GlobalMatrixCoefficient:{const auto product=uint32_t(uint16_t(uint16_t(k[0])<<1))*uint16_t(k[1]);return {uint32_t(uint16_t(product>>16))*256,product,0};}
 case Formula::GlobalClamp:{const auto product=uint32_t(unsigned(uint8_t(uint8_t(k[0])<<1))*175)*uint16_t(k[1]);auto value=uint16_t(product>>16);const bool low=value<4;if(low)value=4;const bool high=value>0x7ff8;if(high)value=0x7ff8;return {value,product,uint8_t(unsigned(low)|(unsigned(high)<<1))};}
 case Formula::GlobalMaster:return {(uint32_t(uint8_t(uint8_t(k[0])<<1))<<8)*0x8102,0,0};
 case Formula::GlobalEffect:return {uint16_t(unsigned(uint8_t(k[0]))*230+0x666),uint16_t(unsigned(uint8_t(uint8_t(k[1])+104))*170),0};
 default:throw std::logic_error("Invalid parameter dependency formula");
 }
}
static_assert(unsigned(Formula::Count)<=32,"Dependency mask capacity");
class DependencyCache final {
public:
 DependencyValue resolve(Formula f,const DependencyKey&key){if(f>=Formula::Count)throw std::logic_error("Invalid parameter dependency key");used|=1u<<unsigned(f);auto&e=entries_[unsigned(f)];if(!e.valid||e.key!=key){e.key=key;e.value=calculate(f,key);e.valid=true;++changed;}else ++reused;return e.value;}
 void invalidate(){for(auto&e:entries_)e.valid=false;++epoch;}
 uint64_t changed=0,reused=0,epoch=0;uint32_t used=0;
private:struct Entry{DependencyKey key{};DependencyValue value{};bool valid=false;};std::array<Entry,unsigned(Formula::Count)>entries_{};
};
inline DependencyValue resolve(DependencyCache*cache,Formula f,const DependencyKey&key){return cache?cache->resolve(f,key):calculate(f,key);}
}
