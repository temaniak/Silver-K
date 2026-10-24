#include "ksStateEditActionForeground.h"
#include "ksControllerState.h"
#include "ksControlArithmetic.h"
#include <stdexcept>
namespace ks {namespace {
using T=StateEditActionService;using A=StateEditActionForeground::CallerAbi;
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}
void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}
void move8(A&a,uint8_t v){a.flags=uint8_t((a.flags&0xf1)|nz8(v));}
void gate(A&a,const StateScreenService::GateResult&g){auto&v=a.values;const auto entry=a.flags;low(v[0],uint8_t(g.accepted));uint8_t hc=entry&0x21;if(g.hardware)low(v[1],g.hardware);else{word(v[1],uint16_t((unsigned(g.screen)<<8)|g.page));hc=controlArithmetic::subtract(int16_t(uint16_t(v[1])),int16_t(0x902),0).flags&0x21;if(g.editObserved)low(v[1],g.edit);}a.flags=uint8_t((entry&0xd0)|hc|nz8(uint8_t(v[0])));}
void reset(A&a,uint8_t last){a.values[0]=0x816000;a.values[1]=0x159ec;a.values[2]=(a.values[2]&0xff00u)|last;a.flags=uint8_t((a.flags&0xf1)|4);}
void set(A&a,const StateScreenService::Summary&s){a.values[2]=0x16800u|s.setReset;StateScreenForeground::projectRender(a,s);gate(a,s.gate);if(s.reset)reset(a,s.resetLast);}
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return T::dataSpan(a,1)||(a>=0x1c0&&a<0x300)||(a>=0x800000&&a<0x900000)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff51||a==0xffff61||a==0xffff62||a==0xfffeb1||a==0xffff8a||a==0xffffa5;}
int packing(uint32_t raw){constexpr unsigned recipes[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==recipes[n])return int(n);return-1;}
struct Resolver{uint32_t&recipe;StateFieldCoordinator::Handler field(uint8_t category,uint32_t raw){constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(category>=9||raw!=targets[category])throw std::logic_error("Unsupported evolving edit action field binding");return StateFieldCoordinator::Handler(category);}StatePackFieldsTask::Kind operator()(uint32_t raw){const auto n=packing(raw);if(n<0)throw std::logic_error("Unsupported evolving edit action packing binding");recipe=raw;return StatePackFieldsTask::Kind(n);}};
}
void StateEditActionForeground::project(CallerAbi&a,const T&s,uint32_t recipe){using K=T::Kind;const auto k=s.kind();const auto&f=s.summary();auto&v=a.values;
 if(k==K::Release)return;if(k==K::ClearDigits){const auto saved=uint16_t(a.values[0]);a.flags=uint8_t((a.flags&0xd0)|(!saved?4:0)|(saved&0x8000?8:0));return;}if(k==K::ClearEdit){low(v[0],0);move8(a,0);return;}
 if(k==K::ToggleEdit){if(!s.input().pressed)return;low(v[0],0);move8(a,0);low(v[0],f.page);move8(a,f.page);if(f.page)return;a.flags=uint8_t((a.flags&0xd0)|4);high(v[6],f.editAfter);move8(a,f.editAfter);if(f.gateOutcome)gate(a,f.gateOutcome->gate);if(f.calibration)reset(a,f.resetLast);if(f.ui)StateFieldForeground::projectUi(a,*s.ui());return;}
 if(k==K::SelectEdit){move8(a,uint8_t(s.input().event>>8));if(!uint8_t(s.input().event>>8))return;low(v[0],0);move8(a,0);low(v[0],f.page);move8(a,f.page);if(f.page)return;a.flags=uint8_t((a.flags&0xd0)|4);high(v[6],f.screen);move8(a,f.screen);a.flags=controlArithmetic::subtract(int8_t(s.input().event),int8_t(f.screen),a.flags).flags;low(v[0],f.editAfter);move8(a,f.editAfter);if(f.setOutcome)set(a,*f.setOutcome);if(f.ui)StateFieldForeground::projectUi(a,*s.ui());return;}
 if(!s.input().pressed)return;const auto offset=k==K::ToggleOne?215u:213u;v[0]=offset;a.flags=uint8_t((a.flags&0xd0)|nz8(uint8_t(offset)));v[1]=f.reference;const auto added=controlArithmetic::add(int32_t(f.patch),int32_t(offset),a.flags);a.flags=uint8_t((added.flags&0xf1)|nz8(f.valueBefore));high(v[6],uint8_t(f.valueBefore^1));if(f.apply)StateFieldForeground::projectApply(a,*s.apply(),recipe);
}
bool StateEditActionForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0)return false;T::Kind kind;switch(h.getPC()){case 0x49b0:kind=T::Kind::ToggleEdit;break;case 0x49e8:kind=T::Kind::SelectEdit;break;case 0x4a32:kind=T::Kind::ToggleOne;break;case 0x4a4c:kind=T::Kind::ToggleTwo;break;case 0x601a:kind=T::Kind::ClearDigits;break;case 0x48cc:kind=T::Kind::ClearEdit;break;case 0x4862:kind=T::Kind::Release;break;default:return false;}const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;T::Input in{uint16_t(h.regs[6].r),!(h.ccr&4)};const auto peek=[&](uint32_t raw){const auto at=raw&0xffffff;return at<h.nativeLowSize?h.nativeLowBase[at]:h.memory[at];};if(!T::admitted(kind,in,peek))return false;
  if(kind==T::Kind::ToggleOne||kind==T::Kind::ToggleTwo||kind==T::Kind::ToggleEdit||kind==T::Kind::SelectEdit){for(unsigned n=0;n<6;++n){uint32_t raw=0;for(unsigned i=0;i<4;++i)raw=(raw<<8)|uint8_t(peek(71306+4*n+i));if(packing(raw)<0)return false;}}job_.emplace(kind,in,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;Resolver resolver{j.recipe};
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto at=raw&0xffffff;if(!endpoint(at))throw std::logic_error("Unsupported evolving edit action read endpoint");h.cycles=start+dt;h.lastread=history(at);return h.readResource(at);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto at=raw&0xffffff;if(!endpoint(at))throw std::logic_error("Unsupported evolving edit action write endpoint");h.cycles=start+dt;h.lastwrite=history(at);h.writeResource(at,value);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native edit action phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s,j.recipe);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
