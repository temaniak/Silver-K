#include "ksMidiFileTransportForeground.h"
#include "ksMidiFileTransportResolver.h"
#include "ksControllerState.h"
namespace ks {namespace {int transferHistory(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
bool MidiFileTransportForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0||unsigned(h.getPC())!=0xb17c)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  // The reusable work slot and observer storage already belong to the owner.
  // Capturing the three consumed resource operands does not execute a child.
  Image a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;
  job_.emplace(MidiFileTransportService::Input{uint16_t(h.regs[6].r),uint8_t(h.regs[2].rh),uint32_t(h.regs[5].er)},a,h.nativeLowBase,sp,h.indent,h.exr);++started;
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite,h.lastread);if(!cost)throw std::logic_error("Native file-player phase missing");const auto start=h.cycles;const auto previous=h.lastwrite,previousRead=h.lastread;
 MidiFileTransportResolver resolver(j.metadata);Observer observer{j};
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){if(a>=0xfff000&&a<0xfffc00)throw std::logic_error("File-player observed removed CPU scratch");h.cycles=start+dt;h.lastread=transferHistory(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){if(a>=0xfff000&&a<0xfffc00)throw std::logic_error("File-player wrote removed CPU scratch");h.cycles=start+dt;h.lastwrite=transferHistory(a);h.writeResource(a,v);},resolver,observer,previous,previousRead);
 if(used!=cost)throw std::logic_error("Native file-player admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.priority()==7?7:s.priority()==6?uint8_t(j.priority|6):j.priority;++phases;components|=s.components();
 if(s.complete()){const auto&a=j.accounting.image();for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;for(unsigned n=0;n<returnedRequests.size();++n)returnedRequests[n]+=j.returns[n];uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
