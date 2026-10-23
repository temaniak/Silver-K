#include "ksPanelControlsForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
namespace {
uint8_t signs16(int16_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
uint8_t signs32(int32_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
int history(uint32_t address){return address<0x400000||(address>=0xfffd10&&address<0xffffa0)?int(address):-1;}
}
bool PanelControlsForeground::step(ControllerCpu&h){
 using K=PanelControlsService::Kind;using C=PanelControlsService::Component;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(h.getPC()){case 0x13ae:kind=K::Switches;break;case 0x13b4:kind=K::PedalReset;break;case 0x13be:kind=K::Pedal;break;case 0x1886:kind=K::Encoder;break;case 0x1624:kind=K::AdcStart;break;case 0x1646:kind=K::AdcMux;break;case 0x156e:kind=K::AdcResetLast;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  job_.emplace(kind,sp,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&service=job.service;
 const auto cost=service.nextClocks();const auto start=h.cycles;const auto before=service.component();
 const auto used=service.advanceBefore(cost,[&](uint32_t address,unsigned offset){h.cycles=start+offset;h.lastread=history(address);return h.readResource(address);},[&](uint32_t address,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(address);h.writeResource(address,value);});
 if(!cost||used!=cost)throw std::logic_error("Panel controls phase admission differs");
 // These retained scopes and the sole logical child call used internal RAM.
 if((service.kind()==K::Pedal&&job.phase==0)||(service.kind()==K::AdcStart&&job.phase<2)||(before==C::AdcStart&&service.component()==C::AdcMux))h.lastwrite=-1;
 ++job.phase;++phases;components|=service.components();
 if(service.complete()){
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
  uint8_t flags=h.ccr&0xd0;
  switch(service.kind()){
  case K::Switches:h.regs[6].rl=int8_t(service.switches());flags=uint8_t((h.ccr&0xf1)|(!service.switches()?4:0));break;
  case K::PedalReset:h.regs[0].rl=-1;flags=uint8_t((h.ccr&0xf1)|8);break;
  case K::Pedal:h.regs[6].rh=int8_t(service.pedalResult());flags=uint8_t((h.ccr&0xf0)|unsigned(service.pedalCarry())|signs32(h.regs[0].er));break;
  case K::Encoder:{
   const auto delta=service.delta(),gain=service.encoderGain();h.regs[5].r=int16_t(0x400|gain);
   h.regs[6].r=int16_t(delta?(unsigned(uint8_t(service.previousDelta()^delta))<<8)|gain:unsigned(service.quarter())<<8);
   if(!delta)flags|=4;
   else if(delta&128){const auto difference=controlArithmetic::subtract(int8_t(service.previousQuarter()),int8_t(service.quarter()),uint8_t(0));flags|=difference.flags&0x21;}
   else flags|=0x29;
   break;
  }
  case K::AdcStart:{
   const auto difference=controlArithmetic::subtract(int8_t(service.initialAdcGroup()),int8_t(32),uint8_t(0));
   flags|=uint8_t((difference.flags&0x20)|(service.usedMux()?unsigned(bool(service.muxGroup()&2)):unsigned(bool(difference.flags&1)))|signs16(h.regs[0].r));break;
  }
  case K::AdcMux:h.regs[0].r=int16_t(0x900|(service.muxGroup()>>2));flags=uint8_t((h.ccr&0xf0)|unsigned(bool(service.muxGroup()&2)));break;
  case K::AdcResetLast:h.regs[0].rl=-1;flags=uint8_t((h.ccr&0xf1)|8);break;
  }
  h.ccr=flags;h.setSP(job.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
