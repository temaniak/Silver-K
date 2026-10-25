#include "ksStateFactoryForeground.h"
#include "ksStateFactoryAbi.h"
#include "ksStateFactoryDspAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return StateFactoryService::dataSpan(a,1)||(a>=0x1c0&&a<0x300)||(a>=0x800000&&a<0x900000)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff51||a==0xffff61||a==0xffff62||a==0xfffeb1;}
struct Resolver{StatePackFieldsTask::Kind operator()(uint32_t raw){constexpr uint32_t recipes[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==recipes[n])return StatePackFieldsTask::Kind(n);throw std::logic_error("Unsupported factory packing binding");}bool operator()(uint8_t category,uint32_t raw){constexpr uint32_t targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};return category<9&&raw==targets[category];}};
}
void StateFactoryForeground::project(CallerAbi&a,const StateFactoryService&s){projectStateFactory(a,s);}void StateFactoryForeground::project(CallerAbi&a,const StateFactoryDspService&s){projectStateFactoryDsp(a,s);}
bool StateFactoryForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x18a00||!h.interruptMode2||h.indent<=0)return false;Kind kind;switch(h.getPC()){case 0x66dc:kind=Kind::Provided;break;case 0x66ea:kind=Kind::Factory;break;case 0x69c6:kind=Kind::GlobalLoad;break;case 0x7666:kind=Kind::Reset;break;case 0x76a6:kind=Kind::Upload;break;case 0x77a0:kind=Kind::Poll;break;case 0x77ae:kind=Kind::Stream;break;default:return false;}const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto peek=[&](uint32_t raw){const auto at=raw&0xffffff;return at<h.nativeLowSize?h.nativeLowBase[at]:h.memory[at];};
  if(unsigned(kind)<3){StateFactoryService::Input in{uint32_t(h.regs[1].er)};if(!StateFactoryService::admitted(StateFactoryService::Kind(unsigned(kind)),in,peek))return false;job_.emplace(kind,in,sp,h.indent);}else {StateFactoryDspService::Input in{uint32_t(h.regs[1].er),uint32_t(h.regs[2].er),uint16_t(h.regs[3].e),h.regs[5].er==0x600000?DspPort::B:DspPort::A};if(kind==Kind::Stream&&(h.regs[5].er!=0x400000&&h.regs[5].er!=0x600000))return false;if(!StateFactoryDspService::admitted(StateFactoryDspService::Kind(unsigned(kind)-3),in,peek))return false;job_.emplace(kind,in,sp,h.indent);}++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;const auto start=h.cycles;const auto previous=h.lastwrite;Resolver resolver;unsigned cost=0;
 std::visit([&](auto&s){cost=s.nextClocks(previous);const auto priority=s.writePriority();const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving factory read resource");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving factory write resource");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,value);},resolver,previous);if(!cost||used!=cost)throw std::logic_error("Native factory phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;if(!priority&&s.writePriority()){j.savedPriority=h.exr;h.exr|=6;}else if(priority&&!s.writePriority())h.exr=j.savedPriority;++phases;using T=std::decay_t<decltype(s)>;components|=s.components()<<(std::is_same_v<T,StateFactoryService>?0:5);
  if(s.complete()){CallerAbi result;for(unsigned n=0;n<7;++n)result.values[n]=uint32_t(h.regs[n].er);result.flags=h.ccr;project(result,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(result.values[n]);h.ccr=result.flags;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;}
 },j.work);
 const bool done=std::visit([](const auto&s){return s.complete();},j.work);if(done){job_.reset();++completed;}h.cycles=start+cost;return true;
}
}
