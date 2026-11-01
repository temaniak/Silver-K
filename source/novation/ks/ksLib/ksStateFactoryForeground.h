#pragma once
#include "ksStateFactoryService.h"
#include "ksStateFactoryDspService.h"
#include "ksStateSaveAbi.h"
#include <array>
#include <optional>
#include <variant>
namespace ks {class ControllerCpu;
class StateFactoryForeground final {
public:
 enum class Kind {Provided,Factory,GlobalLoad,Reset,Upload,Poll,Stream};
 using CallerAbi=StateSaveCallerAbi;
 static void project(CallerAbi&,const StateFactoryService&);static void project(CallerAbi&,const StateFactoryDspService&);
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}
 const StateFactoryService*service()const{return job_?std::get_if<StateFactoryService>(&job_->work):nullptr;}const StateFactoryDspService*dsp()const{return job_?std::get_if<StateFactoryDspService>(&job_->work):nullptr;}
 bool at(Kind k)const{return job_&&job_->kind==k;}bool at(StateFactoryService::Component c)const{const auto*s=service();return s&&s->component()==c;}bool at(StateFactoryDspService::Component c)const{const auto*s=dsp();return s&&s->component()==c;}
 template<class T>const T*task()const{if(const auto*s=service())return s->template task<T>();if(const auto*s=dsp())return s->template task<T>();return nullptr;}
 void resetMetrics(){started=completed=phases=components=0;initial=active();kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,7>kinds{};
private:
 struct Job {Job(Kind k,StateFactoryService::Input i,uint32_t c,int d):kind(k),work(std::in_place_type<StateFactoryService>,StateFactoryService::Kind(unsigned(k)),i),caller(c),depth(d){}Job(Kind k,StateFactoryDspService::Input i,uint32_t c,int d):kind(k),work(std::in_place_type<StateFactoryDspService>,StateFactoryDspService::Kind(unsigned(k)-3),i),caller(c),depth(d){}Kind kind;std::variant<StateFactoryService,StateFactoryDspService>work;uint32_t caller;int depth;uint8_t savedPriority=0;};
 std::optional<Job>job_;
};}
