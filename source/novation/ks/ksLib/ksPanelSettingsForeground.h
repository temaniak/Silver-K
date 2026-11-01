#pragma once
#include "ksPanelSettingsService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelSettingsForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 bool at(PanelSettingsService::Kind k)const{return job_&&job_->service.kind()==k;}
 bool at(PanelSettingsService::Component c)const{return job_&&job_->service.component()==c;}
 template<class T>const T* task()const{if(!job_)return nullptr;const auto&s=job_->service;if constexpr(std::is_same_v<T,PanelSettingsTask>)return s.settingsTask();else if constexpr(std::is_same_v<T,PanelAdcScanTask>)return s.warmupTask();else if constexpr(std::is_same_v<T,FlashPageProgramTask>)return s.flashTask()?s.flashTask()->programTask():nullptr;else if constexpr(std::is_same_v<T,FlashPageCompareTask>)return s.flashTask()?s.flashTask()->compareTask():nullptr;else return nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,13>kinds{};
private:
 struct Job{PanelSettingsService service;uint32_t caller;int depth;uint8_t savedPriority=0;Job(PanelSettingsService::Kind k,PanelSettingsService::Input in,uint32_t stack,int level):service(k,in),caller(stack),depth(level){}};
 std::optional<Job>job_;
};
}
