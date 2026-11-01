#pragma once
#include "kstationPatchUnpackTask.h"
#include "kstationNativeMidiPump.h"
#include <array>
#include <stdexcept>

namespace kstation {
class NativePatchUnpack final {
    using T=PatchUnpackTask;using S=T::Stage;
    struct Job{T task;PatchTransposeTask transpose{0};uint32_t source=0,destination=0;bool wrapper=false,transposing=false;unsigned phase=0;};
public:
    struct CodeRange{uint32_t begin,end;};
    inline static constexpr std::array<CodeRange,2>CodeRanges{{{0x4f48,0x525c},{0x3f3a,0x3f54}}};
    static bool candidate(uint32_t at){return (at>=0x4f48&&at<0x525c)||(at>=0x3f3a&&at<0x3f54);}
    void reset(){depth_=0;}bool active()const{return depth_!=0;}
    bool step(Stage1Cpu&c){
        if(midi_projection::blocked(c))return false;const auto at=uint32_t(c.getPC());
        if(depth_){auto&j=jobs_[depth_-1];if(j.wrapper&&j.phase==3&&at==0x4f56)j.phase=4;
            else if(!j.wrapper&&!j.transposing&&j.task.awaitingUi()&&at==0x5258)j.task.resumeUi();}
        if(!depth_||boundary(jobs_[depth_-1])!=at){
            if(at!=0x4f48&&at!=0x4f5c&&at!=0x3f3a)return false;
            if(depth_==jobs_.size())throw std::logic_error("Native K unpack continuation capacity");
            Job j;j.wrapper=at==0x4f48;j.transposing=at==0x3f3a;j.transpose=PatchTransposeTask(c.reg8(6));j.source=uint32_t(c.reg32(5));j.destination=uint32_t(c.reg32(4));jobs_[depth_++]=j;
        }
        auto&j=jobs_[depth_-1];if(boundary(j)!=at)return false;
        if(c.pending_irqs)c.read8(0xfffff2);
        const auto len=length(j);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));
        if(j.transposing){
            using P=PatchTransposeTask::Stage;const auto phase=j.transpose.stage();PatchTransposeTask::Credits cr;
            cr.ret=5+bus(c.getSP(),4,int(at+len-1));cr.write=9+bus(phase==P::PublishPrevious?0xcd79:0xcd78,1,c.writeHistory());const auto cost=j.transpose.nextClocks(cr);
            if(phase==P::BindScale){c.reg8(14)=12;midi_projection::val(c,c.reg8(14));}
            else if(phase==P::Multiply){c.reg16(6)=int16_t(int(c.reg8(6))*int(c.reg8(14)));c.ccr=uint8_t((c.ccr&~12)|(c.reg16(6)<0?8:0)|(!c.reg16(6)?4:0));c.clockI(12);}
            else if(phase==P::Return)midi_projection::ret(c);
            const auto used=j.transpose.advanceBefore(cost,cr,[&]{c.reg8(6)=c.read8(0xcd78);midi_projection::val(c,c.reg8(6));return uint8_t(c.reg8(6));},[&](bool previous,uint8_t value){const int r=previous?6:14;midi_projection::val(c,c.reg8(r));c.write8(int8_t(value),previous?0xcd79:0xcd78);});
            if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K transpose phase clocks differ");
            if(j.transpose.complete())--depth_;else c.pc=c.makepc(int(boundary(j)));return true;
        }
        if(j.wrapper){
            switch(j.phase){case 0:c.reg32(5)=0xba5c;midi_projection::val(c,c.reg32(5));++j.phase;c.pc=c.makepc(0x4f4e);break;
            case 1:c.reg32(4)=0xba5c;midi_projection::val(c,c.reg32(4));++j.phase;c.pc=c.makepc(0x4f54);break;
            case 2:midi_projection::call(c,0x4f5c,0x4f56,false);++j.phase;break;
            case 4:c.reg32(4)=0xcf36;midi_projection::val(c,c.reg32(4));j=Job{};j.source=uint32_t(c.reg32(5));j.destination=uint32_t(c.reg32(4));c.pc=c.makepc(0x4f5c);break;
            default:throw std::logic_error("Native K unpack wrapper wait");}
            return true;
        }
        auto&task=j.task;const auto phase=task.stage();T::Credits cr;
        cr.retain=5+bus(c.getSP()-2,2,c.writeHistory());cr.restore=5+bus(c.getSP(),2,int(at+len-1));
        cr.call=8+bus(c.getSP()-4,4,c.writeHistory());cr.ret=5+bus(c.getSP(),4,int(at+len-1));
        cr.read=12+bus(j.source+task.field().packed,1,int(at+len-1));
        cr.write=12+bus(j.destination+(phase==S::PublishSigned?0xad:task.field().decoded),1,c.writeHistory());
        const auto cost=task.nextClocks(cr);
        switch(phase){
        case S::Copy:c.reg8(0)=c.reg8(8);midi_projection::val(c,c.reg8(0));break;
        case S::Mask:{auto&r=c.reg8(task.field().direct?8:0);r=int8_t(uint8_t(r)&task.field().mask);midi_projection::val(c,r);break;}
        case S::Bias:c.reg8(0)=int8_t(midi_projection::arithmetic(c,uint8_t(c.reg8(0)),task.field().bias,8,false));break;
        case S::Shift:case S::SignLeft:case S::SignRight:{auto v=uint8_t(c.reg8(8));const auto carry=phase==S::SignLeft?v>>7:v&1;
            v=phase==S::SignLeft?uint8_t(v<<1):phase==S::SignRight?uint8_t((v>>1)|(v&0x80)):uint8_t(v>>1);
            c.ccr=uint8_t((c.ccr&~1)|carry);c.reg8(8)=int8_t(v);midi_projection::val(c,c.reg8(8));break;}
        case S::RetainUi:c.setSP(c.getSP()-2);c.write16(c.reg16(6),int(c.getSP()));c.clockI(2);midi_projection::val(c,c.reg16(6));break;
        case S::BindUi:c.reg8(6)=c.reg8(8);midi_projection::val(c,c.reg8(6));break;
        case S::DispatchUi:midi_projection::call(c,0x3f3a,0x5258,true);break;
        case S::RestoreUi:c.reg16(6)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);midi_projection::val(c,c.reg16(6));break;
        case S::Return:midi_projection::ret(c);break;default:break;
        }
        const auto used=task.advanceBefore(cost,cr,[&](unsigned offset){c.reg8(8)=c.read8(int(j.source+offset));midi_projection::val(c,c.reg8(8));return uint8_t(c.reg8(8));},[&](unsigned offset,uint8_t value){
            const auto r=phase==S::PublishSigned||task.field().direct?8:0;
            if(uint8_t(c.reg8(r))!=value)throw std::logic_error("Native K unpack value differs");
            c.write8(int8_t(value),int(j.destination+offset));midi_projection::val(c,c.reg8(r));
        });
        if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K unpack phase clocks differ");
        if(task.complete())--depth_;else if(!task.awaitingUi())c.pc=c.makepc(int(boundary(j)));
        return true;
    }
