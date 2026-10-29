#pragma once
#include "kstationPatchPackTask.h"
#include "kstationNativeMidiPump.h"
namespace kstation {
class NativePatchPackByte final {
 using T=PatchPackTask;using S=T::Stage;
public:
 NativePatchPackByte(unsigned index,uint32_t base):task_(index,base),at_(PatchPackSpecs[index].entry){}const T&task()const{return task_;}uint32_t boundary()const{return at_;}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.complete()||uint32_t(c.getPC())!=at_)return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();unsigned len=p==S::Read?8:p==S::BindDestination?6:2;c.fetchClocks(len);c.pc=c.makepc(int(at_+1));switch(p){case S::Bias:{int r=task_.group()?0:8;c.reg8(r)=int8_t(arithmetic(c,uint8_t(c.reg8(r)),0xc1,8,false));break;}case S::Merge:c.reg8(8)=int8_t(uint8_t(c.reg8(8))|uint8_t(c.reg8(0)));val(c,c.reg8(8));break;case S::Rotate:{uint8_t v=uint8_t(c.reg8(8));bool left=task_.spec().rotations[task_.group()]<0;uint8_t bit=left?v>>7:v&1;v=left?uint8_t((v<<1)|(v>>7)):uint8_t((v>>1)|(v<<7));c.ccr=uint8_t((c.ccr&~1)|bit);c.reg8(8)=int8_t(v);val(c,c.reg8(8));break;}case S::ShiftHigh:{uint8_t v=uint8_t(c.reg8(0));c.ccr=uint8_t((c.ccr&~1)|(v>>7));c.reg8(0)=int8_t(v<<1);val(c,c.reg8(0));break;}case S::MaskHigh:c.reg8(0)=int8_t(uint8_t(c.reg8(0))&0x7f);val(c,c.reg8(0));break;case S::BindDestination:c.reg32(1)=task_.spec().destination;val(c,c.reg32(1));break;case S::Return:ret(c);break;default:break;}task_.advance([&](uint32_t addr)->uint8_t{int r=task_.group()?0:8;c.reg8(r)=c.read8(int(addr));val(c,c.reg8(r));return uint8_t(c.reg8(r));});at_+=len;if(!task_.complete())c.pc=c.makepc(int(at_));return true;}
private:T task_;uint32_t at_;
};
}
namespace kstation {
class NativePatchPackRefresh final {
 using T=PatchPackRefreshTask;using S=T::Stage;
public:
 const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){return task_.awaitingChild()&&uint32_t(c.getPC())==0x5260&&task_.resume();}uint32_t boundary()const{switch(task_.stage()){case S::Begin:return 0x525c;case S::Pack:return 0x525e;case S::Write:return 0x5260;case S::Increment:return 0x5262;case S::Compare:return 0x5264;case S::Select:return 0x5266;case S::Return:return 0x5268;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();c.fetchClocks(2);c.pc=c.makepc(int(at+1));switch(p){case S::Begin:c.reg8(10)=1;val(c,c.reg8(10));break;case S::Pack:call(c,0x526a,0x5260,false);break;case S::Increment:{int old=c.reg8(10),v=old+1;val(c,v);c.ccr=uint8_t((c.ccr&~2)|((~old&v&0x80)?2:0));c.reg8(10)=int8_t(v);break;}case S::Compare:arithmetic(c,uint8_t(c.reg8(10)),12,8,true);break;case S::Return:ret(c);break;default:break;}task_.advance([&](){c.write8(c.reg8(8),c.reg32(1));val(c,c.reg8(8));});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
class NativePatchPackSelect final {
 using T=PatchPackSelectTask;using S=T::Stage;
public:
 explicit NativePatchPackSelect(uint8_t index):task_(index){}const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){return task_.awaitingChild()&&uint32_t(c.getPC())==0x5288&&task_.resume();}uint32_t boundary()const{switch(task_.stage()){case S::Save:return 0x526a;case S::Decrement:return 0x526c;case S::ShiftOne:return 0x526e;case S::ShiftTwo:return 0x5270;case S::ExtendWord:return 0x5272;case S::ExtendLong:return 0x5274;case S::ReadTarget:return 0x5276;case S::BindPatch:return 0x5280;case S::Call:return 0x5286;case S::Restore:return 0x5288;case S::Return:return 0x528a;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();unsigned len=p==S::ReadTarget?10:p==S::BindPatch?6:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::Save:c.setSP(c.getSP()-2);c.write16(c.reg16(2),int(c.getSP()));c.clockI(2);val(c,c.reg16(2));break;case S::Decrement:{int old=c.reg8(10),v=old-1;val(c,v);c.ccr=uint8_t((c.ccr&~2)|((old&~v&0x80)?2:0));c.reg8(10)=int8_t(v);break;}case S::ShiftOne:case S::ShiftTwo:{uint8_t v=uint8_t(c.reg8(10));c.ccr=uint8_t((c.ccr&~1)|(v>>7));c.reg8(10)=int8_t(v<<1);val(c,c.reg8(10));break;}case S::ExtendWord:c.reg16(2)=uint8_t(c.reg8(10));val(c,c.reg16(2));break;case S::ExtendLong:c.reg32(2)=uint16_t(c.reg16(2));val(c,c.reg32(2));break;case S::BindPatch:c.reg32(4)=0xba5c;val(c,c.reg32(4));break;case S::Call:call(c,uint32_t(c.reg32(2)),0x5288,false);break;case S::Restore:c.reg16(2)=c.read16(int(c.getSP()));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(2));break;case S::Return:ret(c);break;default:break;}task_.advance([&](uint32_t addr)->uint32_t{c.reg32(2)=c.read32(int(addr));val(c,c.reg32(2));return uint32_t(c.reg32(2));});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
}
