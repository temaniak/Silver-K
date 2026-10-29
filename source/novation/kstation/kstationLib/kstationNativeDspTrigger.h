#pragma once
#include "kstationDspTriggerTask.h"
#include "kstationNativeMidiPump.h"
#include <array>
#include <variant>
namespace kstation {
// Diagnostic caller projection; typed protocol tasks own every branch.
class NativeDspTrigger final {
 using T=DspTriggerTask;using V=DspTriggerValueTask;using B=DspTriggerTableTask;
 struct Job{std::variant<T,V,B>task;uint32_t returned=0;};
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1>CodeRanges{{{0x8260,0x833a}}};
 static bool candidate(uint32_t pc){return pc>=0x8260&&pc<0x833a;}bool active()const{return depth_!=0;}void reset(){depth_=0;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c))return false;auto at=uint32_t(c.getPC());
  if(depth_&&waiting(jobs_[depth_-1])&&at==jobs_[depth_-1].returned){auto&j=jobs_[depth_-1];std::visit([](auto&t){using J=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<J,T>)t.resumeChild();else t.resumeDsp();},j.task);}
  if(!depth_||at!=boundary(jobs_[depth_-1])){Job j;if(at==0x8260)j.task.emplace<T>(uint8_t(c.reg8(8)));else if(at==0x82f8)j.task.emplace<V>(uint8_t(c.reg8(8)),uint16_t(c.reg16(4)));else if(at==0x8310)j.task.emplace<B>(uint8_t(c.reg8(8)),uint8_t(c.reg8(9)),uint16_t(c.reg16(4)));else return false;if(depth_==jobs_.size())throw std::logic_error("K DSP trigger capacity");jobs_[depth_++]=j;}
  auto&j=jobs_[depth_-1];if(waiting(j))return false;if(c.pending_irqs)c.read8(0xfffff2);auto len=bytes(j);auto begin=c.cycles;auto cr=credits(j,c,len);auto cost=std::visit([&](const auto&t){return t.nextClocks(cr);},j.task);c.fetchClocks(len);c.pc=c.makepc(int(at+1));
  if(auto*t=std::get_if<T>(&j.task)){using S=T::Stage;switch(t->stage()){
   case S::RetainProgram:save(c,6);break;case S::RetainScratch:push(c,8);break;case S::RetainAddress:push(c,4);break;case S::RetainValue:save(c,1);break;case S::RetainPort:save(c,5);break;
   case S::SelectMode:c.clockI(2);break;case S::BindProgram:c.reg32(6)=0xba5c;val(c,c.reg32(6));break;case S::CompareMode:arithmetic(c,uint8_t(c.reg8(0)),uint8_t(c.reg8(8)),8,true);break;
   case S::BindSingleAddress:case S::BindFirstAddress:case S::BindSecondAddress:c.reg16(4)=int16_t(t->address());val(c,c.reg16(4));break;
   case S::SendSingle:case S::SendFirst:case S::SendSecond:j.returned=at+len;call(c,t->stage()==S::SendSingle?0x82f8:0x8310,j.returned,false);break;
   case S::ShiftFirst:case S::ShiftSecond:case S::ShiftThird:shiftRight(c,8);break;case S::ShiftOtherFirst:case S::ShiftOtherSecond:shiftRight(c,9);break;
   case S::RestorePort:restore(c,5);break;case S::RestoreValue:restore(c,1);break;case S::RestoreAddress:pop(c,4);break;case S::RestoreScratch:pop(c,8);break;case S::RestoreProgram:restore(c,6);break;case S::RestoreInput:pop(c,0);break;case S::Return:ret(c);break;default:break;}
   t->advanceBefore(cost,cr,[&](T::Read f){auto offset=f==T::Read::Mode?0xc7:f==T::Read::Single?t->singleOffset():f==T::Read::First||f==T::Read::Second?0x4f:0x4e;int r=f==T::Read::Mode?0:f==T::Read::Other||f==T::Read::OtherSecond?9:8;c.reg8(r)=c.read8(int(uint32_t(c.reg32(6))+offset));val(c,c.reg8(r));return uint8_t(c.reg8(r));});
  }else if(auto*t=std::get_if<V>(&j.task)){using S=V::Stage;switch(t->stage()){
   case S::Test:val(c,c.reg8(8));break;case S::Decrement:{int old=c.reg8(8),v=old-1;val(c,v);c.ccr=uint8_t((c.ccr&~2)|((old&~v&0x80)?2:0));c.reg8(8)=int8_t(v);break;}
   case S::Clear:arithmetic(c,uint16_t(c.reg16(9)),uint16_t(c.reg16(9)),16,true);c.reg16(9)=0;break;case S::ShiftFirst:case S::ShiftSecond:shiftRight(c,8);break;
   case S::RotateFirst:case S::RotateSecond:{auto v=uint16_t(c.reg16(9));auto oldCarry=unsigned(c.ccr&1);c.ccr=uint8_t((c.ccr&~1)|(v&1));v=uint16_t((v>>1)|(oldCarry<<15));c.reg16(9)=int16_t(v);val(c,c.reg16(9));break;}
   case S::ClearLow:c.reg8(1)=0;val(c,0);break;case S::Send:verifyWord(c,t->word());j.returned=at+len;call(c,0x165e,j.returned,true);break;case S::Return:ret(c);break;default:break;}t->advanceBefore(cost,cr);
  }else{auto&table=std::get<B>(j.task);using S=B::Stage;switch(table.stage()){
   case S::Copy:c.reg8(0)=c.reg8(8);val(c,c.reg8(0));break;case S::Mask:c.reg8(0)&=6;val(c,c.reg8(0));break;case S::Compare:arithmetic(c,uint8_t(c.reg8(0)),6,8,true);break;
   case S::ShiftFirst:case S::ShiftSecond:case S::ShiftThird:case S::ShiftFourth:{auto v=uint8_t(c.reg8(8));c.ccr=uint8_t((c.ccr&~1)|(v>>7));c.reg8(8)=int8_t(v<<1);val(c,c.reg8(8));break;}
   case S::MaskIndex:c.reg8(8)&=0x10;val(c,c.reg8(8));break;case S::MaskOther:c.reg8(9)&=0xc;val(c,c.reg8(9));break;case S::Merge:c.reg8(8)|=c.reg8(9);val(c,c.reg8(8));break;case S::ClearPointer:arithmetic(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(1)),32,true);c.reg32(1)=0;break;case S::CopyIndex:c.reg8(9)=c.reg8(8);val(c,c.reg8(9));break;case S::BindTable:c.reg32(1)=int32_t(arithmetic(c,uint32_t(c.reg32(1)),0xc124,32,false));break;case S::Send:verifyWord(c,table.word());j.returned=at+len;call(c,0x165e,j.returned,true);break;case S::Return:ret(c);break;default:break;}
   table.advanceBefore(cost,cr,[&](uint32_t address){c.reg32(1)=int32_t(c.read32(int(address)));val(c,c.reg32(1));return uint32_t(c.reg32(1));});
  }
  if(c.cycles-begin!=cost)throw std::logic_error("K DSP trigger phase clocks");bool complete=std::visit([](const auto&t){return t.complete();},j.task);if(complete)--depth_;else if(!waiting(j))c.pc=c.makepc(int(boundary(j)));return true;
 }
