#pragma once
#include "kstationVoiceAllocationTask.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>
#include <variant>
#include <type_traits>
#include <cstdio>
namespace kstation {
// Temporary H8 caller projection for 72ba. The pure task owns all decisions;
// this adapter is an explicit subroutine boundary, not the 6fde allocator.
class NativeVoiceAddress final {
 using T=VoiceAddressTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x72ba,0x72ce}}};
 const T& task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{constexpr uint32_t pc[]{0x72ba,0x72c0,0x72c2,0x72c4,0x72c6,0x72cc};return complete()?0:pc[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto s=task_.stage();const auto at=boundary();const unsigned len=s==S::ReadSlot||s==S::AddBase?6:2;T::Credits credit{3,5+busCost(c.getSP(),4,int(at+len-1))};const auto cost=task_.nextClocks(credit);const auto start=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::BindStride:c.reg8(3)=18;val(c,c.reg8(3));break;
 case S::Multiply:c.reg16(3)=int16_t(unsigned(uint8_t(c.reg8(3)))*uint8_t(c.reg8(11)));c.clockI(12);break;
 case S::Extend:c.reg32(3)=uint16_t(c.reg16(3));val(c,c.reg32(3));break;
 case S::AddBase:c.reg32(3)=int32_t(add(c,uint32_t(c.reg32(3)),0xd030));break;
 case S::Return:{c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));break;}
 default:break;}
 const auto used=task_.advanceBefore(cost,credit,[&](){c.reg8(11)=c.read8(0xd023);val(c,c.reg8(11));return uint8_t(c.reg8(11));});if(used!=cost||c.cycles-start!=cost)throw std::logic_error("K voice address phase clock mismatch");if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t add(Stage1Cpu&c,uint32_t a,uint32_t b){const auto r=a+b,carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x80000000)?8:0)|((carry&0x80000000)?1:0)|((carry&0x08000000)?32:0)|((over&0x80000000)?2:0));return r;}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_{};
};
class NativeVoicePartAddress final {
 using T=VoicePartAddressTask;using S=T::Stage;
public:
 NativeVoicePartAddress()=default;explicit NativeVoicePartAddress(uint8_t part):task_(part){}
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x72ce,0x72e2}}};
 const T& task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{constexpr uint32_t pc[]{0x72ce,0x72d4,0x72d6,0x72d8,0x72da,0x72e0};return complete()?0:pc[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto s=task_.stage();const auto at=boundary();const unsigned len=s==S::ReadSlot||s==S::AddBase?6:2;T::Credits credit{3,5+busCost(c.getSP(),4,int(at+len-1))};const auto cost=task_.nextClocks(credit);const auto start=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::BindStride:c.reg8(0)=16;val(c,c.reg8(0));break;
 case S::Multiply:c.reg16(0)=int16_t(unsigned(uint8_t(c.reg8(0)))*uint8_t(c.reg8(8)));c.clockI(12);break;
 case S::Extend:c.reg32(0)=uint16_t(c.reg16(0));val(c,c.reg32(0));break;
 case S::AddBase:c.reg32(0)=int32_t(add(c,uint32_t(c.reg32(0)),0xbfaa));break;
 case S::Return:{c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));break;}
 default:break;}
 const auto used=task_.advanceBefore(cost,credit,[&](){c.reg8(8)=c.read8(0xd028);val(c,c.reg8(8));return uint8_t(c.reg8(8));});if(used!=cost||c.cycles-start!=cost)throw std::logic_error("K voice address phase clock mismatch");if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t add(Stage1Cpu&c,uint32_t a,uint32_t b){const auto r=a+b,carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x80000000)?8:0)|((carry&0x80000000)?1:0)|((carry&0x08000000)?32:0)|((over&0x80000000)?2:0));return r;}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_{};
};
// Temporary caller projection of the closed selection prefix. On reaching
// 71a0 the caller owns the still-unported voice activation continuation.
class NativeVoiceSelection final {
 using T=VoiceSelectionTask;using S=T::Stage;
 struct Site{uint32_t pc;unsigned len;};
 inline static constexpr Site sites[]={{0x6fde,4},{0x6fe2,8},{0x6fea,8},{0x6ff2,8},{0x6ffa,6},{0x7000,2},{0x7002,2},{0x7004,6},{0x700a,6},{0x7010,6},{0x7016,2},{0x7018,4},{0x701c,2},{0x701e,2},{0x7020,2},{0x7022,4},{0x7026,2},{0x7028,6},{0x702e,2},{0x7030,4},{0x7034,10},{0x703e,8},{0x7046,8},{0x704e,2},{0x7050,2},{0x7052,2},{0x7054,2},{0x7056,2},{0x7058,6},{0x705e,6},{0x7064,2},{0x7066,6},{0x706c,2},{0x706e,2},{0x7070,6},{0x7076,6},{0x707c,2},{0x707e,4},{0x7082,4},{0x7086,2},{0x7088,4},{0x708c,10},{0x7096,8},{0x709e,8},{0x70a6,2},{0x70a8,2},{0x70aa,2},{0x70ac,2},{0x70ae,4},{0x70b2,2},{0x70b4,2},{0x70b6,2},{0x70b8,6},{0x70be,6},{0x70c4,2},{0x70c6,6},{0x70cc,2},{0x70ce,2},{0x70d0,6},{0x70d6,2},{0x70d8,2},{0x70da,2},{0x70dc,6},{0x70e2,4},{0x70e6,2},{0x70e8,2},{0x70ea,2},{0x70ec,6},{0x70f2,4},{0x70f6,10},{0x7100,2},{0x7102,4},{0x7106,2},{0x7108,2},{0x710a,2},{0x710c,6},{0x7112,6},{0x7118,2},{0x711a,6},{0x7120,2},{0x7122,2},{0x7124,6},{0x712a,4},{0x712e,6},{0x7134,2},{0x7136,6},{0x713c,4},{0x7140,10},{0x714a,2},{0x714c,2},{0x714e,8},{0x7156,8},{0x715e,2},{0x7160,2},{0x7162,8},{0x716a,2},{0x716c,2},{0x716e,2},{0x7170,6},{0x7176,2},{0x7178,2},{0x717a,2},{0x717c,2},{0x717e,6},{0x7184,6},{0x718a,2},{0x718c,6},{0x7192,2},{0x7194,2},{0x7196,6},{0x719c,2},{0x719e,2}};
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x6fde,0x71a0}}};
 explicit NativeVoiceSelection(uint32_t patch):task_(patch){}
 const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{return complete()?0x71a0:task_.child()!=T::Child::None?expectedReturn_:sites[unsigned(task_.stage())].pc;}
 bool resumeChild(Stage1Cpu&c){if(task_.child()==T::Child::None||uint32_t(c.getPC())!=expectedReturn_)return false;const auto v=task_.child()==T::Child::Part?uint32_t(c.reg32(0)):uint32_t(c.reg32(3));return task_.resumeChild(v);}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||task_.child()!=T::Child::None||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto st=task_.stage();const auto site=sites[unsigned(st)];uint32_t ra=0,wa=0;unsigned rn=0,wn=0;access(task_,ra,rn,wa,wn);const unsigned rc=rn?busCost(ra,rn,int(site.pc+site.len-1)):0,wc=wn?busCost(wa,wn,c.writeHistory()):0,sc=8+busCost(c.getSP()-4,4,c.writeHistory());T::Credits cr{rc,wc,sc};const auto cost=task_.nextClocks(cr);const auto start=c.cycles;c.fetchClocks(site.len);c.pc=c.makepc(int(site.pc+1));if(isDispatch(st)){expectedReturn_=site.pc+site.len;const auto sp=c.getSP()-4;c.write32(int32_t(expectedReturn_),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);c.clockI(2);}else project(c,st);const auto used=task_.advanceBefore(cost,cr,[&](T::Read r){return read(c,task_,r);},[&](T::Write w,uint8_t v){write(c,task_,w,v);});if(used!=cost||c.cycles-start!=cost)throw std::logic_error("K voice selection phase clocks differ");if(task_.child()!=T::Child::None)c.pc=c.makepc(task_.child()==T::Child::Part?0x72ce:0x72ba);else c.pc=c.makepc(int(boundary()));return true;}
