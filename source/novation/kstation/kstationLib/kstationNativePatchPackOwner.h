#pragma once
#include "kstationNativePatchPack.h"
#include <array>
#include <type_traits>
#include <variant>
namespace kstation {
class NativePatchPack final {
 using R=NativePatchPackRefresh;using S=NativePatchPackSelect;using B=NativePatchPackByte;using Job=std::variant<std::monostate,R,S,B>;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1>CodeRanges{{{0x525c,0x5556}}};static bool candidate(uint32_t pc){return pc>=0x525c&&pc<0x5556;}
 void reset(){for(auto&j:jobs_)j.emplace<std::monostate>();depth_=0;callerReturn_=0;}bool active()const{return depth_!=0;}uint32_t callerReturn()const{return callerReturn_;}
 // Caller owns admission, including verification of the return address.
 bool admitSelect(Stage1Cpu&c,uint32_t verifiedReturn){if(depth_||midi_projection::blocked(c)||uint32_t(c.getPC())!=0x526a)return false;callerReturn_=verifiedReturn&0xffffff;jobs_[0].emplace<S>(uint8_t(c.reg8(10)));depth_=1;return true;}
 uint32_t externalTarget()const{if(!depth_)return 0;if(auto*p=std::get_if<S>(&jobs_[depth_-1]);p&&p->task().awaitingChild())return p->task().target()&0xffffff;return 0;}
 bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;for(unsigned n=0;n<jobs_.size()+1;++n){if(!depth_){if(uint32_t(c.getPC())!=0x525c)return false;jobs_[depth_++].emplace<R>();}auto&j=jobs_[depth_-1];if(awaiting(j)){if(resume(j,c)){if(complete(j)){j.emplace<std::monostate>();--depth_;}continue;}if(!pushChild(c,j))return false;continue;}bool ok=std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return false;else return v.step(c);},j);if(!ok)return false;if(complete(j)){j.emplace<std::monostate>();--depth_;}return true;}return false;}
private:
 static bool awaiting(const Job&j){if(auto*p=std::get_if<R>(&j))return p->task().awaitingChild();if(auto*p=std::get_if<S>(&j))return p->task().awaitingChild();return false;}
 static bool complete(const Job&j){return std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return true;else return v.task().complete();},j);}
 static bool resume(Job&j,Stage1Cpu&c){if(auto*p=std::get_if<R>(&j))return p->resumeChild(c);if(auto*p=std::get_if<S>(&j))return p->resumeChild(c);return false;}
 bool pushChild(Stage1Cpu&c,const Job&parent){if(depth_==jobs_.size())return false;auto&j=jobs_[depth_];if(std::holds_alternative<R>(parent)){if(uint32_t(c.getPC())!=0x526a)return false;j.emplace<S>(uint8_t(c.reg8(10)));}else if(std::holds_alternative<S>(parent)){unsigned i=0;while(i<PatchPackSpecs.size()&&PatchPackSpecs[i].entry!=uint32_t(c.getPC()))++i;if(i==PatchPackSpecs.size())return false;j.emplace<B>(i,uint32_t(c.reg32(4)));}else return false;++depth_;return true;}
 std::array<Job,3>jobs_{};unsigned depth_=0;uint32_t callerReturn_=0;
};
}
