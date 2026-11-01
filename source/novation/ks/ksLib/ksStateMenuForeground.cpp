#include "ksStateMenuForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}
uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}
uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool dataSpan(uint32_t a,unsigned count){a&=0xffffff;return count&&((a>=0xb8e8&&a<0x200000&&count<=0x200000-a)||(a>=0xa0b8e8&&a<0xc00000&&count<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&count<=0xfff000-a));}
uint32_t repeatAddress(StateMenuService::RepeatAction action){return action==StateMenuService::RepeatAction::Previous?0x46da:0x471a;}
uint32_t tailAddress(StateMenuService::Exit exit){using E=StateMenuService::Exit;switch(exit){case E::Release:return 0x4862;case E::RefreshToggle:return 0x6c3c;case E::Auxiliary:return 0xb83c;case E::ReleaseNavigation:return 0x5eb0;case E::PreviousName:return 0x75a0;case E::PreviousSpecial:return 0xb60c;case E::NextName:return 0x7548;case E::NextSpecial:return 0xb5e4;case E::Edit:return 0x5db2;case E::PreviousProgram:return 0x58e4;case E::NextProgram:return 0x58ce;default:return 0;}}
uint32_t tailHistory(StateMenuService::Kind kind,StateMenuService::Exit exit){using K=StateMenuService::Kind;using E=StateMenuService::Exit;switch(exit){case E::Release:return kind==K::Toggle?0x452b:0x453d;case E::RefreshToggle:return 0x4539;case E::Auxiliary:return 0x4541;case E::ReleaseNavigation:return kind==K::Previous?0x46af:0x46ef;case E::PreviousName:return 0x46bb;case E::PreviousSpecial:return 0x46c5;case E::NextName:return 0x46fb;case E::NextSpecial:return 0x4705;case E::Edit:return kind==K::Previous?0x46d3:0x4713;case E::PreviousProgram:return 0x46eb;case E::NextProgram:return 0x472b;default:return 0;}}
}
bool StateMenuForeground::step(ControllerCpu&h){
 using T=StateMenuService;using K=T::Kind;using C=T::Component;using M=StateMenuActionsTask::Stage;using E=StateEditContextTask::Stage;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(h.getPC()){case 0x4528:kind=K::Toggle;break;case 0x453a:kind=K::Auxiliary;break;case 0x46ac:kind=K::Previous;break;case 0x46da:kind=K::RepeatPrevious;break;case 0x46ec:kind=K::Next;break;case 0x471a:kind=K::RepeatNext;break;case 0x4efe:kind=K::EditActive;break;case 0x4134:kind=K::RepeatArm;break;case 0x12fa:kind=K::Clock;break;case 0x5a46:kind=K::BindContext;break;case 0x5a7c:kind=K::ClearContext;break;case 0x52ac:kind=K::IndexedAddress;break;case 0x52e6:kind=K::DirectAddress;break;case 0x52fc:kind=K::GlobalAddress;break;default:return false;}
  const auto stack=h.getSP();if((stack&1)||stack<0xfff400||stack>0xfffbfc)return false;
  T::Input input;input.event=uint16_t(h.regs[6].r);input.interval=input.event;input.pressed=!(h.ccr&4);input.index=uint32_t(h.regs[1].er);input.descriptor=uint32_t(h.regs[0].er);
  if(kind==K::RepeatArm){const auto action=uint32_t(h.regs[5].er);if(action!=0x46da&&action!=0x471a)return false;input.action=action==0x46da?T::RepeatAction::Previous:T::RepeatAction::Next;}
  if(kind==K::BindContext&&!dataSpan(input.index+71586,2))return false;
  if(kind==K::IndexedAddress||kind==K::DirectAddress||kind==K::GlobalAddress){
   if(!dataSpan(input.descriptor+2,258))return false;
   if(kind!=K::GlobalAddress){
    if(h.nativeLowSize<=0x167bf)return false;
    uint32_t base=0;for(unsigned n=0;n<4;++n)base=(base<<8)|h.nativeLowBase[0x167bc+n];
    const uint8_t*descriptor=nullptr;const auto at=(input.descriptor+2)&0xffffff;
    if(at<0x200000){if(at>=h.nativeLowSize)return false;descriptor=h.nativeLowBase+at;}else descriptor=h.memory+at;
    if(!dataSpan(base,258)||(kind==K::IndexedAddress&&*descriptor!=255&&!dataSpan(base+*descriptor,2)))return false;
   }
  }
  job_.emplace(kind,input,stack,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&service=job.service;
 const auto cost=service.nextClocks(h.lastwrite);const auto start=h.cycles;bool retained=false,released=false;
 if(service.component()==C::Menu){const auto stage=service.menuTask()->stage();retained=stage==M::ObservePreviousEdit||stage==M::ObserveNextEdit||stage==M::SchedulePrevious||stage==M::ScheduleNext;}
 if(service.component()==C::RepeatTimer){retained=service.timerPhase()==2;released=service.timerPhase()==4;}
 if(service.component()==C::EditGate)released=service.gatePhase()==1;
 if(service.component()==C::Clock)released=service.clockTask()->stage()==ControllerClockReadTask::Stage::Return;
 if(service.component()==C::Context){switch(service.contextTask()->stage()){case E::SaveContextValue:case E::SaveContextIndex:case E::SaveClearValue:retained=true;break;case E::RestoreContextIndex:case E::RestoreContextValue:case E::ReturnContext:case E::RestoreClearValue:case E::ReturnClear:released=true;break;default:break;}}
 const auto previousWrite=h.lastwrite;
 const auto writeByte=[&](uint32_t at,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(at);h.writeResource(at,value);};
 const auto used=service.advanceBefore(cost,[&](uint32_t at,unsigned offset){h.cycles=start+offset;h.lastread=history(at);return h.readResource(at);},writeByte,[&](T::RepeatAction action,unsigned offset){const auto value=repeatAddress(action);for(unsigned n=0;n<4;++n)writeByte(0x16774+n,uint8_t(value>>(24-8*n)),offset-3*((3-n)/2));},previousWrite);
 if(!cost||used!=cost)throw std::logic_error("State menu phase admission differs");
 if(retained)h.lastwrite=-1;if(released)h.lastread=-1;++phases;components|=service.components();
 if(service.complete()){
  const auto kind=service.kind();uint8_t flags=h.ccr;
  if(unsigned(kind)<6){
   const auto exit=service.exit();const auto*menu=service.menuTask();
   if(kind==K::Toggle&&exit==T::Exit::RefreshToggle){h.regs[0].er=0x167a1;flags&=0xf1;}
   else if(kind==K::Previous||kind==K::Next){
    if(exit!=T::Exit::ReleaseNavigation){
     const auto difference=controlArithmetic::subtract(int8_t(menu->page()),int8_t(2),h.ccr);flags=difference.flags;
     h.regs[0].rl=int8_t(exit==T::Exit::PreviousName||exit==T::Exit::NextName?menu->page():menu->special());
     if(exit!=T::Exit::PreviousName&&exit!=T::Exit::NextName)flags=uint8_t((flags&0xf1)|nz8(menu->special()));
     if(service.components()&(1u<<unsigned(C::EditGate))){h.regs[1].rl=int8_t(service.edit());flags=uint8_t((flags&0xf1)|nz8(service.edit()));}
     if(exit==T::Exit::Edit){h.regs[6].r=int16_t(kind==K::Previous?0xffff:0x1ff);flags=uint8_t((flags&0xf1)|nz16(uint16_t(h.regs[6].r)));}
    }
   }
   if(exit==T::Exit::PreviousProgram||exit==T::Exit::NextProgram){h.regs[5].er=int32_t(repeatAddress(service.repeatAction()));h.regs[6].er=int32_t(service.observedClock());flags=uint8_t((flags&0xf1)|nz16(uint16_t(service.observedClock())));}
  }else {switch(kind){
   case K::EditActive:h.regs[1].rl=int8_t(service.edit());flags=uint8_t((flags&0xf1)|nz8(service.edit()));break;
   case K::RepeatArm:h.regs[6].er=int32_t(service.observedClock());flags=uint8_t((flags&0xf1)|nz16(uint16_t(service.observedClock())));break;
   case K::Clock:h.regs[6].er=int32_t(service.observedClock());flags=uint8_t((flags&0xf1)|nz32(service.observedClock()));break;
   case K::BindContext:{const auto*t=service.contextTask();uint8_t bits=controlArithmetic::subtract(int8_t(t->kind()),int8_t(126),0).flags&0x21;if(t->kind()<126&&t->screen())bits=0;flags=uint8_t((flags&0xd0)|bits|nz32(uint32_t(h.regs[0].er)));break;}
   case K::ClearContext:flags=uint8_t((flags&0xd0)|nz32(uint32_t(h.regs[0].er)));break;
   case K::IndexedAddress:case K::DirectAddress:case K::GlobalAddress:{const auto*t=service.addressTask();const auto result=controlArithmetic::add(int32_t(t->offset()),int32_t(t->base()),flags);h.regs[1].er=result.value;if(kind!=K::GlobalAddress)h.regs[2].er=int32_t(t->base());flags=result.flags;break;}
   default:break;
  }}
  h.ccr=flags;
  if(service.exit()==T::Exit::Return){uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}h.setSP(job.caller+4);h.pc=target&0xffffff;--h.indent;}
  else {h.lastread=int(tailHistory(kind,service.exit()));h.pc=tailAddress(service.exit());}
  job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
