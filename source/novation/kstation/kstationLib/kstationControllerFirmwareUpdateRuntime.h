#pragma once
#include "kstationControllerFlashStateRuntime.h"
#include "kstationControllerNumericDisplayRuntime.h"
namespace kstation {
// Firmware image publication, progress and the original warning/error holds.
// Actual page programming is delegated to the same native Flash service.
class ControllerFirmwareUpdateRuntime final {
 enum class S:uint8_t{Inactive,ReadSize,AddChecksum,CompareLimit,SelectLimit,CopyUpper,CopyPageHigh,CopyPageLow,TestTail,SelectTail,RoundPages,CallPages,Return,
  RetainTotal,ClearCounter,WriteDirty,CallPage,TestPage,SelectPage,Increment,CopyCounter,MaskCounter,SelectProgress,RetainCounter,RetainPages,CallProgress,RestorePages,RestoreCounter,Decrement,Repeat,CallNotice,PagesReturn,
  ReadDirty,SelectDirty,BindWarning,CallWarning,RetainDelay,CountSettling,RetainInner,CountDelay,TickDelay,RepeatDelay,RestoreInner,TickSettling,RepeatSettling,RestoreDelay,NoticeReturn,
  CopyTotal,BindPercent,Multiply,Divide,RetainSource,BindPosition,CallPosition,CallNumber,BindPercentSign,CallPercentSign,RestoreSource,ProgressReturn,ErrorCall,ErrorReturn,ErrorHold};
 enum class Child:uint8_t{None,Page,Lcd,Number};
public:
 struct Result{unsigned clocks=0;bool returned=false;};
 bool bindBootResources(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(3))return false;for(auto r:ranges_)for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a])return false;for(auto r:ranges_)if(!d.protectImmutableVerified(r.first,r.second,image+r.first))return false;bound_=&d;return true;}
 bool bindMirroredFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_!=&d||mirrored_||active()||!image||size!=49728||!d.hasImmutableCapacity(11))return false;for(auto r:ranges_)for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!flash_.bindVerifiedFirmware(d,image,size))return false;for(auto r:ranges_)if(!d.protectImmutableVerified(0x400000+r.first,0x400000+r.second,image+r.first))return false;mirrored_=true;return true;}
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(14))return false;for(auto r:ranges_)for(auto a=r.first;a<r.second;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;return bindBootResources(d,image,size)&&bindMirroredFirmware(d,image,size);}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}
 bool startProgress(uint16_t counter,uint16_t total){if(!bound_||active())return false;counter_=counter;total_=total;progressOnly_=true;stage_=S::CopyTotal;return true;}
 bool active()const{return stage_!=S::Inactive;}bool held()const{return stage_==S::ErrorHold;}void reset(){stage_=S::Inactive;child_=Child::None;flash_.reset();}
 bool start(uint32_t source=0x410000,uint32_t target=0x210000){if(!bound_||!mirrored_||active()||source!=0x410000||target!=0x210000)return false;source_=source;target_=target;stage_=S::ReadSize;child_=Child::None;progressOnly_=false;return true;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerLcdRuntime&lcd,const ControllerNumericDisplayRuntime&numeric)const{
  if(bound_!=&d||!active()||(!irq.boundTo(d)&&irq.mask()!=ControllerInterruptRuntime::Mask::Disabled)||!lcd.boundTo(d)||!numeric.boundTo(d)||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;
  if(child_==Child::Page)return flash_.nextClocks(d,irq,lcd);if(child_==Child::Lcd)return lcd.nextClocks(d,irq);if(child_==Child::Number)return numeric.nextClocks(d,irq,lcd);
  auto n=cost(d);return n+(d.pendingIrqs?1u:0u);
 }
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerLcdRuntime&lcd,ControllerNumericDisplayRuntime&numeric,unsigned budget){
  auto full=nextClocks(d,irq,lcd,numeric);if(!full||budget<full)return {};
  if(child_!=Child::None){unsigned clocks=0;bool done=false;if(child_==Child::Page){auto r=flash_.advanceBefore(d,irq,lcd,budget);clocks=r.clocks;done=r.returned;if(done){pageFailed_=flash_.failed();source_=flash_.sourceCursor();target_=flash_.targetCursor();}}else if(child_==Child::Lcd){auto r=lcd.advanceBefore(d,irq,budget);clocks=r.clocks;done=r.returned;}else{auto r=numeric.advanceBefore(d,irq,lcd,budget);clocks=r.clocks;done=r.returned;}if(done)child_=Child::None;return {clocks,false};}
  auto arb=d.pendingIrqs?1u:0u;if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;auto n=full-arb;auto s=stage_;auto next=S(unsigned(s)+1);bool stack=false;
  switch(s){case S::ReadSize:d.cycles=begin+15;size_=d.read32(source_+0x104);break;case S::AddChecksum:size_+=4;break;case S::SelectLimit:if(size_>0xfd00)next=S::ErrorCall;break;
   case S::CopyPageLow:remaining_=uint16_t(size_>>8);break;case S::SelectTail:if(!(size_&255))next=S::CallPages;break;case S::RoundPages:++remaining_;break;case S::CallPages:next=S::RetainTotal;stack=true;break;
   case S::RetainTotal:total_=remaining_;break;case S::ClearCounter:counter_=0;break;case S::WriteDirty:d.cycles=begin+9;d.write16(0xfffe32,0);break;
   case S::CallPage:if(!flash_.start(ControllerFlashStateRuntime::Kind::Page,source_,target_))throw std::logic_error("K firmware page admission");child_=Child::Page;stack=true;break;case S::SelectPage:if(pageFailed_)next=S::ErrorCall;break;
   case S::Increment:++counter_;break;case S::SelectProgress:if(counter_&7)next=S::Decrement;break;case S::RetainCounter:case S::RetainPages:stack=true;break;case S::CallProgress:next=S::CopyTotal;stack=true;break;
   case S::Decrement:--remaining_;break;case S::Repeat:if(remaining_)next=S::CallPage;break;case S::CallNotice:next=S::ReadDirty;stack=true;break;case S::PagesReturn:next=S::Return;break;case S::Return:next=S::Inactive;break;
   case S::ReadDirty:d.cycles=begin+9;dirty_=d.read16(0xfffe32);break;case S::SelectDirty:if(dirty_)next=S::NoticeReturn;break;case S::CallWarning:if(!lcd.start(LcdTextTask::Kind::String,0,0,0xa9fc))throw std::logic_error("K firmware warning LCD");child_=Child::Lcd;stack=true;break;
   case S::RetainDelay:case S::RetainInner:stack=true;break;case S::CountSettling:settling_=200;break;case S::CountDelay:delay_=2663;break;case S::TickDelay:--delay_;break;case S::RepeatDelay:if(delay_)next=S::TickDelay;break;case S::TickSettling:--settling_;break;case S::RepeatSettling:if(settling_)next=S::RetainInner;break;case S::NoticeReturn:next=S::PagesReturn;break;
   case S::Divide:percent_=uint16_t(total_?(uint32_t(counter_)*100u)/total_:0u);break;case S::RetainSource:stack=true;break;
   case S::CallPosition:case S::CallPercentSign:if(!lcd.start(LcdTextTask::Kind::Character,s==S::CallPosition?0x8d:0x25))throw std::logic_error("K firmware progress LCD");child_=Child::Lcd;stack=true;break;
   case S::CallNumber:if(!numeric.start(NumericDisplayTask::Kind::ZeroTwo,percent_))throw std::logic_error("K firmware progress number");child_=Child::Number;stack=true;break;case S::ProgressReturn:next=progressOnly_?S::Inactive:S::RestorePages;break;
   case S::ErrorCall:stack=true;next=S::ErrorReturn;break;case S::ErrorReturn:next=S::ErrorHold;break;case S::ErrorHold:next=S::ErrorHold;break;default:break;
  }
  stage_=next;d.cycles=begin+n;d.lastRead=-1;if(stack)d.lastWrite=-1;return {full,!active()};
 }
