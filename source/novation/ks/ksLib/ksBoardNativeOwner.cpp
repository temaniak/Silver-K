#include "ksBoardNativeOwner.h"
#include "ksNativeStartupController.h"
#include "ksNativeControllerExecutor.h"
#include "ksControllerImageValidation.h"
#include <stdexcept>
namespace ks {
struct BoardNativeOwner::Impl final {
 enum class Phase {None,Startup,Interrupt,Playback};
 struct Plan{Phase phase=Phase::None;unsigned cost=0;NativeControllerExecutor::PreparedPhase playback{};};
 NativeStartupController startup;
 NativeControllerExecutor playback;
 const uint8_t*rom;unsigned romBytes;
 bool published=false,overlayChecked=false;
 uint64_t startupPhaseBase=0;
 Impl(const uint8_t*image,unsigned size):rom(image),romBytes(size){
  startup.bindVerifiedRom(image,size); // authenticate complete ROM before either metadata owner
  if(!playback.bindVerifiedRom(image))throw std::logic_error("Native playback metadata admission failed");
 }
 Plan plan(const ControllerDeviceState&d)const{
  if(published){const auto p=playback.preparePlaybackPhase(d);return{Phase::Playback,p.cost,p};}
  // Ordinary reset mode has no enabled interrupt ownership before mode2 is
  // established. Pending device events remain retained for that transition.
  if(d.interruptMode2){
   const auto mask=startup.priority();
   const auto p=playback.prepareInterruptPhase(d,uint8_t(mask));
   if(p.blocked!=NativeControllerExecutor::Blocked::None)return{};
   if(p.cost)return{Phase::Interrupt,p.cost,p};
  }else if(playback.activeInterrupt())return{};
  return{Phase::Startup,startup.nextClocks(d),{}};
 }
 unsigned advance(ControllerDeviceState&d,unsigned available){
  const auto p=plan(d);if(!p.cost||available<p.cost)return 0;return consume(d,p);
 }
 unsigned advanceNextPhase(ControllerDeviceState&d){
  const auto p=plan(d);return p.cost?consume(d,p):0;
 }
 unsigned advanceNextBoardPhase(ControllerDeviceState&d){
  if(!published)return advanceNextPhase(d);
  const auto p=playback.prepareBoardPhase(d);if(!p.phase.cost)return 0;
  const Plan ordinary{Phase::Playback,p.phase.cost,p.phase};return consume(d,ordinary,&p);
 }
 unsigned consume(ControllerDeviceState&d,const Plan&p,const NativeControllerExecutor::PreparedBoardPhase*board=nullptr){
  // Old Board authenticated executable SRAM before IRQ dispatch as well as
  // foreground. This readonly guard charges no clocks/history/callbacks and
  // publishes protection only after whole-phase budget admission.
  if(!overlayChecked&&d.nativeLowWritable){controllerImage::verifyOverlay(rom,romBytes,d.nativeLowBase,d.nativeLowSize);overlayChecked=true;}
  unsigned used=0;
  switch(p.phase){
  case Phase::Startup:used=startup.advanceBefore(d,p.cost);break;
  case Phase::Interrupt:case Phase::Playback:used=board?playback.consumeBoardPhase(d,*board):playback.consumePreparedPhase(d,p.playback);break;
  default:break;
  }
  if(used!=p.cost)throw std::logic_error("Native Board owner phase differs from planned cost");
  if(!published&&startup.firstMainAvailable()){
   const auto&result=startup.result();
   if(!result.known())throw std::logic_error("Native startup playback producer is missing");
   NativeControllerExecutor::Input input{result.presentation,result.activation,result.expressionSource};
   if(!playback.beginPlayback(input))throw std::logic_error("Native startup playback ownership transfer failed");
   published=true;
  }else if(!published&&startup.complete()&&startup.outcome()==NativeStartupController::Outcome::MissingDomain)
   throw std::logic_error("Native startup playback producer is missing");
  return used;
 }
};
BoardNativeOwner::BoardNativeOwner(ControllerDeviceState&,const uint8_t*rom,unsigned size):impl_(std::make_unique<Impl>(rom,size)){}
BoardNativeOwner::~BoardNativeOwner()=default;
unsigned BoardNativeOwner::nextClocks(const ControllerDeviceState&d)const{return impl_->plan(d).cost;}
unsigned BoardNativeOwner::advanceBefore(ControllerDeviceState&d,unsigned available){return impl_->advance(d,available);}
unsigned BoardNativeOwner::advanceNextPhase(ControllerDeviceState&d){return impl_->advanceNextPhase(d);}
unsigned BoardNativeOwner::advanceNextBoardPhase(ControllerDeviceState&d){return impl_->advanceNextBoardPhase(d);}
bool BoardNativeOwner::mainEntered()const{return impl_->startup.mainEntered();}
bool BoardNativeOwner::ready()const{return impl_->startup.ready();}
bool BoardNativeOwner::protectedCode()const{return impl_->overlayChecked||impl_->startup.overlayVerified();}
void BoardNativeOwner::resetMetrics(){impl_->playback.resetMetrics();impl_->startupPhaseBase=impl_->startup.metrics().phases;}
const NativeStartupController&BoardNativeOwner::startup()const{return impl_->startup;}
const NativeControllerExecutor&BoardNativeOwner::playback()const{return impl_->playback;}
bool BoardNativeOwner::playbackPublished()const{return impl_->published;}
uint64_t BoardNativeOwner::startupPhasesSinceReset()const{return impl_->startup.metrics().phases-impl_->startupPhaseBase;}
}
