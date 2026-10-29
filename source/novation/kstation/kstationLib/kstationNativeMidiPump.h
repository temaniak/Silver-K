#pragma once
#include "kstationMidiPumpTask.h"
#include "kstationHdiWait.h"
#include <cstdint>
#include <array>
namespace kstation {
namespace midi_projection {
inline void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
inline uint32_t arithmetic(Stage1Cpu&c,uint32_t a,uint32_t b,unsigned bits,bool sub){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=1u<<(bits-5);a&=mask;b&=mask;const uint32_t r=(sub?a-b:a+b)&mask;const auto carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r));const auto over=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((over&sign)?2:0));return r;}
inline void save(Stage1Cpu&c,int reg){c.setSP(c.getSP()-4);c.write32(c.reg32(reg),int(c.getSP()));val(c,c.reg32(reg));c.clockI(2);}
inline void restore(Stage1Cpu&c,int reg){c.reg32(reg)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);val(c,c.reg32(reg));c.clockI(2);}
inline void ret(Stage1Cpu&c){c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}
inline void call(Stage1Cpu&c,uint32_t target,uint32_t returned,bool wide){const auto sp=c.getSP()-4;c.write32(int32_t(returned),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(wide)c.clockI(2);c.pc=c.makepc(int(target));}
inline bool blocked(const Stage1Cpu&c){return c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80));}
}
// Fixed instruction-boundary projection. The semantic task owns only MIDI
// fields; the calling bridge owns the temporary H8 frame.
class NativeMidiDequeue final {
 using T=MidiDequeueTask;using S=T::Stage;
public:
 explicit NativeMidiDequeue(T::Kind k):task_(k){}const T&task()const{return task_;}
 uint32_t boundary()const{const bool e=task_.kind()==T::Kind::External;switch(task_.stage()){
  case S::ClearResult:return 0x27b6;case S::SaveFirst:return e?0xa02:0x27b8;case S::SaveSecond:return e?0xa06:0x27bc;
  case S::LocateStatus:return 0xa0a;case S::SaveMask:return 0xa10;case S::Mask:return 0xa12;case S::ReadStatus:return 0xa14;case S::ClearByte:return 0xa16;case S::ClearStatus:return 0xa18;case S::RestoreMask:return 0xa1a;
  case S::ReadConsumer:return e?0xa1c:0x27c0;case S::ReadProducer:return e?0xa22:0x27c6;case S::Compare:return e?0xa28:0x27cc;case S::SelectData:return e?0xa2a:0x27ce;case S::LocateData:return e?0xa2c:0x27d0;case S::Normalize:return e?0xa32:0x27d6;case S::ResolveData:return e?0xa34:0x27d8;case S::Increment:return e?0xa36:0x27da;case S::MaskIndex:return e?0xa38:0x27dc;case S::WriteConsumer:return e?0xa3c:0x27e0;case S::ReadData:return e?0xa42:0x27e6;case S::MarkData:return e?0xa44:0x27e8;case S::RestoreSecond:return e?0xa46:0x27ea;case S::RestoreFirst:return e?0xa4a:0x27ee;case S::Return:return e?0xa4e:0x27f2;case S::Complete:return 0;}return 0;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto p=task_.stage();const auto at=boundary();const auto len=length(p);c.fetchClocks(len);c.pc=c.makepc(int(at+1));const bool ext=task_.kind()==T::Kind::External;const uint32_t cons=ext?0xfffd2e:0xc9b8,prod=ext?0xfffd2c:0xc9b6;
 switch(p){
  case S::ClearResult:c.reg16(6)=int16_t(arithmetic(c,uint16_t(c.reg16(6)),uint16_t(c.reg16(6)),16,true));break;
  case S::SaveFirst:save(c,0);break;case S::SaveSecond:save(c,1);break;
  case S::LocateStatus:c.reg32(0)=0xfffe30;val(c,c.reg32(0));break;case S::SaveMask:c.reg8(9)=int8_t(c.ccr);break;case S::Mask:c.ccr|=0xc0;break;
  case S::ClearByte:c.reg8(14)=0;val(c,c.reg8(14));break;case S::RestoreMask:c.ccr=uint8_t(c.reg8(9));break;
  case S::Compare:arithmetic(c,uint16_t(c.reg16(0)),uint16_t(c.reg16(8)),16,true);break;
  case S::LocateData:c.reg32(1)=int32_t(ext?0xfffd30:0xc9ba);val(c,c.reg32(1));break;
  case S::Normalize:c.reg32(0)=uint16_t(c.reg16(0));val(c,c.reg32(0));break;
  case S::ResolveData:c.reg32(1)=int32_t(arithmetic(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(0)),32,false));break;
  case S::Increment:{const int old=c.reg16(0),v=old+1;val(c,v);c.ccr=uint8_t((c.ccr&~2)|((~old&v&0x8000)?2:0));c.reg16(0)=int16_t(v);break;}
  case S::MaskIndex:c.reg16(0)=int16_t(uint16_t(c.reg16(0))&255);val(c,c.reg16(0));break;
  case S::MarkData:c.reg8(6)=int8_t(uint8_t(c.reg8(6))|1);val(c,c.reg8(6));break;
  case S::RestoreSecond:restore(c,1);break;case S::RestoreFirst:restore(c,0);break;case S::Return:ret(c);break;default:break;
 }
 task_.advance([&](T::Read field)->uint32_t{switch(field){
  case T::Read::Status:c.reg8(6)=c.read8(int(uint32_t(c.reg32(0))));val(c,c.reg8(6));return uint8_t(c.reg8(6));
  case T::Read::Consumer:c.reg16(0)=int16_t(c.read16(int(cons)));val(c,c.reg16(0));return uint16_t(c.reg16(0));
  case T::Read::Producer:c.reg16(8)=int16_t(c.read16(int(prod)));val(c,c.reg16(8));return uint16_t(c.reg16(8));
  case T::Read::Data:c.reg8(14)=c.read8(int(uint32_t(c.reg32(1))));val(c,c.reg8(14));return uint8_t(c.reg8(14));}return 0;},[&](T::Write field,uint32_t v){if(field==T::Write::Status){val(c,c.reg8(14));c.write8(int8_t(v),int(uint32_t(c.reg32(0))));}else{c.write16(int16_t(v),int(cons));val(c,c.reg16(0));}});
 if(!task_.complete())c.pc=c.makepc(int(boundary()));return true;
 }
