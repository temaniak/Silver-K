#pragma once
#include "kstationVoiceSyncTask.h"
#include "kstationNativeVoiceUpdate.h"
#include <array>
#include <optional>
namespace kstation {
class NativeVoiceSync final {
 using T=VoiceSyncTask;using S=T::Stage;
 struct Job{T task;uint32_t expected=0;explicit Job(T::Kind k):task(k){}};
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1>CodeRanges{{{0x8366,0x837c}}};static bool candidate(uint32_t pc){return pc>=0x8366&&pc<0x837c;}bool active()const{return depth_!=0;}unsigned depth()const{return depth_;}void reset(){for(auto&j:jobs_)j.reset();depth_=0;}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c))return false;auto at=uint32_t(c.getPC());if(depth_&&jobs_[depth_-1]->task.awaitingDsp()){auto&j=*jobs_[depth_-1];if(at==j.expected)j.task.resumeDsp();else if(at!=0x8366&&at!=0x836a)return false;}if(!depth_||boundary(*jobs_[depth_-1])!=at){if(at!=0x8366&&at!=0x836a){if(candidate(at))throw std::logic_error("Unowned K voice sync boundary");return false;}if(depth_==jobs_.size())throw std::logic_error("K voice sync capacity");jobs_[depth_++].emplace(at==0x8366?T::Kind::First:T::Kind::Second);}auto&j=*jobs_[depth_-1];auto s=j.task.stage();if(c.pending_irqs)c.read8(0xfffff2);unsigned len=s==S::SetAddress||s==S::Dispatch?4:2;T::Credits cr{s==S::Dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):0,s==S::Return?5+bus(c.getSP(),4,int(at+len-1)):0};auto cost=j.task.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::SetValue:c.reg8(1)=int8_t(j.task.value());val(c,c.reg8(1));break;case S::SetAddress:c.reg16(4)=int16_t(0x802a);val(c,c.reg16(4));break;case S::ClearHigh:c.reg16(9)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;case S::Dispatch:j.expected=at+len;call(c,0x165e,j.expected);break;case S::Return:ret(c);break;default:break;}if(j.task.advanceBefore(cost,cr)!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice sync phase clocks differ");if(j.task.complete()){jobs_[--depth_].reset();return true;}if(!j.task.awaitingDsp())c.pc=c.makepc(int(boundary(j)));return true;}
private:
 static uint32_t boundary(const Job&j){switch(j.task.stage()){case S::SetValue:return j.task.kind()==T::Kind::First?0x8366:0x836a;case S::Jump:return j.task.kind()==T::Kind::First?0x8368:0x836c;case S::SetAddress:return 0x8370;case S::ClearHigh:return 0x8374;case S::Dispatch:return 0x8376;case S::Return:return 0x837a;case S::AwaitDsp:return j.expected;case S::Complete:return 0;}return 0;}
 std::array<std::optional<Job>,8>jobs_{};unsigned depth_=0;
};
}
