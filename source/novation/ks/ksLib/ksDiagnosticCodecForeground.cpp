#include "ksDiagnosticCodecForeground.h"
#include "ksDiagnosticCodecAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
bool DiagnosticCodecForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;
 using K=DiagnosticCodecService::Kind;K k;switch(h.getPC()){case 0x1a2c:k=K::Prefix;break;case 0x1a46:k=K::Word;break;case 0x1a58:k=K::Long;break;case 0x1a8a:k=K::Block;break;case 0x1a98:k=K::Group;break;case 0x1aac:k=K::Sample;break;case 0x1ab6:k=K::Tail;break;default:return false;}
 const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;DiagnosticCodecService::Input i;i.payload=uint32_t(h.regs[1].er);i.source=uint32_t(h.regs[6].er);i.sum=uint32_t(h.regs[3].er);i.weight=uint16_t(h.regs[2].r);i.carry=uint16_t(h.regs[2].e);const auto width=k==K::Block?448u:k==K::Group?14u:k==K::Sample?2u:0u;if(width&&!DiagnosticCodecService::readSpan(i.source,width))return false;
 job_.emplace(k,i,sp,h.indent);++started;}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 auto read=[&](uint32_t a,unsigned dt){h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);};auto write=[&](uint32_t a,uint8_t v,unsigned dt){h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);};
 const auto used=s.advanceBefore(cost,read,write,previous);if(!cost||used!=cost)throw std::logic_error("Monitor phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;
 if(s.complete()){struct Abi{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;projectDiagnosticCodec(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
 uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
