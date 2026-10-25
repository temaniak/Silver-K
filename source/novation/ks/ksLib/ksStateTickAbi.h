#pragma once
#include "ksStateTickService.h"
#include "ksStateEncoderAbi.h"
#include "ksPanelPedalOutputAbi.h"
#include "ksStateRefreshAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// Fold completed semantic results once, at the external caller return.
// No ABI image participates in the suspended panel transaction.
template<class A>void projectStateTick(A&a,const StateTickService&s){
 if(!s.complete())throw std::logic_error("Incomplete tick projection");
 const auto&v=s.summary();const auto nz8=[](uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));};
 const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};
 const auto nz32=[](uint32_t x){return uint8_t((x&0x80000000u?8:0)|(!x?4:0));};
 a.values[6]=(uint32_t(v.tick)<<16)|v.elapsed;a.values[5]=(a.values[5]&0xffff0000u)|v.last;
 a.flags=controlArithmetic::subtract(int16_t(v.elapsed),int16_t(25),a.flags).flags;
 if(!v.due)return;
 if(!s.buttons()||!s.adc()||!s.encoder()||!s.pedal()||!s.refresh())throw std::logic_error("Missing tick child outcome");
 a.flags=uint8_t((a.flags&0xd0)|nz32(a.values[0])|(s.buttons()->lastRowUnchanged()?32:0));
 const auto*adc=s.adc();const auto diff=controlArithmetic::subtract(int8_t(adc->initialAdcGroup()),int8_t(32),uint8_t(0));
 a.flags=uint8_t((a.flags&0xd0)|(diff.flags&32)|(adc->usedMux()?unsigned(bool(adc->muxGroup()&2)):unsigned(bool(diff.flags&1)))|nz16(uint16_t(a.values[0])));
 projectStateEncoder(a,*s.encoder());
 a.values[6]=(a.values[6]&0xffff00ffu)|(uint32_t(v.pedal)<<8);
 a.flags=uint8_t((a.flags&0xf0)|unsigned(s.pedal()->pedalCarry())|nz32(a.values[0]));
 a.flags=uint8_t((a.flags&0xf1)|nz8(v.pedal));
 if(v.sent){if(!s.output()||!s.output()->complete())throw std::logic_error("Missing tick pedal output");projectPanelPedalOutput(a,*s.output());}
 projectStateRefresh(a,*s.refresh());
}
}
