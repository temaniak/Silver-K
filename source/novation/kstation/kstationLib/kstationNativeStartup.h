#pragma once
#include "kstationResetImageTask.h"
#include "kstationHdiWait.h"
#include <optional>
namespace kstation {
// Temporary observation adapter for the image/overlay subtree only. Native
// image arithmetic is owned by ResetImageTask. This adapter has no fallback;
// display/update children are explicit, independently owned service requests.
class NativeStartup final {
 using T=ResetImageTask;using S=T::Stage;
public:
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,3>CodeRanges{{{0x100,0x104},{0x1bc,0x240},{0x2fe,0x42c}}};
 void reset(){task_.reset();steps_=0;}bool active()const{return bool(task_);}uint64_t steps()const{return steps_;}
 T::Child child()const{return task_?task_->child():T::Child::None;}
 const T*task()const{return task_?&*task_:nullptr;}
 static bool candidate(uint32_t pc){return (pc>=0x2fe&&pc<0x42c)||(pc>=0x1bc&&pc<0x240)||pc==0x100||((pc&0xffffff)>=0xfffd10&&(pc&0xffffff)<0xfffd1c);}
 bool step(Stage1Cpu&c){if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&128)))return false;const auto pc=uint32_t(c.getPC());
  if(!task_){if(pc==0x2fe)task_.emplace(T::Entry::Verify);else if(pc==0x382)task_.emplace(T::Entry::RomCopy);else if((pc&0xffffff)==0xfffd10)task_.emplace(T::Entry::Relocated);else return false;}
  auto&t=*task_;if(t.awaiting()){if(pc!=boundary(t))return false;t.resumeChild();}if(pc!=boundary(t))return false;
  if(c.pending_irqs)c.read8(0xfffff2);
  auto st=t.stage();auto length=bytes(t);auto cr=credits(t,c,length);auto cost=t.nextClocks(cr);const auto begin=c.cycles;
  if(pc>=0xfffd10){for(unsigned n=0;n<length;++n){int history=c.readHistory();c.clockMem(int((pc+n)&0xffffff),history);c.histories(history,c.writeHistory());}}else c.fetchClocks(length);
  c.pc=c.makepc(int(pc+1));project(t,c);
  t.advanceBefore(cost,cr,[&](T::Read f,unsigned i){return read(t,c,f,i);},[&](T::Write f,unsigned i,uint32_t v){write(t,c,f,i,v);});
  if(c.cycles-begin!=cost)throw std::logic_error("Native K startup phase cost differs");
  if(t.complete())task_.reset();else if(!t.awaiting())c.pc=c.makepc(int(boundary(t)));++steps_;return true;
 }
 static uint32_t boundary(const T&t){switch(t.stage()){
 case S::Begin:return 0x2fe;case S::Prepare:return 0x300;case S::BindSignature:return 0x302;case S::BindStoredSignature:return 0x308;case S::CountSignature:return 0x30e;case S::ReadSignature:return 0x312;case S::ReadStoredSignature:return 0x314;case S::CompareSignature:return 0x316;case S::SelectSignature:return 0x318;case S::TickSignature:return 0x31a;case S::RepeatSignature:return 0x31c;case S::AdmitSignature:return 0x31e;case S::ReadVersion:return 0x320;case S::ReadStoredVersion:return 0x326;case S::CompareVersion:return 0x32c;case S::SelectVersion:return 0x32e;case S::BindImage:return 0x330;
 case S::ReadSize:return t.sourceBase()?0x336:0x388;case S::SelectSize:return t.sourceBase()?0x340:0x392;case S::MarkEmpty:return t.sourceBase()?0x342:0x394;case S::SelectEmpty:return t.sourceBase()?0x344:0x396;
 case S::CountWords:return t.warmCopy()?0x1e2:t.sourceBase()?0x346:0x398;case S::ClearWordUpper:return t.warmCopy()?0x1e4:t.sourceBase()?0x348:0x39a;case S::ClearSum:return t.warmCopy()?0x1e6:t.sourceBase()?0x34a:0x39c;
 case S::ReadWord:return t.warmCopy()?0x1e8:t.sourceBase()?0x34c:0x39e;case S::WriteWord:return t.warmCopy()?0x1ea:t.sourceBase()?0x34e:0x3a0;case S::AddWord:return t.warmCopy()?0x1f2:t.sourceBase()?0x356:0x3a8;case S::TickWords:return t.warmCopy()?0x1f4:t.sourceBase()?0x358:0x3aa;case S::RepeatWords:return t.warmCopy()?0x1f6:t.sourceBase()?0x35a:0x3ac;
 case S::ReadChecksum:return t.warmCopy()?0x1f8:t.sourceBase()?0x35c:0x3ae;case S::WriteChecksum:return t.warmCopy()?0x1fc:t.sourceBase()?0x360:0x3b2;case S::CountTail:return t.warmCopy()?0x206:t.sourceBase()?0x36a:0x3bc;case S::ClearTailValue:return t.warmCopy()?0x208:t.sourceBase()?0x36c:0x3be;case S::WriteTail:return t.warmCopy()?0x20a:t.sourceBase()?0x36e:0x3c0;case S::AdvanceTail:return t.warmCopy()?0x212:t.sourceBase()?0x376:0x3c8;case S::TickTail:return t.warmCopy()?0x214:t.sourceBase()?0x378:0x3ca;case S::RepeatTail:return t.warmCopy()?0x216:t.sourceBase()?0x37a:0x3cc;case S::CompareChecksum:return t.warmCopy()?0x218:t.sourceBase()?0x37c:0x3ce;case S::SelectChecksum:return 0x37e;
 case S::BindRom:return 0x382;case S::ReadMarker:return 0x3d0;case S::CompareMarker55:return 0x3d6;case S::SelectMarker55:return 0x3d8;case S::CompareMarkerAA:return 0x3da;case S::SelectMarkerAA:return 0x3dc;case S::BindError:return 0x3de;case S::PublishError:return 0x3e4;case S::RetainDelay:return 0x3e8;case S::CountSettling:return 0x3ea;case S::RetainInner:return 0x3ee;case S::CountDelay:return 0x3f0;case S::TickDelay:return 0x3f4;case S::RepeatDelay:return 0x3f6;case S::ReleaseInner:return 0x3f8;case S::TickSettling:return 0x3fa;case S::RepeatSettling:return 0x3fc;case S::ReleaseDelay:return 0x3fe;
 case S::Mask:return 0x400;case S::BindTrampoline:return 0x402;case S::BindTrampolineTarget:return 0x408;case S::CountTrampoline:return 0x40e;case S::ReadTrampoline:return 0x412;case S::WriteTrampoline:return 0x414;case S::AdvanceTrampoline:return 0x416;case S::TickTrampoline:return 0x418;case S::RepeatTrampoline:return 0x41a;case S::EnterTrampoline:return 0x41c;case S::PrepareOverlay:return 0xfffffd10;case S::PublishOverlay:return 0xfffffd12;case S::PrepareAccessState:return 0xfffffd14;case S::PublishAccessState:return 0xfffffd16;case S::Restart:return 0xfffffd18;case S::EnterWarm:return 0x100;
 case S::ReadWarmMarker:return 0x1bc;case S::CompareWarm55:return 0x1c2;case S::SelectWarm55:return 0x1c4;case S::CompareWarmAA:return 0x1c6;case S::SelectWarmAA:return 0x1c8;case S::ReturnWarm:return 0x1ca;case S::BindWarmImage:return 0x1cc;case S::ReadWarmSize:return 0x1d2;case S::SelectWarmSize:return 0x1dc;case S::MarkWarmEmpty:return 0x1de;case S::SelectWarmEmpty:return 0x1e0;
 case S::BindUpdate:return 0x21a;case S::PublishUpdate:return 0x220;case S::BindUpdateTarget:return 0x224;case S::BindUpdateSource:return 0x22a;case S::UpdateFirmware:return 0x230;case S::BindDone:return 0x234;case S::PublishDone:return 0x23a;case S::HoldUpdate:return 0x23e;case S::Complete:return 0;}return 0;}
