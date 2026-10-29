#pragma once
#include "kstationVoiceEffectsTask.h"
#include "kstationNativeVoiceUpdate.h"
#include <optional>
namespace kstation {
class NativeVoiceEffectSource final {
 using T=VoiceEffectSourceTask;using S=T::Stage;
public:
 NativeVoiceEffectSource(uint32_t patch,unsigned lane):task_(patch,lane){}const T&task()const{return task_;}bool complete()const{return task_.complete();}bool resumeChild(Stage1Cpu&c){if(task_.child()==T::Child::None||uint32_t(c.getPC())!=expectedReturn_)return false;return task_.resumeChild(task_.child()==T::Child::Float?uint32_t(c.reg32(0)):0);}
 uint32_t boundary()const{constexpr uint32_t off[]={0,8,10,12,16,20,22,24,26,28,30,36,40,44,48,50,58,60,62,64,68,70,72,76,80,82,84,86,88,90,92,94,96,98,100,102,104,106,110,114,116,120};auto base=task_.lane()?0x8434:0x83b8;if(complete())return base+124;if(task_.child()!=T::Child::None)return expectedReturn_;return base+off[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||task_.child()!=T::Child::None||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();bool rd=s==S::ReadMode||s==S::ReadFloat||s==S::ReadTable,push=s==S::PushFloat,pop=s==S::PopScale,dispatch=s==S::DispatchTableFirst||s==S::DispatchTableSecond||s==S::DispatchFloat||s==S::DispatchFloatFirst||s==S::DispatchFloatSecond;unsigned len=(s==S::ReadMode||s==S::ReadFloat)?8:(s==S::BiasPointer)?6:(s==S::ReadTable||s==S::SetFloatScale||s==S::BiasFloat||s==S::SetAddressFirst||s==S::SetAddressSecond||s==S::SetAddressFloat||s==S::SetAddressZero||dispatch)?4:2;auto addr=s==S::ReadMode?task_.patch()+0x49+3*task_.lane():s==S::ReadFloat?task_.patch()+0x48+3*task_.lane():s==S::ReadTable?task_.tableAddress():push?c.getSP()-2:c.getSP();unsigned size=s==S::ReadTable?4:(push||pop)?2:1;T::Credits cr{(rd||push||pop)?bus(addr,size,(push?c.writeHistory():int(at+len-1))):0,dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):0};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::SetAddressFirst:case S::SetAddressSecond:case S::SetAddressFloat:case S::SetAddressZero:c.reg16(4)=int16_t((task_.lane()?0x62:0x5d)+((s==S::SetAddressSecond||s==S::SetAddressZero)?1:0));val(c,c.reg16(4));break;
 case S::ClearTableFirst:case S::ClearFloatSecond:c.reg32(1)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;
 case S::Decrement:{auto x=int(c.reg8(10));auto y=x-1;val(c,y);c.ccr=uint8_t((c.ccr&~2)|((x==-128)?2:0));c.reg8(10)=int8_t(y);break;}
 case S::ShiftTableFirst:case S::ShiftTableSecond:shiftLeft(c,c.reg8(10),8);break;
 case S::ExtendWord:c.reg16(2)=uint8_t(c.reg8(10));val(c,c.reg16(2));break;case S::ExtendLong:c.reg32(2)=uint16_t(c.reg16(2));val(c,c.reg32(2));break;
 case S::BiasPointer:c.reg32(2)=int32_t(arith(c,32,uint32_t(c.reg32(2)),0xc59a,false));break;
 case S::ClearFloatLow:c.reg8(8)=0;val(c,c.reg8(8));break;
 case S::PushFloat:c.setSP(c.getSP()-2);c.write16(c.reg16(0),int(c.getSP()));c.clockI(2);val(c,c.reg16(0));break;
 case S::ShiftFloat:shiftRight(c,c.reg16(0),16);break;
 case S::SetFloatScale:c.reg16(8)=int16_t(0xaaab);val(c,c.reg16(8));break;
 case S::MultiplyFloat:c.reg32(0)=int32_t(uint32_t(uint16_t(c.reg16(8)))*uint16_t(c.reg16(0)));c.clockI(20);break;
 case S::RetainFloat:c.reg16(0)=c.reg16(8);val(c,c.reg16(0));break;
 case S::BiasFloat:c.reg16(0)=int16_t(arith(c,16,uint16_t(c.reg16(0)),0x4700,false));break;
 case S::PopScale:c.reg16(2)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(2));break;
 case S::CompareScale:arith(c,8,uint8_t(c.reg8(2)),0x20,true);break;
 case S::ClampScale:c.reg8(2)=0x20;val(c,c.reg8(2));break;
 case S::ShiftScaleFirst:case S::ShiftScaleSecond:shiftLeft(c,c.reg8(2),8);break;
 case S::CopyFloatHigh:c.reg16(1)=c.reg16(8);val(c,c.reg16(1));break;
 case S::MultiplyHigh:c.reg32(1)=int32_t(uint32_t(uint16_t(c.reg16(1)))*uint16_t(c.reg16(2)));c.clockI(20);break;
 case S::MultiplyLow:c.reg32(0)=int32_t(uint32_t(uint16_t(c.reg16(0)))*uint16_t(c.reg16(2)));c.clockI(20);break;
 case S::RetainLow:c.reg16(0)=c.reg16(8);val(c,c.reg16(0));break;
 case S::ClearLowHigh:c.reg16(8)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;
 case S::AddProducts:c.reg32(1)=int32_t(arith(c,32,uint32_t(c.reg32(1)),uint32_t(c.reg32(0)),false));break;
 case S::ShiftProduct:shiftLeft(c,c.reg32(1),32);break;
 case S::DispatchTableFirst:case S::DispatchTableSecond:case S::DispatchFloat:case S::DispatchFloatFirst:case S::DispatchFloatSecond:if(s==S::DispatchFloat){if(uint16_t(c.reg16(0))!=task_.input())throw std::logic_error("K effect source float projection");}else if(((uint32_t(uint16_t(c.reg16(9)))<<8)|uint8_t(c.reg8(1)))!=task_.word())throw std::logic_error("K effect source word projection");expectedReturn_=at+len;call(c,s==S::DispatchFloat?0x5c04:0x5bec,expectedReturn_);break;default:break;}
 auto used=task_.advanceBefore(cost,cr,[&](T::Read what)->uint32_t{if(what==T::Read::Stack)return uint16_t(c.reg16(2));if(what==T::Read::Table){c.reg32(1)=int32_t(c.read32(int(addr)));val(c,c.reg32(1));return uint32_t(c.reg32(1));}int r=what==T::Read::Mode?10:0;c.reg8(r)=c.read8(int(addr));val(c,c.reg8(r));return uint8_t(c.reg8(r));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K effect source phase clocks differ");if(task_.child()==T::Child::None)c.pc=c.makepc(int(boundary()));return true;}
private:
 static uint32_t arith(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=sign>>4;const auto r=(sub?a-b:a+b)&mask,carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r)),ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 template<class R>static void shiftLeft(Stage1Cpu&c,R&v,unsigned bits){uint32_t x=uint32_t(v);c.ccr=uint8_t((c.ccr&~1)|((x&(1u<<(bits-1)))?1:0));x<<=1;v=R(x);voice_update_projection::val(c,v);}
 template<class R>static void shiftRight(Stage1Cpu&c,R&v,unsigned bits){uint32_t x=uint32_t(v)&((1u<<bits)-1);c.ccr=uint8_t((c.ccr&~1)|(x&1));x>>=1;v=R(x);voice_update_projection::val(c,v);}
 T task_;uint32_t expectedReturn_=0;
};


class NativeVoiceEffects final {
 using T=VoiceEffectsTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x83a2,0x84b2}}};static bool candidate(uint32_t pc){return pc>=0x83a2&&pc<0x84b2;}bool active()const{return active_;}void reset(){task_=T{};source_.reset();active_=false;lane_=0;expectedReturn_=0;}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c))return false;auto pc=uint32_t(c.getPC());if(!active_){if(pc!=0x83a2){if(candidate(pc))throw std::logic_error("Unowned K voice effects boundary");return false;}if(task_.complete())reset();active_=true;}
 if(task_.awaitingSelect()){if(pc!=expectedReturn_)return false;task_.resumeSelect();}
 if(task_.stage()==S::Sources){if(!source_)source_.emplace(0xba5c,lane_);if(source_->task().child()!=VoiceEffectSourceTask::Child::None){if(pc!=source_->boundary())return false;if(!source_->resumeChild(c))return false;}if(source_->complete()){source_.reset();if(++lane_==2){task_.sourcesDone();}else source_.emplace(0xba5c,lane_);}if(task_.stage()==S::Sources)return source_->step(c);}
 constexpr uint32_t p[]={0x83a2,0x83a8,0x83ae,0x83b2,0,0,0x84b0};auto st=task_.stage();auto at=p[unsigned(st)];if(pc!=at){if(candidate(pc))throw std::logic_error("Unowned K voice effects phase");return false;}if(c.pending_irqs)c.read8(0xfffff2);bool rd=st==S::ReadSlot,wr=st==S::WriteSlot,dispatch=st==S::DispatchSelect,retiring=st==S::Return;unsigned len=(rd||wr||st==S::SetPatch)?6:dispatch?4:2;auto addr=rd?0xd0f0:0xd023;T::Credits cr{(rd||wr)?bus(addr,1,wr?c.writeHistory():int(at+len-1)):0,dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):retiring?5+bus(c.getSP(),4,int(at+len-1)):0};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));if(dispatch){expectedReturn_=at+len;call(c,0x5bb8,expectedReturn_);}else if(st==S::SetPatch){c.reg32(6)=0xba5c;val(c,c.reg32(6));}else if(retiring)ret(c);auto used=task_.advanceBefore(cost,cr,[&](){c.reg8(8)=c.read8(int(addr));val(c,c.reg8(8));return uint8_t(c.reg8(8));},[&](uint8_t){val(c,c.reg8(8));c.write8(c.reg8(8),int(addr));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice effects outer clocks differ");if(task_.complete()){active_=false;return true;}if(!task_.awaitingSelect())c.pc=c.makepc(int(at+len));return true;}
private:T task_{};std::optional<NativeVoiceEffectSource> source_{};bool active_=false;unsigned lane_=0;uint32_t expectedReturn_=0;
};

}
