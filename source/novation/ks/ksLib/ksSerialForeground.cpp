#include "ksSerialForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
namespace {
uint8_t signs16(int16_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
uint8_t signs32(int32_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
int history(uint32_t address){return address<0x400000||(address>=0xfffd10&&address<0xffffa0)?int(address):-1;}
}
bool SerialForeground::step(ControllerCpu&h){
 using K=SerialForegroundService::Kind;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(uint32_t(h.getPC())){case 0xfe4:kind=K::ResetRx;break;case 0x100a:kind=K::PopRx;break;case 0x10d0:kind=K::Receive;break;case 0x10e4:kind=K::ResetTx;break;case 0x1104:kind=K::Transmit;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  if((kind==K::PopRx||kind==K::Receive)&&(h.memory[0xffec64]||h.memory[0xffec66]))return false;
  if(kind==K::Transmit&&(h.memory[0xffed6c]||h.memory[0xffed6e]))return false;
  job_.emplace(kind,uint8_t(h.regs[6].rl),sp,h.exr,h.indent);
  ++started;++kinds[unsigned(kind)];
  // The first retained scope used internal RAM. Its data bytes are deliberately
  // absent; only the boundary's bus-history disposition crosses this edge.
  h.lastwrite=-1;
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;
 auto&service=job.service;const auto cost=service.nextClocks();const auto start=h.cycles;
 const auto used=service.advanceBefore(cost,[&](uint32_t address,unsigned offset){h.cycles=start+offset;h.lastread=history(address);return h.readResource(address);},
  [&](uint32_t address,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(address);h.writeResource(address,value);});
 if(!cost||used!=cost)throw std::logic_error("Foreground UART phase admission differs");
 uint32_t returnTarget=0;
 if(service.complete()){
  // This is the existing outer caller edge, read live at its original return
  // clocks. No private graph continuation or inner saved frame exists here.
  for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;returnTarget=(returnTarget<<8)|h.readResource(job.sp+n);}
 }
 h.cycles=start+cost;h.exr=service.critical()?7:job.priority;++phases;
 if(service.complete()){
  // Exact caller-visible status follows the semantic outcome. No intermediate
  // condition byte or register file was maintained inside the native graph.
  uint8_t flags=h.ccr&0xd0;
  switch(service.kind()){
  case K::ResetRx:case K::ResetTx:flags|=signs16(h.regs[0].r);break;
  case K::PopRx:h.regs[6].r=int16_t(service.result());flags|=signs32(h.regs[0].er);break;
  case K::Receive:h.regs[0].rl=int8_t(service.data());flags|=uint8_t(signs16(h.regs[6].r)|1|((service.data()&15)<8?32:0));break;
  case K::Transmit:flags|=uint8_t(signs32(h.regs[0].er)|(service.consumer()<service.nextProducer()?1:0)|((service.consumer()&0xfff)<(service.nextProducer()&0xfff)?32:0));break;
  }
  h.ccr=flags;h.exr=job.priority;h.setSP(job.sp+4);h.pc=returnTarget&0xffffff;--h.indent;h.lastread=-1;
  job_.reset();++completed;
 }
 return true;
}
}
