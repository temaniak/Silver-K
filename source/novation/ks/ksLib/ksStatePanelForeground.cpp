#include "ksStatePanelForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {
bool StatePanelForeground::step(ControllerCpu&h){
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0||h.getPC()!=0x4406)return false;
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc||h.nativeLowSize<0x30000)return false;
  const auto word=[&](unsigned a){return(unsigned(h.nativeLowBase[a])<<8)|h.nativeLowBase[a+1];};
  for(unsigned a:{0x156e8u,0x156eau})if(word(a)>=512||(word(a)&1))return false;
  for(unsigned a:{0x15b04u,0x15b06u})if(word(a)>=256)return false;
  if(h.memory[0xffed6c]||h.memory[0xffed6e])return false;
  job_.emplace(sp,h.indent,h.exr);++started;++kinds[0];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;
 auto&service=job.service;const auto cost=service.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;const bool retained=service.clearsWriteHistory();
 const auto history=[](uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;};
 const auto used=service.advanceBefore(cost,[&](uint32_t a,unsigned offset){h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},
  [&](uint32_t a,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,value);},previous);
 if(!cost||used!=cost)throw std::logic_error("Upper State panel phase differs");
 if(retained)h.lastwrite=-1;h.exr=service.critical()?7:job.priority;++phases;components|=service.components();
 if(service.complete()){
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
  if(service.biasObserved())h.regs[0].rl=int8_t(service.bias());h.regs[1].er=service.controlAddress();h.regs[6].r=service.event();
  h.ccr=uint8_t((h.ccr&0xd0)|4);h.exr=job.priority;h.setSP(job.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
