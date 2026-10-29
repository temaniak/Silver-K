#pragma once
#include "kstationParameterUpdateTask.h"
#include "kstationNativeMidiPump.h"
#include <array>
#include <stdexcept>
namespace kstation {
class NativeParameterUpdate final {
    using T=ParameterUpdateTask;using S=T::Stage;using G=ActiveParameterGroupTask;
    struct Job{T task;G group;bool readingGroup=false;uint32_t returned=0;};
public:
    struct CodeRange{uint32_t begin,end;};
    inline static constexpr std::array<CodeRange,2>CodeRanges{{{0x5778,0x57b8},{0x40b0,0x40c0}}};
    static bool candidate(uint32_t at){return (at>=0x5778&&at<0x57b8)||(at>=0x40b0&&at<0x40c0);}
    void reset(){depth_=0;}bool active()const{return depth_!=0;}
    bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;const auto at=uint32_t(c.getPC());
        if(depth_){auto&j=jobs_[depth_-1];if(!j.readingGroup&&j.task.awaitingChild()&&at==j.returned)j.task.resumeChild(j.task.child(),uint8_t(c.reg8(9)));}
        if(!depth_||boundary(jobs_[depth_-1])!=at){
            if(at!=0x5778&&at!=0x40b0)return false;if(depth_==jobs_.size())throw std::logic_error("Native K parameter continuation capacity");
            Job j;j.readingGroup=at==0x40b0;j.task=T(uint32_t(c.reg32(1)),uint8_t(c.reg8(6)));jobs_[depth_++]=j;
        }
        auto&j=jobs_[depth_-1];if(boundary(j)!=at)return false;if(c.pending_irqs)c.read8(0xfffff2);
        const auto len=length(j);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));
        if(j.readingGroup){using P=G::Stage;const auto p=j.group.stage();switch(p){
            case P::BindField:c.reg8(9)=int8_t(0xb2);midi_projection::val(c,c.reg8(9));break;
            case P::NormalizeWord:c.reg16(1)=int16_t(uint8_t(c.reg8(9)));midi_projection::val(c,c.reg16(1));break;
            case P::NormalizeLong:c.reg32(1)=uint16_t(c.reg16(1));midi_projection::val(c,c.reg32(1));break;
            case P::Locate:c.reg32(1)=int32_t(midi_projection::arithmetic(c,uint32_t(c.reg32(1)),0xba5c,32,false));break;
            case P::Return:midi_projection::ret(c);break;default:break;}
            j.group.advance([&]{c.reg8(9)=c.read8(int(uint32_t(c.reg32(1))));midi_projection::val(c,c.reg8(9));return uint8_t(c.reg8(9));});
            if(j.group.complete())--depth_;else c.pc=c.makepc(int(boundary(j)));return true;
        }
        const auto phase=j.task.stage();T::Credits cr;cr.retain=5+bus(c.getSP()-2,2,c.writeHistory());cr.restore=5+bus(c.getSP(),2,int(at+len-1));
        cr.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.ret=5+bus(c.getSP(),4,int(at+len-1));
        cr.write=12+bus((phase==S::PublishProgram?0xba5c:0xcf36)+j.task.offset(),1,c.writeHistory());
        const auto cost=j.task.nextClocks(cr);
        switch(phase){
        case S::RetainOffset:c.setSP(c.getSP()-2);c.write16(c.reg16(1),int(c.getSP()));c.clockI(2);midi_projection::val(c,c.reg16(1));break;
        case S::RestoreOffset:c.reg16(1)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);midi_projection::val(c,c.reg16(1));break;
        case S::CompareOffset:midi_projection::arithmetic(c,uint8_t(c.reg8(9)),uint8_t(c.reg8(8)),8,true);break;
        case S::Unpack:case S::UpdateVoices:case S::ReadActiveGroup:case S::Display:case S::Panel:{
            const auto target=phase==S::Unpack?0x4f48:phase==S::UpdateVoices?0x7930:phase==S::ReadActiveGroup?0x40b0:phase==S::Display?0x4358:0x571e;
            j.returned=at+len;midi_projection::call(c,target,j.returned,true);break;}
        case S::Return:midi_projection::ret(c);break;default:break;}
        const auto used=j.task.advanceBefore(cost,cr,[&](T::Read field){c.reg8(8)=c.read8(field==T::Read::Editor?0xcd7f:0xcd81);midi_projection::val(c,c.reg8(8));return uint8_t(c.reg8(8));},[&](T::Write field,uint8_t value){
            c.write8(int8_t(value),int((field==T::Write::Program?0xba5c:0xcf36)+j.task.offset()));midi_projection::val(c,c.reg8(6));});
        if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K parameter phase clocks differ");
        if(j.task.complete())--depth_;else if(!j.task.awaitingChild())c.pc=c.makepc(int(boundary(j)));return true;
    }
private:
    static uint32_t boundary(const Job&j){
        if(j.readingGroup){constexpr uint32_t sites[]{0x40b0,0x40b2,0x40b4,0x40b6,0x40bc,0x40be};return j.group.complete()?0:sites[unsigned(j.group.stage())];}
        if(j.task.awaitingChild())return 0;constexpr uint32_t sites[]{0x5778,0x5780,0x5788,0x578a,0x578e,0x5792,0x5794,0x579a,0x579c,0x57a2,0x57a4,0x57a6,0x57a8,0x57ac,0x57ae,0x57b2,0x57b6};return j.task.complete()?0:sites[unsigned(j.task.stage())];
    }
    static unsigned length(const Job&j){if(j.readingGroup)return j.group.stage()==G::Stage::Locate?6:2;switch(j.task.stage()){
        case S::PublishProgram:case S::PublishMirror:return 8;case S::ReadEditor:case S::ReadEditorOffset:return 6;
        case S::Unpack:case S::UpdateVoices:case S::ReadActiveGroup:case S::Display:case S::Panel:return 4;default:return 2;}}
    static unsigned bus(uint32_t at,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
    std::array<Job,4>jobs_{};unsigned depth_=0;
};
}
