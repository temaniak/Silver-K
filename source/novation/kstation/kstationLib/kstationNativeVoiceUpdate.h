#pragma once
#include "kstationVoiceUpdateTask.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>
#include <variant>
#include <type_traits>
namespace kstation {
namespace voice_update_projection {
inline void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
inline uint16_t add(Stage1Cpu&c,uint16_t a,uint16_t b){const uint32_t r=uint16_t(a+b),carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x8000)?8:0)|((carry&0x8000)?1:0)|((carry&0x800)?32:0)|((over&0x8000)?2:0));return uint16_t(r);}
inline void clearLong(Stage1Cpu&c,int r){c.reg32(r)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);}
inline unsigned bus(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
inline bool blocked(const Stage1Cpu&c){return c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80));}
inline void call(Stage1Cpu&c,uint32_t target,uint32_t ret){auto sp=c.getSP()-4;c.write32(int32_t(ret),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);c.clockI(2);c.pc=c.makepc(int(target));}
inline void ret(Stage1Cpu&c){c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}
}
class NativeVoiceDspWrite final {
 using T=VoiceDspWriteTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5bec,0x5c04}}};
 explicit NativeVoiceDspWrite(uint16_t command):task_(command){}const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}bool resumeDsp(Stage1Cpu&c){return uint32_t(c.getPC())==0x5c02&&task_.resumeDsp();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x5bec,0x5bf2,0x5bf4,0x5bf6,0x5bfc,0x5bfe,0,0x5c02};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||awaitingDsp()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();auto len=(s==S::ReadFirstBase||s==S::ReadSecondBase)?6:s==S::DispatchWord?4:2;const bool dispatch=s==S::DispatchWord;const auto readAt=s==S::ReadSecondBase?0xd026:0xd024;T::Credits cr{bus(readAt,2,int(at+len-1)),dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::TestSign:val(c,c.reg16(4));break;case S::AddBase:c.reg16(4)=int16_t(add(c,uint16_t(c.reg16(4)),uint16_t(c.reg16(0))));break;case S::DispatchWord:call(c,0x165e,at+len);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr,[&](bool second){c.reg16(0)=int16_t(c.read16(second?0xd026:0xd024));val(c,c.reg16(0));return uint16_t(c.reg16(0));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice DSP write clocks differ");if(!awaitingDsp()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
class NativeVoiceDspAddress final {
 using T=VoiceDspWriteTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5ba0,0x5bb8}}};
 explicit NativeVoiceDspAddress(uint16_t command):task_(command){}const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}bool resumeDsp(Stage1Cpu&c){return uint32_t(c.getPC())==0x5bb6&&task_.resumeDsp();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x5ba0,0x5ba6,0x5ba8,0x5baa,0x5bb0,0x5bb2,0,0x5bb6};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||awaitingDsp()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();auto len=(s==S::ReadFirstBase||s==S::ReadSecondBase)?6:s==S::DispatchWord?4:2;const bool dispatch=s==S::DispatchWord;const auto readAt=s==S::ReadSecondBase?0xd026:0xd024;T::Credits cr{bus(readAt,2,int(at+len-1)),dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::TestSign:val(c,c.reg16(4));break;case S::AddBase:c.reg16(4)=int16_t(add(c,uint16_t(c.reg16(4)),uint16_t(c.reg16(0))));break;case S::DispatchWord:call(c,0x1652,at+len);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr,[&](bool second){c.reg16(0)=int16_t(c.read16(second?0xd026:0xd024));val(c,c.reg16(0));return uint16_t(c.reg16(0));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice DSP write clocks differ");if(!awaitingDsp()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
class NativeVoiceGate final {
 using T=VoiceGateTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x6f44,0x6f50}}};
 const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingWrite()const{return task_.awaitingWrite();}bool resumeWrite(Stage1Cpu&c){return uint32_t(c.getPC())==0x6f4e&&task_.resumeWrite();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x6f44,0x6f46,0x6f4a,0,0x6f4e};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||awaitingWrite()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();auto len=(s==S::SetGate||s==S::DispatchWrite)?4:2;T::Credits cr{s==S::DispatchWrite?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::ClearWord:clearLong(c,1);break;case S::SetGate:c.reg16(4)=int16_t(0x8052);val(c,c.reg16(4));break;case S::DispatchWrite:call(c,0x5bec,at+len);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr);if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice gate clocks differ");if(!awaitingWrite()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_{};
};
class NativeVoiceExpressionValues final {
 using T=VoiceExpressionValuesTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5e64,0x5ea8}}};
 const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}bool resumeDsp(Stage1Cpu&c){return uint32_t(c.getPC())==expectedReturn_&&task_.resumeDsp();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x5e64,0x5e6a,0x5e6c,0x5e6e,0x5e70,0x5e72,0x5e76,0x5e7a,0x5e80,0x5e82,0x5e84,0x5e86,0x5e88,0x5e8c,0x5e90,0x5e96,0x5e98,0x5e9a,0x5e9c,0x5e9e,0x5ea2,0x5ea6};return complete()?0:awaitingDsp()?expectedReturn_:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||awaitingDsp()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();const bool rd=s==S::ReadFirst||s==S::ReadSecond||s==S::ReadThird,callPhase=s==S::DispatchFirst||s==S::DispatchSecond||s==S::DispatchThird;const unsigned len=rd?6:(callPhase||s==S::BindFirst||s==S::BindSecond||s==S::BindThird)?4:2;const unsigned addr=s==S::ReadFirst?0xbfc0:s==S::ReadSecond?0xbfc1:0xbfc3;T::Credits cr{bus(addr,1,int(at+len-1)),callPhase?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::CopyFirst:case S::CopySecond:case S::CopyThird:c.reg8(9)=c.reg8(1);val(c,c.reg8(9));break;case S::ShiftFirst:case S::ShiftSecond:case S::ShiftThird:{unsigned v=uint8_t(c.reg8(9));c.ccr=uint8_t((c.ccr&~1)|((v&128)?1:0));v<<=1;val(c,int32_t(v<<24));c.reg8(9)=int8_t(v);break;}case S::RetainFirst:case S::RetainSecond:case S::RetainThird:c.reg16(9)=c.reg16(1);val(c,c.reg16(9));break;case S::ClearFirst:case S::ClearSecond:case S::ClearThird:c.reg8(1)=0;c.ccr=uint8_t((c.ccr&~0x2f)|4);break;case S::BindFirst:case S::BindSecond:case S::BindThird:c.reg16(4)=s==S::BindFirst?0x14:s==S::BindSecond?0x15:0x16;val(c,c.reg16(4));break;case S::DispatchFirst:case S::DispatchSecond:case S::DispatchThird:expectedReturn_=at+len;call(c,0x165e,expectedReturn_);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr,[&](unsigned offset){c.reg8(1)=c.read8(0xbfc0+int(offset));val(c,c.reg8(1));return uint8_t(c.reg8(1));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K expression values clocks differ");if(!awaitingDsp()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_{};uint32_t expectedReturn_=0;
};

// Refresh walker. Address/copy/DSP-select and modulation are explicit child
// requests; 5c3a and 5ea8 are not yet closed by NativeVoiceUpdate.
class NativeVoiceRefresh final {
 using T=VoiceRefreshTask;using S=T::Stage;
public:
 explicit NativeVoiceRefresh(T::Kind kind):task_(kind){}const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{const bool p=task_.kind()==T::Kind::Pitch;switch(task_.stage()){
 case S::ClearIndex:return 0x785c;case S::WriteIndex:return 0x785e;case S::ReadPartCopy:return p?0x78c2:0x7864;case S::DispatchCopy:return p?0x78c8:0x786a;case S::ReadFlag:return 0x78cc;case S::SelectFlag:return 0x78d2;case S::ClearLoopIndex:return 0x78d4;case S::WriteLoopIndex:return 0x78d6;case S::ReadCount:return p?0x78dc:0x786e;case S::ReadOwner:return p?0x78e2:0x7874;case S::RetainCount:return p?0x78e8:0x787a;case S::RetainOwner:return p?0x78ea:0x787c;case S::DispatchVoice:return p?0x78ec:0x787e;case S::ReadAge:return p?0x78f0:0x7882;case S::SelectAge:return p?0x78f8:0x788a;case S::ReadVoiceOwner:return p?0x78fa:0x788c;case S::CompareOwner:return p?0x7902:0x7894;case S::SelectOwner:return p?0x7904:0x7896;case S::ReadPatch:return p?0x7906:0x7898;case S::DispatchDspSelect:return p?0x7910:0x78a2;case S::DispatchUpdate:return p?0x7914:0x78a6;case S::ReadIndex:return p?0x7918:0x78aa;case S::IncrementIndex:return p?0x791e:0x78b0;case S::WriteIndexNext:return p?0x7920:0x78b2;case S::RestoreOwner:return p?0x7926:0x78b8;case S::RestoreCount:return p?0x7928:0x78ba;case S::DecrementCount:return p?0x792a:0x78bc;case S::Repeat:return p?0x792c:0x78be;case S::Return:return p?0x792e:0x78c0;case S::AwaitChild:return expectedReturn_;case S::Complete:return 0;}return 0;}
 bool resumeChild(Stage1Cpu&c){if(task_.child()==T::Child::None||uint32_t(c.getPC())!=expectedReturn_)return false;return task_.resumeChild(task_.child()==T::Child::Voice?uint32_t(c.reg32(3)):0);}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||task_.child()!=T::Child::None||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();bool callPhase=s==S::DispatchCopy||s==S::DispatchVoice||s==S::DispatchDspSelect||s==S::DispatchUpdate;bool save=s==S::RetainCount||s==S::RetainOwner,restore=s==S::RestoreOwner||s==S::RestoreCount;unsigned len=callPhase?4:s==S::ReadPatch?10:(s==S::ReadAge||s==S::ReadVoiceOwner)?8:(s==S::ReadPartCopy||s==S::ReadFlag||s==S::ReadCount||s==S::ReadOwner||s==S::ReadIndex||s==S::WriteIndex||s==S::WriteLoopIndex||s==S::WriteIndexNext)?6:2;uint32_t addr=0;unsigned size=0;switch(s){case S::ReadPartCopy:case S::ReadOwner:addr=0xd028;size=1;break;case S::ReadFlag:addr=0xbfc8;size=1;break;case S::ReadCount:addr=0xd022;size=1;break;case S::ReadIndex:case S::WriteIndex:case S::WriteLoopIndex:case S::WriteIndexNext:addr=0xd023;size=1;break;case S::ReadAge:addr=task_.voice()+8;size=2;break;case S::ReadVoiceOwner:addr=task_.voice()+4;size=1;break;case S::ReadPatch:addr=task_.voice();size=4;break;default:break;}bool wr=s==S::WriteIndex||s==S::WriteLoopIndex||s==S::WriteIndexNext;unsigned rc=size&&!wr?bus(addr,size,int(at+len-1)):0,wc=size&&wr?bus(addr,size,c.writeHistory()):0;unsigned stack=callPhase?8+bus(c.getSP()-4,4,c.writeHistory()):save?5+bus(c.getSP()-2,2,c.writeHistory()):restore?5+bus(c.getSP(),2,int(at+len-1)):5+bus(c.getSP(),4,int(at+len-1));T::Credits cr{rc,wc,stack};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::ClearIndex:case S::ClearLoopIndex:c.reg8(8)=0;val(c,c.reg8(8));break;
 case S::RetainCount:case S::RetainOwner:{int reg=s==S::RetainCount?0:1;auto sp=c.getSP()-2;c.setSP(sp);c.write16(c.reg16(reg),int(sp));c.clockI(2);val(c,c.reg16(reg));break;}
 case S::RestoreOwner:case S::RestoreCount:{int reg=s==S::RestoreOwner?1:0;auto sp=c.getSP();c.reg16(reg)=int16_t(c.read16(int(sp)));c.setSP(sp+2);c.clockI(2);val(c,c.reg16(reg));break;}
 case S::CompareOwner:{const unsigned a=uint8_t(c.reg8(9)),b=uint8_t(c.reg8(8)),r=a-b,carry=(~a&b)|(~a&r)|(b&r),ov=(a&~b&~r)|(~a&b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!(r&255)?4:0)|((r&128)?8:0)|((carry&128)?1:0)|((carry&8)?32:0)|((ov&128)?2:0));break;}
 case S::IncrementIndex:case S::DecrementCount:{const int old=c.reg8(8),v=old+(s==S::IncrementIndex?1:-1);val(c,v);if(s==S::IncrementIndex?((~old&v)&128):((old&~v)&128))c.ccr|=2;c.reg8(8)=int8_t(v);break;}
 case S::DispatchCopy:case S::DispatchVoice:case S::DispatchDspSelect:case S::DispatchUpdate:{expectedReturn_=at+len;const auto target=s==S::DispatchCopy?0x72e2:s==S::DispatchVoice?0x72ba:s==S::DispatchDspSelect?0x5bb8:task_.kind()==T::Kind::Pitch?0x5ea8:0x5c3a;call(c,target,expectedReturn_);break;}
 case S::Return:ret(c);break;default:break;}
 auto used=task_.advanceBefore(cost,cr,[&](T::Read r)->uint32_t{switch(r){case T::Read::Part:{const int reg=s==S::ReadOwner?9:8;c.reg8(reg)=c.read8(0xd028);val(c,c.reg8(reg));return uint8_t(c.reg8(reg));}case T::Read::Flag:c.reg8(9)=c.read8(0xbfc8);val(c,c.reg8(9));return uint8_t(c.reg8(9));case T::Read::Count:c.reg8(8)=c.read8(0xd022);val(c,c.reg8(8));return uint8_t(c.reg8(8));case T::Read::Age:c.reg16(0)=int16_t(c.read16(int(task_.voice()+8)));val(c,c.reg16(0));return uint16_t(c.reg16(0));case T::Read::VoiceOwner:c.reg8(8)=c.read8(int(task_.voice()+4));val(c,c.reg8(8));return uint8_t(c.reg8(8));case T::Read::Patch:c.reg32(6)=int32_t(c.read32(int(task_.voice())));val(c,c.reg32(6));return uint32_t(c.reg32(6));case T::Read::Index:c.reg8(8)=c.read8(0xd023);val(c,c.reg8(8));return uint8_t(c.reg8(8));}throw std::logic_error("K voice refresh read");},[&](T::Write,uint8_t v){if(uint8_t(c.reg8(8))!=v)throw std::logic_error("K voice refresh index projection");val(c,c.reg8(8));c.write8(int8_t(v),0xd023);});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice refresh clocks differ");if(task_.child()==T::Child::None&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;uint32_t expectedReturn_=0;
};

