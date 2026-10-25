#pragma once
#include "ksStateStatusService.h"
#include "ksPanelMonitorAbi.h"
#include "ksControllerDspAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectStateStatus(A&a,const StateStatusService&s,unsigned index=0){
 if(!s.complete())throw std::logic_error("Incomplete status projection");const auto&f=s.outcome(index);const auto&t=f.task;auto&v=a.values;using K=StateStatusService::Kind;using Route=StateStatusService::Route;
 const auto low=[](uint32_t&x,uint8_t n){x=(x&0xffffff00u)|n;};const auto high=[](uint32_t&x,uint8_t n){x=(x&0xffff00ffu)|(uint32_t(n)<<8);};const auto word=[](uint32_t&x,uint16_t n){x=(x&0xffff0000u)|n;};const auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};
 const auto lcdHC=[&]()->uint8_t{const auto*l=s.lcd();if(!l)return uint8_t(a.flags&0x21);const auto&r=l->lcdResult();if(!r.observed)return uint8_t(a.flags&0x21);return r.action==PanelTextTask::Action::Write?controlArithmetic::subtract(int8_t(r.reply),int8_t(20),uint8_t(0)).flags&0x21:r.action==PanelTextTask::Action::Command?controlArithmetic::subtract(int8_t(r.input),int8_t(251),uint8_t(0)).flags&0x21:0;};
 const auto compare=[&](){v[0]=t.source();v[1]=t.target();word(v[2],uint16_t(t.program()));v[3]=(uint32_t(t.remaining())<<16)|(t.targetByte()<<8)|t.sourceByte();word(v[6],f.outputPresentation);a.flags=uint8_t((a.flags&0xd0)|lcdHC()|nz16(uint16_t(v[0])));};
 if(f.kind==K::Compare||(f.kind==K::Part&&f.single)){compare();return;}
 if(f.kind==K::Part){
  if(f.active){v[0]=0x169fc+256*t.index();v[1]=t.savedPatch();projectStateStatus(a,s,f.child);v[0]=t.savedPatch();word(v[1],uint16_t(t.savedProgram()));a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(t.savedProgram())));}
  else {low(v[0],uint8_t(t.index()));high(v[0],uint8_t(t.mask()));low(v[1],f.attempts);word(v[6],f.outputPresentation);a.flags=uint8_t((a.flags&0xd0)|lcdHC()|nz16(uint16_t(v[0])));}
  return;
 }
 if(f.kind==K::Tick){
  high(v[0],f.busyBytes[3]);low(v[0],uint8_t(t.busy()));
  if(f.route!=Route::Busy&&f.route!=Route::Special)low(v[1],f.gate);
  if(f.route==Route::Special){low(v[0],f.special);a.flags=uint8_t((a.flags&0xd0)|lcdHC());}
  else if(f.route==Route::Part){low(v[0],f.gate);projectStateStatus(a,s,f.child);}
  else if(f.route==Route::Clock){low(v[0],uint8_t(t.page()));projectPanelMonitor(a,*s.monitor());}
  else if(f.route==Route::Bar){word(v[4],0x8059);projectControllerDsp(a,*s.dsp());const auto product=uint32_t(f.raw)*768;uint16_t meter=uint16_t((product>>16)-717);if(meter&0x8000)meter&=0xff00;meter=uint16_t((meter&0xff00)|(uint8_t(meter)>>1));v[0]=(product&0xffff0000u)|meter;projectPanelMonitor(a,*s.monitor());}
  else if(f.route==Route::None){low(v[0],f.screen?f.screen:uint8_t(t.page()));a.flags=controlArithmetic::subtract(int8_t(f.screen?f.screen:uint8_t(t.page())),int8_t(f.screen?0:12),a.flags).flags;}
 }
 const auto*timer=s.timer();if(!timer||!timer->complete())throw std::logic_error("Missing status scheduling outcome");v[5]=0x55e2;v[6]=timer->summary().counter;a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(v[6])));
}
}
