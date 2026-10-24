#include "ksStateDisplayForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
uint8_t lcdArithmetic(const StateDisplayService&s,uint8_t fallback){const auto&r=s.lcdResult();if(!r.observed)return fallback&0x21;using A=PanelTextTask::Action;return r.action==A::Write?controlArithmetic::subtract(int8_t(r.reply),int8_t(20),0).flags&0x21:r.action==A::Command?controlArithmetic::subtract(int8_t(r.input),int8_t(251),0).flags&0x21:0;}


}
bool StateDisplayForeground::step(ControllerCpu&h){using T=StateDisplayService;using K=T::Kind;using P=T::Presentation;
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0||h.nativeLowSize<0x167d0)return false;K kind;switch(h.getPC()){case 0x5ae2:kind=K::Format;break;case 0x5da8:kind=K::Off;break;case 0x9b8:kind=K::Text;break;case 0x976:kind=K::Character;break;case 0x8fc:kind=K::Command;break;default:return false;}const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  T::Input in;in.type=uint8_t(h.regs[1].rl);in.value=uint8_t(h.regs[0].rl);in.bit=uint8_t(h.regs[2].rh);in.character=uint16_t(h.regs[6].r);in.source=uint32_t(h.regs[6].er);
  const auto byte=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|byte(a+n);return v;};
  if(kind==K::Text){bool end=false;for(unsigned n=0;n<256;++n){if(!T::dataSpan(in.source+n,1))return false;if(!byte(in.source+n)){end=true;break;}}if(!end)return false;}
  if(kind==K::Format){if(in.type==47&&!T::dataSpan(wide(0x167bc)+87,1))return false;unsigned pointer=in.type==4||(in.type==5&&in.value)?0x167c4:in.type==8?0x167c8:in.type==25?0x167cc:0;if(pointer){const auto table=wide(pointer);if(!T::dataSpan(table,1))return false;const auto width=byte(table);const auto index=unsigned(in.type==5?uint8_t(in.value-1):in.value);const auto count=1+index*width+(width?width:256u);if(!T::dataSpan(table,count))return false;}}
  job_.emplace(kind,in,sp,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&s=job.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const bool retain=s.endsWriteTransfer(),release=s.endsReadTransfer();const auto previous=h.lastwrite;
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned offset){if(a!=0xffff51&&!T::dataSpan(a,1))throw std::runtime_error("Unsupported evolving LCD data resource");h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,value);},previous);
 if(!cost||used!=cost)throw std::logic_error("State display phase admission differs");if(retain)h.lastwrite=-1;if(release)h.lastread=-1;++phases;components|=s.components();
 if(s.complete()){
  uint8_t flags=h.ccr&0xd0;const auto kind=s.kind();const auto&in=s.input();const auto presentation=s.presentation();
  if(presentation==P::Unknown)flags=controlArithmetic::subtract(int8_t(in.type),int8_t(49),h.ccr).flags;
  else if(presentation==P::Table){h.regs[0].er=int32_t(s.cursor());h.regs[1].er=int32_t(s.tableAfterHeader());h.regs[2].rl=0;if(in.type==38||(in.type==39&&in.value)){h.regs[2].rh=in.value/12<2?45:32;h.regs[3].er=int32_t(s.formatTask()->record());}h.regs[6].r=int16_t(s.output());flags|=uint8_t(lcdArithmetic(s,0)|4);}
  else if(presentation==P::Number){h.regs[0].rl=int8_t(s.numberValue());if(in.type==47){h.regs[1].er=int32_t((s.formatTask()->patch()&0xffffff00u)|s.formatTask()->mode());}flags|=uint8_t(lcdArithmetic(s,0)|(s.signedNumber()?nz32(uint32_t(h.regs[0].er)):nz16(uint16_t(h.regs[0].r))));}
  else if(presentation==P::Disabled||presentation==P::Text){uint8_t fallback=kind==K::Off||kind==K::Text?h.ccr&0x21:0;if(kind==K::Format&&in.type==47){h.regs[1].er=int32_t((s.formatTask()->patch()&0xffffff00u)|s.formatTask()->mode());fallback=controlArithmetic::subtract(int8_t(s.formatTask()->mode()),int8_t(7),0).flags&0x21;}h.regs[6].er=int32_t((s.textSource()&0xffff0000u)|s.output());flags|=uint8_t(lcdArithmetic(s,fallback)|nz32(uint32_t(h.regs[5].er)));}
  else if(presentation==P::Character){h.regs[6].r=int16_t(s.output());flags|=uint8_t(lcdArithmetic(s,h.ccr)|nz16(uint16_t(h.regs[0].r)));}
  else flags|=uint8_t((h.ccr&0x21)|nz16(uint16_t(h.regs[0].r)));
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}h.setSP(job.caller+4);h.pc=target&0xffffff;h.ccr=flags;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
