#pragma once
#include "kstationControllerPanelHardwareRuntime.h"
#include "kstationControllerNumericDisplayRuntime.h"
#include "kstationStartupDisplayTask.h"
namespace kstation {
// Program/startup display. General edit-menu tails remain named UI requests.
class ControllerStartupDisplayRuntime final {
 using T=StartupDisplayTask;using K=T::Kind;using S=T::Stage;struct Job{T task;uint32_t source=0xbe76;uint8_t input=0;};enum class Child:uint8_t{None,Panel,Lcd,Numeric};
public:
 enum class Service:uint8_t{None,EditMenu,CurrentControl,OtherMode};struct Request{Service service=Service::None;uint8_t mode=0,input=0;};enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,ServiceBusy,AwaitService};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};struct Range{uint32_t begin,end;};inline static constexpr std::array<Range,3>CodeRanges{{{0x4358,0x438a},{0x4b5c,0x4b6a},{0x4bca,0x4c36}}};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2*CodeRanges.size()))return false;for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:CodeRanges)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return depth_!=0;}void reset(){depth_=0;child_=Child::None;tail_=false;external_={};}Request request()const{return external_;}
 bool start(K kind=K::Refresh,uint32_t source=0xbe76,uint8_t input=0){if(!bound_||active()||source>0xffffff)return false;jobs_[depth_++]={T(kind),source,input};return true;}
 bool completeService(Service service){if(service==Service::None||external_.service!=service)return false;external_={};finish();return true;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerPanelHardwareRuntime&panel,const ControllerNumericDisplayRuntime&numeric,const ControllerLcdRuntime&lcd)const{if(bound_!=&d||!active()||!irq.boundTo(d)||!panel.boundTo(d)||!numeric.boundTo(d)||!lcd.boundTo(d)||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||external_.service!=Service::None)return 0;if(child_==Child::Panel)return panel.nextClocks(d,irq);if(child_==Child::Lcd)return lcd.nextClocks(d,irq);if(child_==Child::Numeric)return numeric.nextClocks(d,irq,lcd);auto s=jobs_[depth_-1].task.stage();if(s==S::SetMode&&panel.active()||lcdCall(s)&&lcd.active()||s==S::ProgramNumber&&(numeric.active()||lcd.active()))return 0;auto n=jobs_[depth_-1].task.nextClocks(credits(d));return n?n+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerPanelHardwareRuntime&panel,ControllerNumericDisplayRuntime&numeric,ControllerLcdRuntime&lcd,unsigned available){if(bound_!=&d||!irq.boundTo(d)||!panel.boundTo(d)||!numeric.boundTo(d)||!lcd.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(external_.service!=Service::None)return {0,false,Block::AwaitService};if(child_!=Child::None){unsigned clocks;bool done;if(child_==Child::Panel){auto r=panel.advanceBefore(d,irq,available);clocks=r.clocks;done=r.returned;}else if(child_==Child::Numeric){auto r=numeric.advanceBefore(d,irq,lcd,available);clocks=r.clocks;done=r.returned;}else{auto r=lcd.advanceBefore(d,irq,available);clocks=r.clocks;done=r.returned;}if(done){child_=Child::None;if(tail_){tail_=false;finish();}else jobs_[depth_-1].task.resume();}return {clocks,depth_==0,Block::None};}
 auto total=nextClocks(d,irq,panel,numeric,lcd);if(!total||available<total)return {};auto arb=d.pendingIrqs?1u:0u;if(arb)(void)d.read8(0xfffff2);auto clocks=total-arb;auto begin=d.cycles;auto&j=jobs_[depth_-1];auto&t=j.task;auto stage=t.stage();auto cr=credits(d);auto value=t.value();bool stack=stage==S::Retain||stage==S::SetMode||stage==S::DemoText||stage==S::LabelChild||stage==S::BankCharacter||stage==S::ProgramNumber;bool lcdStart=false;auto lcdKind=LcdTextTask::Kind::String;uint8_t character=0;
 if(stage==S::BindLabel)j.source=0xbe76;if(stage==S::BindDemo)j.source=0xbe66;if(stage==S::BindWrite)j.source=0xbe94;if(stage==S::BindOverwrite)j.source=0xbea3;if(stage==S::ClearHigh)j.input=0;
 if(t.advanceBefore(clocks,cr,[&]{auto a=readAddress(t);d.cycles=begin+(stage==S::ReadMode?12u:9u);return d.read8(a);},[&](unsigned v){d.cycles=begin+9;d.write8(t.kind()==K::ClearSelection?0xce1d:0xcd7f,uint8_t(v));})!=clocks)throw std::logic_error("K domain startup display phase");
 d.cycles=begin+clocks;d.lastRead=-1;if(stack)d.lastWrite=-1;
 if(stage==S::SetMode){if(!panel.start(ControllerPanelHardwareRuntime::Kind::First,t.mode()))throw std::logic_error("K display panel admission");child_=Child::Panel;}
 else if(stage==S::LabelChild){if(depth_==jobs_.size())throw std::logic_error("K display continuation capacity");jobs_[depth_++]={T(K::Label),j.source,0};}
 else if(stage==S::ProgramNumber){if(!numeric.start(NumericDisplayTask::Kind::ZeroTwo,value))throw std::logic_error("K display number admission");child_=Child::Numeric;}
 else if(stage==S::DemoText)lcdStart=true;
 else if(stage==S::BankCharacter){lcdStart=true;lcdKind=LcdTextTask::Kind::Character;character=uint8_t(value+0x30);}
 else if(t.complete()){
  if(stage==S::SelectEdit)external_={Service::EditMenu,t.mode(),j.input};
  else if(stage==S::SelectSelection)external_={Service::CurrentControl,t.mode(),0};
  else if(stage==S::OtherMode)external_={Service::OtherMode,t.mode(),0};
  else if(stage==S::SelectMode){j.task=T(K::Program);j.source=0xbe76;}
  else if(stage==S::CursorTail){lcdStart=true;tail_=true;lcdKind=LcdTextTask::Kind::Character;character=0x8d;}
  else if(stage==S::SelectWriteLabel||stage==S::OverwriteTail||stage==S::LabelTail){lcdStart=true;tail_=true;}
  else finish();
 }
 if(lcdStart){if(!lcd.start(lcdKind,character,0,j.source))throw std::logic_error("K display LCD admission");child_=Child::Lcd;}
 return {total,depth_==0,Block::None};}
private:static bool lcdCall(S s){return s==S::DemoText||s==S::BankCharacter||s==S::CursorTail||s==S::OverwriteTail||s==S::LabelTail||s==S::SelectWriteLabel;}static unsigned readAddress(const T&t){switch(t.stage()){case S::ReadMode:return 0xbb0e;case S::ReadEdit:case S::ReadEditMode:return 0xcd7c;case S::ReadSelection:return 0xcd7f;case S::ReadDemo:return 0xcd77;case S::ReadBank:return 0xcd82;case S::ReadProgram:return 0xcd83;case S::ReadWriteMode:return 0xcd7b;default:return 0;}}
 void finish(){--depth_;if(depth_)jobs_[depth_-1].task.resume();}
 static unsigned bus(unsigned a,int last){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?((last&1)||a!=unsigned(last|1)?(a<0x400000?3u:1u):0u):(a>=0xffffa0?1u:3u);}
 FlashCredits credits(const ControllerDeviceState&d)const{FlashCredits cr;cr.pushWord=cr.popWord=6;cr.call=10;cr.shortCall=5;cr.ret=7;cr.read=3;cr.write=bus(jobs_[depth_-1].task.kind()==K::ClearSelection?0xce1d:0xcd7f,d.lastWrite);return cr;}
 ControllerDeviceState*bound_=nullptr;std::array<Job,3>jobs_{};unsigned depth_=0;Child child_=Child::None;bool tail_=false;Request external_{};
};
}
