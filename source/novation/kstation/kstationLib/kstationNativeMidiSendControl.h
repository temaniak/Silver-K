#pragma once
#include "kstationMidiSendControlTask.h"
#include "kstationNativeArpMain.h"
#include <array>
#include <type_traits>
#include <variant>
namespace kstation {
class NativeMidiSendControlTask final {
 using T=MidiSendControlTask;using S=T::Stage;
public:const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){return task_.awaitingChild()&&uint32_t(c.getPC())==return_&&task_.resume();}uint32_t boundary()const{switch(task_.stage()){case S::SavePointer:return 0x274a;case S::SaveData:return 0x274e;case S::BindPatch:return 0x2750;case S::ReadChannel:return 0x2756;case S::Status:return 0x275e;case S::SendStatus:return 0x2760;case S::RestoreData:return 0x2764;case S::RestorePointer:return 0x2766;case S::SendController:return 0x276a;case S::CopyValue:return 0x276e;case S::SendValue:return 0x2770;case S::Return:return 0x2774;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();unsigned len=p==S::ReadChannel?8:p==S::BindPatch?6:p==S::SavePointer||p==S::SendStatus||p==S::RestorePointer||p==S::SendController||p==S::SendValue?4:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::SavePointer:save(c,0);break;case S::SaveData:c.setSP(c.getSP()-2);c.write16(c.reg16(6),int(c.getSP()));c.clockI(2);val(c,c.reg16(6));break;case S::BindPatch:c.reg32(0)=0xba5c;val(c,c.reg32(0));break;case S::Status:c.reg8(14)=int8_t(uint8_t(c.reg8(14))|0xb0);val(c,c.reg8(14));break;case S::SendStatus:case S::SendController:case S::SendValue:return_=at+len;call(c,0xb0c,return_,true);break;case S::RestoreData:c.reg16(6)=c.read16(int(c.getSP()));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(6));break;case S::RestorePointer:restore(c,0);break;case S::CopyValue:c.reg8(14)=c.reg8(6);val(c,c.reg8(14));break;case S::Return:ret(c);break;default:break;}task_.advance([&](){c.reg8(14)=c.read8(c.reg32(0)+0xbb);val(c,c.reg8(14));});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}private:T task_;uint32_t return_=0;
};
class NativeMidiSendControl final {
 using C=NativeMidiSendControlTask;using Q=NativeArpMidiEnqueue;using Job=std::variant<std::monostate,C,Q>;
public:struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,2>CodeRanges{{{0xb0c,0xb50},{0x274a,0x2776}}};static bool candidate(uint32_t pc){for(auto r:CodeRanges)if(pc>=r.begin&&pc<r.end)return true;return false;}bool active()const{return depth_!=0;}void reset(){for(auto&j:jobs_)j.emplace<std::monostate>();depth_=0;}
 bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;for(unsigned i=0;i<jobs_.size()+1;++i){if(!depth_){if(uint32_t(c.getPC())!=0x274a)return false;jobs_[depth_++].emplace<C>();}auto&j=jobs_[depth_-1];if(auto*p=std::get_if<C>(&j);p&&p->task().awaitingChild()){if(p->resumeChild(c))continue;if(uint32_t(c.getPC())!=0xb0c||depth_==jobs_.size())return false;jobs_[depth_++].emplace<Q>(uint8_t(c.reg8(14)));continue;}bool ok=std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return false;else return v.step(c);},j);if(!ok)return false;bool done=std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return true;else return v.task().complete();},j);if(done){j.emplace<std::monostate>();--depth_;}return true;}return false;}
private:std::array<Job,2>jobs_{};unsigned depth_=0;
};
}