private:
 static unsigned bytes(const T&t){switch(t.stage()){
 case S::ReadSize:case S::ReadWarmSize:case S::WriteChecksum:return 10;
 case S::WriteWord:case S::WriteTail:return 8;
 case S::BindSignature:case S::BindStoredSignature:case S::ReadVersion:case S::ReadStoredVersion:case S::BindImage:case S::BindRom:case S::ReadMarker:case S::BindError:case S::BindTrampoline:case S::BindTrampolineTarget:case S::ReadWarmMarker:case S::BindWarmImage:case S::BindUpdate:case S::BindUpdateTarget:case S::BindUpdateSource:case S::BindDone:return 6;
 case S::SelectChecksum:case S::CountSignature:case S::ReadChecksum:case S::CountTrampoline:case S::EnterTrampoline:case S::Restart:case S::EnterWarm:case S::CountSettling:case S::CountDelay:case S::PublishError:case S::PublishUpdate:case S::UpdateFirmware:case S::PublishDone:return 4;
 default:return 2;}}
 static unsigned bus(uint32_t a,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){auto at=(a+i)&0xffffff;if(at<0x400000||(at>=0xfffd10&&at<0xffffa0)){if((last&1)||at!=uint32_t(last|1))cost+=at<0x400000?3:1;last=int(at);}else{cost+=at>=0xffffa0?1:3;last=-1;}}return cost;}
 static T::Credits credits(const T&t,const Stage1Cpu&c,unsigned length){T::Credits cr;int last=int(c.getPC()+length-1);cr.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.ret=5+bus(c.getSP(),4,last);cr.push=5+bus(c.getSP()-2,2,c.writeHistory());cr.pop=5+bus(c.getSP(),2,last);switch(t.stage()){
 case S::ReadSignature:cr.read=bus(0x110+t.index(),1,last);break;case S::ReadStoredSignature:cr.read=bus(0x210110+t.targetIndex(),1,last);break;case S::ReadVersion:cr.read=bus(0x138,2,last);break;case S::ReadStoredVersion:cr.read=bus(0x210138,2,last);break;case S::ReadSize:case S::ReadWarmSize:cr.read=bus(t.sourceBase()+0x104,4,last);break;case S::ReadWord:cr.read=bus(t.sourceBase()+t.index(),2,last);break;case S::ReadChecksum:cr.read=bus(t.sourceBase()+t.index(),4,last);break;case S::ReadMarker:case S::ReadWarmMarker:cr.read=bus(0xfffe3d,1,last);break;case S::ReadTrampoline:cr.read=bus(0x420+t.index(),1,last);break;
 case S::WriteWord:cr.write=bus(t.targetBase()+t.index()-2,2,c.writeHistory());break;case S::WriteChecksum:cr.write=bus(t.targetBase()+t.index()-4,4,c.writeHistory());break;case S::WriteTail:cr.write=bus(t.targetBase()+t.index(),2,c.writeHistory());break;case S::WriteTrampoline:cr.write=bus(0xfffd10+t.targetIndex(),1,c.writeHistory());break;case S::PublishOverlay:cr.write=1;break;case S::PublishAccessState:cr.write=1;break;default:break;}return cr;}
 static void value(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
 static uint32_t arithmetic(Stage1Cpu&c,uint32_t a,uint32_t b,unsigned bits,bool sub){uint32_t mask=bits==32?~uint32_t(0):(1u<<bits)-1,sign=1u<<(bits-1),half=1u<<(bits-5);a&=mask;b&=mask;auto r=(sub?a-b:a+b)&mask;auto carry=sub?((~a&b)|(~a&r)|(b&r)):((a&b)|(a&~r)|(b&~r));auto over=sub?((a&~b&~r)|(~a&b&r)):((a&b&~r)|(~a&~b&r));c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&sign)?8:0)|((carry&sign)?1:0)|((carry&half)?32:0)|((over&sign)?2:0));return r;}
 static void decrement(Stage1Cpu&c,unsigned r,unsigned bits){int old=bits==8?c.reg8(r):bits==16?c.reg16(r):c.reg32(r);int64_t v=int64_t(old)-1;value(c,int(v));if((uint32_t(old)&~uint32_t(v))&(bits==8?0x80u:bits==16?0x8000u:0xff000000u))c.ccr|=2;if(bits==8)c.reg8(r)=int8_t(v);else if(bits==16)c.reg16(r)=int16_t(v);else c.reg32(r)=int32_t(v);}
 static void call(Stage1Cpu&c,unsigned target,unsigned returned){c.write32(returned,int(c.getSP()-4));c.setSP(c.getSP()-4);c.irqDepth(c.irqDepth()+1);c.clockI(2);c.pc=c.makepc(target);}
 static void project(const T&t,Stage1Cpu&c){switch(t.stage()){
 case S::BindSignature:c.reg32(0)=0x110;value(c,c.reg32(0));break;case S::BindStoredSignature:c.reg32(1)=0x210110;value(c,c.reg32(1));break;case S::CountSignature:c.reg16(10)=40;value(c,40);break;
 case S::CompareSignature:(void)arithmetic(c,uint8_t(c.reg8(10)),uint8_t(c.reg8(2)),8,true);break;case S::TickSignature:case S::TickTrampoline:decrement(c,10,16);break;
 case S::CompareVersion:(void)arithmetic(c,uint16_t(c.reg16(1)),uint16_t(c.reg16(0)),16,true);break;
 case S::BindImage:c.reg32(1)=0x210000;value(c,c.reg32(1));break;case S::BindRom:c.reg32(1)=0;value(c,0);break;
 case S::MarkEmpty:case S::MarkWarmEmpty:c.reg8(8)=1;value(c,1);break;
 case S::CountWords:{auto old=uint32_t(c.reg32(0));c.reg32(0)=int32_t(old>>1);c.ccr=uint8_t((c.ccr&~1)|(old&1));value(c,c.reg32(0));break;}
 case S::ClearWordUpper:c.reg16(10)=int16_t(arithmetic(c,uint16_t(c.reg16(10)),uint16_t(c.reg16(10)),16,true));break;case S::ClearSum:c.reg32(3)=int32_t(arithmetic(c,c.reg32(3),c.reg32(3),32,true));break;
 case S::AddWord:c.reg32(3)=int32_t(arithmetic(c,c.reg32(3),c.reg32(2),32,false));break;case S::TickWords:decrement(c,0,32);break;case S::CountTail:c.reg8(8)=int8_t(128);value(c,-128);break;case S::ClearTailValue:c.reg16(8)=int16_t(arithmetic(c,uint16_t(c.reg16(8)),uint16_t(c.reg16(8)),16,true));break;
 case S::AdvanceTail:c.reg32(1)=int32_t(uint32_t(c.reg32(1))+2);break;case S::TickTail:decrement(c,8,8);break;case S::CompareChecksum:(void)arithmetic(c,c.reg32(3),c.reg32(2),32,true);break;
 case S::CompareMarker55:case S::CompareWarm55:(void)arithmetic(c,uint8_t(c.reg8(8)),0x55,8,true);break;case S::CompareMarkerAA:case S::CompareWarmAA:(void)arithmetic(c,uint8_t(c.reg8(8)),0xaa,8,true);break;
 case S::BindError:c.reg32(6)=0xaab7;value(c,c.reg32(6));break;case S::PublishError:call(c,0x239e,0x3e8);break;
 case S::RetainDelay:case S::RetainInner:c.setSP(c.getSP()-2);c.write16(c.reg16(0),int(c.getSP()));c.clockI(2);value(c,c.reg16(0));break;
 case S::CountSettling:c.reg16(0)=500;value(c,500);break;case S::CountDelay:c.reg16(0)=2663;value(c,2663);break;case S::TickDelay:case S::TickSettling:decrement(c,0,16);break;
 case S::ReleaseInner:case S::ReleaseDelay:c.reg16(0)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);value(c,c.reg16(0));break;
 case S::Mask:c.ccr|=0xc0;break;case S::BindTrampoline:c.reg32(0)=0x420;value(c,c.reg32(0));break;case S::BindTrampolineTarget:c.reg32(1)=0xfffd10;value(c,c.reg32(1));break;case S::CountTrampoline:c.reg16(10)=12;value(c,12);break;
 case S::AdvanceTrampoline:{auto old=uint32_t(c.reg32(1)),v=old+1;c.reg32(1)=int32_t(v);value(c,c.reg32(1));if((~old&v)&0x80000000)c.ccr|=2;break;}
 case S::EnterTrampoline:case S::Restart:case S::EnterWarm:case S::SelectChecksum:c.clockI(2);break;case S::PrepareOverlay:c.reg8(8)=0;value(c,0);break;case S::PrepareAccessState:c.reg8(8)=int8_t(0xfa);value(c,-6);break;
 case S::ReturnWarm:{c.clockI(2);auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(c.read32(int(sp))&0xffffff);c.irqDepth(c.irqDepth()-1);break;}
 case S::BindWarmImage:c.reg32(1)=0x200000;value(c,c.reg32(1));break;case S::BindUpdate:c.reg32(6)=0xaaab;value(c,c.reg32(6));break;case S::PublishUpdate:call(c,0x239e,0x224);break;case S::BindUpdateTarget:c.reg32(5)=0x410000;value(c,c.reg32(5));break;case S::BindUpdateSource:c.reg32(6)=0x210000;value(c,c.reg32(6));break;case S::UpdateFirmware:call(c,0x105a,0x234);break;case S::BindDone:c.reg32(6)=0xaa91;value(c,c.reg32(6));break;case S::PublishDone:call(c,0x239e,0x23e);break;
 default:break;}}
 static uint32_t read(const T&t,Stage1Cpu&c,T::Read f,unsigned i){uint32_t result=0;switch(f){case T::Read::Signature:result=uint8_t(c.read8(0x110+i));c.reg8(10)=int8_t(result);++c.reg32(0);value(c,c.reg8(10));c.clockI(2);break;case T::Read::StoredSignature:result=uint8_t(c.read8(0x210110+i));c.reg8(2)=int8_t(result);++c.reg32(1);value(c,c.reg8(2));c.clockI(2);break;
 case T::Read::Version:result=c.read16(0x138);c.reg16(0)=int16_t(result);value(c,c.reg16(0));break;case T::Read::StoredVersion:result=c.read16(0x210138);c.reg16(1)=int16_t(result);value(c,c.reg16(1));break;
 case T::Read::ImageSize:result=c.read32(t.sourceBase()+0x104);c.reg32(0)=int32_t(result);value(c,c.reg32(0));break;
 case T::Read::ImageWord:result=c.read16(t.sourceBase()+i);c.reg16(2)=int16_t(result);c.reg32(1)=int32_t(uint32_t(c.reg32(1))+2);c.clockI(2);value(c,c.reg16(2));break;
 case T::Read::ExpectedChecksum:result=c.read32(t.sourceBase()+i);c.reg32(2)=int32_t(result);c.reg32(1)=int32_t(uint32_t(c.reg32(1))+4);value(c,c.reg32(2));c.clockI(2);break;
 case T::Read::Marker:result=uint8_t(c.read8(0xfffe3d));c.reg8(8)=int8_t(result);value(c,c.reg8(8));break;
 case T::Read::Trampoline:result=uint8_t(c.read8(0x420+i));c.reg8(10)=int8_t(result);++c.reg32(0);value(c,c.reg8(10));c.clockI(2);break;}return result;}
 static void write(const T&t,Stage1Cpu&c,T::Write f,unsigned i,uint32_t v){switch(f){case T::Write::ImageWord:c.write16(int16_t(v),t.targetBase()+i);value(c,c.reg16(2));break;case T::Write::Checksum:c.write32(int32_t(v),t.targetBase()+i);value(c,c.reg32(3));break;case T::Write::Tail:c.write16(0,t.targetBase()+i);value(c,c.reg16(8));break;case T::Write::Trampoline:c.write8(int8_t(v),0xfffd10+i);value(c,c.reg8(10));break;case T::Write::Overlay:value(c,c.reg8(8));c.write8(0,0xffffcf);break;case T::Write::AccessState:value(c,c.reg8(8));c.write8(int8_t(v),0xffffed);break;}}
 std::optional<T>task_;uint64_t steps_=0;
};
}