private:
 static bool isDispatch(S s){return s==S::DispatchPart||s==S::DispatchOwnerVoice||s==S::DispatchNewestVoice||s==S::DispatchFreeVoice||s==S::DispatchNoteVoice;}
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t arithmetic(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=sign>>4;const auto r=(sub?a-b:a+b)&mask,carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r)),ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 template<class R>static void increment(Stage1Cpu&c,R&v,int direction,unsigned amount,unsigned bits){const int64_t old=v,r=old+direction*int64_t(amount);val(c,int(r));const uint64_t sign=uint64_t(1)<<(bits-1);if(direction>0?((~uint64_t(old)&uint64_t(r)&sign)!=0):((uint64_t(old)&~uint64_t(r)&sign)!=0))c.ccr|=2;v=R(r);}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 static void project(Stage1Cpu&c,S s){switch(s){
case S::RetainTag:c.reg16(13)=c.reg16(5);val(c,c.reg16(13));break;
case S::ClearCandidate:c.reg8(2)=arithmetic(c,8,c.reg8(2),c.reg8(2),true);break;
case S::InitializeAge:c.reg32(1)=-1;val(c,c.reg32(1));break;
case S::CompareMode:arithmetic(c,8,c.reg8(12),1u,true);break;
case S::SelectMode:c.clockI(2);break;
case S::TestHeld:c.reg8(4)=c.reg8(4);val(c,c.reg8(4));break;
case S::SetHeldAllocation:c.reg8(8)=1u;val(c,c.reg8(8));break;
case S::SkipOwner:c.clockI(2);break;
case S::SetReuseAllocation:c.reg8(8)=4u;val(c,c.reg8(8));break;
case S::SetOwnerAllocation:c.reg8(12)=1u;val(c,c.reg8(12));break;
case S::CompareOwner:arithmetic(c,16,c.reg16(5),c.reg16(13),true);break;
case S::CompareOldest:arithmetic(c,32,c.reg32(0),c.reg32(1),true);break;
case S::RetainOldest:c.reg32(1)=c.reg32(0);val(c,c.reg32(1));break;
case S::AdvanceOwnerIndex:increment(c,c.reg8(8),1,1,8);break;
case S::DecrementOwner:increment(c,c.reg8(10),-1,1,8);break;
case S::TestOldestHigh:c.reg16(9)=c.reg16(9);val(c,c.reg16(9));break;
case S::SelectOldestHigh:c.clockI(2);break;
case S::SkipOwnerFinish:c.clockI(2);break;
case S::ClearNewestAge:c.reg32(1)=arithmetic(c,32,c.reg32(1),c.reg32(1),true);break;
case S::CompareNewestOwner:arithmetic(c,16,c.reg16(5),c.reg16(13),true);break;
case S::CopyNewestHigh:c.reg16(5)=c.reg16(8);val(c,c.reg16(5));break;
case S::MaskNewestHigh:c.reg8(5)&=4294967232u;val(c,c.reg8(5));break;
case S::SelectNewestActive:c.clockI(2);break;
case S::CompareNewest:arithmetic(c,32,c.reg32(0),c.reg32(1),true);break;
case S::RetainNewest:c.reg32(1)=c.reg32(0);val(c,c.reg32(1));break;
case S::AdvanceNewestIndex:increment(c,c.reg8(8),1,1,8);break;
case S::DecrementNewest:increment(c,c.reg8(10),-1,1,8);break;
case S::TestNewestHigh:c.reg16(9)=c.reg16(9);val(c,c.reg16(9));break;
case S::SetNewestAllocation:c.reg8(12)=2u;val(c,c.reg8(12));break;
case S::SkipNewestFinish:c.clockI(2);break;
case S::ComparePolyMode:arithmetic(c,8,c.reg8(12),2u,true);break;
case S::SetPolyAllocation:c.reg8(8)=0u;val(c,c.reg8(8));break;
case S::TestFreeHigh:c.reg16(8)=c.reg16(8);val(c,c.reg16(8));break;
case S::SelectFreeHigh:c.clockI(2);break;
case S::CompareFreeAge:arithmetic(c,32,c.reg32(0),c.reg32(1),true);break;
case S::RetainFreeAge:c.reg32(1)=c.reg32(0);val(c,c.reg32(1));break;
case S::AdvanceFreeIndex:increment(c,c.reg8(8),1,1,8);break;
case S::DecrementFree:increment(c,c.reg8(10),-1,1,8);break;
case S::SkipFreeFinish:c.clockI(2);break;
case S::SetNoteAllocation:c.reg8(8)=0u;val(c,c.reg8(8));break;
case S::TestNoteHigh:c.reg16(8)=c.reg16(8);val(c,c.reg16(8));break;
case S::CompareNoteOwner:arithmetic(c,16,c.reg16(5),c.reg16(13),true);break;
case S::CompareNote:arithmetic(c,8,c.reg8(12),c.reg8(13),true);break;
case S::SetRetriggerAllocation:c.reg8(8)=4u;val(c,c.reg8(8));break;
case S::CompareNoteAge:arithmetic(c,32,c.reg32(0),c.reg32(1),true);break;
case S::RetainNoteAge:c.reg32(1)=c.reg32(0);val(c,c.reg32(1));break;
case S::AdvanceNoteIndex:increment(c,c.reg8(8),1,1,8);break;
case S::DecrementNote:increment(c,c.reg8(10),-1,1,8);break;
case S::TestNoteBestHigh:c.reg16(9)=c.reg16(9);val(c,c.reg16(9));break;

 default:break;}}
 static void access(const T&t,uint32_t&ra,unsigned&rn,uint32_t&wa,unsigned&wn){switch(t.stage()){
 case S::ReadMode:ra=t.patchAddress()+0x80;rn=1;break;case S::ReadHeld:ra=t.partAddress()+14;rn=1;break;case S::ReadUnit:ra=t.partAddress()+15;rn=1;break;case S::ReadPart:ra=0xd028;rn=1;break;case S::ReadCount:ra=0xd022;rn=1;break;
 case S::ReadOwnerAge:case S::ReadNewestAge:case S::ReadFreeAge:case S::ReadNoteAge:ra=t.voiceAddress()+8;rn=4;break;
 case S::ReadOwnerPart:case S::ReadNewestPart:case S::ReadNotePart:ra=t.voiceAddress()+4;rn=1;break;
 case S::ReadOwnerUnit:case S::ReadNewestUnit:case S::ReadNoteUnit:ra=t.voiceAddress()+12;rn=1;break;
 case S::ReadOldestIndex:case S::ReadOwnerIndex:case S::ReadNewestIndex:case S::ReadNewestNext:case S::ReadFreeIndex:case S::ReadFreeNext:case S::ReadNoteIndex:case S::ReadNoteNext:ra=0xd023;rn=1;break;
 case S::ReadIncoming:ra=0xd02a;rn=1;break;case S::ReadNote:ra=t.voiceAddress()+5;rn=1;break;
 case S::PublishInitial:case S::PublishOwnerIndex:case S::PublishOwnerCandidate:case S::PublishNewestIndex:case S::PublishNewestCandidate:case S::PublishFreeIndex:case S::PublishFreeCandidate:case S::PublishNoteIndex:case S::PublishNoteCandidate:wa=0xd023;wn=1;break;
 case S::PublishReuse:case S::PublishOwnerAllocation:case S::PublishNewestAllocation:case S::PublishPolyAllocation:case S::PublishNoteAllocation:case S::PublishRetrigger:wa=0xd02c;wn=1;break;default:break;}}
 static uint32_t read(Stage1Cpu&c,const T&t,T::Read x){uint32_t at=0;int reg=0;bool wide=false;switch(x){case T::Read::Mode:at=t.patchAddress()+0x80;reg=12;break;case T::Read::Held:at=t.partAddress()+14;reg=4;break;case T::Read::Unit:at=t.partAddress()+15;reg=5;break;case T::Read::Part:at=0xd028;reg=13;break;case T::Read::Count:at=0xd022;reg=10;break;case T::Read::Age:at=t.voiceAddress()+8;wide=true;break;case T::Read::VoicePart:at=t.voiceAddress()+4;reg=13;break;case T::Read::VoiceUnit:at=t.voiceAddress()+12;reg=5;break;case T::Read::Index:at=0xd023;reg=(t.stage()==S::ReadOldestIndex||t.stage()==S::ReadNewestIndex||t.stage()==S::ReadFreeIndex||t.stage()==S::ReadNoteIndex)?2:8;break;case T::Read::Incoming:at=0xd02a;reg=12;break;case T::Read::VoiceNote:at=t.voiceAddress()+5;reg=13;break;}if(wide){c.reg32(0)=int32_t(c.read32(int(at)));val(c,c.reg32(0));return uint32_t(c.reg32(0));}c.reg8(reg)=c.read8(int(at));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));}
 static void write(Stage1Cpu&c,const T&t,T::Write w,uint8_t v){const auto s=t.stage();const bool candidate=s==S::PublishInitial||s==S::PublishOwnerCandidate||s==S::PublishNewestCandidate||s==S::PublishFreeCandidate||s==S::PublishNoteCandidate;const bool useR4=s==S::PublishOwnerAllocation||s==S::PublishNewestAllocation;const int reg=candidate?2:useR4?12:8;const uint32_t at=w==T::Write::Index?0xd023:0xd02c;if(uint8_t(c.reg8(reg))!=v)throw std::logic_error("K voice selection write differs");val(c,c.reg8(reg));c.write8(int8_t(v),int(at));}
 T task_;uint32_t expectedReturn_=0;
};

