#pragma once
#include "ksStateGlobalsTask.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
// File playback temporarily backs up global settings and installs the factory
// globals. Returning playback restores the exact21-byte backup, withoutFlash.
class MidiFileGlobalsService final {
public:
 enum class Kind{BackupFactory,Restore};struct Effect{bool read=false,write=false;};
 struct Summary{uint8_t backupLast=0,factoryLast=0;unsigned copied=0,factoryCopied=0;};
 explicit MidiFileGlobalsService(Kind k):kind_(k),task_(k==Kind::BackupFactory?StateGlobalsTask::Kind::Factory:StateGlobalsTask::Kind::RestoreBackup){}
 Kind kind()const{return kind_;}const Summary&summary()const{return summary_;}bool complete()const{return task_.complete();}bool critical()const{return false;}const Effect&effect()const{return effect_;}const StateGlobalsTask&policy()const{return task_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateGlobalsTask>)return complete()?nullptr:&task_;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{return task_.nextClocks(byteClock(target(),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};using T=StateGlobalsTask;const auto stage=task_.stage();const auto offset=task_.byteOffset();task_.advanceBefore(cost,[&](T::Read q)->uint32_t{if(q==T::Read::Byte)return summary_.backupLast=uint8_t(read((kind_==Kind::BackupFactory?0x169e6u:0x16dfcu)+offset,cost-2));if(q==T::Read::FactoryByte)return summary_.factoryLast=uint8_t(read(0x81fe00u+offset,cost-2));throw std::logic_error("Unexpected file global source");},[&](T::Write q,uint8_t value){if(q==T::Write::Byte){write((kind_==Kind::BackupFactory?0x16dfcu:0x169e6u)+offset,value,cost);++summary_.copied;}else if(q==T::Write::FactoryByte){write(0x169e6u+offset,value,cost);++summary_.factoryCopied;}else if(q==T::Write::FactoryMarker)write(0x169ef,value,cost);else throw std::logic_error("Unexpected file global target");},byteClock(target(),previous));effect_.read=stage==T::Stage::ReturnCopy;return cost;}
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 uint32_t target()const{using S=StateGlobalsTask::Stage;return task_.stage()==S::WriteFactoryByte?0x169e6u+task_.byteOffset():task_.stage()==S::WriteGlobal?0x169efu:(kind_==Kind::BackupFactory?0x16dfcu:0x169e6u)+task_.byteOffset();}
 Kind kind_;StateGlobalsTask task_;Summary summary_{};Effect effect_{};
};
template<class A>void projectMidiFileGlobals(A&a,const MidiFileGlobalsService&s){if(!s.complete())throw std::logic_error("Incomplete file global outcome");const auto last=s.kind()==MidiFileGlobalsService::Kind::BackupFactory?s.summary().factoryLast:s.summary().backupLast;a.values[0]=s.kind()==MidiFileGlobalsService::Kind::BackupFactory?0x168fc:0x16e11;a.values[1]=s.kind()==MidiFileGlobalsService::Kind::BackupFactory?0x16901:0x169fb;a.values[2]=(a.values[2]&0xff00u)|last;a.flags=uint8_t((a.flags&0xf1)|(s.kind()==MidiFileGlobalsService::Kind::Restore?4:0));}
}
