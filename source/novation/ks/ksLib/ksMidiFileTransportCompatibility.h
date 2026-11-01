#pragma once
#include "ksMidiFileTransportService.h"
#include "ksMidiFileEventCompatibility.h"
#include "ksNativeArpOwnedCompatibility.h"
#include "ksStateCentralActionsLeavesAbi.h"
#include "ksSysExNativeLeavesAbi.h"
#include "ksMidiFileTimeAbi.h"
#include "ksStateTickAbi.h"
#include "ksVoiceFeedbackAbi.h"
#include "ksPanelMasksAbi.h"
#include "ksPartFeedbackAbi.h"
namespace ks {
// Temporary external ABI observer. The file-player graph never owns or reads
// this object. A Noop observer must leave all native phases and results exact.
class MidiFileTransportCompatibility final {
public:
 using Service=MidiFileTransportService;using Stage=Service::Stage;using Request=Service::Request;using Image=StateFieldForeground::CallerAbi;
 template<class A>explicit MidiFileTransportCompatibility(const A&a){copy(image_,a);}
 const Image&image()const{return image_;}
 void operator()(Service::Notice notice,Stage stage,const Service&s){if(notice!=Service::Notice::Policy)throw std::logic_error("Unexpected file-player policy accounting notice");policy(stage,s.coordinator().phaseObservation());}
 void operator()(Service::Notice notice,const Service&s){using N=Service::Notice;
  if(notice==N::Dispatch){inArp_=s.child()==Request::Arp;if(s.child()==Request::Arp)arp_.emplace(NativeArpOwnedService::Kind::Schedule,image_);else if(s.child()==Request::Event)event_.emplace(image_);else if(s.child()==Request::LoadMulti){reloadIndicator_.reset();reloadSet_=0;reloadVoiceKnown_=false;}return;}
  if(notice==N::ChildPhase){observeReload(s);return;}
  if(notice!=N::ChildComplete)return;
  std::visit([&](const auto&child){using T=std::decay_t<decltype(child)>;
   if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Missing completed file-player child accounting");
   else if constexpr(std::is_same_v<T,MidiFileGlobalsService>)projectMidiFileGlobals(image_,child);
   else if constexpr(std::is_same_v<T,StatePlaybackModeService>)StatePlaybackModeForeground::project(image_,child);
   else if constexpr(std::is_same_v<T,StateDisplayService>){if(child.kind()!=StateDisplayService::Kind::Character)throw std::logic_error("Unsupported file-player display accounting");word(6,child.output());image_.flags=uint8_t((image_.flags&0xd0)|centralActionsLeavesBoundary::lcdArithmetic(child.lcdResult(),image_.flags)|nz16(uint16_t(image_.values[0])));}
   else if constexpr(std::is_same_v<T,StateReloadService>){if(reloadVoiceKnown_)image_.values[6]=reloadVoicePatch_;if(!child.panel())throw std::logic_error("Missing completed file-player reload panel");sysexLeavesBoundary::panel(image_,*child.panel(),reloadIndicator_,reloadSet_,child.coordinator().expandedCurrentBase());}
   else if constexpr(std::is_same_v<T,StateFieldService>){if(s.child()==Request::ReleaseKey)StateFieldForeground::project(image_,child,child.input());else StateFieldForeground::projectUi(image_,child);}
   else if constexpr(std::is_same_v<T,MidiFileTimeService>)projectMidiFileTime(image_,child);
   else if constexpr(std::is_same_v<T,MidiFileEventService>){if(!event_)throw std::logic_error("Missing file-event observer");copy(image_,event_->image());}
   else if constexpr(std::is_same_v<T,NativeArpOwnedService>){if(!arp_)throw std::logic_error("Missing file-player arp observer");copy(image_,arp_->image());}
   else if constexpr(std::is_same_v<T,VoiceFeedbackService>)projectVoiceFeedback(image_,child);
   else if constexpr(std::is_same_v<T,PanelMasksService>)projectPanelMasks(image_,child);
   else if constexpr(std::is_same_v<T,PartFeedbackService>)projectPartFeedback(image_,child);
   else if constexpr(std::is_same_v<T,StateTickService>)projectStateTick(image_,child);
   else if constexpr(std::is_same_v<T,StateEventsService>)centralActionsLeavesBoundary::events(image_,child);
   else if constexpr(std::is_same_v<T,StateResetService>)image_.flags=uint8_t((image_.flags&0xd0)|nz16(uint16_t(image_.values[6])));
   else if constexpr(std::is_same_v<T,SerialForegroundService>){if(child.kind()!=SerialForegroundService::Kind::ResetRx)throw std::logic_error("Unsupported file-player serial accounting");image_.flags=uint8_t((image_.flags&0xd0)|nz16(uint16_t(image_.values[0])));}
   else if constexpr(std::is_same_v<T,StatePlaybackModeCoordinator>)playbackModeProjection::lower(image_,child);
   else if constexpr(std::is_same_v<T,StateProgramLoadService>)StateProgramLoadForeground::project(image_,child);
   else static_assert(!sizeof(T),"Unsupported completed file-player child type");
  },s.engines());
 }
 template<class...A>void operator()(NativeArpOwnedService::Notice n,A&&...a){requireArp();(*arp_)(n,std::forward<A>(a)...);}
 template<class...A>void operator()(NativeArpService::Notice n,A&&...a){requireArp();(*arp_)(n,std::forward<A>(a)...);}
 template<class...A>void operator()(NativeArpMainService::Notice n,A&&...a){requireArp();(*arp_)(n,std::forward<A>(a)...);}
 template<class...A>void operator()(NativeMonoArpCoordinator::Notice n,A&&...a){requireArp();(*arp_)(n,std::forward<A>(a)...);}
 template<class...A>void operator()(MidiFileEventService::Notice n,A&&...a){if(inArp_){requireArp();(*arp_)(n,std::forward<A>(a)...);}else{requireEvent();(*event_)(n,std::forward<A>(a)...);}}
 template<class...A>void operator()(MidiFileEventCoordinator::Notice n,A&&...a){if(inArp_){requireArp();(*arp_)(n,std::forward<A>(a)...);}else{requireEvent();(*event_)(n,std::forward<A>(a)...);}}
private:
 template<class A,class B>static void copy(A&a,const B&b){a.values=b.values;a.flags=b.flags;}
 static uint8_t nz16(uint16_t v){return uint8_t((v&32768?8:0)|(!v?4:0));}
 void nz(uint32_t v,unsigned bits){const auto mask=bits==32?~0u:(uint32_t(1)<<bits)-1;v&=mask;image_.flags=uint8_t((image_.flags&0xf1)|(v&(uint32_t(1)<<(bits-1))?8:0)|(!v?4:0));}
 void low(unsigned n,uint8_t v){image_.values[n]=(image_.values[n]&0xffffff00u)|v;nz(v,8);}
 void word(unsigned n,uint16_t v){image_.values[n]=(image_.values[n]&0xffff0000u)|v;nz(v,16);}
 void upper(unsigned n,uint16_t v){image_.values[n]=(image_.values[n]&65535u)|(uint32_t(v)<<16);nz(v,16);}
 void full(unsigned n,uint32_t v){image_.values[n]=v;nz(v,32);}
 template<class T>void increment(unsigned n,T value,int delta,bool highWord=false){const auto old=int32_t(value),result=int32_t(uint32_t(old)+uint32_t(delta));nz(uint32_t(result),32);const auto sign=uint32_t(1)<<(8*sizeof(T)-1);const bool overflow=(delta>0?(~uint32_t(old)&uint32_t(result)):(uint32_t(old)&~uint32_t(result)))&sign;image_.flags=uint8_t((image_.flags&~2u)|(overflow?2:0));if(highWord)image_.values[n]=(image_.values[n]&65535u)|(uint32_t(uint16_t(result))<<16);else image_.values[n]=uint32_t(result);}
 void policy(Stage stage,const MidiFileTransportCoordinator::PhaseObservation&o){auto&v=image_.values;switch(stage){
  case Stage::Enable:case Stage::SelectMulti:case Stage::SetPlaying:low(0,1);break;
  case Stage::Disable:low(0,0);break;
  case Stage::PlaybackFlag:case Stage::PublishProgram:case Stage::PublishMulti:case Stage::PublishPlaying:case Stage::PublishStopped:nz(uint8_t(v[0]),8);break;
  case Stage::BindSaveSource:case Stage::BindRestoreDestination:full(0,0x167aa);break;
  case Stage::BindSaveDestination:case Stage::BindRestoreSource:full(1,0x1732a);break;
  case Stage::SaveCount:case Stage::RestoreCount:upper(2,18);break;
  case Stage::SaveRead:low(2,uint8_t(o.value));++v[0];break;
  case Stage::RestoreRead:low(2,uint8_t(o.value));++v[1];break;
  case Stage::SavePublish:case Stage::RestorePublish:nz(uint8_t(v[2]),8);break;
  case Stage::SaveAdvance:increment(1,int32_t(v[1]),1);break;
  case Stage::RestoreAdvance:increment(0,int32_t(v[0]),1);break;
  case Stage::SaveTick:case Stage::RestoreTick:increment(2,int16_t(v[2]>>16),-1,true);break;
  case Stage::OutputMask:low(6,255);break;
  case Stage::SelectProgram:low(0,99);break;
  case Stage::ClearTime:{const auto result=controlArithmetic::subtract(int32_t(v[0]),int32_t(v[0]),image_.flags);v[0]=uint32_t(result.value);image_.flags=result.flags;break;}
  case Stage::PublishTime:case Stage::PublishDeadline:nz(v[0],32);break;
  case Stage::BindFile:full(4,0x81a000);break;
  case Stage::ReadDivision:word(0,uint16_t(o.value));break;
  case Stage::PublishDivision:nz(uint16_t(v[0]),16);break;
  case Stage::DefaultTempo:full(0,500000);break;
  case Stage::AdvanceFile:{const auto result=controlArithmetic::add(int32_t(v[4]),int32_t(22),image_.flags);v[4]=uint32_t(result.value);image_.flags=result.flags;break;}
  case Stage::ReadPlaying:case Stage::ReadMode:low(0,uint8_t(o.value));break;
  case Stage::RetainStream:savedStream_=v[4];nz(savedStream_,32);break;
  case Stage::RestoreStream:case Stage::ExitRestoreStream:full(4,savedStream_);break;
  case Stage::ObserveKey:nz(uint8_t(v[6]>>8),8);break;
  case Stage::ObserveControl:nz(uint16_t(v[6]>>16),16);break;
  case Stage::CompareControl:image_.flags=controlArithmetic::subtract(int8_t(v[6]),int8_t(27),image_.flags).flags;break;
  default:break;
 }}
 void observeReload(const Service&s){const auto*reload=s.template current<StateReloadService>();if(!reload)return;if(const auto*v=reload->voices())if(v->complete()&&v->releasedAny()){reloadVoiceKnown_=true;reloadVoicePatch_=v->lastPatch();}if(const auto*p=reload->panel())if(const auto*t=p->indicatorTask()){reloadIndicator_=*t;if(t->stage()==PanelControlTask::Stage::CompareToggle)reloadSet_=t->set();}}
 void requireArp(){if(!arp_)throw std::logic_error("Missing active file-player arp observer");}
 void requireEvent(){if(!event_)throw std::logic_error("Missing active file-player event observer");}
 Image image_{};uint32_t savedStream_=0,reloadVoicePatch_=0;bool reloadVoiceKnown_=false,inArp_=false;std::optional<PanelControlTask>reloadIndicator_;uint8_t reloadSet_=0;
 std::optional<NativeArpOwnedCompatibility>arp_;std::optional<MidiFileEventCompatibility>event_;
};
}