class NativeVoicePartCopy final {
 using T=VoicePartCopyTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x72e2,0x7302}}};
 explicit NativeVoicePartCopy(uint8_t part):task_(part){}const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{constexpr uint32_t pc[]={0x72e2,0x72e4,0x72e8,0x72ea,0x72f0,0x72f6,0x72f8,0x72fa,0x72fc,0x72fe,0x7300};return complete()?0:pc[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto s=task_.stage();const auto at=boundary();const unsigned len=s==S::SetCount?4:s==S::SelectSource||s==S::SelectDestination?6:2;T::Credits cr{busCost(task_.source(),1,int(at+len-1)),busCost(task_.destination(),1,c.writeHistory()),5+busCost(c.getSP(),4,int(at+len-1))};const auto cost=task_.nextClocks(cr);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::NormalizePart:c.reg16(0)=uint16_t(c.reg16(0))&255;val(c,c.reg16(0));break;case S::SetCount:c.reg16(10)=16;val(c,c.reg16(10));break;
 case S::Multiply:c.reg32(0)=int32_t(unsigned(uint16_t(c.reg16(10)))*uint16_t(c.reg16(0)));c.clockI(20);break;
 case S::SelectSource:c.reg32(0)=int32_t(add(c,uint32_t(c.reg32(0)),0xbfaa));break;case S::SelectDestination:c.reg32(1)=0xbfba;val(c,c.reg32(1));break;
 case S::AdvanceDestination:{const int64_t old=c.reg32(1),v=old+1;val(c,int(v));if((~uint64_t(old)&uint64_t(v)&0x80000000u))c.ccr|=2;c.reg32(1)=int32_t(v);break;}
 case S::DecrementCount:{const int old=c.reg16(10),v=old-1;val(c,v);if((old&~v)&0x8000)c.ccr|=2;c.reg16(10)=int16_t(v);break;}
 case S::Return:{c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));break;}default:break;}
 const auto used=task_.advanceBefore(cost,cr,[&](){c.reg8(10)=c.read8(int(task_.source()));++c.reg32(0);val(c,c.reg8(10));c.clockI(2);return uint8_t(c.reg8(10));},[&](uint8_t v){if(uint8_t(c.reg8(10))!=v)throw std::logic_error("K part copy projection differs");c.write8(int8_t(v),int(task_.destination()));val(c,c.reg8(10));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K part copy phase clocks differ");if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t add(Stage1Cpu&c,uint32_t a,uint32_t b){const auto r=a+b,carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x80000000)?8:0)|((carry&0x80000000)?1:0)|((carry&0x08000000)?32:0)|((over&0x80000000)?2:0));return r;}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_;
};

class NativeVoiceVelocity final {
 using T=VoiceVelocityTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x7260,0x72b8}}};
 NativeVoiceVelocity(uint8_t velocity,uint8_t exponent,uint8_t curve):task_(velocity,exponent,curve){}const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x7260,0x7262,0x7264,0x7266,0x7268,0x726a,0x726c,0x726e,0x7270,0x7272,0x7274,0x7276,0x7278,0x727a,0x727c,0x727e,0x7280,0x7282,0x7284,0x7286,0x7288,0x728a,0x728c,0x728e,0x7290,0x7292,0x7294,0x7296,0x7298,0x729a,0x729c,0x72a2,0x72a4,0x72a6,0x72a8,0x72aa,0x72ac,0x72ae,0x72b0,0x72b2,0x72b4,0x72b6};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);const auto s=task_.stage();const auto at=boundary();const unsigned len=s==S::LocateCurve?6:2;T::Credits cr{busCost(task_.tableAddress(),1,int(at+len-1)),5+busCost(c.getSP(),4,int(at+len-1))};const auto cost=task_.nextClocks(cr);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){
 case S::CopyInput:c.reg8(8)=c.reg8(1);val(c,c.reg8(8));break;
 case S::DecrementExponent:dec(c,c.reg8(2));break;
 case S::CopyPower:case S::CopyInverse:case S::CopySquare:c.reg8(9)=c.reg8(1);val(c,c.reg8(9));break;
 case S::MultiplyPower:c.reg16(1)=int16_t(unsigned(uint8_t(c.reg8(8)))*uint8_t(c.reg8(9)));c.clockI(12);break;
 case S::MultiplySquare:case S::MultiplyCross:c.reg16(1)=int16_t(unsigned(uint8_t(c.reg8(1)))*uint8_t(c.reg8(9)));c.clockI(12);break;
 case S::ShiftPower:case S::ShiftSquare:case S::ShiftCross:{uint32_t v=uint16_t(c.reg16(1));c.ccr=uint8_t((c.ccr&~1)|((v&0x8000)?1:0));v<<=1;val(c,int32_t(v<<16));c.reg16(1)=int16_t(v);break;}
 case S::XorInverse:c.reg8(9)^=0x7f;val(c,c.reg8(9));break;case S::TestCurve:val(c,c.reg8(10));break;
 case S::DecrementCurve1:case S::DecrementCurve2:case S::DecrementCurve3:case S::DecrementCurve4:dec(c,c.reg8(10));break;
 case S::CopyInverseOutput:c.reg8(1)=c.reg8(9);val(c,c.reg8(1));break;
 case S::NormalizeCurveWord:c.reg16(2)=int16_t(uint16_t(c.reg16(2))&255);val(c,c.reg16(2));break;
 case S::NormalizeCurveLong:c.reg32(2)=uint16_t(c.reg16(2));val(c,c.reg32(2));break;
 case S::LocateCurve:c.reg32(2)=int32_t(add(c,uint32_t(c.reg32(2)),0xc06c));break;
 case S::TestParity:c.ccr=uint8_t((c.ccr&~4)|((uint8_t(c.reg8(10))&1)?0:4));break;
 case S::CompareEven:case S::CompareOdd:subtract(c,uint8_t(c.reg8(8)),uint8_t(c.reg8(10)));break;
 case S::SetOff:c.reg8(1)=int8_t(0x80);val(c,c.reg8(1));break;
 case S::ReturnOriginal:case S::ReturnInverse:case S::ReturnSquare:case S::ReturnCross:case S::ReturnEven:case S::ReturnOdd:case S::ReturnOff:{c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));break;}default:break;}
 const auto used=task_.advanceBefore(cost,cr,[&](){c.reg8(10)=c.read8(int(task_.tableAddress()));val(c,c.reg8(10));return uint8_t(c.reg8(10));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K velocity curve phase clocks differ");if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static void dec(Stage1Cpu&c,int8_t&v){const int old=v,n=old-1;val(c,n);if((old&~n)&0x80)c.ccr|=2;v=int8_t(n);}
 static uint32_t add(Stage1Cpu&c,uint32_t a,uint32_t b){const auto r=a+b,carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x80000000)?8:0)|((carry&0x80000000)?1:0)|((carry&0x08000000)?32:0)|((over&0x80000000)?2:0));return r;}
 static void subtract(Stage1Cpu&c,unsigned a,unsigned b){const auto r=a-b,carry=(~a&b)|(~a&r)|(b&r),over=(a&~b&~r)|(~a&b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!(r&255)?4:0)|((r&128)?8:0)|((carry&128)?1:0)|((carry&8)?32:0)|((over&128)?2:0));}
 static unsigned busCost(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_;
};

