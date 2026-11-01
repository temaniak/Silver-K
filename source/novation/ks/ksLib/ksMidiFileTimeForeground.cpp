#include "ksMidiFileTimeForeground.h"
#include "ksMidiFileTimeAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
bool MidiFileTimeForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;
 using K=MidiFileTimeService::Kind;K k;switch(h.getPC()){case 0xb2e0:k=K::Delay;break;case 0xb2fe:k=K::Due;break;case 0xb312:k=K::Vlq;break;case 0xb3c0:k=K::Tempo;break;case 0xb494:k=K::ArpDelay;break;default:return false;}
 const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;MidiFileTimeService::Input i;i.stream=uint32_t(h.regs[4].er);i.tempo=uint32_t(h.regs[0].er);i.record=uint32_t(h.regs[0].er);if((k==K::Delay||k==K::Vlq||k==K::ArpDelay)&&!MidiFileTimeService::readable(i.stream,1))return false;if(k==K::ArpDelay&&!MidiFileTimeService::readable(i.record+56,4))return false;
 job_.emplace(k,i,sp,h.indent);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 auto read=[&](uint32_t a,unsigned dt){h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);};auto write=[&](uint32_t a,uint8_t v,unsigned dt){h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);};
 const auto used=s.advanceBefore(cost,read,write,previous);if(!cost||used!=cost)throw std::logic_error("Monitor phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;
 if(s.complete()){struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectMidiFileTime(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
 uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