private:
 static unsigned length(S s){switch(s){case S::SaveFirst:case S::SaveSecond:case S::RestoreSecond:case S::RestoreFirst:case S::MaskIndex:return 4;case S::LocateStatus:case S::ReadConsumer:case S::ReadProducer:case S::LocateData:case S::WriteConsumer:return 6;default:return 2;}}
 T task_;
};

class NativeMidiPump final {
 using T=MidiPumpTask;using S=T::Stage;
public:
 const T&task()const{return task_;}
 uint32_t boundary()const{switch(task_.stage()){case S::PopExternal:return 0x252c;case S::CompareExternalData:return 0x2530;case S::SelectExternalData:return 0x2532;case S::ParseExternal:return 0x2534;case S::RepeatExternal:return 0x2538;case S::CompareExternalError:return 0x253a;case S::SelectExternalError:return 0x253c;case S::ResetExternal:return 0x253e;case S::PopInternal:return 0x2542;case S::CompareInternalData:return 0x2546;case S::SelectInternalData:return 0x2548;case S::ParseInternal:return 0x254a;case S::RepeatInternal:return 0x254e;case S::CompareInternalError:return 0x2550;case S::SelectInternalError:return 0x2552;case S::ResetInternal:return 0x2554;case S::Return:return 0x2558;default:return 0;}}
 bool resumeChild(Stage1Cpu&c){if(!task_.awaitingChild())return false;uint32_t ret=0;switch(task_.child()){case T::Child::PopExternal:ret=0x2530;break;case T::Child::ParseExternal:ret=0x2538;break;case T::Child::ResetExternal:ret=0x2542;break;case T::Child::PopInternal:ret=0x2546;break;case T::Child::ParseInternal:ret=0x254e;break;case T::Child::ResetInternal:ret=0x2558;break;}if(uint32_t(c.getPC())!=ret)return false;return task_.resume(task_.child(),uint16_t(c.reg16(6)));}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();auto len=length(p);c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){
 case S::PopExternal:call(c,0xa02,at+len,true);break;case S::ParseExternal:call(c,0x2954,at+len,true);break;case S::ResetExternal:call(c,0x9de,at+len,true);break;
 case S::PopInternal:call(c,0x27b6,at+len,true);break;case S::ParseInternal:call(c,0x2838,at+len,true);break;case S::ResetInternal:call(c,0x279c,at+len,true);break;
 case S::CompareExternalData:case S::CompareInternalData:arithmetic(c,uint8_t(c.reg8(6)),1,8,true);break;
 case S::CompareExternalError:case S::CompareInternalError:arithmetic(c,uint8_t(c.reg8(6)),0,8,true);break;
 case S::Return:ret(c);break;default:break;}
 task_.advance();if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static unsigned length(S s){switch(s){case S::PopExternal:case S::ParseExternal:case S::ResetExternal:case S::PopInternal:case S::ParseInternal:case S::ResetInternal:return 4;default:return 2;}}
 T task_;
};
class NativeMidiDescriptor final {
 using T=MidiDescriptorTask;using S=T::Stage;
public:
 explicit NativeMidiDescriptor(uint8_t input):task_(input){}const T&task()const{return task_;}
 uint32_t boundary()const{switch(task_.stage()){
 case S::Compare:return 0x2a56;case S::Select:return 0x2a58;case S::MaskChannel:return 0x2a5a;case S::Shift1:return 0x2a5c;case S::Shift2:return 0x2a5e;case S::Shift3:return 0x2a60;case S::Shift4:return 0x2a62;case S::WidthChannel:return 0x2a64;case S::MultiplyChannel:return 0x2a66;case S::ExtendChannel:return 0x2a68;case S::AddChannel:return 0x2a6a;case S::ReturnChannel:return 0x2a70;case S::MaskSystem:return 0x2a72;case S::WidthSystem:return 0x2a74;case S::MultiplySystem:return 0x2a76;case S::ExtendSystem:return 0x2a78;case S::AddSystem:return 0x2a7a;case S::ReturnSystem:return 0x2a80;case S::Complete:return 0;}return 0;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto p=task_.stage();const auto at=boundary();const unsigned len=(p==S::AddChannel||p==S::AddSystem)?6:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){
 case S::Compare:arithmetic(c,uint8_t(c.reg8(8)),0xf0,8,true);break;
 case S::MaskChannel:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&0x70);val(c,c.reg8(8));break;
 case S::MaskSystem:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&15);val(c,c.reg8(8));break;
 case S::Shift1:case S::Shift2:case S::Shift3:case S::Shift4:{auto v=uint8_t(c.reg8(8));c.ccr=uint8_t((c.ccr&~1)|(v&1));v>>=1;c.reg8(8)=int8_t(v);val(c,c.reg8(8));break;}
 case S::WidthChannel:case S::WidthSystem:c.reg8(0)=6;val(c,c.reg8(0));break;
 case S::MultiplyChannel:case S::MultiplySystem:c.reg16(0)=int16_t(uint8_t(c.reg8(0))*uint8_t(c.reg8(8)));c.clockI(12);break;
 case S::ExtendChannel:case S::ExtendSystem:c.reg32(0)=uint16_t(c.reg16(0));val(c,c.reg32(0));break;
 case S::AddChannel:case S::AddSystem:c.reg32(0)=int32_t(arithmetic(c,uint32_t(c.reg32(0)),p==S::AddChannel?0xab36:0xab60,32,false));break;
 case S::ReturnChannel:case S::ReturnSystem:ret(c);break;default:break;}
 task_.advance();if(!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};