namespace voice_dsp_projection {
inline void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
inline unsigned bus(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
inline void call(Stage1Cpu&c,uint32_t target,uint32_t ret){auto sp=c.getSP()-4;c.write32(int32_t(ret),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);c.clockI(2);c.pc=c.makepc(int(target));}
inline void ret(Stage1Cpu&c){c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}
inline uint16_t add16(Stage1Cpu&c,uint16_t a,uint16_t b){const uint32_t r=uint16_t(a+b),carry=(a&b)|(a&~r)|(b&~r),over=(a&b&~r)|(~a&~b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x8000)?8:0)|((carry&0x8000)?1:0)|((carry&0x800)?32:0)|((over&0x8000)?2:0));return uint16_t(r);}
inline bool blocked(const Stage1Cpu&c){return c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80));}
}
class NativeVoiceDspSelect final {
 using T=VoiceDspSelectTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5bb8,0x5bde}}};
 const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}
 bool resumeDsp(Stage1Cpu&c){return uint32_t(c.getPC())==0x5bdc&&task_.resumeDsp();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x5bb8,0x5bbe,0x5bc0,0x5bc4,0x5bc6,0x5bcc,0x5bd0,0x5bd2,0x5bd8,0,0x5bdc};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_dsp_projection;if(blocked(c)||complete()||awaitingDsp()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();unsigned len=(s==S::ReadSlot||s==S::WriteFirst||s==S::WriteSecond)?6:(s==S::SetFirst||s==S::SetSecond||s==S::DispatchInvalidate)?4:2;const bool callPhase=s==S::DispatchInvalidate;T::Write field=s==S::WriteFirst?T::Write::First:T::Write::Second;unsigned out=field==T::Write::First?0xd024:0xd026;T::Credits cr{3,bus(out,2,c.writeHistory()),callPhase?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::NormalizeSlot:c.reg16(1)=uint16_t(c.reg16(1))&255;val(c,c.reg16(1));break;case S::SetFirst:c.reg16(0)=0x51;val(c,c.reg16(0));break;case S::SetSecond:c.reg16(0)=0x1e;val(c,c.reg16(0));break;case S::MultiplyFirst:case S::MultiplySecond:c.reg32(0)=int32_t(unsigned(uint16_t(c.reg16(1)))*uint16_t(c.reg16(0)));c.clockI(20);break;case S::DispatchInvalidate:call(c,0x1640,at+len);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr,[&](){c.reg8(9)=c.read8(0xd023);val(c,c.reg8(9));return uint8_t(c.reg8(9));},[&](T::Write w,uint16_t v){const auto dest=w==T::Write::First?0xd024:0xd026;if(uint16_t(c.reg16(0))!=v)throw std::logic_error("K voice DSP select projection");c.write16(int16_t(v),dest);val(c,c.reg16(0));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice DSP select clocks");if(!awaitingDsp()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_{};
};
class NativeVoiceDspRead final {
 using T=VoiceDspReadTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x5bde,0x5bec}}};
 explicit NativeVoiceDspRead(uint16_t command):task_(command){}const T&task()const{return task_;}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}
 bool resumeDsp(Stage1Cpu&c){return uint32_t(c.getPC())==0x5bea&&task_.resumeDsp();}
 uint32_t boundary()const{constexpr uint32_t p[]={0x5bde,0x5be4,0x5be6,0,0x5bea};return complete()?0:p[unsigned(task_.stage())];}
 bool step(Stage1Cpu&c){using namespace voice_dsp_projection;if(blocked(c)||complete()||awaitingDsp()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto at=boundary();unsigned len=s==S::ReadBase?6:s==S::DispatchRead?4:2;T::Credits cr{bus(0xd026,2,int(at+len-1)),s==S::DispatchRead?8+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(at+len-1))};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(s){case S::AddBase:c.reg16(4)=int16_t(add16(c,uint16_t(c.reg16(4)),uint16_t(c.reg16(0))));break;case S::DispatchRead:call(c,0x16fa,at+len);break;case S::Return:ret(c);break;default:break;}auto used=task_.advanceBefore(cost,cr,[&](){c.reg16(0)=int16_t(c.read16(0xd026));val(c,c.reg16(0));return uint16_t(c.reg16(0));});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice DSP read clocks");if(!awaitingDsp()&&!complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};

class NativeVoiceActivation final {
 using T=VoiceActivationTask;using S=T::Stage;
 struct Site{uint32_t pc;unsigned len;};inline static constexpr Site sites[]={{0x71a0,4},{0x71a4,4},{0x71a8,10},{0x71b2,6},{0x71b8,8},{0x71c0,8},{0x71c8,2},{0x71ca,8},{0x71d2,2},{0x71d4,8},{0x71dc,8},{0x71e4,8},{0x71ec,6},{0x71f2,8},{0x71fa,8},{0x7202,2},{0x7204,2},{0x7206,8},{0x720e,6},{0x7214,10},{0x721e,6},{0x7224,8},{0x722c,4},{0x7230,4},{0x7234,4},{0x7238,4},{0x723c,2},{0x723e,8},{0x7246,2},{0x7248,2},{0x724a,2},{0x724c,8},{0x7254,2},{0x7256,8},{0x725e,2}};
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,1> CodeRanges{{{0x71a0,0x7260}}};
 explicit NativeVoiceActivation(uint32_t patch):task_(patch){}const T&task()const{return task_;}bool complete()const{return task_.complete();}
 uint32_t boundary()const{return complete()?0:task_.child()!=T::Child::None?expectedReturn_:sites[unsigned(task_.stage())].pc;}
 bool resumeChild(Stage1Cpu&c){auto child=task_.child();if(child==T::Child::None||uint32_t(c.getPC())!=expectedReturn_)return false;uint32_t value=0;if(child==T::Child::Voice)value=uint32_t(c.reg32(3));else if(child==T::Child::Part)value=uint32_t(c.reg32(0));else if(child==T::Child::Velocity)value=uint8_t(c.reg8(1));else if(child==T::Child::WriteDsp)value=uint16_t(c.reg16(4));return task_.resumeChild(value);}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80))||complete()||task_.child()!=T::Child::None||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto s=task_.stage();auto site=sites[unsigned(s)];uint32_t ra=0,wa=0;unsigned rn=0,wn=0;access(task_,ra,rn,wa,wn);auto rc=rn?bus(ra,rn,int(site.pc+site.len-1)):0,wc=wn?bus(wa,wn,c.writeHistory()):0;bool call=isDispatch(s);unsigned sc=call?(site.len==2?3:8)+bus(c.getSP()-4,4,c.writeHistory()):5+bus(c.getSP(),4,int(site.pc+site.len-1));T::Credits cr{rc,wc,sc};auto cost=task_.nextClocks(cr);auto begin=c.cycles;c.fetchClocks(site.len);c.pc=c.makepc(int(site.pc+1));if(call){expectedReturn_=site.pc+site.len;auto sp=c.getSP()-4;c.write32(int32_t(expectedReturn_),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(site.len==4)c.clockI(2);}else if(s==S::Return){c.irqDepth(c.irqDepth()-1);c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}else project(c,s);auto used=task_.advanceBefore(cost,cr,[&](T::Read r){return read(c,task_,r);},[&](T::Write w,uint32_t v){write(c,task_,w,v);});if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("K voice activation phase clocks differ");if(task_.child()!=T::Child::None){uint32_t dest=0;switch(task_.child()){case T::Child::Voice:dest=0x72ba;break;case T::Child::Part:dest=0x72ce;break;case T::Child::Velocity:dest=0x7260;break;case T::Child::Copy:dest=0x72e2;break;case T::Child::SelectDsp:dest=0x5bb8;break;case T::Child::WriteDsp:dest=0x5bde;break;default:break;}c.pc=c.makepc(int(dest));}else if(!complete())c.pc=c.makepc(int(boundary()));return true;}
