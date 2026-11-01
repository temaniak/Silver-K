#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationLcdTask.h"
#include "kstationLcdTextTask.h"
#include <variant>
namespace kstation {
// Shared foreground LCD protocol, including pre-overlay reset operation.
// Timer1 has its own LCD phases and must win arbitration while an IRQ is active.
class ControllerLcdRuntime final {
 using L=LcdTask;using T=LcdTextTask;struct Job{std::variant<L,T>task;uint8_t argument=0;};
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};struct Range{uint32_t begin,end;};
 inline static constexpr std::array<Range,4>CodeRanges{{{0x20e2,0x20f6},{0x2284,0x22da},{0x22fc,0x2312},{0x2318,0x23b2}}};
 bool bindBootResources(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(CodeRanges.size()))return false;for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a])return false;for(auto r:CodeRanges)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin))return false;bound_=&d;return true;}
 bool bindMirroredFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_!=&d||mirrored_||!image||size!=49728||!d.hasImmutableCapacity(CodeRanges.size()))return false;for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:CodeRanges)if(!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;mirrored_=true;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}bool active()const{return depth_!=0;}void reset(){depth_=0;value_=0;}
 uint8_t result()const{return value_;}
 bool start(L::Kind kind,uint8_t value=0){if(!bound_||active())return false;push(kind,value);return true;}
 bool start(T::Kind kind,uint8_t value=0,uint8_t count=0,uint32_t source=0){if(!bound_||active()||source>0xffffff)return false;push(kind,value,count,source);return true;}
 bool stackWrites()const{if(!active())return false;const auto&j=jobs_[depth_-1];return std::visit([](const auto&t){using X=std::decay_t<decltype(t)>;auto s=t.stage();if constexpr(std::is_same_v<X,L>)return s==L::Stage::Retain||s==L::Stage::DispatchReady||s==L::Stage::DispatchWrite||s==L::Stage::FirstRead||s==L::Stage::FinalRead||s==L::Stage::DispatchNotice;else return s==T::Stage::Retain||s==T::Stage::Ready||s==T::Stage::DispatchData||s==T::Stage::Wrap||s==T::Stage::ReadyAgain||s==T::Stage::Command1||s==T::Stage::Command2||s==T::Stage::Command3||s==T::Stage::CursorCommand||s==T::Stage::WriteSpace||s==T::Stage::LineCursor||s==T::Stage::LineSpaces||s==T::Stage::LineCursorAgain||s==T::Stage::ClearFirst||s==T::Stage::ClearSecond||s==T::Stage::ControlCommand||s==T::Stage::DispatchCharacter;},j.task);}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{if(bound_!=&d||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(!irq.boundTo(d)&&irq.mask()!=ControllerInterruptRuntime::Mask::Disabled)||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;auto cr=credits(d);auto n=std::visit([&](const auto&t){return t.nextClocks(cr);},jobs_[depth_-1].task);return n?n+(d.pendingIrqs?1u:0u):0;}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned available){if(bound_!=&d)return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(!irq.boundTo(d)&&irq.mask()!=ControllerInterruptRuntime::Mask::Disabled)||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};auto total=nextClocks(d,irq);if(!total||available<total)return {};auto arb=d.pendingIrqs?1u:0u;if(arb)(void)d.read8(0xfffff2);auto clocks=total-arb;auto begin=d.cycles;auto&j=jobs_[depth_-1];auto cr=credits(d);bool stack=stackWrites(),done=false,child=false;L::Kind lk=L::Kind::Read;T::Kind tk=T::Kind::RawData;bool text=false;uint8_t argument=0,count=0;
 if(auto*t=std::get_if<L>(&j.task)){auto stage=t->stage();if(stage==L::Stage::DispatchReady){child=true;lk=L::Kind::Ready;}else if(stage==L::Stage::DispatchWrite){child=true;lk=L::Kind::Write;argument=j.argument;}else if(stage==L::Stage::FirstRead||stage==L::Stage::FinalRead){child=true;lk=L::Kind::Read;}else if(stage==L::Stage::DispatchNotice){child=true;lk=L::Kind::Command;argument=0x0e;}
 auto used=t->advanceBefore(clocks,cr,[&]{d.cycles=begin+3;return d.read8(0xffffc7);},[&](L::Write field,uint8_t value){unsigned a=field==L::Write::Direction?0xffffc5:field==L::Write::Port?0xffffd3:field==L::Write::Data?0xffffc7:0xfffe91;d.cycles=begin+(field==L::Write::NoticeCount?9u:3u);d.write8(a,value);});if(used!=clocks)throw std::logic_error("K domain LCD command phase");done=t->complete();if(done)value_=t->kind()==L::Kind::Read||t->kind()==L::Kind::Ready?t->value():j.argument;
 }else{auto&u=std::get<T>(j.task);using S=T::Stage;auto s=u.stage();argument=u.value();switch(s){case S::Ready:case S::ReadyAgain:child=true;lk=L::Kind::Ready;break;case S::Wrap:child=true;lk=L::Kind::Write;break;case S::Command1:case S::Command2:case S::Command3:case S::CursorCommand:case S::ControlCommand:child=true;lk=L::Kind::Command;break;case S::DispatchData:child=text=true;tk=u.kind()==T::Kind::Character?T::Kind::Data:T::Kind::RawData;argument=u.kind()==T::Kind::Character?u.value():u.original();break;case S::DispatchCharacter:child=text=true;tk=T::Kind::Character;break;case S::WriteSpace:child=text=true;tk=T::Kind::Data;break;case S::LineCursor:case S::LineCursorAgain:child=text=true;tk=T::Kind::Cursor;break;case S::LineSpaces:child=text=true;tk=T::Kind::Spaces;count=8;break;case S::ClearFirst:child=text=true;tk=T::Kind::FirstLine;break;case S::ClearSecond:child=text=true;tk=T::Kind::SecondLine;break;default:break;}
 auto used=u.advanceBefore(clocks,cr,[&](unsigned index){d.cycles=begin+3;return d.read8(u.source()+index);},[&](T::Write field,unsigned value){d.cycles=begin+3;d.write8(field==T::Write::Port?0xffffd3:0xffffc7,uint8_t(value));});if(used!=clocks)throw std::logic_error("K domain LCD text phase");done=u.complete();if(done)value_=j.argument;
 }
 d.cycles=begin+clocks;d.lastRead=-1;if(stack)d.lastWrite=-1;
 if(child){if(text)push(tk,argument,count);else push(lk,argument);}else if(done){--depth_;if(depth_)std::visit([&](auto&t){checkResume(t.resumeChild(value_));},jobs_[depth_-1].task);}
 return {total,depth_==0,Block::None};}