private:
    // Caller-projection locations are separate from the program field layout.
    inline static constexpr uint32_t publications[]{
        0x4f68,0x4f78,0x4f86,0x4f94,0x4fa2,0x4fb6,0x4fc4,0x4fd2,0x4fde,
        0x4ff2,0x5000,0x500e,0x501c,0x502a,0x5038,0x504c,0x505c,0x506c,0x507a,
        0x5090,0x50a2,0x50b4,0x50c2,0x50d6,0x50e8,0x50f8,0x510c,0x511a,0x5128,0x5138,
        0x514c,0x515e,0x5172,0x5182,0x5192,0x51a6,0x51b8,0x51c6,0x51d4,0x51e0,
        0x51f4,0x5204,0x5214,0x5222,0x5236};
    static uint32_t boundary(const Job&j){
        if(j.transposing){constexpr uint32_t sites[]{0x3f3a,0x3f3c,0x3f40,0x3f46,0x3f4c,0x3f52};return j.transpose.complete()?0:sites[unsigned(j.transpose.stage())];}
        if(j.wrapper)return j.phase==0?0x4f48:j.phase==1?0x4f4e:j.phase==2?0x4f54:j.phase==4?0x4f56:0;
        const auto&t=j.task;const auto f=t.field();const auto pub=publications[t.index()],mask=pub-(f.bias?4:2),copy=mask-2;
        switch(t.stage()){
        case S::ReadPacked:return copy-8;case S::Shift:return (f.direct?mask:copy)-2*f.shift+2*t.shiftIndex();
        case S::Copy:return copy;case S::Mask:return mask;case S::Bias:return pub-2;case S::Publish:return pub;
        case S::SignLeft:return 0x523e;case S::SignRight:return 0x5240+2*t.signIndex();case S::PublishSigned:return 0x5248;
        case S::RetainUi:return 0x5250;case S::BindUi:return 0x5252;case S::DispatchUi:return 0x5254;
        case S::RestoreUi:return 0x5258;case S::Return:return 0x525a;default:return 0;}
    }
    static unsigned length(const Job&j){if(j.transposing){using P=PatchTransposeTask::Stage;auto p=j.transpose.stage();return p==P::Multiply?4:p==P::ReadPrevious||p==P::PublishPrevious||p==P::PublishCurrent?6:2;}if(j.wrapper)return j.phase==2?2:6;switch(j.task.stage()){
        case S::ReadPacked:case S::Publish:case S::PublishSigned:return 8;case S::DispatchUi:return 4;default:return 2;}}
    static unsigned bus(uint32_t at,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
    std::array<Job,4>jobs_{};unsigned depth_=0;
};
}