class NativeVoiceModulationDepth final {
 using T=VoiceModulationDepthTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5c3a,0x5e64}}};
 explicit NativeVoiceModulationDepth(uint32_t patch):task_(patch){}const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingWrite()const{return task_.awaitingWrite();}
 bool resumeWrite(Stage1Cpu&c){return uint32_t(c.getPC())==expectedReturn_&&task_.resumeWrite();}
 uint32_t boundary()const{constexpr uint32_t off[]={0x0,0x8,0xa,0xc,0x14,0x16,0x1c,0x20,0x22,0x2a,0x2c,0x32,0x36,0x38,0x40,0x42,0x48,0x4c,0x4e,0x52,0x56,0x58,0x5c,0x5e,0x60,0x62,0x64,0x66,0x6a,0x6c,0x70,0x72,0x74,0x7c,0x80,0x82,0x86};if(complete())return 0;if(awaitingWrite())return expectedReturn_;if(task_.stage()==S::Return)return 0x5e62;return 0x5c3a+0x8a*task_.lane()+off[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||awaitingWrite()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();unsigned len=length(s);uint32_t addr=readAddress(task_,s);unsigned size=s==S::ReadTable?2:1;T::Credits cr{addr?bus(addr,size,int(at+len-1)):0,s==S::DispatchWord?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::BiasBase:c.reg8(9)=int8_t(arithmetic(c,8,uint8_t(c.reg8(9)),0xc0,false));break;
 case S::SignBase:c.reg16(1)=int16_t(int8_t(c.reg8(9)));val(c,c.reg16(1));break;
 case S::BiasFirst:case S::BiasSecond:case S::BiasThird:c.reg8(8)=int8_t(arithmetic(c,8,uint8_t(c.reg8(8)),0xc0,false));break;
 case S::MultiplyFirst:case S::MultiplySecond:case S::MultiplyThird:{int v=int(c.reg8(0))*int(c.reg8(8));c.reg16(0)=int16_t(v);mulFlags(c,c.reg16(0));c.clockI(12);break;}
 case S::RetainFirst:c.reg16(2)=c.reg16(0);val(c,c.reg16(2));break;
 case S::AccumulateSecond:c.reg16(2)=int16_t(arithmetic(c,16,uint16_t(c.reg16(2)),uint16_t(c.reg16(0)),false));break;
 case S::AccumulateThird:c.reg16(0)=int16_t(arithmetic(c,16,uint16_t(c.reg16(0)),uint16_t(c.reg16(2)),false));break;
 case S::SetGain:c.reg16(8)=0x200;val(c,c.reg16(8));break;
 case S::MultiplyGain:c.reg32(0)=int32_t(c.reg16(8))*int32_t(c.reg16(0));mulFlags(c,c.reg32(0));c.clockI(20);break;
 case S::ExtractGain:c.reg16(0)=c.reg16(8);val(c,c.reg16(0));break;
 case S::SetMagnitude:c.reg16(2)=0x5555;val(c,c.reg16(2));break;
 case S::AccumulateBase:c.reg16(1)=int16_t(arithmetic(c,16,uint16_t(c.reg16(1)),uint16_t(c.reg16(0)),false));break;
 case S::CopyResult:c.reg16(0)=c.reg16(1);val(c,c.reg16(0));break;
 case S::NegateResult:c.reg16(1)=int16_t(arithmetic(c,16,0,uint16_t(c.reg16(1)),true));break;
 case S::NegateMagnitude:c.reg16(2)=int16_t(arithmetic(c,16,0,uint16_t(c.reg16(2)),true));break;
 case S::CompareMagnitude:arithmetic(c,16,uint16_t(c.reg16(1)),64,true);break;
 case S::Clamp:c.reg16(1)=64;val(c,c.reg16(1));break;
 case S::ShiftIndex:{uint32_t v=uint16_t(c.reg16(1));c.ccr=uint8_t((c.ccr&~1)|((v&0x8000)?1:0));v<<=1;val(c,int32_t(v<<16));c.reg16(1)=int16_t(v);break;}
 case S::ExtendIndex:c.reg32(1)=uint16_t(c.reg16(1));val(c,c.reg32(1));break;
 case S::MultiplyTable:c.reg32(1)=int32_t(c.reg16(2))*int32_t(c.reg16(1));mulFlags(c,c.reg32(1));c.clockI(20);break;
 case S::ShiftResult:{auto v=uint32_t(c.reg32(1));c.ccr=uint8_t((c.ccr&~1)|((v&0x80000000)?1:0));v<<=1;c.reg32(1)=int32_t(v);val(c,c.reg32(1));break;}
 case S::BindCommand:c.reg16(4)=int16_t(task_.command());val(c,c.reg16(4));break;
 case S::DispatchWord:if(((uint32_t(uint16_t(c.reg16(9)))<<8)|uint8_t(c.reg8(1)))!=task_.word())throw std::logic_error("K modulation word projection");expectedReturn_=at+len;call(c,0x5bec,expectedReturn_);break;
 case S::Return:ret(c);break;default:break;}
 auto used=task_.advanceBefore(cost,cr,[&](T::Read r)->uint32_t{auto a=readAddress(task_,s);if(r==T::Read::Table){c.reg16(1)=int16_t(c.read16(int(a)));val(c,c.reg16(1));return uint16_t(c.reg16(1));}int reg=r==T::Read::Base?9:(r==T::Read::FirstControl||r==T::Read::SecondControl||r==T::Read::ThirdControl)?0:8;c.reg8(reg)=c.read8(int(a));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K modulation depth clocks differ");if(!complete()&&!awaitingWrite())c.pc=c.makepc(int(boundary()));return true;}
private:
 static uint32_t arithmetic(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=(1u<<bits)-1,sign=1u<<(bits-1),half=sign>>4;const auto r=(sub?a-b:a+b)&mask,carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r)),ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 static void mulFlags(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~12)|(v<0?8:0)|(!v?4:0));}
 static uint32_t readAddress(const T&t,S s){switch(s){case S::ReadBase:return t.baseAddress();case S::ReadFirst:return t.paramAddress(0);case S::ReadSecond:return t.paramAddress(1);case S::ReadThird:return t.paramAddress(2);case S::ReadFirstControl:return 0xbfc0;case S::ReadSecondControl:return 0xbfc1;case S::ReadThirdControl:return 0xbfc3;case S::ReadTable:return t.tableAddress();default:return 0;}}
 static unsigned length(S s){switch(s){case S::ReadBase:case S::ReadFirst:case S::ReadSecond:case S::ReadThird:case S::ReadTable:return 8;case S::ReadFirstControl:case S::ReadSecondControl:case S::ReadThirdControl:return 6;case S::MultiplyFirst:case S::MultiplySecond:case S::MultiplyThird:case S::SetGain:case S::MultiplyGain:case S::SetMagnitude:case S::CompareMagnitude:case S::Clamp:case S::MultiplyTable:case S::BindCommand:case S::DispatchWord:return 4;default:return 2;}}
 T task_;uint32_t expectedReturn_=0;
};

