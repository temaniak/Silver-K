#pragma once
#include "ksMidiFileTimeTask.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
class MidiFileTimeService final {
public:
 using Kind=MidiFileTimeTask::Kind;struct Input{uint32_t stream=0,tempo=0,record=0;};struct Effect{bool read=false,write=false;};
 struct Summary{uint32_t stream=0,value=0,now=0,deadline=0,published=0,lastAddBase=0;uint16_t division=0,period=0;std::array<uint8_t,4>vlqBytes{};unsigned vlqCount=0;bool due=false;};
 explicit MidiFileTimeService(Kind k):MidiFileTimeService(k,Input{}){}
 MidiFileTimeService(Kind k,Input i):kind_(k),input_(i),parent_(k,i.stream,i.tempo,i.record){}
 uint32_t components()const{return components_;}Kind kind()const{return kind_;}const Input&input()const{return input_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}bool complete()const{return parent_.complete();}bool critical()const{return false;}unsigned priority()const{return 0;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,MidiFileTimeTask>)return activeVlq_?&*vlq_:&parent_;else return nullptr;}
 unsigned nextClocks(int previous=-1,int=-1)const{const auto&t=current();return t.nextClocks(busCost(t,previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1,int=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};using T=MidiFileTimeTask;using S=T::Stage;auto&t=current();const auto stage=t.stage();
 if(stage==S::VlqAddLast)summary_.lastAddBase=t.value();
 const auto transferRead=[&](uint32_t raw,unsigned width,unsigned post)->uint32_t{if(!readable(raw,width)&&raw!=0xffee82)throw std::logic_error("Unsupported MIDI-file time read span");const auto a=raw&0xffffff;const auto total=accessCost(a,width,-1);unsigned elapsed=cost-post-total;uint32_t value=0;int p=-1;for(unsigned n=0;n<width;++n){elapsed+=byteCost(a+n,p);p=int(a+n);value=(value<<8)|uint8_t(read((a+n)&0xffffff,elapsed));}return value;};
 t.advanceBefore(cost,[&](S q)->uint32_t{switch(q){case S::VlqReadFirst:case S::VlqReadSecond:case S::VlqReadThird:case S::VlqReadFourth:{const auto v=uint8_t(transferRead(t.stream(),1,2));if(summary_.vlqCount==4)throw std::logic_error("MIDI-file VLQ exceeds four observations");summary_.vlqBytes[summary_.vlqCount++]=v;return v;}case S::DelayReadDeadline:return summary_.deadline=transferRead(0x17322,4,0);case S::ArpReadDeadline:return summary_.deadline=transferRead(t.timeRecord()+56,4,0);case S::DueReadNow:return summary_.now=transferRead(0xffee82,4,0);case S::DueReadDeadline:return summary_.deadline=transferRead(0x17322,4,0);case S::TempoReadDivision:return summary_.division=uint16_t(transferRead(0x17326,2,0));default:throw std::logic_error("Unknown MIDI-file time observation");}},[&](S q,uint32_t v){const auto raw=q==S::DelayPublishDeadline?0x17322:q==S::TempoPublish?0xffee86:t.timeRecord()+56;const auto a=raw&0xffffff;if(!readable(raw,4)&&raw!=0xffee86)throw std::logic_error("Unsupported MIDI-file time write span");const auto total=accessCost(a,4,previous);unsigned elapsed=cost-total;int p=previous;for(unsigned n=0;n<4;++n){elapsed+=byteCost(a+n,p);p=int(a+n);write((a+n)&0xffffff,uint8_t(v>>(24-8*n)),elapsed);}summary_.published=v;},[&](S){return busCost(t,previous);});
 effect_.write=stage==S::DelayReadDelta||stage==S::ArpReadDelta;effect_.read=stage==S::DelayFinish||stage==S::DueFinish||stage==S::VlqFinish||stage==S::TempoFinish||stage==S::ArpFinish;
 if(t.awaitingChild()){vlq_.emplace(Kind::Vlq,t.stream());activeVlq_=true;components_|=2;}
 else if(t.complete()&&activeVlq_){parent_.resumeVlq(t.value(),t.stream());activeVlq_=false;}
 if(parent_.complete()){summary_.stream=parent_.stream();summary_.value=parent_.value();summary_.period=parent_.period();summary_.due=parent_.due();}return cost;
 }
 static bool readable(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return n&&((a>=0xb8e8&&a<0x200000&&n<=0x200000-a)||(a>=0x800000&&a<0x900000&&n<=0x900000-a)||(a>=0xa0b8e8&&a<0xc00000&&n<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&n<=0xfff000-a));}
private:
 MidiFileTimeTask&current(){return activeVlq_?*vlq_:parent_;}const MidiFileTimeTask&current()const{return activeVlq_?*vlq_:parent_;}
 static unsigned byteCost(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static unsigned accessCost(uint32_t raw,unsigned n,int p){unsigned c=0;for(unsigned k=0;k<n;++k){const auto a=(raw+k)&0xffffff;c+=byteCost(a,p);p=int(a);}return c;}
 static unsigned busCost(const MidiFileTimeTask&t,int previous){using S=MidiFileTimeTask::Stage;switch(t.stage()){case S::VlqReadFirst:case S::VlqReadSecond:case S::VlqReadThird:case S::VlqReadFourth:return 3;case S::DelayReadDeadline:case S::DueReadDeadline:return 6;case S::DueReadNow:return 12;case S::TempoReadDivision:return 3;case S::ArpReadDeadline:return accessCost(t.timeRecord()+56,4,-1);case S::DelayPublishDeadline:return accessCost(0x17322,4,previous);case S::ArpPublishDeadline:return accessCost(t.timeRecord()+56,4,previous);case S::TempoPublish:return 12;default:return 0;}}
 uint32_t components_=1;Kind kind_;Input input_;MidiFileTimeTask parent_;std::optional<MidiFileTimeTask>vlq_;bool activeVlq_=false;Effect effect_{};Summary summary_{};
};
}
