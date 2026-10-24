#pragma once
#include "ksStateSaveService.h"
#include "ksStateSaveAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateSaveForeground final {
public:
 using CallerAbi=StateSaveCallerAbi;
 static void project(CallerAbi&,const StateSaveService&,StateSaveService::Input,uint32_t descriptor=0);
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}
 bool at(StateSaveService::Kind k)const{return job_&&job_->service.kind()==k;}bool at(StateSaveService::Component c)const{return job_&&job_->service.component()==c;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 bool at(FlashPageProgramTask::Stage s)const{const auto*t=task<FlashPageProgramTask>();return t&&t->stage()==s;}bool at(FlashPageCompareTask::Stage s)const{const auto*t=task<FlashPageCompareTask>();return t&&t->stage()==s;}
 const StateStoreService*preparation()const{return task<StateStoreService>();}const StateSavePageService*page()const{return task<StateSavePageService>();}const StateReloadService*reload()const{return task<StateReloadService>();}
 bool atPreparation(StateStoreService::Kind k)const{const auto*p=preparation();return p&&p->kind()==k;}bool atPreparation(StateStoreService::Component c)const{const auto*p=preparation();return p&&p->component()==c;}
 bool atPage(StateSavePageService::Component c)const{const auto*p=page();return p&&p->component()==c;}bool atReload(StateReloadService::Component c)const{const auto*p=reload();return p&&p->component()==c;}
 bool at(StatePresetFormatTask::Stage s)const{const auto*t=task<StatePresetFormatTask>();return t&&t->stage()==s;}bool at(StateMultiSaveTask::Stage s)const{const auto*t=task<StateMultiSaveTask>();return t&&t->stage()==s;}bool at(StateFlashCommitTask::Stage s)const{const auto*t=task<StateFlashCommitTask>();return t&&t->stage()==s;}
 bool at(StateStoreUiTask::Stage s)const{const auto*t=task<StateStoreUiTask>();return t&&t->stage()==s;}
 void resetMetrics(){started=completed=phases=components=0;initial=active();kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,15>kinds{};
private:
 struct Job{Job(StateSaveService::Kind k,StateSaveService::Input i,uint32_t c,int d):service(k,i),input(i),caller(c),depth(d){}StateSaveService service;StateSaveService::Input input;uint32_t caller=0,descriptor=0;int depth=0;uint8_t savedPriority=0;};
 std::optional<Job>job_;
};}
