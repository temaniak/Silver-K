#pragma once
#include "ksHardwareSetupTask.h"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace ks {
// A physical reset-publication owner. The calibrated task owns order/delays;
// callbacks publish only real resources, at phase-relative timestamps.
class StartupHardwareService final {
public:
 using Kind=HardwareSetupTask::Kind;
 struct Effect{bool read=false,write=false;};
 explicit StartupHardwareService(Kind kind,unsigned foregroundMask=0):task_(kind),mask_(foregroundMask){if(mask_>7)throw std::logic_error("Startup hardware priority range");}
 bool complete()const{return task_.complete();}
 unsigned priority()const{return mask_;}
 const Effect&effect()const{return effect_;}
 const HardwareSetupTask&task()const{return task_;}
 static uint32_t address(HardwareSetupTask::Field f){using F=HardwareSetupTask::Field;switch(f){
 case F::P3Direction:return 0xffff39;case F::BusControl:return 0xffffac;
 case F::PBControl:return 0xfffeb1;case F::InputSelect:return 0xffff62;
 case F::PCControl:return 0xfffeb2;case F::PEControl:return 0xfffeb5;
 case F::PHControl:return 0xfffeb9;case F::P9Direction:return 0xffff45;
 case F::PortFDirection:return 0xfffebe;case F::SystemControl:return 0xffff6f;
 case F::PortGDirection:return 0xfffebf;case F::WaitControl:return 0xfffed1;
 case F::Wait0:return 0xfffed2;case F::Wait1:return 0xfffed3;
 case F::WaitEnable:return 0xfffed4;case F::DramMode:return 0xfffec8;
 case F::DramRefresh:return 0xfffec9;case F::P4Direction:return 0xffff3c;
 case F::P4Data:return 0xffff3d;case F::TimerPeriod:return 0xffffd8;
 case F::TimerControl:return 0xffffd0;case F::TimerEnable:return 0xffffd4;
 case F::ClockCounter:return 0xffec10;case F::InterruptControl:return 0xffffe0;
 case F::PriorityControl:return 0xfffff1;case F::ClockMode:return 0xfffff6;
 case F::InterruptMode:return 0xffffc0;default:throw std::logic_error("No startup publication resource");}}
 static unsigned byteClocks(uint32_t a,int previous){a&=0xffffff;
  if(a<0x400000)return !(previous&1)&&a==(uint32_t(previous)|1u)?0u:3u;
  if(a>=0xfffd10&&a<0xffffa0)return !(previous&1)&&a==(uint32_t(previous)|1u)?0u:1u;
  return a>=0xffffa0?1u:3u;
 }
 static int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
 static unsigned publicationClocks(HardwareSetupTask::Publication p,int previous){unsigned clocks=0;const auto a=address(p.field);for(unsigned n=0;n<p.width;++n){clocks+=byteClocks(a+n,previous);previous=history(a+n);}return clocks;}
 unsigned nextClocks(int previousWrite=-1)const{return task_.nextClocks(task_.phase()==HardwareSetupTask::Phase::Publish?publicationClocks(task_.publication(),previousWrite):0);}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&,Write&&write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||budget<cost)return 0;
  effect_={true,false};
  const auto used=task_.advanceBefore(cost,[&](HardwareSetupTask::Publication p){
   const auto a=address(p.field);unsigned remaining=publicationClocks(p,previousWrite);int previous=previousWrite;
   for(unsigned n=0;n<p.width;++n){const auto byteCost=byteClocks(a+n,previous);remaining-=byteCost;write(a+n,uint8_t(p.value>>(8*(p.width-n-1))),cost-remaining);previous=history(a+n);}
  },[&](HardwareSetupTask::Publication p){return publicationClocks(p,previousWrite);});
  if(used!=cost)throw std::logic_error("Startup hardware phase ownership");return cost;
 }
private:HardwareSetupTask task_;unsigned mask_=0;Effect effect_{};
};
}
