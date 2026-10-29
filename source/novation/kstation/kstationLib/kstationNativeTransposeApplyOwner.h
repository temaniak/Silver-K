#pragma once
#include "kstationNativeTransposeApply.h"
#include "kstationNativeMidiEvent.h"
#include "kstationNativeArpMain.h"
#include <array>
#include <type_traits>
#include <variant>
namespace kstation {
class NativeTransposeApplyOwner final {
 using C=NativeTransposeCheck;using R=NativeTransposeReset;using M=NativeMidiAllNotesOff;using H=NativeMidiClearNotes;using Q=NativeArpMidiEnqueue;using Job=std::variant<std::monostate,C,R,M,H,Q>;
public:enum class External:uint8_t {None,Release};struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,5>CodeRanges{{{0xb0c,0xb50},{0x26e8,0x2712},{0x2b7a,0x2baa},{0x3f2a,0x3f3a},{0x3f54,0x3f66}}};static bool candidate(uint32_t pc){for(auto r:CodeRanges)if(pc>=r.begin&&pc<r.end)return true;return false;}void reset(){for(auto&j:jobs_)j.emplace<std::monostate>();depth_=0;}bool active()const{return depth_!=0;}
 bool admit(Stage1Cpu&c,uint32_t verifiedReturn){if(depth_||midi_projection::blocked(c))return false;auto at=uint32_t(c.getPC());if(at==0x3f54)jobs_[depth_++].emplace<C>(verifiedReturn);else if(at==0x3f2a)jobs_[depth_++].emplace<R>();else if(at==0x26e8)jobs_[depth_++].emplace<M>();else return false;return true;}
 External external()const{if(!depth_)return External::None;if(auto*p=std::get_if<R>(&jobs_[depth_-1]);p&&p->task().awaitingChild()&&p->task().child()==TransposeResetTask::Child::Release)return External::Release;return External::None;}
 bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;for(unsigned n=0;n<jobs_.size()+1;++n){if(!depth_)return false;auto&j=jobs_[depth_-1];if(awaiting(j)){if(resume(j,c)){if(complete(j)){j.emplace<std::monostate>();--depth_;}continue;}if(depth_==jobs_.size())return false;if(std::holds_alternative<C>(j)&&uint32_t(c.getPC())==0x3f2a){jobs_[depth_++].emplace<R>();continue;}if(auto*p=std::get_if<R>(&j)){using X=TransposeResetTask::Child;if(p->task().child()==X::ClearHeld&&uint32_t(c.getPC())==0x2b7a){jobs_[depth_++].emplace<H>();continue;}if(p->task().child()==X::AllNotesOff&&uint32_t(c.getPC())==0x26e8){jobs_[depth_++].emplace<M>();continue;}}if(std::holds_alternative<M>(j)&&uint32_t(c.getPC())==0xb0c){jobs_[depth_++].emplace<Q>(uint8_t(c.reg8(14)));continue;}return false;}bool ok=std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return false;else return v.step(c);},j);if(!ok)return false;if(complete(j)){j.emplace<std::monostate>();--depth_;}return true;}return false;}
private:static bool awaiting(const Job&j){if(auto*p=std::get_if<C>(&j))return p->task().awaitingChild();if(auto*p=std::get_if<R>(&j))return p->task().awaitingChild();if(auto*p=std::get_if<M>(&j))return p->task().awaitingChild();return false;}static bool complete(const Job&j){return std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return true;else return v.task().complete();},j);}static bool resume(Job&j,Stage1Cpu&c){if(auto*p=std::get_if<C>(&j))return p->resumeChild(c);if(auto*p=std::get_if<R>(&j))return p->resumeChild(c);if(auto*p=std::get_if<M>(&j))return p->resumeChild(c);return false;}std::array<Job,4>jobs_{};unsigned depth_=0;
};
}