private:static void checkResume(bool ok){if(!ok)throw std::logic_error("K domain LCD continuation");}
 void push(L::Kind k,uint8_t v){if(depth_==jobs_.size())throw std::logic_error("K domain LCD capacity");auto&j=jobs_[depth_++];j.task.emplace<L>(k,v);j.argument=v;}
 void push(T::Kind k,uint8_t v,uint8_t count=0,uint32_t source=0){if(depth_==jobs_.size())throw std::logic_error("K domain LCD capacity");auto&j=jobs_[depth_++];j.task.emplace<T>(k,v,count,source);j.argument=v;}
 static unsigned bus(unsigned a,int last){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?((last&1)||a!=unsigned(last|1)?(a<0x400000?3u:1u):0u):(a>=0xffffa0?1u:3u);}
 FlashCredits credits(const ControllerDeviceState&d)const{FlashCredits cr;cr.pushWord=6;cr.popWord=6;cr.pushLong=10;cr.popLong=10;cr.call=10;cr.shortCall=5;cr.ret=7;cr.write=1;cr.read=1;if(auto*t=std::get_if<T>(&jobs_[depth_-1].task))if(t->stage()==T::Stage::Read)cr.read=bus(t->source()+t->index(),-1);(void)d;return cr;}
 ControllerDeviceState*bound_=nullptr;bool mirrored_=false;std::array<Job,16>jobs_{};unsigned depth_=0;uint8_t value_=0;
};
}