private:
 static unsigned writeCost(const ControllerDeviceState&d){return (d.lastWrite&1)||0xfffe32u!=uint32_t(d.lastWrite|1)?1:0;}
 unsigned cost(const ControllerDeviceState&d)const{switch(stage_){case S::ReadSize:return 27;case S::CompareLimit:case S::BindWarning:return 9;case S::CallPages:case S::CallNotice:return 5;
  case S::CallPage:case S::CallProgress:case S::CallWarning:case S::CallPosition:case S::CallNumber:case S::CallPercentSign:case S::ErrorCall:return 10;
  case S::Return:case S::PagesReturn:case S::NoticeReturn:case S::ProgressReturn:case S::ErrorReturn:return 7;case S::RetainPages:case S::RestorePages:case S::RetainDelay:case S::RetainInner:case S::RestoreInner:case S::RestoreDelay:return 6;
  case S::RetainCounter:case S::RestoreCounter:case S::RetainSource:case S::RestoreSource:return 10;case S::WriteDirty:return 9+writeCost(d);case S::ReadDirty:return 10;
  case S::CountSettling:case S::CountDelay:case S::BindPercent:return 6;case S::Multiply:case S::Divide:return 23;default:return 3;}}
 inline static constexpr std::pair<uint32_t,uint32_t>ranges_[]={{0x105a,0x10e2},{0x13f4,0x1418},{0x1432,0x1434}};
 ControllerDeviceState*bound_=nullptr;ControllerFlashStateRuntime flash_;S stage_=S::Inactive;Child child_=Child::None;uint32_t source_=0,target_=0,size_=0;uint16_t remaining_=0,total_=0,counter_=0,dirty_=0,percent_=0,delay_=0,settling_=0;bool pageFailed_=false,progressOnly_=false,mirrored_=false;
};
}
