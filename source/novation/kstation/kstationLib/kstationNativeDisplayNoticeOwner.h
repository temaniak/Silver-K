#pragma once
#include "kstationNativeDisplayNotice.h"
#include <array>
#include <type_traits>
#include <variant>
namespace kstation {
class NativeDisplayNoticeOwner final {
 using D=NativeDisplayNotice;using S=NativeDeferredSchedule;using Job=std::variant<std::monostate,D,S>;
public:enum class External:uint8_t {None,Lcd,Tick,Refresh};struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,2>CodeRanges{{{0x38f2,0x390c},{0x438a,0x43b2}}};static bool candidate(uint32_t pc){for(auto r:CodeRanges)if(pc>=r.begin&&pc<r.end)return true;return false;}void reset(){for(auto&j:jobs_)j.emplace<std::monostate>();depth_=0;}bool active()const{return depth_!=0;}External external()const{if(!depth_)return External::None;if(auto*p=std::get_if<S>(&jobs_[depth_-1]);p&&p->task().awaitingChild())return External::Tick;if(auto*p=std::get_if<D>(&jobs_[depth_-1]);p&&p->task().awaitingChild()){auto c=p->task().child();return c==DisplayNoticeTask::Child::Print?External::Lcd:c==DisplayNoticeTask::Child::Refresh?External::Refresh:External::None;}return External::None;}
 bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;for(unsigned n=0;n<jobs_.size()+1;++n){if(!depth_){auto at=uint32_t(c.getPC());if(at==0x438a||at==0x43a6)jobs_[depth_++].emplace<D>(at==0x438a?DisplayNoticeTask::Kind::Show:DisplayNoticeTask::Kind::Expire);else if(at==0x38f2)jobs_[depth_++].emplace<S>(uint32_t(c.reg32(5)),uint16_t(c.reg16(6)));else return false;}auto&j=jobs_[depth_-1];if(awaiting(j)){if(resume(j,c))continue;if(auto*p=std::get_if<D>(&j);p&&p->task().child()==DisplayNoticeTask::Child::Schedule&&uint32_t(c.getPC())==0x38f2&&depth_<jobs_.size()){jobs_[depth_++].emplace<S>(uint32_t(c.reg32(5)),uint16_t(c.reg16(6)));continue;}return false;}bool ok=std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return false;else return v.step(c);},j);if(!ok)return false;bool complete=std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return true;else return v.task().complete();},j);if(complete){j.emplace<std::monostate>();--depth_;}return true;}return false;}
private:static bool awaiting(const Job&j){if(auto*p=std::get_if<D>(&j))return p->task().awaitingChild();if(auto*p=std::get_if<S>(&j))return p->task().awaitingChild();return false;}static bool resume(Job&j,Stage1Cpu&c){if(auto*p=std::get_if<D>(&j))return p->resumeChild(c);if(auto*p=std::get_if<S>(&j))return p->resumeChild(c);return false;}std::array<Job,2>jobs_{};unsigned depth_=0;
};
}
