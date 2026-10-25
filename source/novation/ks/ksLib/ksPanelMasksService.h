#pragma once
#include "ksPanelMasksTask.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
// Physical mask/invert publication and its exact strobe cadence. The activation
// byte is a domain outcome; no instruction, register or condition-code frame.
class PanelMasksService final {
public:
 using Kind=PanelMasksTask::Kind;
 struct Input {uint8_t activation=0;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t activation=0;bool ran=false,changed=false;};
 explicit PanelMasksService(Kind k=Kind::Service):PanelMasksService(k,Input{}){}
 PanelMasksService(Kind k,Input i):task_(k),input_(i){summary_.activation=k==Kind::Invert?i.activation:0;}
 Kind kind()const{return task_.kind();}bool complete()const{return task_.stage()==PanelMasksTask::Stage::Complete;}
 bool critical()const{return false;}const PanelMasksTask&maskTask()const{return task_;}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,PanelMasksService>)return this;else if constexpr(std::is_same_v<T,PanelMasksTask>)return &task_;else return nullptr;}
 unsigned nextClocks(int=-1)const{return task_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int=-1){
  const auto cost=nextClocks();if(!cost||cost>budget)return 0;
  using T=PanelMasksTask;using S=T::Stage;const auto stage=task_.stage();effect_={};
  if(stage==S::LocateMask||stage==S::LocateInvert)summary_.ran=true;
  if(stage==S::InitializeActivation)summary_.activation=255;
  if(stage==S::ClearInvertValue)summary_.activation=0;
  if(stage==S::ShiftActivation)summary_.activation>>=1;
  if(stage==S::ShiftInvert)summary_.activation=uint8_t((uint16_t(task_.mask())<<8|summary_.activation)>>1);
  if(stage==S::WriteMask||stage==S::WriteInvert)summary_.changed=true;
  effect_.write=stage==S::HoldMaskDelay||stage==S::HoldInvertDelay;
  effect_.read=stage==S::RestoreMaskDelay||stage==S::RestoreInvertDelay||stage==S::FinishEmpty||stage==S::FinishMask||stage==S::FinishInvert;
  const auto decay=[&](){return (task_.inverted()?0xffec24u:0xffec16u)+task_.index();};
  return task_.advanceBefore(cost,[&](T::Read f){
   if(f==T::Read::ActivationInput)return input_.activation;
   uint32_t at=0;switch(f){case T::Read::PendingMask:at=0xffec1b;break;case T::Read::PendingInvert:at=0xffec29;break;case T::Read::Decay:at=decay();break;case T::Read::Previous:at=task_.inverted()?0xffec28:0xffec1a;break;case T::Read::Toggle:at=0xffec20;break;case T::Read::Other:at=task_.inverted()?0xffec1a:0xffec28;break;case T::Read::Port:at=0xffff62;break;default:throw std::logic_error("Unknown panel mask observation");}
   unsigned dt=cost;if(stage==S::SetActivation||stage==S::SetInvertActivation)dt-=3;
   if(stage==S::ReadMaskDecay||stage==S::ReadInvertDecay)dt-=2;
   if(f==T::Read::Port)dt-=1;
   return uint8_t(read(at,dt));
  },[&](T::Write f,uint8_t value){uint32_t at;
   switch(f){case T::Write::Pending:at=task_.inverted()?0xffec29:0xffec1b;break;case T::Write::Decay:at=decay();break;case T::Write::Mask:at=task_.inverted()?0xffec28:0xffec1a;break;case T::Write::Data:at=0xffff61;break;default:at=0xffff62;break;}
   write(at,value,cost);
  });
 }
private:PanelMasksTask task_;Input input_;Summary summary_{};Effect effect_{};
};
}