private:
 static bool isDispatch(S s){return s==S::DispatchVoice||s==S::DispatchPart||s==S::DispatchVelocity||s==S::DispatchCopy||s==S::DispatchSelectDsp||s==S::DispatchWriteDsp;}
 static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t arithmetic(Stage1Cpu&c,unsigned bits,uint32_t a,uint32_t b,bool sub){const uint32_t mask=bits==32?0xffffffffu:((1u<<bits)-1),sign=1u<<(bits-1),half=sign>>4;const auto r=(sub?a-b:a+b)&mask,carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r)),ov=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((ov&sign)?2:0));return r;}
 static unsigned bus(uint32_t at,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 static void project(Stage1Cpu&c,S s){switch(s){case S::AddTranspose:c.reg8(9)=arithmetic(c,8,c.reg8(9),c.reg8(10),false);break;
case S::ClearHeld:c.reg8(9)=arithmetic(c,8,c.reg8(9),c.reg8(9),true);break;
case S::AdvanceExponent:c.reg8(2)=arithmetic(c,8,c.reg8(2),2u,false);break;
case S::SetAge:c.reg32(0)=2147549183u;val(c,c.reg32(0));break;
case S::BindCommand:c.reg16(4)=32863u;val(c,c.reg16(4));break;
case S::BiasCommand:c.reg8(4)=arithmetic(c,8,c.reg8(4),60u,false);break;
case S::SubtractCommand:c.reg8(12)=arithmetic(c,8,c.reg8(12),c.reg8(4),true);break;
case S::NegateCommand:c.reg8(12)=arithmetic(c,8,0,c.reg8(12),true);break;
case S::ClearFlag:c.reg8(8)=0u;val(c,c.reg8(8));break;
 default:break;}}
 static void access(const T&t,uint32_t&ra,unsigned&rn,uint32_t&wa,unsigned&wn){switch(t.stage()){
 case S::ReadNote:ra=0xd02a;rn=1;break;case S::ReadTranspose:ra=t.partAddress()+10;rn=1;break;case S::ReadUnit:ra=t.partAddress()+15;rn=1;break;case S::ReadVelocity:ra=0xd02b;rn=1;break;case S::ReadCurve:ra=t.partAddress()+13;rn=1;break;case S::ReadExponent:ra=t.patch()+0xc3;rn=1;break;case S::ReadPart:ra=0xd028;rn=1;break;case S::ReadActualNote:ra=t.voice()+13;rn=1;break;
 case S::WritePatch:wa=t.voice();wn=4;break;case S::WriteNote:wa=t.voice()+5;wn=1;break;case S::WriteTransposed:wa=t.voice()+13;wn=1;break;case S::WriteHeld:wa=t.partAddress()+14;wn=1;break;case S::WriteUnit:wa=t.voice()+12;wn=1;break;case S::WriteVelocity:wa=t.voice()+6;wn=1;break;case S::WriteAge:wa=t.voice()+8;wn=4;break;case S::WritePart:wa=t.voice()+4;wn=1;break;case S::WriteDelta:wa=t.voice()+7;wn=1;break;case S::WriteFlag:wa=t.voice()+17;wn=1;break;default:break;}}
 static uint32_t read(Stage1Cpu&c,const T&t,T::Read r){uint32_t at=0;int reg=0;switch(r){case T::Read::Note:at=0xd02a;reg=9;break;case T::Read::Transpose:at=t.partAddress()+10;reg=10;break;case T::Read::Unit:at=t.partAddress()+15;reg=9;break;case T::Read::Velocity:at=0xd02b;reg=1;break;case T::Read::Curve:at=t.partAddress()+13;reg=10;break;case T::Read::Exponent:at=t.patch()+0xc3;reg=2;break;case T::Read::Part:at=0xd028;reg=8;break;case T::Read::ActualNote:at=t.voice()+13;reg=12;break;}c.reg8(reg)=c.read8(int(at));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));}
 static void write(Stage1Cpu&c,const T&t,T::Write w,uint32_t v){uint32_t at=0;int reg=0;bool wide=false;switch(w){case T::Write::Patch:at=t.voice();reg=6;wide=true;break;case T::Write::Note:at=t.voice()+5;reg=9;break;case T::Write::Transposed:at=t.voice()+13;reg=9;break;case T::Write::Held:at=t.partAddress()+14;reg=9;break;case T::Write::Unit:at=t.voice()+12;reg=9;break;case T::Write::Velocity:at=t.voice()+6;reg=1;break;case T::Write::Age:at=t.voice()+8;reg=0;wide=true;break;case T::Write::Part:at=t.voice()+4;reg=8;break;case T::Write::Delta:at=t.voice()+7;reg=12;break;case T::Write::Flag:at=t.voice()+17;reg=8;break;}if(wide){if(uint32_t(c.reg32(reg))!=v)throw std::logic_error("K voice activation long projection");c.write32(int32_t(v),int(at));val(c,c.reg32(reg));}else{if(uint8_t(c.reg8(reg))!=uint8_t(v))throw std::logic_error("K voice activation byte projection");c.write8(int8_t(v),int(at));val(c,c.reg8(reg));}}
 T task_;uint32_t expectedReturn_=0;
};

