#pragma once
#include "ksVoiceRefreshTask.h"
#include "ksVoiceTrackingService.h"
#include "ksPartControlTasks.h"
#include "ksParameterUploadService.h"
#include "ksGlobalUpdateService.h"
#include "ksStateStoreService.h"
#include <optional>
#include <type_traits>
namespace ks {
class StateVoiceRefreshService final {
public:
 enum class Kind{All};struct Input{};enum class Component{Refresh,Mirror,Lookup,Dsp,Upload,Tracking,Global};struct Effect{bool read=false,write=false;};
 struct Summary{VoiceRefreshTask refresh{VoiceRefreshTask::Kind::All};uint32_t lastVoiceReference=0;uint8_t lastMirrorByte=0;uint64_t uploads=0,inspections=0;DspPort port=DspPort::A;uint16_t address=0;bool voiceObserved=false;};
 StateVoiceRefreshService():task_(VoiceRefreshTask::Kind::All){}explicit StateVoiceRefreshService(Kind):StateVoiceRefreshService(){}StateVoiceRefreshService(Kind,Input):StateVoiceRefreshService(){}
 bool complete()const{return complete_;}bool critical()const{return false;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const GlobalUpdateService*global()const{return global_?&*global_:nullptr;}const VoiceTrackingService*tracking()const{return tracking_?&*tracking_:nullptr;}
 Component component()const{return mirror_?Component::Mirror:lookup_?Component::Lookup:dsp_?Component::Dsp:upload_?Component::Upload:tracking_&&!tracking_->complete()?Component::Tracking:global_?Component::Global:Component::Refresh;}
 uint32_t components()const{return components_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,VoiceRefreshTask>)return &task_;else if constexpr(std::is_same_v<T,PartMirrorTask>)return mirror_?&*mirror_:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>)return lookup_?&*lookup_:nullptr;else if constexpr(std::is_same_v<T,ControllerDspService>)return dsp_?&*dsp_:nullptr;else if constexpr(std::is_same_v<T,ParameterUploadService>)return upload_?&*upload_:nullptr;else if constexpr(std::is_same_v<T,VoiceTrackingService>)return tracking_&&!tracking_->complete()?&*tracking_:nullptr;else if constexpr(std::is_same_v<T,GlobalUpdateService>)return global_&&!global_->complete()?&*global_:nullptr;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;if(mirror_)return mirror_->stage()==PartMirrorTask::Stage::Complete?17:mirror_->nextClocks(mirror_->stage()==PartMirrorTask::Stage::Write?byteClock(0x17512+mirror_->position(),previous):3);if(lookup_)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();if(dsp_)return dsp_->nextClocks(previous);if(upload_)return upload_->nextClocks(previous);if(tracking_&&!tracking_->complete())return tracking_->nextClocks(previous);if(global_&&!global_->complete())return global_->nextClocks(previous);return task_.nextClocks([&]{return byteClock(0x174a7,previous);});}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};components_|=1u<<unsigned(component());
  const auto byte=[&](uint32_t a,unsigned back=0){if(!dataSpan(a,1))throw std::logic_error("Unsupported evolving refresh byte");return uint8_t(read(a&0xffffff,cost-back));};
  const auto wide=[&](uint32_t a,unsigned n){if(!dataSpan(a,n))throw std::logic_error("Unsupported evolving refresh span");uint32_t v=0;for(unsigned j=0;j<n;++j)v=(v<<8)|byte(a+j,3*((n-1-j)/2));return v;};
  if(mirror_){if(mirror_->stage()==PartMirrorTask::Stage::Complete){mirror_.reset();effect_.read=true;task_.resumeChild();}else mirror_->advanceBefore(cost,[&](unsigned part,unsigned index){return summary_.lastMirrorByte=byte(0x174ae +20*part+index,2);},[&](unsigned index,uint8_t v){write(0x17512+index,v,cost);},[&](unsigned index){return byteClock(0x17512+index,previous);});return cost;}
  if(lookup_){if(lookup_->stage()==PartLookupTask::Stage::Complete){const auto slot=lookup_->index();summary_.lastVoiceReference=0x17560+20*slot;summary_.voiceObserved=true;++summary_.inspections;lookup_.reset();effect_.read=true;task_.resumeChild(slot);}else lookup_->advanceBefore(cost,[&](auto){return byte(0x174a7);});return cost;}
  if(dsp_){dsp_->advanceBefore(cost,read,write,previous);effect_={dsp_->effect().read,dsp_->effect().write};if(dsp_->complete()){summary_.port=dsp_->port();summary_.address=dsp_->address();dsp_.reset();task_.resumeChild();}return cost;}
  if(upload_){upload_->advanceBefore(cost,read,write,previous);effect_={upload_->effect().read,upload_->effect().write};if(upload_->complete()){summary_.port=upload_->summary().port;summary_.address=upload_->summary().input.address;summary_.lastVoiceReference=0x17560+20*upload_->summary().voice;++summary_.uploads;upload_.reset();task_.resumeChild();}return cost;}
  if(tracking_&&!tracking_->complete()){tracking_->advanceBefore(cost,read,write,previous);effect_={tracking_->effect().read,tracking_->effect().write};if(tracking_->complete()){if(tracking_->summary().patchObserved){summary_.port=tracking_->summary().port;summary_.address=tracking_->summary().address;}task_.resumeChild();}return cost;}
  if(global_&&!global_->complete()){global_->advanceBefore(cost,read,write,previous);effect_={global_->effect().read,global_->effect().write};if(global_->complete())task_.resumeChild();return cost;}
  using T=VoiceRefreshTask;using S=T::Stage;const auto stage=task_.stage();const auto voice=0x17560u+20*task_.slot();
  task_.advanceBefore(cost,[&](T::Read r)->uint32_t{switch(r){case T::Read::Part:case T::Read::Match:return byte(0x17526);case T::Read::Count:return byte(0x174a6);case T::Read::Age:return wide(voice+8,2);case T::Read::Owner:return byte(voice+4);case T::Read::Patch:{const auto p=wide(voice,4);if(!ParameterUploadService::dataSpan(p,256))throw std::logic_error("Unsupported evolving voice patch");return p;}case T::Read::Index:return byte(0x174a7);default:throw std::logic_error("Unsupported full refresh observation");}},[&](uint8_t n){write(0x174a7,n,cost);},[&]{return byteClock(0x174a7,previous);});
  if(stage==S::RetainRemaining||stage==S::RetainMatch)effect_.write=true;if(stage==S::RestoreRemaining||stage==S::RestoreMatch)effect_.read=true;
  if(stage==S::PrimeOffset)summary_.address=0x2c;
  if(task_.child()!=T::Child::None){effect_.write=true;switch(task_.child()){case T::Child::Control:mirror_.emplace(task_.match());break;case T::Child::Voice:lookup_.emplace(PartLookupTask::Layout::Voice);break;case T::Child::Select:startDsp(DspRoutingTask::Kind::VoiceB,summary_.address);break;case T::Child::Prime:startDsp(DspRoutingTask::Kind::PrimeRelative,0x2c);break;case T::Child::Upload:upload_.emplace(ParameterUploadService::Input{task_.patch(),summary_.port,summary_.address});break;case T::Child::Tracking:tracking_.emplace();break;case T::Child::Global:global_.emplace(GlobalUpdateService::Input{0,summary_.port,summary_.address});break;default:throw std::logic_error("Unsupported full refresh child");}}
  if(stage==S::Finish){summary_.refresh=task_;effect_.read=true;complete_=true;}return cost;
 }
 static bool dataSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return StateStoreService::dataSpan(raw,n)||(n&&a>=0x800000&&a<0x900000&&n<=0x900000-a);}
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 void startDsp(DspRoutingTask::Kind k,uint16_t address){ControllerDspService::Input i;i.routing=k;i.port=summary_.port;i.address=address;dsp_.emplace(ControllerDspService::Kind::Routing,i);}
 VoiceRefreshTask task_;Summary summary_{};Effect effect_{};bool complete_=false;uint32_t components_=0;
 std::optional<PartMirrorTask>mirror_;std::optional<PartLookupTask>lookup_;std::optional<ControllerDspService>dsp_;std::optional<ParameterUploadService>upload_;std::optional<VoiceTrackingService>tracking_;std::optional<GlobalUpdateService>global_;
};
}
