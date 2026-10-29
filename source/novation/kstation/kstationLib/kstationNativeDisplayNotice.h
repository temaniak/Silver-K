#pragma once
#include "kstationDisplayNoticeTask.h"
#include "kstationNativeMidiPump.h"
namespace kstation {
class NativeDisplayNotice final {
 using T=DisplayNoticeTask;using S=T::Stage;
public:
 explicit NativeDisplayNotice(T::Kind k):task_(k){}const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){if(!task_.awaitingChild())return false;auto ch=task_.child();uint32_t ret=ch==T::Child::Print?0x438e:ch==T::Child::Schedule?0x43a4:0x43b0;return uint32_t(c.getPC())==ret&&task_.resume(ch);}uint32_t boundary()const{switch(task_.stage()){case S::Print:return 0x438a;case S::SetActive:return 0x438e;case S::WriteActive:return 0x4390;case S::BindDelay:return 0x4396;case S::BindCallback:return 0x439a;case S::Schedule:return 0x43a0;case S::ClearActive:return 0x43a6;case S::WriteInactive:return 0x43a8;case S::Refresh:return 0x43ae;case S::Return:return task_.kind()==T::Kind::Show?0x43a4:0x43b0;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();unsigned len=p==S::WriteActive||p==S::WriteInactive||p==S::BindCallback?6:p==S::Print||p==S::BindDelay||p==S::Schedule?4:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::Print:call(c,0x239e,0x438e,true);break;case S::SetActive:c.reg8(14)=1;val(c,c.reg8(14));break;case S::BindDelay:c.reg16(6)=0x2ee;val(c,c.reg16(6));break;case S::BindCallback:c.reg32(5)=0x43a6;val(c,c.reg32(5));break;case S::Schedule:call(c,0x38f2,0x43a4,true);break;case S::ClearActive:c.reg8(8)=0;val(c,c.reg8(8));break;case S::Refresh:call(c,0x4358,0x43b0,false);break;case S::Return:ret(c);break;default:break;}task_.advance([&](uint8_t v){val(c,c.reg8(p==S::WriteActive?14:8));c.write8(int8_t(v),0xcd7d);});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
class NativeDeferredSchedule final {
 using T=DeferredScheduleTask;using S=T::Stage;
public:
 NativeDeferredSchedule(uint32_t callback,uint16_t delay):task_(callback,delay){}const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){return task_.awaitingChild()&&uint32_t(c.getPC())==0x3904&&task_.resume(uint16_t(c.reg16(6)));}uint32_t boundary()const{switch(task_.stage()){case S::WriteCallback:return 0x38f2;case S::WriteDelay:return 0x38fa;case S::Tick:return 0x3900;case S::WriteStart:return 0x3904;case S::Return:return 0x390a;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();unsigned len=p==S::WriteCallback?8:p==S::WriteDelay||p==S::WriteStart?6:p==S::Tick?4:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::Tick:call(c,0x9aa,0x3904,true);break;case S::Return:ret(c);break;default:break;}task_.advance([&](unsigned field,uint32_t value){if(field==0){c.write32(int(value),0xcd4e);val(c,c.reg32(5));}else{c.write16(int16_t(value),field==1?0xcd4c:0xcd4a);val(c,c.reg16(6));}});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
}
