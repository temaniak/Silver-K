#pragma once
#include "ksStateEncoderService.h"
#include "ksControlArithmetic.h"
#include "ksStateFieldForeground.h"
#include "ksStateProgramBrowserForeground.h"
namespace ks {
namespace stateEncoderBoundary {
using A=StateFieldForeground::CallerAbi;
inline uint8_t nz8(uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));}
inline void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}
inline void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}
inline void move(A&a,unsigned n,uint8_t b){low(a.values[n],b);a.flags=uint8_t((a.flags&0xf1)|nz8(b));}
inline void gate(A&a,const StateScreenService::GateResult&g){const auto entry=a.flags;low(a.values[0],uint8_t(g.accepted));uint8_t hc=entry&0x21;if(g.hardware)low(a.values[1],g.hardware);else{word(a.values[1],uint16_t((unsigned(g.screen)<<8)|g.page));hc=controlArithmetic::subtract(int16_t(uint16_t(a.values[1])),int16_t(0x902),0).flags&0x21;if(g.editObserved)low(a.values[1],g.edit);}a.flags=uint8_t((entry&0xd0)|hc|nz8(uint8_t(a.values[0])));}
inline void panel(A&a,const PanelControlsService&s){const auto delta=s.delta();word(a.values[5],uint16_t(0x400|s.encoderGain()));word(a.values[6],delta?uint16_t((unsigned(uint8_t(s.previousDelta()^delta))<<8)|s.encoderGain()):uint16_t(unsigned(s.quarter())<<8));uint8_t flags=a.flags&0xd0;if(!delta)flags|=4;else if(delta&128)flags|=controlArithmetic::subtract(int8_t(s.previousQuarter()),int8_t(s.quarter()),0).flags&0x21;else flags|=0x29;a.flags=flags;}
inline void screenEdit(A&a,const StateScreenService&s){const auto&o=s.summary();if(o.apply.observed){a.values[2]=(o.editBase&0xffff0000u)|(unsigned(o.apply.cached)<<8)|o.apply.type;if(o.apply.packed){a.values[2]=(o.apply.packingDescriptor&0xffff0000u)|uint16_t(a.values[2]);a.values[4]=o.apply.packingBase;}if(o.apply.signalled){word(a.values[4],0x8041);a.values[5]=0x400000;}}
 // Render's heading is absent for Edit and its final flags are replaced below.
 // The completed value presentation sets the complete label index before use.
 StateScreenForeground::projectRender(a,o);a.values[6]=0x10000u|(unsigned(uint8_t((64u-o.level)*2+64))<<8)|o.level;a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::add(int8_t(uint8_t((64u-o.level)*2)),int8_t(64),0).flags&0xfdu));}
}
// Completion-only external caller projection. Never invoked by the pure graph.
template<class CallerAbi>void projectStateEncoder(CallerAbi&out,const StateEncoderService&s){
 if(!s.complete())throw std::logic_error("Encoder projection before completion");using namespace stateEncoderBoundary;A a;a.values=out.values;a.flags=out.flags;gate(a,s.context()->summary().gate);
 const auto&o=s.summary();if(o.read){panel(a,*s.reader());move(a,6,uint8_t(o.encoder));if(uint8_t(o.encoder)){move(a,0,o.page);a.flags=controlArithmetic::subtract(int8_t(o.page),int8_t(2),a.flags).flags;
  if(o.tail!=StateEncoderService::Component::Name){a.flags=controlArithmetic::subtract(int8_t(o.page),int8_t(3),a.flags).flags;
   if(o.tail!=StateEncoderService::Component::Category){move(a,0,o.special);if(o.tail!=StateEncoderService::Component::Special){move(a,0,o.control);
    if(o.tail==StateEncoderService::Component::Field){word(a.values[6],s.encoderTask().event());a.flags=uint8_t((a.flags&0xf1)|8);StateFieldForeground::project(a,*s.field(),s.field()->input(),s.field()->handlerBinding(),s.field()->recipeBinding());}
    else{low(a.values[1],s.probe()->edit());a.flags=uint8_t((a.flags&0xf1)|nz8(s.probe()->edit()));if(o.tail==StateEncoderService::Component::Screen)screenEdit(a,*s.screen());else{word(a.values[6],s.encoderTask().event());a.flags=uint8_t((a.flags&0xf1)|8);StateProgramBrowserForeground::project(a,*s.browser());}}
   }}
  }
  if(o.tail==StateEncoderService::Component::Name||o.tail==StateEncoderService::Component::Category||o.tail==StateEncoderService::Component::Special)StateFieldForeground::projectSceneEdit(a,*s.field());
 }}out.values=a.values;out.flags=a.flags;
}
}