// Fixed-capacity owner for all closed K 6fde children. NativeDspTransfer is an
// explicit external child at 1640/16fa; it is never interpreted here.
class NativeVoiceAllocation final {
 using Selection=NativeVoiceSelection;using Activation=NativeVoiceActivation;
 using VAddr=NativeVoiceAddress;using PAddr=NativeVoicePartAddress;using Curve=NativeVoiceVelocity;using Copy=NativeVoicePartCopy;using DSelect=NativeVoiceDspSelect;using DRead=NativeVoiceDspRead;
 using Frame=std::variant<std::monostate,Selection,Activation,VAddr,PAddr,Curve,Copy,DSelect,DRead>;
public:
 struct CodeRange{uint32_t begin,end;};inline static constexpr std::array<CodeRange,3> CodeRanges{{{0x5bb8,0x5bec},{0x6fde,0x72b8},{0x72ba,0x7302}}};
 static bool candidate(uint32_t pc){return (pc>=0x5bb8&&pc<0x5bec)||(pc>=0x6fde&&pc<0x72b8)||(pc>=0x72ba&&pc<0x7302);}
 bool active()const{return depth_!=0;}unsigned depth()const{return depth_;}
 void reset(){for(auto&f:frames_)f.template emplace<std::monostate>();depth_=0;}
 bool step(Stage1Cpu&c){if(voice_dsp_projection::blocked(c))return false;const auto pc=uint32_t(c.getPC());
  if(depth_){auto&top=frames_[depth_-1];if(auto*p=std::get_if<DSelect>(&top)){if(p->awaitingDsp()){if(pc==0x5bdc)p->resumeDsp(c);else if(!entry(pc))return false;}}
   else if(auto*p=std::get_if<DRead>(&top)){if(p->awaitingDsp()){if(pc==0x5bea)p->resumeDsp(c);else if(!entry(pc))return false;}}
  }
  if(!depth_||!atTop(pc)){if(!entry(pc)){if(candidate(pc)){char detail[96];std::snprintf(detail,sizeof(detail),"Unowned K voice allocation boundary PC=0x%06x depth=%u",pc,depth_);throw std::logic_error(detail);}return false;}push(c,pc);}
  auto&frame=frames_[depth_-1];bool stepped=false,done=false;uint32_t patch=0;bool selectionDone=false;
  std::visit([&](auto&job){using J=std::decay_t<decltype(job)>;if constexpr(!std::is_same_v<J,std::monostate>){stepped=job.step(c);done=job.complete();if constexpr(std::is_same_v<J,Selection>){selectionDone=done;if(done)patch=job.task().patchAddress();}}},frame);
  if(!stepped)return false;
  if(selectionDone){if(uint32_t(c.getPC())!=0x71a0)throw std::logic_error("K voice selection continuation differs");frame.template emplace<Activation>(patch);return true;}
  if(done){frame.template emplace<std::monostate>();--depth_;if(depth_){auto&parent=frames_[depth_-1];if(auto*p=std::get_if<Selection>(&parent)){if(p->task().child()!=VoiceSelectionTask::Child::None)p->resumeChild(c);}else if(auto*p=std::get_if<Activation>(&parent)){if(p->task().child()!=VoiceActivationTask::Child::None)p->resumeChild(c);}}}
  return true;
 }
private:
 static bool entry(uint32_t pc){return pc==0x6fde||pc==0x71a0||pc==0x72ba||pc==0x72ce||pc==0x72d4||pc==0x7260||pc==0x72e2||pc==0x5bb8||pc==0x5bde;}
 bool atTop(uint32_t pc)const{bool yes=false;std::visit([&](const auto&job){using J=std::decay_t<decltype(job)>;if constexpr(!std::is_same_v<J,std::monostate>)yes=!job.complete()&&job.boundary()==pc;},frames_[depth_-1]);return yes;}
 void push(Stage1Cpu&c,uint32_t pc){if(depth_==frames_.size())throw std::logic_error("K voice allocation continuation capacity");auto&f=frames_[depth_++];switch(pc){
 case 0x6fde:f.template emplace<Selection>(uint32_t(c.reg32(6)));break;case 0x71a0:f.template emplace<Activation>(uint32_t(c.reg32(6)));break;
 case 0x72ba:f.template emplace<VAddr>();break;case 0x72ce:f.template emplace<PAddr>();break;case 0x72d4:f.template emplace<PAddr>(uint8_t(c.reg8(8)));break;case 0x7260:f.template emplace<Curve>(uint8_t(c.reg8(1)),uint8_t(c.reg8(2)),uint8_t(c.reg8(10)));break;
 case 0x72e2:f.template emplace<Copy>(uint8_t(c.reg8(8)));break;case 0x5bb8:f.template emplace<DSelect>();break;case 0x5bde:f.template emplace<DRead>(uint16_t(c.reg16(4)));break;default:throw std::logic_error("K voice entry");}}
 std::array<Frame,16>frames_{};unsigned depth_=0;
};

}
