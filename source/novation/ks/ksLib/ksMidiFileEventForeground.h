#pragma once
#include "ksMidiFileEventCompatibility.h"
#include "ksMidiFileEventBindings.h"
#include <variant>
namespace ks {class ControllerCpu;
class MidiFileEventForeground final{
public:
 using Kind=MidiFileEventService::Kind;using Input=MidiFileEventService::Input;struct CallerAbi{std::array<uint32_t,7>values{};uint8_t flags=0;};using Frames=std::variant<std::monostate,MidiFileEventService>;
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const MidiFileEventForeground*service()const{return job_?this:nullptr;}unsigned component()const{return job_?std::visit([](const auto&s)->unsigned{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return unsigned(s.component());},job_->frame):0;}
 template<class T>const T*task()const{if(!job_)return nullptr;return std::visit([](const auto&s)->const T*{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return nullptr;else return s.template task<T>();},job_->frame);}
 void resetMetrics(){initial=active();started=completed=phases=components=0;}uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;
 uint8_t read(ControllerCpu&,uint32_t,unsigned,uint64_t)const;void write(ControllerCpu&,uint32_t,uint8_t,unsigned,uint64_t)const;
private:
 struct Job{Frames frame;MidiFileEventCompatibility external;uint32_t caller;int depth;uint8_t priority;Job(Kind k,Input in,uint32_t c,int d,uint8_t p,const CallerAbi&a):external(a),caller(c),depth(d),priority(p){frame.emplace<MidiFileEventService>(k,in);}};std::optional<Job>job_;
};}