class NativeMidiParser final {
 using T=MidiParserTask;using S=T::Stage;
public:
 NativeMidiParser(T::Kind k,uint8_t input):task_(k,input){}const T&task()const{return task_;}
 uint32_t boundary()const{const bool e=task_.kind()==T::Kind::External;switch(task_.stage()){
 case S::CopyInput:return e?0x2954:0x2838;case S::SelectStatus:return e?0x2956:0x283a;case S::WriteReceived:return e?0x2958:0x283c;case S::ClearActivity:return 0x295e;case S::WriteActivity:return 0x2960;case S::RequestDescriptor:return e?0x2966:0x2842;case S::ReadLength:return e?0x296a:0x2846;case S::WriteExpected:return e?0x2972:0x284e;case S::ReadFlags:return e?0x2978:0x2854;case S::MaskImmediate:return e?0x2980:0x285c;case S::SelectImmediate:return e?0x2982:0x285e;case S::WriteRemaining:return e?0x2984:0x2862;case S::SelectLength:return e?0x298a:0x2868;case S::WriteRunning:return e?0x298c:0x286c;case S::MaskChannel:return e?0x2992:0x2872;case S::WriteChannel:return e?0x2994:0x2874;case S::BindCursor:return e?0x299a:0x287a;case S::WriteCursor:return e?0x29a0:0x2880;case S::ReturnStatus:return e?0x29a8:0x2888;
 case S::ReadRemaining:return e?0x29b8:0x288a;case S::SelectRemaining:return e?0x29be:0x2890;case S::ReadCursor:return e?0x29c0:0x2892;case S::WriteData:return e?0x29c8:0x289a;case S::DecrementRemaining:return e?0x29ca:0x289c;case S::WriteDataRemaining:return e?0x29cc:0x289e;case S::SelectComplete:return e?0x29d2:0x28a4;case S::IncrementCursor:return e?0x29d4:0x28a6;case S::WriteDataCursor:return e?0x29d6:0x28a8;case S::ReturnData:return e?0x29de:0x28b0;
 case S::ReadRunning:return e?0x29e0:0x28b2;case S::RequestRunningDescriptor:return e?0x29e6:0x28b8;case S::ReadRunningFlags:return e?0x29e8:0x28bc;case S::MaskRunning:return e?0x29f0:0x28c4;case S::SelectRunning:return e?0x29f2:0x28c6;case S::ReadRunningLength:return e?0x29f4:0x28c8;case S::WriteRunningRemaining:return e?0x29fc:0x28d0;case S::BindRunningCursor:return e?0x2a02:0x28d6;case S::WriteRunningCursor:return e?0x2a08:0x28dc;
 case S::ReadHandler:return task_.immediate()?0x29aa:(e?0x2a10:0x28e4);case S::BindPayload:return e?0x2a1a:0x28ee;case S::ReadChannel:return e?0x2a20:0x28f4;case S::WriteDispatchChannel:return e?0x2a26:0x28fa;case S::RequestHandler:return task_.immediate()?0x29b4:(e?0x2a2c:0x2900);
 case S::PostReadStatus:return 0x2a2e;case S::PostCompareStatus:return 0x2a34;case S::PostSelectStatus:return 0x2a36;case S::PostBindProgram:return 0x2a38;case S::PostReadProgramChannel:return 0x2a3e;case S::PostReadChannel:return 0x2a46;case S::PostCompareChannel:return 0x2a4c;case S::PostSelectChannel:return 0x2a4e;case S::PostActivity:return 0x2a50;case S::Return:return task_.immediate()?0x29b6:(e?0x2a54:0x2902);default:return 0;}}
 bool resumeChild(Stage1Cpu&c){if(!task_.awaitingChild())return false;const bool e=task_.kind()==T::Kind::External;switch(task_.child()){
 case T::Child::Descriptor:{const uint32_t ret=task_.descriptorForRunning()?(e?0x29e8:0x28bc):(e?0x296a:0x2846);if(uint32_t(c.getPC())!=ret)return false;return task_.resumeDescriptor(uint32_t(c.reg32(0)));}
 case T::Child::Handler:{const uint32_t ret=task_.immediate()?0x29b6:(e?0x2a2e:0x2902);if(uint32_t(c.getPC())!=ret)return false;return task_.resumeChild(T::Child::Handler);}
 case T::Child::Activity:if(uint32_t(c.getPC())!=0x2a54)return false;return task_.resumeChild(T::Child::Activity);
 }return false;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto p=task_.stage();const auto at=boundary();const bool e=task_.kind()==T::Kind::External;const auto len=length(p,e);c.fetchClocks(len);c.pc=c.makepc(int(at+1));const uint32_t received=e?0xcad0:0xcac2,expected=e?0xcad2:0xcac4,remaining=e?0xcad1:0xcac3,running=e?0xcacf:0xcac1,channel=e?0xcace:0xcac0,cursor=e?0xcaca:0xcabc,payload=e?0xcad3:0xcac5;
 switch(p){
 case S::CopyInput:c.reg8(8)=c.reg8(14);val(c,c.reg8(8));break;
 case S::SelectImmediate:case S::SelectLength:if(!e)c.clockI(2);break;
 case S::ClearActivity:c.reg8(0)=0;val(c,c.reg8(0));break;
 case S::RequestDescriptor:case S::RequestRunningDescriptor:call(c,0x2a56,at+len,len==4);break;
 case S::MaskImmediate:c.reg8(1)=int8_t(uint8_t(c.reg8(1))&2);val(c,c.reg8(1));break;
 case S::MaskChannel:c.reg8(14)=int8_t(uint8_t(c.reg8(14))&15);val(c,c.reg8(14));break;
 case S::BindCursor:c.reg32(0)=int32_t(payload);val(c,c.reg32(0));break;
 case S::DecrementRemaining:{int old=c.reg8(8),v=old-1;val(c,v);c.ccr=uint8_t((c.ccr&~2)|((old&~v&0x80)?2:0));c.reg8(8)=int8_t(v);break;}
 case S::IncrementCursor:{uint32_t old=uint32_t(c.reg32(1)),v=old+1;val(c,int32_t(v));c.ccr=uint8_t((c.ccr&~2)|((~old&v&0x80000000u)?2:0));c.reg32(1)=int32_t(v);break;}
 case S::MaskRunning:c.reg8(9)=int8_t(uint8_t(c.reg8(9))&1);val(c,c.reg8(9));break;
 case S::BindRunningCursor:c.reg32(1)=int32_t(payload);val(c,c.reg32(1));break;
 case S::BindPayload:c.reg32(4)=int32_t(payload);val(c,c.reg32(4));break;
 case S::RequestHandler:call(c,uint32_t(c.reg32(0))&0xffffff,at+len,false);break;
 case S::PostCompareStatus:arithmetic(c,uint8_t(c.reg8(8)),0xf0,8,true);break;
 case S::PostBindProgram:c.reg32(0)=0xba5c;val(c,c.reg32(0));break;
 case S::PostCompareChannel:arithmetic(c,uint8_t(c.reg8(8)),uint8_t(c.reg8(0)),8,true);break;
 case S::PostActivity:call(c,0x20ba,at+len,true);break;
 case S::ReturnStatus:case S::ReturnData:case S::Return:ret(c);break;default:break;}
 task_.advance([&](T::Read field)->uint32_t{switch(field){
 case T::Read::Length:c.reg8(9)=c.read8(int(uint32_t(c.reg32(0))+4));val(c,c.reg8(9));return uint8_t(c.reg8(9));
 case T::Read::Flags:if(p==S::ReadFlags){c.reg8(1)=c.read8(int(uint32_t(c.reg32(0))+5));val(c,c.reg8(1));return uint8_t(c.reg8(1));}c.reg8(9)=c.read8(int(uint32_t(c.reg32(0))+5));val(c,c.reg8(9));return uint8_t(c.reg8(9));
 case T::Read::Remaining:c.reg8(8)=c.read8(int(remaining));val(c,c.reg8(8));return uint8_t(c.reg8(8));
 case T::Read::Cursor:c.reg32(1)=int32_t(c.read32(int(cursor)));val(c,c.reg32(1));return uint32_t(c.reg32(1));
 case T::Read::Running:c.reg8(8)=c.read8(int(running));val(c,c.reg8(8));return uint8_t(c.reg8(8));
 case T::Read::Handler:c.reg32(0)=int32_t(c.read32(int(uint32_t(c.reg32(0)))));val(c,c.reg32(0));return uint32_t(c.reg32(0));
 case T::Read::Channel:if(p==S::PostReadChannel){c.reg8(0)=c.read8(int(channel));val(c,c.reg8(0));return uint8_t(c.reg8(0));}c.reg8(9)=c.read8(int(channel));val(c,c.reg8(9));return uint8_t(c.reg8(9));
 case T::Read::ProgramChannel:c.reg8(8)=c.read8(0xbb16);val(c,c.reg8(8));return uint8_t(c.reg8(8));}return 0;},[&](T::Write field,uint32_t v){uint32_t addr=0;int reg=0;bool wide=false;switch(field){case T::Write::Received:addr=received;reg=14;break;case T::Write::Activity:addr=0xcd0e;reg=0;break;case T::Write::Expected:addr=expected;reg=9;break;case T::Write::Remaining:addr=remaining;reg=(p==S::WriteRemaining||p==S::WriteRunningRemaining)?9:8;break;case T::Write::Running:addr=running;reg=14;break;case T::Write::Channel:addr=channel;reg=14;break;case T::Write::Cursor:addr=cursor;reg=p==S::WriteCursor?0:1;wide=true;break;case T::Write::Data:addr=uint32_t(c.reg32(1));reg=14;break;case T::Write::DispatchChannel:addr=0xcad5;reg=9;break;}
 if(wide){c.write32(int32_t(v),int(addr));val(c,c.reg32(reg));}else if(field==T::Write::Data){c.write8(int8_t(v),int(addr));val(c,c.reg8(reg));}else{val(c,c.reg8(reg));c.write8(int8_t(v),int(addr));}});
 if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;
 }
private:
 static unsigned length(S s,bool e){if(s==S::RequestRunningDescriptor)return e?2:4;if(s==S::SelectImmediate||s==S::SelectLength)return e?2:4;switch(s){case S::ReadHandler:return 10;case S::ReadLength:case S::ReadFlags:case S::ReadCursor:case S::WriteCursor:case S::WriteDataCursor:case S::ReadRunningFlags:case S::ReadRunningLength:case S::WriteRunningCursor:case S::PostReadProgramChannel:return 8;case S::WriteReceived:case S::WriteActivity:case S::WriteExpected:case S::WriteRemaining:case S::WriteRunning:case S::WriteChannel:case S::BindCursor:case S::ReadRemaining:case S::WriteDataRemaining:case S::ReadRunning:case S::WriteRunningRemaining:case S::BindRunningCursor:case S::BindPayload:case S::ReadChannel:case S::WriteDispatchChannel:case S::PostReadStatus:case S::PostBindProgram:case S::PostReadChannel:return 6;case S::RequestDescriptor:case S::PostActivity:return 4;default:return 2;}}
 T task_;
};

