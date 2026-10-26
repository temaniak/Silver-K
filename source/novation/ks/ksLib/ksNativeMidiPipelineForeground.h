#pragma once
#include "ksNativeMidiPumpAbi.h"
#include "ksNativeInternalMidiAbi.h"
#include <variant>
namespace ks {class ControllerCpu;
class NativeMidiPipelineForeground final{
public:
 enum class Kind{External,Queued,Internal,Pump,Initialize,Parser,System,Controls};struct CallerAbi{std::array<uint32_t,7>values{};uint8_t flags=0;};using Frames=std::variant<std::monostate,NativeMidiDecodeService,NativeQueuedMidiDecodeService,NativeInternalMidiService,NativeMidiPumpService,NativeMidiInitializationService>;
 struct Input{uint8_t byte=0,activation=0;uint32_t payload=0,source=0,binding=0;};bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const NativeMidiPipelineForeground*service()const{return job_?this:nullptr;}unsigned component()const{return job_?std::visit([](const auto&s)->unsigned{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return unsigned(s.component());},job_->frame):0;}
 template<class T>const T*task()const{if(!job_)return nullptr;return std::visit([](const auto&s)->const T*{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return nullptr;else return s.template task<T>();},job_->frame);}
 void resetMetrics(){initial=active();started=completed=phases=components=0;}uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;
 uint8_t read(ControllerCpu&,uint32_t,unsigned,uint64_t)const;void write(ControllerCpu&,uint32_t,uint8_t,unsigned,uint64_t)const;
private:
 struct Job{Frames frame;CallerAbi external;uint32_t caller;int depth;uint8_t priority;Job(Kind k,Input in,uint32_t c,int d,uint8_t p):caller(c),depth(d),priority(p){switch(k){case Kind::External:frame.emplace<NativeMidiDecodeService>(NativeMidiDecodeService::Input{in.byte,in.activation,in.binding});break;case Kind::Queued:frame.emplace<NativeQueuedMidiDecodeService>(NativeQueuedMidiDecodeService::Input{in.byte,in.activation});break;case Kind::Internal:frame.emplace<NativeInternalMidiService>(NativeInternalMidiService::Input{in.byte,in.payload,in.activation,in.source});break;case Kind::Pump:frame.emplace<NativeMidiPumpService>(NativeMidiPumpService::Input{in.activation});break;default:frame.emplace<NativeMidiInitializationService>(NativeMidiInitializationService::Kind(unsigned(k)-unsigned(Kind::Initialize)));break;}}};std::optional<Job>job_;
};}