class NativeVoiceModulationPitch final {
 using T=VoiceModulationPitchTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5ea8,0x610e}}};
 explicit NativeVoiceModulationPitch(uint32_t patch):task_(patch){}const T&task()const{return task_;}bool complete()const{return task_.complete();}
 bool resumeChild(Stage1Cpu&c){if(task_.child()==T::Child::None||uint32_t(c.getPC())!=expectedReturn_)return false;return task_.resumeChild(task_.child()==T::Child::Voice?uint32_t(c.reg32(3)):0);}
 uint32_t boundary()const{constexpr uint32_t off[]={0x0,0x6,0xe,0x10,0x12,0x16,0x18,0x20,0x22,0x24,0x2c,0x34,0x36,0x3c,0x3e,0x40,0x42,0x46,0x4e,0x50,0x52,0x54,0x58,0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x70,0x74,0x7e,0x86,0x8a,0x8c,0x8e,0x90,0x92,0x94,0x96,0x9a,0x9e,0xa0,0xa8,0xaa,0xac,0xae,0xb4,0xb8,0xbc,0xbe,0xc2,0xc4,0xc8};if(complete())return 0;if(task_.child()!=T::Child::None)return expectedReturn_;if(task_.stage()==S::Return)return 0x610c;return 0x5ea8+0xcc*task_.lane()+off[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_update_projection;if(blocked(c)||complete()||task_.child()!=T::Child::None||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();unsigned len=length(s);uint32_t addr=readAddress(task_,s);unsigned size=s==S::ReadBase||s==S::ReadWheel?2:s==S::ReadVoicePointer?4:1;const bool dispatch=s==S::DispatchVoice||s==S::DispatchWord;T::Credits cr{addr?bus(addr,size,int(at+len-1)):0,dispatch?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::BiasTune:case S::BiasCoarse:case S::BiasAmount:c.reg8(8)=int8_t(arithmetic(c,8,uint8_t(c.reg8(8)),0xc0,false));break;
 case S::SetTwelve:c.reg8(0)=12;val(c,c.reg8(0));break;
 case S::MultiplyTune:c.reg16(0)=int16_t(int(c.reg8(0))*int(c.reg8(8)));mulFlags(c,c.reg16(0));c.clockI(12);break;
 case S::AddTune:case S::AddCoarse:c.reg8(1)=int8_t(arithmetic(c,8,uint8_t(c.reg8(1)),uint8_t(c.reg8(8)),false));break;
 case S::AddFine:case S::AddOffset:c.reg8(8)=int8_t(arithmetic(c,8,uint8_t(c.reg8(8)),uint8_t(c.reg8(0)),false));break;
 case S::ClearCarry:c.reg8(0)=0;val(c,c.reg8(0));break;
 case S::CaptureCarry:{bool z=c.ccr&4;const auto carry=c.ccr&1;auto r=arithmetic(c,8,0,carry,false);if(!r&&z)c.ccr|=4;c.reg8(0)=int8_t(r);break;}
 case S::BiasKey:c.reg16(0)=int16_t(arithmetic(c,16,uint16_t(c.reg16(0)),0xc0,true));break;
 case S::ShiftPolyFirst:case S::ShiftPolySecond:case S::ShiftDivisor:shift(c,c.reg8(10),8);break;
 case S::MaskUnit:c.reg8(2)&=7;val(c,c.reg8(2));break;
 case S::ShiftUnitFirst:case S::ShiftUnitSecond:case S::ShiftUnitThird:shift(c,c.reg8(2),8);break;
 case S::IncrementUnit:c.reg8(2)=int8_t(arithmetic(c,8,uint8_t(c.reg8(2)),1,false));break;
 case S::SubtractPoly:c.reg8(2)=int8_t(arithmetic(c,8,uint8_t(c.reg8(2)),uint8_t(c.reg8(10)),true));break;
 case S::BiasDivisor:c.reg8(10)=int8_t(arithmetic(c,8,uint8_t(c.reg8(10)),12,false));break;
 case S::Divide:{const int divisor=c.reg8(10),dividend=c.reg16(2),q=divisor?dividend/divisor:0;c.ccr=uint8_t((c.ccr&~12)|(!divisor?4:0)|((((divisor&~dividend)|(~divisor&dividend))&0x80)?8:0));c.reg8(10)=int8_t(q);c.reg8(2)=int8_t(dividend-q*divisor);c.clockI(12);break;}
 case S::MultiplyDepth:c.reg16(2)=int16_t(int(c.reg8(2))*int(c.reg8(10)));mulFlags(c,c.reg16(2));c.clockI(12);break;
 case S::ExtractDepth:c.reg8(10)=c.reg8(2);val(c,c.reg8(10));break;case S::SignDepth:c.reg16(2)=int16_t(int8_t(c.reg8(10)));val(c,c.reg16(2));break;
 case S::AddDepth:c.reg16(0)=int16_t(arithmetic(c,16,uint16_t(c.reg16(0)),uint16_t(c.reg16(2)),false));break;
 case S::ShiftKeyFirst:case S::ShiftKeySecond:case S::ShiftKeyThird:shift(c,c.reg16(0),16);break;
 case S::SetGain:c.reg16(8)=0x51eb;val(c,c.reg16(8));break;
 case S::MultiplyGain:case S::MultiplyWheel:c.reg32(0)=int32_t(c.reg16(8))*int32_t(c.reg16(0));mulFlags(c,c.reg32(0));c.clockI(20);break;
 case S::AddGain:c.reg16(1)=int16_t(arithmetic(c,16,uint16_t(c.reg16(1)),uint16_t(c.reg16(8)),false));break;
 case S::SignAmount:c.reg16(0)=int16_t(int8_t(c.reg8(8)));val(c,c.reg16(0));break;case S::RetainAmount:c.reg16(8)=c.reg16(0);val(c,c.reg16(8));break;
 case S::BiasWheel:c.reg16(0)=int16_t(arithmetic(c,16,uint16_t(c.reg16(0)),0x100,true));break;
 case S::AddWheel:c.reg16(1)=int16_t(arithmetic(c,16,uint16_t(c.reg16(1)),uint16_t(c.reg16(0)),false));break;
 case S::SetScale:c.reg16(9)=int16_t(0xaaab);val(c,c.reg16(9));break;
 case S::MultiplyScale:c.reg32(1)=int32_t(uint32_t(uint16_t(c.reg16(9)))*uint16_t(c.reg16(1)));c.clockI(20);break;
 case S::BindCommand:c.reg16(4)=int16_t(task_.command());val(c,c.reg16(4));break;
 case S::DispatchVoice:case S::DispatchWord:if(s==S::DispatchWord&&((uint32_t(uint16_t(c.reg16(9)))<<8)|uint8_t(c.reg8(1)))!=task_.word())throw std::logic_error("K pitch word projection");expectedReturn_=at+len;call(c,s==S::DispatchVoice?0x72ba:0x5bec,expectedReturn_);break;
 case S::Return:ret(c);break;default:break;}
 auto used=task_.advanceBefore(cost,cr,[&](T::Read r)->uint32_t{auto a=readAddress(task_,s);if(r==T::Read::VoicePointer){c.reg32(3)=int32_t(c.read32(int(a)));val(c,c.reg32(3));return uint32_t(c.reg32(3));}if(r==T::Read::Base||r==T::Read::Wheel){int reg=r==T::Read::Base?1:0;c.reg16(reg)=int16_t(c.read16(int(a)));val(c,c.reg16(reg));return uint16_t(c.reg16(reg));}int reg=(r==T::Read::Fine||r==T::Read::Offset)?0:(r==T::Read::Poly)?10:(r==T::Read::Unit||r==T::Read::Depth)?2:8;c.reg8(reg)=c.read8(int(a));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K modulation pitch clocks differ");if(!complete()&&task_.child()==T::Child::None)c.pc=c.makepc(int(boundary()));return true;}
private:
 static uint32_t arithmetic(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=(1u<<bits)-1,sign=1u<<(bits-1),half=sign>>4;const auto r=(sub?a-b:a+b)&mask,carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r)),ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 template<class R>static void shift(Stage1Cpu&c,R&v,unsigned bits){uint32_t x=uint32_t(v)&((1u<<bits)-1);c.ccr=uint8_t((c.ccr&~1)|((x&(1u<<(bits-1)))?1:0));x<<=1;voice_update_projection::val(c,int32_t(x<<(32-bits)));v=R(x);}
 static void mulFlags(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~12)|(v<0?8:0)|(!v?4:0));}
 static uint32_t readAddress(const T&t,S s){switch(s){case S::ReadBase:return 0xbf9a;case S::ReadTune:return t.tuneAddress();case S::ReadCoarse:return t.coarseAddress();case S::ReadKey:return t.patch()+0xc2;case S::ReadFine:return t.fineAddress();case S::ReadOffset:return 0xbfc5;case S::ReadPoly:return t.patch()+0x81;case S::ReadUnit:return t.voice()+12;case S::ReadVoicePointer:return t.voice();case S::ReadDepth:return t.pointer()+1;case S::ReadAmount:return t.amountAddress();case S::ReadWheel:return 0xbfbe;default:return 0;}}
 static unsigned length(S s){switch(s){case S::ReadVoicePointer:return 10;case S::ReadTune:case S::ReadCoarse:case S::ReadKey:case S::ReadFine:case S::ReadPoly:case S::ReadUnit:case S::ReadDepth:case S::ReadAmount:return 8;case S::ReadBase:case S::ReadOffset:case S::ReadWheel:return 6;case S::MultiplyTune:case S::BiasKey:case S::DispatchVoice:case S::Divide:case S::MultiplyDepth:case S::SetGain:case S::MultiplyGain:case S::BiasWheel:case S::MultiplyWheel:case S::SetScale:case S::BindCommand:case S::DispatchWord:return 4;default:return 2;}}
 T task_;uint32_t expectedReturn_=0;
};

// Fixed-capacity owner; DSP work remains an explicit NativeDspTransfer child.
class NativeVoiceUpdate final {
 using Frame=std::variant<std::monostate,NativeVoiceGate,NativeVoiceDspWrite,NativeVoiceDspAddress,NativeVoiceExpressionValues,NativeVoiceModulationDepth,NativeVoiceModulationPitch,NativeVoiceRefresh>;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,7> CodeRanges{{{0x5ba0,0x5bb8},{0x5bec,0x5c04},{0x5c3a,0x5e64},{0x5e64,0x5ea8},{0x5ea8,0x610e},{0x6f44,0x6f50},{0x785c,0x7930}}};
 static bool candidate(uint32_t pc){return (pc>=0x5ba0&&pc<0x5bb8)||(pc>=0x5bec&&pc<0x5c04)||(pc>=0x5c3a&&pc<0x610e)||(pc>=0x6f44&&pc<0x6f50)||(pc>=0x785c&&pc<0x7930);}
 bool active()const{return depth_!=0;}unsigned depth()const{return depth_;}void reset(){for(auto&f:frames_)f.template emplace<std::monostate>();depth_=0;}
 bool step(Stage1Cpu&c){if(voice_update_projection::blocked(c))return false;const auto pc=uint32_t(c.getPC());if(depth_){if(auto*p=std::get_if<NativeVoiceDspWrite>(&frames_[depth_-1]))if(p->awaitingDsp()){if(pc==0x5c02)p->resumeDsp(c);else if(!entry(pc))return false;}if(auto*p=std::get_if<NativeVoiceDspAddress>(&frames_[depth_-1]))if(p->awaitingDsp()){if(pc==0x5bb6)p->resumeDsp(c);else if(!entry(pc))return false;}if(auto*p=std::get_if<NativeVoiceExpressionValues>(&frames_[depth_-1]))if(p->awaitingDsp()){if(pc==p->boundary())p->resumeDsp(c);else if(!entry(pc))return false;}if(auto*p=std::get_if<NativeVoiceModulationPitch>(&frames_[depth_-1]))if(p->task().child()!=VoiceModulationPitchTask::Child::None){if(pc==p->boundary())p->resumeChild(c);else if(!entry(pc))return false;}if(auto*p=std::get_if<NativeVoiceRefresh>(&frames_[depth_-1]))if(p->task().child()!=VoiceRefreshTask::Child::None){if(pc==p->boundary())p->resumeChild(c);else if(!entry(pc))return false;}}
  if(!depth_||!atTop(pc)){if(!entry(pc)){if(candidate(pc))throw std::logic_error("Unowned K voice update boundary");return false;}push(c,pc);}
  auto&frame=frames_[depth_-1];bool stepped=false,done=false;std::visit([&](auto&job){using J=std::decay_t<decltype(job)>;if constexpr(!std::is_same_v<J,std::monostate>){stepped=job.step(c);done=job.complete();}},frame);if(!stepped)return false;
  if(done){frame.template emplace<std::monostate>();--depth_;if(depth_){auto&parent=frames_[depth_-1];if(auto*p=std::get_if<NativeVoiceGate>(&parent)){if(p->awaitingWrite())p->resumeWrite(c);}else if(auto*p=std::get_if<NativeVoiceModulationDepth>(&parent)){if(p->awaitingWrite())p->resumeWrite(c);}else if(auto*p=std::get_if<NativeVoiceModulationPitch>(&parent)){if(p->task().child()!=VoiceModulationPitchTask::Child::None)p->resumeChild(c);}else if(auto*p=std::get_if<NativeVoiceRefresh>(&parent)){if(p->task().child()!=VoiceRefreshTask::Child::None)p->resumeChild(c);}}}return true;}
private:
 static bool entry(uint32_t pc){return pc==0x6f44||pc==0x5ba0||pc==0x5bec||pc==0x5e64||pc==0x5c3a||pc==0x5ea8||pc==0x785c||pc==0x78c2;}
 bool atTop(uint32_t pc)const{bool yes=false;std::visit([&](const auto&job){using J=std::decay_t<decltype(job)>;if constexpr(!std::is_same_v<J,std::monostate>)yes=!job.complete()&&job.boundary()==pc;},frames_[depth_-1]);return yes;}
 void push(Stage1Cpu&c,uint32_t pc){if(depth_==frames_.size())throw std::logic_error("K voice update continuation capacity");auto&f=frames_[depth_++];if(pc==0x6f44)f.template emplace<NativeVoiceGate>();else if(pc==0x5ba0)f.template emplace<NativeVoiceDspAddress>(uint16_t(c.reg16(4)));else if(pc==0x5e64)f.template emplace<NativeVoiceExpressionValues>();else if(pc==0x5c3a)f.template emplace<NativeVoiceModulationDepth>(uint32_t(c.reg32(6)));else if(pc==0x5ea8)f.template emplace<NativeVoiceModulationPitch>(uint32_t(c.reg32(6)));else if(pc==0x785c||pc==0x78c2)f.template emplace<NativeVoiceRefresh>(pc==0x785c?VoiceRefreshTask::Kind::Depth:VoiceRefreshTask::Kind::Pitch);else f.template emplace<NativeVoiceDspWrite>(uint16_t(c.reg16(4)));}
 std::array<Frame,16>frames_{};unsigned depth_=0;
};

}
