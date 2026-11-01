#include "ksNativeArpOwnedForeground.h"

#include "ksControllerState.h"
#include "ksMidiFileEventBindings.h"
namespace ks {namespace {
struct FullResolver{bool buttonDescriptor(unsigned,unsigned,unsigned){throw std::logic_error("Unexpected preview button descriptor");}ks::StateCentralActionsCoordinator::ButtonAction menuAction(unsigned,unsigned){throw std::logic_error("Unexpected preview menu action");}static void require(bool b,const char*m){if(!b)throw std::logic_error(m);}ks::ScheduledTimerTask::ActionHandle boundAction(ks::StateCentralActionsService::ActionKind k)const{constexpr unsigned targets[]{0x5006,0x55d4,0x55e2,0x46da,0x471a,0x4a80,0x4ac2,0x5e7c,0x5ebe,0x5e80};return{targets[unsigned(k)]};}ks::StateFieldCoordinator::Handler field(uint8_t c,uint32_t raw)const{constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};require(c<9&&raw==targets[c],"Field binding");return ks::StateFieldCoordinator::Handler(c);}bool schema(uint8_t c,uint32_t raw)const{return (c==0||c==2||c==3||c==4||c==6)&&(field(c,raw),true);}bool operator()(uint8_t c,uint32_t raw)const{return schema(c,raw);}bool packing(uint32_t raw)const{constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(auto a:roots)if((raw&0xffffff)==a)return true;return false;}ks::StatePackFieldsTask::Kind operator()(uint32_t raw)const{constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return ks::StatePackFieldsTask::Kind(n);throw std::logic_error("Packing binding");}};
struct FileResolver:FullResolver{ks::MidiFileEventCoordinator::Kind fileMeta(uint8_t i,uint8_t t,uint32_t h)const{return ks::MidiFileEventBindings{}.fileMeta(i,t,h);}};

int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return (a>=0xffee78&&a<0xffee7c)||NativeArpEmissionService::dataSpan(a,1)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff61||a==0xffff62||a==0xffff8a||a==0xffff8c;}
}
bool NativeArpOwnedForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;if(h.getPC()!=0x3770&&h.getPC()!=0x370e)return false;
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;struct A{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;job_.emplace(h.getPC()==0x3770?NativeArpOwnedService::Kind::Main:NativeArpOwnedService::Kind::Schedule,NativeArpOwnedService::Input{uint32_t(h.regs[5].er),uint8_t(h.regs[2].rh)},a,sp,h.indent,h.exr);++started;++kinds[0];}
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto cost=s.nextClocks(h.lastwrite,h.lastread);
 const auto used=s.advanceObservedBefore(cost,[&](uint32_t a,unsigned dt){a&=0xffffff;if(!(a<0x38f||endpoint(a)))throw std::logic_error("Unsupported arp emission read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported arp emission write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},FileResolver{},j.compatibility,h.lastwrite,h.lastread);
 if(!cost||used!=cost)throw std::logic_error("Allocation phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=1u<<unsigned(s.component());
 if(s.complete()){
  const auto&a=j.compatibility.image();if(s.expressionSourceAfter(0)!=a.values[5]||s.activationAfter(0)!=uint8_t(a.values[2]>>8))throw std::logic_error("Main pure context differs from external sink");for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }h.cycles=start+cost;return true;
}
}
