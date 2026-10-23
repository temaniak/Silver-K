#include "ksStateEventsForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
namespace {
uint8_t nz8(uint8_t v){return uint8_t((v&0x80?8:0)|(!v?4:0));}
uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}
uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
// Both bytes must precede the reserved compatibility CPU scratch region.
bool dataPair(uint32_t a){a&=0xffffff;return(a>=0xb8e8&&a<0x1fffff)||(a>=0xa0b8e8&&a<0xbfffff)||(a>=0xffdc00&&a<0xffefff);}
}
bool StateEventsForeground::step(ControllerCpu&h){
 using K=StateEventsService::Kind;using P=PanelInputTask::Stage;using B=KeyboardEventsTask::Stage;using A=PanelAnalogTask::Stage;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0)return false;
  K kind;
  switch(h.getPC()){
  case 0x1412:kind=K::ButtonPop;break;case 0x15da:kind=K::ControlPop;break;case 0x21e2:kind=K::ControlPeek;break;case 0x1f12:kind=K::KeyboardPop;break;
  case 0x20fc:kind=K::Convert;break;case 0x2144:kind=K::Scale;break;case 0x218c:kind=K::Square;break;case 0x20de:kind=K::Coefficients;break;case 0x20c8:kind=K::Range;break;
  default:return false;
  }
  const auto stack=h.getSP();if((stack&1)||stack<0xfff400||stack>0xfffbfc)return false;
  if(unsigned(kind)<4){
   const uint32_t producer=kind==K::ButtonPop?0x1537c:kind==K::KeyboardPop?0x156e8:0x15406;
   const auto limit=kind==K::ButtonPop?128u:512u;
   if(producer+3>=h.nativeLowSize)return false;
   const auto word=[&](uint32_t at){return(unsigned(h.nativeLowBase[at])<<8)|h.nativeLowBase[at+1];};
   if(word(producer)>=limit||word(producer+2)>=limit)return false;
  }
  StateEventsService::Input input{uint8_t(h.regs[6].rh),uint8_t(h.regs[6].rl),uint16_t(h.regs[3].r),uint32_t(h.regs[1].er),uint32_t(h.regs[2].er)};
  if((kind==K::Scale||kind==K::Coefficients)&&!dataPair(input.coefficients))return false;
  if((kind==K::Range||kind==K::Coefficients)&&!dataPair(input.history))return false;
  job_.emplace(kind,input,stack,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;
 auto&service=job.service;const auto cost=service.nextClocks(h.lastwrite);const auto start=h.cycles;
 // Only bus-history dispositions cross the narrow compatibility edge. The
 // graph owns neither saved execution values nor internal return frames.
 bool retained=false,released=false;
 if(const auto*p=service.panelTask())switch(p->stage()){
 case P::SavePop:case P::SavePopAux:case P::ConvertControl:retained=true;break;
 case P::RestoreEmpty:case P::RestorePopAux:case P::RestorePop:case P::ReturnEmpty:case P::ReturnPop:released=true;break;
 default:break;
 }
 if(const auto*b=service.keyboardTask())switch(b->stage()){
 case B::SavePopValue:case B::SavePopIndex:retained=true;break;
 case B::RestoreEmptyPopValue:case B::RestorePopIndex:case B::RestorePopValue:case B::ReturnEmptyPop:case B::ReturnPop:released=true;break;
 default:break;
 }
 if(const auto*a=service.analogTask())switch(a->stage()){
 case A::RetainSpan:case A::SaveFirstAddress:case A::SaveFirstBias:case A::SaveSecondAddress:case A::SaveSecondBias:case A::SaveValue:case A::SaveSquare:case A::ScaleFirst:case A::ScaleSecond:retained=true;break;
 case A::RestoreSpan:case A::RestoreFirstBias:case A::RestoreFirstAddress:case A::RestoreSecondBias:case A::RestoreSecondAddress:case A::RestoreValue:case A::RestoreSquare:
 case A::ReturnCoefficients:case A::ReturnRange:case A::ReturnUnchanged:case A::ReturnFirst:case A::ReturnSecond:case A::ReturnValue:case A::ReturnSquare:released=true;break;
 default:break;
 }
 const auto previousWrite=h.lastwrite;
 const auto used=service.advanceBefore(cost,[&](uint32_t address,unsigned offset){h.cycles=start+offset;h.lastread=history(address);return h.readResource(address);},
  [&](uint32_t address,uint8_t v,unsigned offset){h.cycles=start+offset;h.lastwrite=history(address);h.writeResource(address,v);},previousWrite);
 if(!cost||used!=cost)throw std::logic_error("State event phase admission differs");
 if(retained)h.lastwrite=-1;if(released)h.lastread=-1;++phases;components|=service.components();
 if(service.complete()){
  uint8_t flags=h.ccr&0xd0;
  const auto curveCarry=[&]{const auto selector=service.selector();return uint8_t(selector==0x22?service.value()<128:selector==0x23?(service.scaledValue()&1):selector==0x20?((unsigned(service.squareInput())*service.squareInput())>>8)&1:selector<0x20);};
  switch(service.kind()){
  case K::ButtonPop:case K::KeyboardPop:h.regs[6].r=int16_t(service.event());flags|=nz32(h.regs[0].er);break;
  case K::ControlPeek:case K::ControlPop:
   h.regs[6].er=service.available()?int32_t(0x10000u|service.event()):0;
   flags|=uint8_t(nz32(h.regs[0].er)|(service.kind()==K::ControlPop&&service.available()?curveCarry():0));break;
  case K::Convert:
   h.regs[6].rh=int8_t(service.value());
   if(service.selector()==0x22||service.selector()==0x23)flags|=uint8_t(nz32(h.regs[2].er)|curveCarry());
   else if(service.selector()==0x20)flags|=uint8_t(nz16(h.regs[0].r)|curveCarry());
   else flags=controlArithmetic::subtract(int8_t(service.selector()),int8_t(0x20),h.ccr).flags;
   break;
  case K::Scale:h.regs[6].rh=int8_t(service.value());flags|=uint8_t(nz32(h.regs[0].er)|(service.value()<128?1:0));break;
  case K::Square:h.regs[6].rh=int8_t(service.value());flags|=uint8_t((h.ccr&0x20)|nz16(h.regs[0].r)|(((unsigned(service.squareInput())*service.squareInput())>>8)&1));break;
  case K::Coefficients:{
   const uint8_t span=uint8_t(service.maximum()-service.minimum()),center=uint8_t(service.minimum()+(span>>1));
   const auto offsetFlags=controlArithmetic::subtract(int8_t(0x80),int8_t(center),h.ccr).flags;
   const unsigned quotient=span?service.bias()/span:0,remainder=service.bias()-quotient*span;
   h.regs[0].r=int16_t((unsigned(service.minimum())<<8)|span);h.regs[3].r=int16_t((remainder<<8)|uint8_t(quotient));
   flags|=uint8_t((offsetFlags&0x21)|nz8(service.factor()));break;
  }
  case K::Range:
   flags=controlArithmetic::subtract(int8_t(service.value()),int8_t(service.maximum()),h.ccr).flags;
   if(service.value()>service.maximum())flags=uint8_t((flags&~14u)|nz8(service.value()));
   h.regs[0].rl=int8_t(service.maximum());break;
  }
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
  h.setSP(job.caller+4);h.pc=h.makepc(target&0xffffff);h.ccr=flags;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
