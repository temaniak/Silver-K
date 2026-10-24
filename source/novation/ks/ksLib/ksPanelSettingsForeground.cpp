#include "ksPanelSettingsForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
uint8_t lcdArithmetic(const StateDisplayService::LcdResult&r,uint8_t fallback){using A=PanelTextTask::Action;if(!r.observed)return fallback&0x21;return r.action==A::Write?controlArithmetic::subtract(int8_t(r.reply),int8_t(20),0).flags&0x21:r.action==A::Command?controlArithmetic::subtract(int8_t(r.input),int8_t(251),0).flags&0x21:0;}
uint8_t pageArithmetic(const StateSavePageService&p){using D=StateSavePageService::Decision;switch(p.decision()){case D::SkipProtected:return controlArithmetic::add(int32_t(p.left()),int32_t(p.right()),0).flags&0x21;case D::FlashRange:case D::Protection:return controlArithmetic::subtract(int32_t(p.left()),int32_t(p.right()),0).flags&0x21;default:return controlArithmetic::subtract(int8_t(p.left()),int8_t(p.right()),0).flags&0x21;}}
}
bool PanelSettingsForeground::step(ControllerCpu&h){using T=PanelSettingsService;using K=T::Kind;
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17530||!h.interruptMode2||h.indent<=0)return false;K kind;switch(h.getPC()){case 0x1f84:kind=K::Reset;break;case 0x1f92:kind=K::Select;break;case 0x1f9a:kind=K::Show;break;case 0x1fd8:kind=K::Poll;break;case 0x2070:kind=K::Coefficients;break;case 0x2098:kind=K::First;break;case 0x20ae:kind=K::Second;break;case 0x219a:kind=K::Load;break;case 0x21d0:kind=K::Save;break;case 0x152e:kind=K::Warmup;break;case 0x1510:kind=K::AnalogInitialize;break;case 0x151e:kind=K::AnalogDuty;break;case 0x1522:kind=K::AnalogLevel;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  if(kind==K::Poll){const auto word=[&](uint32_t a){return(unsigned(h.nativeLowBase[a])<<8)|h.nativeLowBase[a+1];};if(word(0x15406)>=512||word(0x15408)>=512)return false;}
  job_.emplace(kind,T::Input{uint16_t(h.regs[6].r),uint8_t(h.regs[0].rl)},sp,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;const bool priority=s.writePriority();
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned offset){h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned offset){h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,v);},0,previous);
 if(!cost||used!=cost)throw std::logic_error("Panel settings phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;if(!priority&&s.writePriority()){j.savedPriority=h.exr;h.exr|=6;}else if(priority&&!s.writePriority())h.exr=j.savedPriority;++phases;components|=s.components();
 if(s.complete()){
  const auto entry=h.ccr;uint8_t flags=entry&0xd0;const auto coefficients=[&]{const auto span=uint8_t(s.maximum()-s.minimum());const auto center=uint8_t(s.minimum()+(span>>1));const unsigned q=span?s.coefficientBias()/span:0,rem=s.coefficientBias()-q*span;h.regs[0].r=int16_t((unsigned(s.minimum())<<8)|span);h.regs[1].er=int32_t(s.coefficientHistory());h.regs[2].er=int32_t(s.coefficientHistory()-256);h.regs[3].r=int16_t((rem<<8)|uint8_t(q));return uint8_t(controlArithmetic::subtract(int8_t(128),int8_t(center),0).flags&0x21);};
  switch(s.kind()){
  case K::Reset:case K::Load:h.regs[0].er=int32_t(0x816000u|(s.kind()==K::Reset?0:s.loadedDuty()));h.regs[1].er=0x159ec;h.regs[2].e=0;h.regs[2].rl=int8_t(s.copiedLastByte());flags|=uint8_t((entry&0x21)|(s.kind()==K::Reset?4:nz8(s.loadedDuty())));break;
  case K::Show:case K::Select:h.regs[0].rl=int8_t(s.mode());if(s.noticePrinted())h.regs[6].e=0;if(s.showedMode())flags|=uint8_t(lcdArithmetic(s.lcdResult(),entry)|nz16(uint16_t(h.regs[6].r)));else flags=controlArithmetic::subtract(int8_t(s.mode()),int8_t(5),entry).flags;break;
  case K::First:case K::Second:flags|=uint8_t(coefficients()|nz8(s.factor()));break;
  case K::Coefficients:case K::Save:if(s.kind()==K::Coefficients)coefficients();h.regs[5].er=int32_t(s.flashTask()->source());h.regs[6].er=int32_t(s.flashTask()->target());flags|=uint8_t(pageArithmetic(*s.flashTask())|nz32(uint32_t(h.regs[0].er)));break;
  case K::Poll:{if(s.inputAvailable()){h.regs[6].er=int32_t(0x10000u|s.pollEvent());if(s.noticePrinted())h.regs[6].e=0;h.regs[0].rl=int8_t(s.mode());}else h.regs[6].er=0;if(s.coefficientsObserved())coefficients();if(s.eventDisplayed())h.regs[0].rl=int8_t(s.value());h.regs[5].r=int16_t(0x400|s.encoderGain());h.regs[6].r=int16_t(s.event());const auto delta=s.encoderDelta()?s.encoderGain():uint8_t(0);if(delta){const int sum=int(s.pollDuty())+int(int8_t(delta));h.regs[0].r=int16_t((sum<0?0:unsigned(sum)&0xff00u)|s.value());h.regs[1].r=int8_t(delta);flags|=uint8_t(lcdArithmetic(s.lcdResult(),0)|nz16(uint16_t(h.regs[0].r)));}else flags|=4;break;}
  case K::Warmup:h.regs[0].rl=-1;flags|=8;break;
  case K::AnalogInitialize:h.regs[0].rl=int8_t(192);flags|=uint8_t((entry&0x21)|8);break;
  case K::AnalogDuty:flags|=uint8_t((entry&0x21)|nz8(uint8_t(s.input().value)));break;
  case K::AnalogLevel:{const auto doubled=uint8_t((64-uint8_t(s.input().event))*2);h.regs[6].rh=int8_t(s.level());flags|=uint8_t((controlArithmetic::add(int8_t(doubled),int8_t(64),entry).flags&0x21)|nz8(s.level()));break;}
  }
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;h.ccr=flags;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