class NativeMidiUtility final {
 using T=MidiUtilityTask;using S=T::Stage;
public:
 explicit NativeMidiUtility(T::Kind k):task_(k){}const T&task()const{return task_;}
 uint32_t boundary()const{const bool e=task_.kind()==T::Kind::ResetExternal,a=task_.kind()==T::Kind::Activity;switch(task_.stage()){
 case S::SaveMask:return 0x9de;case S::Mask:return 0x9e2;case S::SaveWord:return e?0x9e4:a?0x20ba:0x279c;case S::Clear:return e?0x9e6:0x279e;case S::WriteProducer:return e?0x9e8:0x27a0;case S::WriteConsumer:return e?0x9ee:0x27a6;case S::WriteStatus:return e?0x9f4:0x27ac;case S::RestoreWord:return e?0x9fa:a?0x20ca:0x27b2;case S::RestoreMask:return 0x9fc;
 case S::ReadPort:return 0x20bc;case S::TogglePort:return 0x20be;case S::WritePort:return 0x20c0;case S::PrepareHold:return 0x20c2;case S::WriteHold:return 0x20c4;case S::Return:return e?0xa00:a?0x20cc:0x27b4;case S::Complete:return 0;}return 0;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto p=task_.stage();const auto at=boundary();const auto len=length(p);c.fetchClocks(len);c.pc=c.makepc(int(at+1));const bool ext=task_.kind()==T::Kind::ResetExternal,activity=task_.kind()==T::Kind::Activity;const int reg=activity?6:0;
 switch(p){
 case S::SaveMask:c.setSP(c.getSP()-2);c.write16(c.ccr,int(c.getSP()));c.clockI(2);break;case S::Mask:c.ccr|=0xc0;break;
 case S::SaveWord:c.setSP(c.getSP()-2);c.write16(c.reg16(reg),int(c.getSP()));c.clockI(2);val(c,c.reg16(reg));break;
 case S::Clear:c.reg16(0)=int16_t(arithmetic(c,uint16_t(c.reg16(0)),uint16_t(c.reg16(0)),16,true));break;
 case S::RestoreWord:c.reg16(reg)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(reg));break;
 case S::RestoreMask:c.ccr=uint8_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);break;
 case S::TogglePort:c.reg8(6)=int8_t(uint8_t(c.reg8(6))^0x10);break;case S::PrepareHold:c.reg8(14)=5;val(c,c.reg8(14));break;case S::Return:ret(c);break;default:break;}
 task_.advance([&](T::Read)->uint32_t{c.reg8(6)=c.read8(0xffffd2);val(c,c.reg8(6));return uint8_t(c.reg8(6));},[&](T::Write field,uint32_t v){uint32_t addr=0;switch(field){case T::Write::Producer:addr=ext?0xfffd2c:0xc9b6;break;case T::Write::Consumer:addr=ext?0xfffd2e:0xc9b8;break;case T::Write::Status:addr=ext?0xfffe30:0xcaba;break;case T::Write::Port:addr=0xffffd2;break;case T::Write::Hold:addr=0xfffe8f;break;}if(field==T::Write::Producer||field==T::Write::Consumer){c.write16(int16_t(v),int(addr));val(c,c.reg16(0));}else {val(c,c.reg8(field==T::Write::Port?6:activity?14:8));c.write8(int8_t(v),int(addr));}});
 if(!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:static unsigned length(S s){switch(s){case S::SaveMask:case S::RestoreMask:return 4;case S::WriteProducer:case S::WriteConsumer:case S::WriteStatus:case S::WriteHold:return 6;default:return 2;}}T task_;
};
// Only these byte ranges are owned by the pump migration component; callers
// must validate the exact fixed image before allowing architectural projection.
struct NativeMidiPumpImageRanges {
 struct Range{uint32_t begin,end;};
 inline static constexpr std::array<Range,6> ranges{{{0x9de,0xa50},{0x20ba,0x20ce},{0x252c,0x255a},{0x279c,0x27f4},{0x2838,0x2904},{0x2954,0x2a82}}};
};

}
