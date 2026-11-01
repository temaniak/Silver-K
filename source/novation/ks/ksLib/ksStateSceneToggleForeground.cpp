#include "ksStateSceneToggleForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
using T=StateSceneToggleService;using A=StateSceneToggleForeground::CallerAbi;
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}
void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}
void move8(A&a,unsigned n,uint8_t b){low(a.values[n],b);a.flags=uint8_t((a.flags&0xf1)|nz8(b));}
void scene(A&a,const std::optional<StateSceneCoordinator::Summary>&s){if(!s)throw std::logic_error("Missing Scene action completion");StateSceneForeground::project(a,*s,[](auto&,const auto&){throw std::logic_error("Unexpected Scene action voice outcome");},[](auto&,const auto&){throw std::logic_error("Unexpected Scene action UI outcome");});}
void save(A&a,const StateSaveService&s,StateSaveService::Input in){StateSaveForeground::CallerAbi result;result.values=a.values;result.flags=a.flags;StateSaveForeground::project(result,s,in);a.values=result.values;a.flags=result.flags;}
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return StateSceneCoordinator::dataSpan(a,1)||(a>=0x1c0&&a<0x300)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffffa5||a==0xfffeb1||a==0xffff51||a==0xffff61||a==0xffff62||(a>=0xffff80&&a<0xffffa0);}
struct Resolver{
 StateFieldCoordinator::Handler field(uint8_t c,uint32_t raw){constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(c>=9||raw!=targets[c])throw std::logic_error("Unsupported Scene action field handler");return StateFieldCoordinator::Handler(c);}
 StatePackFieldsTask::Kind operator()(uint32_t raw){constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return StatePackFieldsTask::Kind(n);throw std::logic_error("Unsupported Scene action packing descriptor");}
 bool operator()(uint8_t c,uint32_t raw){return (c==0||c==2||c==3||c==4||c==6)&&(field(c,raw),true);}
};
}
void StateSceneToggleForeground::project(CallerAbi&a,const StateSceneToggleService&s){
 const auto&f=s.facts();
 const auto commit=[&]{scene(a,s.bank());a.values[6]=f.commitPage;a.values[5]=0x1738e;a.flags&=0xd0;if(!s.committed())throw std::logic_error("Missing completed Scene Flash transaction");save(a,*s.committed(),s.commitInput());};
 if(s.kind()==Kind::Commit){commit();return;}
 move8(a,0,f.originalBank);if(!f.originalBank){move8(a,0,0);return;}
 move8(a,0,f.originalMode);
 if(!f.originalMode){
  if(!s.checked())throw std::logic_error("Missing completed Scene availability check");save(a,*s.checked(),s.checkInput());if(!f.available)return;
  move8(a,0,1);move8(a,0,0);scene(a,s.backup());
 }else if(f.originalMode==1){
  move8(a,0,2);if(!s.begun())throw std::logic_error("Missing completed Scene name editor");
  StateSceneEditForeground::project(a,*s.begun(),[](auto&,const auto&,auto){throw std::logic_error("Unexpected name-begin Scene child");},[](auto&,const auto&){throw std::logic_error("Unexpected name-begin UI child");},[](auto&,const auto&){throw std::logic_error("Unexpected name-begin display child");});
 }else {
  low(a.values[0],uint8_t(f.originalMode-1));scene(a,s.loaded());scene(a,s.copied());
  const auto*name=s.store();if(!name||!name->complete())throw std::logic_error("Missing Scene name publication");a.values[0]=name->storage()+16;a.values[1]=0x1734c;a.values[2]=uint16_t(a.values[2])&0xff00;a.flags=uint8_t((a.flags&0xf1)|4);
  commit();move8(a,0,0);a.values[6]=0x150fc;
 }
 if(f.refresh){if(!s.field())throw std::logic_error("Missing Scene action UI completion");StateFieldForeground::projectUi(a,*s.field());}
 else if(f.notice){if(!s.save())throw std::logic_error("Missing Scene saved notice");save(a,*s.save(),s.saveInput());}
}
bool StateSceneToggleForeground::step(ControllerCpu&h){
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17600||!h.interruptMode2||h.indent<=0)return false;
  Kind k;if(h.getPC()==0xb83c)k=Kind::Toggle;else if(h.getPC()==0xb696)k=Kind::Commit;else return false;
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  StateSceneToggleService::Input in;in.event=uint16_t(h.regs[6].r);in.noticeAction.value=0x55d4;
  const auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};if(!StateSceneToggleService::admitted(k,in,peek))return false;
  constexpr unsigned recipes[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n){uint32_t raw=0;for(unsigned j=0;j<4;++j)raw=(raw<<8)|uint8_t(peek(71306+4*n+j));bool found=false;for(auto v:recipes)found|=(raw&0xffffff)==v;if(!found)return false;}
  job_.emplace(k,in,sp,h.indent,h.exr);++started;++kinds[unsigned(k)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;const auto priority=s.writePriority();Resolver resolver;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving Scene action read endpoint");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving Scene action write endpoint");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,value);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native Scene action phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;
 if(!priority&&s.writePriority()){j.writePriority=h.exr;h.exr|=6;}else if(priority&&!s.writePriority())h.exr=j.writePriority;else if(!s.writePriority())h.exr=s.critical()?7:j.priority;
 ++phases;components|=s.components();
 if(s.complete()){
  CallerAbi out;for(unsigned n=0;n<7;++n)out.values[n]=uint32_t(h.regs[n].er);out.flags=h.ccr;project(out,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(out.values[n]);h.ccr=out.flags;
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;h.exr=j.priority;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
