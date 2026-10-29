#pragma once
#include "kstationMidiWrappersTask.h"
#include "kstationNativeMidiPump.h"
namespace kstation {
class NativeMidiWrapper final {
 using T=MidiWrapperTask;using S=T::Stage;using K=T::Kind;
public:explicit NativeMidiWrapper(K k):task_(k){}const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){if(!task_.awaitingChild())return false;auto ch=task_.child();uint32_t ret=ch==T::Child::SysEx?0x2f6a:ch==T::Child::Noop?0x2f6e:ch==T::Child::Dsp?0x33f0:ch==T::Child::Match?0x33ce:0x33ea;return uint32_t(c.getPC())==ret&&task_.resume(ch,uint8_t(c.reg8(14)));}
 uint32_t boundary()const{auto k=task_.kind();switch(task_.stage()){case S::Call:return k==K::SysEx?0x2f68:k==K::NoopCall?0x2f6c:k==K::Dsp?0x33ec:0x33ca;case S::Test:return 0x33ce;case S::Select:return 0x33d0;case S::ReadBank:return 0x33d2;case S::SetBank:return 0x33d8;case S::WriteBank:return 0x33da;case S::ReadProgram:return 0x33e0;case S::CompareProgram:return 0x33e2;case S::SelectProgram:return 0x33e4;case S::CallProgram:return 0x33e6;case S::Return:return k==K::SysEx?0x2f6a:k==K::NoopCall?0x2f6e:k==K::Noop?0x2f70:k==K::EmptyDsp?0x33f2:k==K::Dsp?0x33f0:0x33ea;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto k=task_.kind();auto at=boundary();unsigned len=p==S::ReadBank||p==S::WriteBank?6:p==S::CallProgram||(p==S::Call&&(k==K::Dsp||k==K::Program))?4:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::Call:call(c,k==K::SysEx?0x2f72:k==K::NoopCall?0x2f70:k==K::Dsp?0x825c:0x2d5c,at+len,len==4);break;case S::Test:val(c,c.reg8(14));break;case S::SetBank:c.reg8(8)=int8_t(0xff);val(c,c.reg8(8));break;case S::CompareProgram:arithmetic(c,uint8_t(c.reg8(13)),100,8,true);break;case S::CallProgram:call(c,0x4c5e,0x33ea,true);break;case S::Return:ret(c);break;default:break;}task_.advance([&](uint32_t addr){int reg=p==S::ReadBank?5:13;c.reg8(reg)=c.read8(int(p==S::ReadBank?addr:uint32_t(c.reg32(4))));val(c,c.reg8(reg));return uint8_t(c.reg8(reg));},[&](uint32_t addr,uint8_t v){val(c,c.reg8(8));c.write8(int8_t(v),int(addr));});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
class NativeMidiProgramCommit final {
 using T=MidiProgramCommitTask;using S=T::Stage;
public:explicit NativeMidiProgramCommit(uint8_t bank):task_(bank){}const T&task()const{return task_;}bool resumeChild(Stage1Cpu&c){return task_.awaitingChild()&&uint32_t(c.getPC())==(task_.child()==T::Child::Load?0x4c70:0x4c74)&&task_.resume(task_.child());}
 uint32_t boundary()const{switch(task_.stage()){case S::TestBank:return 0x4c5e;case S::SelectBank:return 0x4c60;case S::WriteBank:return 0x4c62;case S::WriteProgram:return 0x4c68;case S::Load:return 0x4c6e;case S::Display:return 0x4c70;case S::Return:return 0x4c74;default:return 0;}}
 bool step(Stage1Cpu&c){using namespace midi_projection;if(blocked(c)||task_.awaitingChild()||task_.complete()||uint32_t(c.getPC())!=boundary())return false;if(c.pending_irqs)c.read8(0xfffff2);auto p=task_.stage();auto at=boundary();unsigned len=p==S::WriteBank||p==S::WriteProgram?6:p==S::Display?4:2;c.fetchClocks(len);c.pc=c.makepc(int(at+1));switch(p){case S::TestBank:val(c,c.reg8(5));break;case S::Load:call(c,0x4ca0,0x4c70,false);break;case S::Display:call(c,0x4358,0x4c74,true);break;case S::Return:ret(c);break;default:break;}task_.advance([&](uint32_t addr,bool bank){val(c,c.reg8(bank?5:13));c.write8(c.reg8(bank?5:13),int(addr));});if(!task_.awaitingChild()&&!task_.complete())c.pc=c.makepc(int(boundary()));return true;}
private:T task_;
};
}