private:
 static void push(Stage1Cpu&c,unsigned r){c.setSP(c.getSP()-2);c.write16(c.reg16(r),int(c.getSP()));c.clockI(2);midi_projection::val(c,c.reg16(r));}
 static void pop(Stage1Cpu&c,unsigned r){c.reg16(r)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);midi_projection::val(c,c.reg16(r));}
 static void shiftRight(Stage1Cpu&c,unsigned r){auto v=uint8_t(c.reg8(r));c.ccr=uint8_t((c.ccr&~1)|(v&1));c.reg8(r)=int8_t(v>>1);midi_projection::val(c,c.reg8(r));}
 static void verifyWord(Stage1Cpu&c,uint32_t expected){if(((uint32_t(uint16_t(c.reg16(9)))<<8)|uint8_t(c.reg8(1)))!=expected)throw std::logic_error("K DSP trigger semantic word");}
 static bool waiting(const Job&j){return std::visit([](const auto&t){using J=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<J,T>)return t.awaitingChild();else return t.awaitingDsp();},j.task);}
 static uint32_t boundary(const Job&j){if(waiting(j))return j.returned;return std::visit([](const auto&t)->uint32_t{using J=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<J,T>){constexpr uint32_t p[]{0x8260,0x8264,0x826a,0x8272,0x8274,0x8278,0x827a,0x827c,0x8280,0x8284,0x828c,0x8290,0x82ae,0x82b6,0x82be,0x82c2,0x82c4,0x82cc,0x82ce,0x82d0,0x82d2,0x82da,0x82dc,0x82de,0x82e2,0x82e4,0x82e8,0x82ec,0x82ee,0x82f0,0x82f4,0x82f6,0};auto at=p[unsigned(t.stage())];if(t.stage()>=T::Stage::ReadSingle&&t.stage()<=T::Stage::SendSingle)at+=14*t.lane();return at;}else if constexpr(std::is_same_v<J,V>){constexpr uint32_t p[]{0x82f8,0x82fa,0x82fc,0x82fe,0x8300,0x8302,0x8304,0x8306,0x8308,0x830a,0x830e,0};return p[unsigned(t.stage())];}else{constexpr uint32_t p[]{0x8310,0x8312,0x8314,0x8316,0x8318,0x831a,0x831c,0x831e,0x8320,0x8322,0x8324,0x8326,0x8328,0x832a,0x8330,0x8334,0x8338,0};return p[unsigned(t.stage())];}},j.task);}
 static unsigned bytes(const Job&j){return std::visit([](const auto&t){using J=std::decay_t<decltype(t)>;auto s=t.stage();if constexpr(std::is_same_v<J,T>){using S=T::Stage;switch(s){case S::BindProgram:return 6u;case S::ReadMode:case S::ReadSingle:case S::ReadFirst:case S::ReadOther:case S::ReadSecond:case S::ReadOtherSecond:return 8u;case S::RetainProgram:case S::RetainValue:case S::RetainPort:case S::RestorePort:case S::RestoreValue:case S::RestoreProgram:case S::SelectMode:case S::BindSingleAddress:case S::BindFirstAddress:case S::BindSecondAddress:return 4u;default:return 2u;}}else if constexpr(std::is_same_v<J,V>)return s==V::Stage::Send?4u:2u;else return s==B::Stage::BindTable?6u:s==B::Stage::ReadTable||s==B::Stage::Send?4u:2u;},j.task);}
 static unsigned bus(uint32_t a,unsigned bytes,int last){unsigned n=0;for(unsigned i=0;i<bytes;++i){auto at=(a+i)&0xffffff;if(at<0x400000||(at>=0xfffd10&&at<0xffffa0)){if((last&1)||at!=uint32_t(last|1))n+=at<0x400000?3:1;last=int(at);}else{n+=at>=0xffffa0?1:3;last=-1;}}return n;}
 static DspTriggerCredits credits(const Job&j,const Stage1Cpu&c,unsigned len){DspTriggerCredits cr;auto r=int(c.getPC()+len-1);cr.retainWord=5+bus(c.getSP()-2,2,c.writeHistory());cr.retainLong=8+bus(c.getSP()-4,4,c.writeHistory());cr.restoreWord=5+bus(c.getSP(),2,r);cr.restoreLong=8+bus(c.getSP(),4,r);cr.shortCall=3+bus(c.getSP()-4,4,c.writeHistory());cr.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.ret=5+bus(c.getSP(),4,r);if(auto*t=std::get_if<B>(&j.task))cr.table=6+bus(t->tableAddress(),4,r);return cr;}
 std::array<Job,4>jobs_{};unsigned depth_=0;
};
}
