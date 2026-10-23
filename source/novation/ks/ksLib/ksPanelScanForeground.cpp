#include "ksPanelScanForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
namespace {
int history(uint32_t address){return address<0x400000||(address>=0xfffd10&&address<0xffffa0)?int(address):-1;}
uint8_t signs16(int16_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
uint8_t signs32(int32_t value){return uint8_t((value<0?8:0)|(!value?4:0));}
}
bool PanelScanForeground::step(ControllerCpu&h){
 using K=PanelScanService::Kind;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(h.getPC()){case 0x13f8:kind=K::Initialize;break;case 0x1456:kind=K::Scan;break;case 0x14e4:kind=K::ButtonPush;break;default:return false;}
  const auto stack=h.getSP();if((stack&1)||stack<0xfff400||stack>0xfffbfc)return false;
  if(h.nativeLowSize<0x15380)return false;
  // The firmware's two-byte ring indices are published even and modulo 128.
  // Invalid diagnostic indices retain their explicit compatibility adapter.
  for(unsigned address:{0x1537cu,0x1537eu})if(h.nativeLowBase[address]||(h.nativeLowBase[address+1]&0x81))return false;
  job_.emplace(kind,uint16_t(h.regs[6].r),stack,h.indent);++started;++kinds[unsigned(kind)];
  if(kind!=K::ButtonPush)h.lastwrite=-1;
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&service=job.service;
 const auto cost=service.nextClocks();const auto start=h.cycles;
 const auto used=service.advanceBefore(cost,[&](uint32_t address,unsigned offset){h.cycles=start+offset;h.lastread=history(address);return h.readResource(address);},
  [&](uint32_t address,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(address);h.writeResource(address,value);});
 if(!cost||used!=cost)throw std::logic_error("Panel scan phase admission differs");
 components|=service.components();++phases;
 if(service.complete()){
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
  uint8_t flags=h.ccr&0xd0;
  switch(service.kind()){
  case K::Initialize:flags|=signs16(h.regs[0].r);break;
  case K::Scan:flags|=uint8_t(signs32(h.regs[0].er)|(service.lastRowUnchanged()?32:0));break;
  case K::ButtonPush:h.regs[4].er=service.producer();h.regs[5].er=0x15380+service.eventIndex();h.regs[6].r=int16_t(service.event());flags|=service.producer()?0:4;break;
  }
  h.ccr=flags;h.setSP(job.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
