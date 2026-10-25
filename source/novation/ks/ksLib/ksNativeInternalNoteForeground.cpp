#include "ksNativeInternalNoteForeground.h"
#include "ksNativeInternalNoteAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a,bool writing){return (NativeVoiceStartService::dataSpan(a,1)||a==0xffff8a||a==0xffff89||a==0xffed6c||a==0xffed6d||a==0xffed6e||a==0xffed6f)||(a>=0xffec00&&a<0xfff000)||a==0xffff8a||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffec22||(a>=0xffec16&&a<=0xffec29)||a==0xffff61||a==0xffff62;}
}
void NativeInternalNoteForeground::project(CallerAbi&a,const NativeInternalNoteService&s){projectNativeInternalNote(a,s);}
bool NativeInternalNoteForeground::step(ControllerCpu&h){
 if(!job_){if((h.getPC()!=0xa882&&h.getPC()!=0xa894)||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;job_.emplace(h.getPC()==0xa882?NativeInternalNoteService::Kind::On:NativeInternalNoteService::Kind::Off,NativeInternalNoteService::Input{uint32_t(h.regs[4].er)},sp,h.indent,h.exr);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,false))throw std::logic_error("Unsupported arp reset read");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a,true))throw std::logic_error("Unsupported arp reset write");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Arp reset admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
