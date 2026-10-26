#pragma once
#include "ksTimingTablesTask.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Actual reciprocal/multiply table initialization, with typed child ownership.
class StartupTimingService final {
public:
 using Kind=TimingTablesTask::Kind;
 struct Effect{bool read=false,write=false;};
 explicit StartupTimingService(Kind k=Kind::Initialize):parent_(k){}
 bool complete()const{return parent_.stage()==TimingTablesTask::Stage::Complete&&!child_;}
 unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 uint16_t counter()const{return parent_.counter();}
 unsigned nextClocks(int=-1)const{return current().nextClocks();}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&read,Write&&write,int=-1){
  const auto cost=nextClocks();if(!cost||budget<cost)return 0;effect_={true,false};
  auto&t=current();using T=TimingTablesTask;const auto stage=t.stage();
  t.advanceBefore(cost,[&](T::Read q){const auto at=q==T::Read::Counter?0xffffe6u:q==T::Read::Rate?0x1512eu:0xf20cu+2*t.index();const unsigned gap=at>=0xffffa0?1u:0u;const unsigned back=q==T::Read::Table?2u:0u;return uint16_t((unsigned(uint8_t(read(at,cost-back-gap)))<<8)|uint8_t(read(at+1,cost-back)));},[&](unsigned n,uint32_t value){const auto at=(t.kind()==Kind::Reciprocal?0x159f4u:0x15a7cu)+4*n;for(unsigned b=0;b<4;++b)write(at+b,uint8_t(value>>(8*(3-b))),cost-3*((3-b+(at+b)%2)/2));});
  if(child_&&child_->stage()==T::Stage::Complete){child_.reset();if(!parent_.resumeChild())throw std::logic_error("Startup timing parent return");}
  else if(!child_&&parent_.child()!=T::Child::None){effect_.write=true;child_.emplace(parent_.child()==T::Child::Reciprocal?Kind::Reciprocal:Kind::Multiply);}
  if(stage==T::Stage::Finish)effect_.read=true;return cost;
 }
private:
 TimingTablesTask&current(){return child_?*child_:parent_;}const TimingTablesTask&current()const{return child_?*child_:parent_;}
 TimingTablesTask parent_;std::optional<TimingTablesTask>child_;Effect effect_{};
};
}
