#pragma once
#include "kstationDisplayFormatTask.h"
#include "kstationControllerLcdRuntime.h"
#include "kstationControllerInterruptRuntime.h"
#include <optional>
namespace kstation {
class ControllerBarGraphRuntime final {
 using T=DisplayBarGraphTask;using S=T::Stage;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,LcdBusy};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x23f8u;a<0x2438;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x23f8,0x2438,image+0x23f8)||!d.protectImmutableVerified(0x4023f8,0x402438,image+0x23f8))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return bool(task_);}bool start(uint8_t value){if(!bound_||active())return false;task_.emplace(value);return true;}void reset(){if(lcd_)lcd_->reset();lcd_=nullptr;task_.reset();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerLcdRuntime&lcd)const{if(bound_!=&d||!irq.boundTo(d)||!lcd.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(lcd.active()&&!lcd_))return 0;if(lcd_)return lcd_->nextClocks(d,irq);auto c=cost();return c?c+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerLcdRuntime&lcd,unsigned budget){if(bound_!=&d||!irq.boundTo(d)||!lcd.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(lcd.active()&&!lcd_)return {0,false,Block::LcdBusy};if(lcd_){auto r=lcd_->advanceBefore(d,irq,budget);if(r.returned){lcd_=nullptr;auto child=task_->child();task_->resume(child);if(task_->complete()){task_.reset();return {r.clocks,true,Block::None};}}return {r.clocks,false,r.blocked==ControllerLcdRuntime::Block::NeedsInterrupt?Block::NeedsInterrupt:Block::None};}auto c=cost(),arb=d.pendingIrqs?1u:0u;if(!c||budget<c+arb)return {};if(arb)(void)d.read8(0xfffff2);auto s=task_->stage();task_->advance();if(task_->awaitingChild()){auto ch=task_->child();auto k=ch==T::Child::Cursor||ch==T::Child::Tail?LcdTextTask::Kind::Character:LcdTextTask::Kind::DirectData;if(!lcd.start(k,task_->character()))return {0,false,Block::LcdBusy};lcd_=&lcd;}d.clockInternal(c);d.lastRead=-1;if(s==S::SendCursor||s==S::SendFull||s==S::SendPartial||s==S::SendEmpty)d.lastWrite=-1;return {c+arb,false,Block::None};}
private:unsigned cost()const{switch(task_->stage()){case S::SendCursor:case S::SendFull:case S::SendPartial:case S::SendEmpty:return 10;case S::SendTerminator:return 8;case S::AwaitChild:case S::Complete:return 0;default:return 3;}}ControllerDeviceState*bound_=nullptr;ControllerLcdRuntime*lcd_=nullptr;std::optional<T>task_{};
};
}
