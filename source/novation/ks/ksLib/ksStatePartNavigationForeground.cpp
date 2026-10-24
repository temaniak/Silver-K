#include "ksStatePartNavigationForeground.h"
#include "ksStatePlaybackModeForeground.h"
#include "ksControllerState.h"
#include "ksControlArithmetic.h"
#include <stdexcept>
namespace ks {namespace {
using T=StatePartNavigationService;using A=StatePartNavigationForeground::CallerAbi;
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}
void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}
void move8(A&a,uint8_t v){a.flags=uint8_t((a.flags&0xf1)|nz8(v));}
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return T::dataSpan(a,1)||(a>=0x1c0&&a<0x300)||(a>=0x800000&&a<0x900000)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff51||a==0xffff61||a==0xffff62||a==0xfffeb1||a==0xffff8a||a==0xffffa5;}
int packing(uint32_t raw){constexpr unsigned recipes[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==recipes[n])return int(n);return-1;}
struct Resolver{StateFieldCoordinator::Handler field(uint8_t category,uint32_t raw){constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(category>=9||raw!=targets[category])throw std::logic_error("Unsupported evolving part navigation field binding");return StateFieldCoordinator::Handler(category);}StatePackFieldsTask::Kind operator()(uint32_t raw){const auto n=packing(raw);if(n<0)throw std::logic_error("Unsupported evolving part navigation packing binding");return StatePackFieldsTask::Kind(n);}};
}
void StatePartNavigationForeground::project(CallerAbi&a,const T&s){if(!s.complete())throw std::logic_error("Part navigation projection before completion");using K=T::Kind;const auto k=s.kind();const auto&f=s.summary();auto&v=a.values;
 if(k==K::Return||f.release)return;
 if(k==K::Down||k==K::Up){low(v[6],uint8_t(f.presentation));move8(a,uint8_t(f.presentation));}
 if(k!=K::PartStep){low(v[0],f.page);move8(a,f.page);a.flags=controlArithmetic::subtract(int8_t(f.page),int8_t(2),a.flags).flags;if(f.page!=2){a.flags=controlArithmetic::subtract(int8_t(f.page),int8_t(3),a.flags).flags;if(f.page!=3){low(v[0],f.special);move8(a,f.special);if(!f.special){low(v[0],f.mode);move8(a,f.mode);if(f.mode){low(v[0],f.multi);move8(a,f.multi);}}}}
  if(f.sceneEdit){StateFieldForeground::projectSceneEdit(a,*s.field());return;}
 }
 if(f.resolved){v[1]=uint8_t(f.selected<<1);low(v[0],f.enabled);a.flags=uint8_t((a.flags&0xd0)|(f.selected&128?1:0)|nz8(f.enabled));}
 if(f.routed)StatePlaybackModeForeground::projectLower(a,*s.routing());
 if(f.ui)StateFieldForeground::projectUi(a,*s.field());
}
bool StatePartNavigationForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0)return false;T::Kind kind;switch(h.getPC()){case 0x496c:kind=T::Kind::Down;break;case 0x4974:kind=T::Kind::Up;break;case 0x497a:kind=T::Kind::Step;break;case 0x5552:kind=T::Kind::PartStep;break;case 0x559c:kind=T::Kind::Return;break;default:return false;}const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;T::Input in{uint16_t(h.regs[6].r),!(h.ccr&4)};const auto peek=[&](uint32_t raw){const auto at=raw&0xffffff;return at<h.nativeLowSize?h.nativeLowBase[at]:h.memory[at];};if(!T::admitted(kind,in,peek))return false;
  if(kind!=T::Kind::Return){for(unsigned n=0;n<6;++n){uint32_t raw=0;for(unsigned i=0;i<4;++i)raw=(raw<<8)|uint8_t(peek(71306+4*n+i));if(packing(raw)<0)return false;}}job_.emplace(kind,in,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;Resolver resolver;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto at=raw&0xffffff;if(!endpoint(at))throw std::logic_error("Unsupported evolving part navigation read endpoint");h.cycles=start+dt;h.lastread=history(at);return h.readResource(at);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto at=raw&0xffffff;if(!endpoint(at))throw std::logic_error("Unsupported evolving part navigation write endpoint");h.cycles=start+dt;h.lastwrite=history(at);h.writeResource(at,value);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native part navigation phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
