#pragma once
#include "kstationDspTransferTask.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>

namespace kstation {
// Transitional caller projection. Protocol decisions belong to the pure
// tasks; no decoder is used in this closed graph, including fault notification.
class NativeDspTransfer final {
    using W=DspWordTask;using A=DspAddressTask;using F=DspFaultTask;using P=DspPollTask;
    enum class Kind {Word,Address,Fault,Invalidate,SetAddress};
    struct Job {
        Kind kind=Kind::Word;W word{0};A address{0};F fault{true};
        uint32_t root=0,returned=0;unsigned setup=0;
    };
public:
    struct CodeRange{uint32_t begin,end;};
    inline static constexpr std::array<CodeRange,3> CodeRanges{{{0x14e6,0x14ec},{0x15a6,0x17ba},{0x20f6,0x2104}}};
    bool active()const{return depth_!=0;}void reset(){depth_=0;}
    static bool candidate(uint32_t pc){return (pc>=0x15a6&&pc<0x17ba)||(pc>=0x14e6&&pc<0x14ec)||(pc>=0x20f6&&pc<0x2104);}
    bool step(Stage1Cpu&c){
        if(c.interruptMode2||c.pending_irqs_high||(c.pending_irqs&&!(c.ccr&0x80)))return false;
        const auto at=uint32_t(c.getPC());
        if(depth_)resume(c);
        if(!depth_||boundary(jobs_[depth_-1])!=at){
            if(!entry(at))return false;
            if(depth_==jobs_.size())throw std::logic_error("Native K DSP continuation capacity");
            Job j;j.root=at;
            if(at==0x165e||at==0x16ba){j.kind=Kind::Word;j.word=W((uint32_t(uint16_t(c.reg16(9)))<<8)|uint8_t(c.reg8(1)),uint16_t(c.reg16(4)),at==0x165e);}
            else if(at==0x15a6||at==0x16fa){j.kind=Kind::Address;j.address=A(uint16_t(c.reg16(4)),at==0x16fa);}
            else if(at==0x14e6||at==0x20f6){j.kind=Kind::Fault;j.fault=F(at==0x14e6);}
            else j.kind=at==0x1640?Kind::Invalidate:Kind::SetAddress;
            jobs_[depth_++]=j;
        }
        auto&j=jobs_[depth_-1];if(boundary(j)!=at)return false;
        if(c.pending_irqs)c.read8(0xfffff2);
        const auto len=length(j);DspTransferCredits cr;
        cr.call=8+busCost(c.getSP()-4,4,c.writeHistory());
        cr.ret=5+busCost(c.getSP(),4,int(at+len-1));
        cr.retainWord=5+busCost(c.getSP()-2,2,c.writeHistory());
        cr.restoreWord=5+busCost(c.getSP(),2,int(at+len-1));
        cr.readCache=9+busCost(0xc598,2,int(at+len-1));
        cr.writeCache=9+busCost(0xc598,2,c.writeHistory());
        const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));
        unsigned cost=0;bool done=false;
        if(j.kind==Kind::Word){
            const auto stage=j.word.stage();const auto ps=j.word.poll().stage();cost=j.word.nextClocks(cr);
            if(stage==W::Stage::PollReady)projectPoll(c,j.word.poll());
            else switch(stage){
            case W::Stage::SelectPort:c.reg32(5)=0x600000;val(c,c.reg32(5));break;
            case W::Stage::CompareCache:sub16(c,uint16_t(c.reg16(4)),uint16_t(c.reg16(0)));break;
            case W::Stage::DispatchAddress:call(c,0x15a6,at+len);break;
            case W::Stage::PrepareWord:c.reg16(0)=c.reg16(9);val(c,c.reg16(0));break;
            case W::Stage::AdvanceAddress:{const int old=c.reg16(4),v=old+1;val(c,v);if((~old&v)&0x8000)c.ccr|=2;c.reg16(4)=int16_t(v);break;}
            case W::Stage::Return:ret(c);break;default:break;
            }
            const auto used=j.word.advanceBefore(cost,cr,[&](bool cache)->uint32_t{
                if(cache){c.reg16(0)=int16_t(c.read16(0xc598));val(c,c.reg16(0));return uint16_t(c.reg16(0));}
                c.reg8(8)=c.read8(0x600004);val(c,c.reg8(8));return uint8_t(c.reg8(8));
            },[&](W::Write field,uint16_t value){
                if(field==W::Write::Cache){c.write16(int16_t(value),0xc598);val(c,c.reg16(4));}
                else{const int r=field==W::Write::High?0:field==W::Write::Middle?8:1;c.write8(int8_t(value),field==W::Write::High?0x60000a:field==W::Write::Middle?0x60000c:0x60000e);val(c,c.reg8(r));}
            });if(used!=cost)throw std::logic_error("Native K DSP word admission");
            if(stage==W::Stage::PollReady&&ps==P::Stage::DispatchFault)j.returned=at+len;
            done=j.word.complete();
        }else if(j.kind==Kind::Address){
            const auto stage=j.address.stage();const auto ps=j.address.poll().stage();cost=j.address.nextClocks(cr);
            if(j.address.polling())projectPoll(c,j.address.poll());
            else switch(stage){
            case A::Stage::SelectPort:c.reg32(5)=0x600000;val(c,c.reg32(5));break;
            case A::Stage::PrepareAssert:c.reg8(8)=8;val(c,c.reg8(8));break;
            case A::Stage::PrepareRelease:c.reg8(8)=0;val(c,c.reg8(8));break;
            case A::Stage::PrepareCommand:c.reg8(8)=int8_t(0x80);val(c,c.reg8(8));break;
            case A::Stage::Return:ret(c);break;default:break;
            }
            const auto used=j.address.advanceBefore(cost,cr,[&](unsigned offset)->uint32_t{
                const int r=offset==4||offset==14?8:offset==10?4:12;c.reg8(r)=c.read8(int(0x600000+offset));val(c,c.reg8(r));return uint8_t(c.reg8(r));
            },[&](A::Write field,uint16_t value){
                if(field==A::Write::Cache){c.write16(int16_t(value),0xc598);val(c,c.reg16(4));}
                else{const int r=field==A::Write::Middle?4:field==A::Write::Low?12:8;c.write8(int8_t(value),field==A::Write::Control?0x600000:field==A::Write::High?0x60000a:field==A::Write::Middle?0x60000c:0x60000e);val(c,c.reg8(r));}
            });if(used!=cost)throw std::logic_error("Native K DSP address admission");
            if((stage==A::Stage::PollReady||stage==A::Stage::PollAsserted||stage==A::Stage::PollResponse)&&ps==P::Stage::DispatchFault)j.returned=at+len;
            done=j.address.complete();
        }else if(j.kind==Kind::Fault){
            const auto stage=j.fault.stage();cost=j.fault.nextClocks(cr);
            switch(stage){case F::Stage::DispatchLatch:call(c,0x20f6,at+len);j.returned=at+len;break;
            case F::Stage::RetainValue:c.setSP(c.getSP()-2);c.write16(c.reg16(6),int(c.getSP()));c.clockI(2);val(c,c.reg16(6));break;
            case F::Stage::RestoreValue:c.reg16(6)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);val(c,c.reg16(6));break;
            case F::Stage::ToggleFirst:c.reg8(6)^=0x10;break;
            case F::Stage::ToggleSecond:c.reg8(6)^=0x20;break;
            case F::Stage::ReturnWrapper:case F::Stage::ReturnLatch:ret(c);break;default:break;}
            const auto used=j.fault.advanceBefore(cost,cr,[&]{c.reg8(6)=c.read8(0xffffd2);val(c,c.reg8(6));return uint8_t(c.reg8(6));},[&](uint8_t v){val(c,c.reg8(6));c.write8(int8_t(v),0xffffd2);});
            if(used!=cost)throw std::logic_error("Native K DSP fault admission");done=j.fault.complete();
        }else{
            cost=setup(c,j,cr);done=j.setup==4;
        }
        if(c.cycles-begin!=cost)throw std::logic_error("Native K DSP phase clocks differ");
        const bool waiting=j.kind==Kind::Word?(j.word.awaitingAddress()||j.word.awaitingFault()):j.kind==Kind::Address?j.address.awaitingFault():j.kind==Kind::Fault?j.fault.awaitingLatch():at==0x1658&&j.kind==Kind::SetAddress;
        if(done)--depth_;else if(!waiting)c.pc=c.makepc(int(boundary(j)));
        return true;
    }
private:
    static bool entry(uint32_t at){return at==0x15a6||at==0x1640||at==0x1652||at==0x165e||at==0x16ba||at==0x16fa||at==0x14e6||at==0x20f6;}
    void resume(Stage1Cpu&c){auto&j=jobs_[depth_-1];const auto at=uint32_t(c.getPC());
        if(j.kind==Kind::Word){if(j.word.awaitingAddress()&&at==0x1672)j.word.resumeAddress();else if(j.word.awaitingFault()&&at==j.returned)j.word.resumeFault();}
        else if(j.kind==Kind::Address){if(j.address.awaitingFault()&&at==j.returned)j.address.resumeFault();}
        else if(j.kind==Kind::Fault){if(j.fault.awaitingLatch()&&at==j.returned)j.fault.resumeLatch();}
        else if(j.kind==Kind::SetAddress&&j.setup==1&&at==0x165c)j.setup=2;
    }
    static unsigned busCost(uint32_t at,unsigned bytes,int previous){unsigned cost=0;for(unsigned n=0;n<bytes;++n){const auto a=(at+n)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((previous&1)||a!=uint32_t(previous|1))cost+=a<0x400000?3:1;previous=int(a);}else{cost+=a>=0xffffa0?1:3;previous=-1;}}return cost;}
    static void val(Stage1Cpu&c,int v){c.ccr=uint8_t((c.ccr&~14)|(v<0?8:0)|(!v?4:0));}
    static void sub16(Stage1Cpu&c,uint16_t a,uint16_t b){const uint32_t r=uint16_t(a-b),carry=(~a&b)|(~a&r)|(b&r),over=(a&~b&~r)|(~a&b&r);c.ccr=uint8_t((c.ccr&~0x2f)|(!r?4:0)|((r&0x8000)?8:0)|((carry&0x8000)?1:0)|((carry&0x800)?32:0)|((over&0x8000)?2:0));}
    static void call(Stage1Cpu&c,uint32_t target,uint32_t returned){const auto sp=c.getSP()-4;c.write32(int32_t(returned),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);c.clockI(2);c.pc=c.makepc(int(target));}
    static void ret(Stage1Cpu&c){c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);c.pc=c.makepc(int(c.read32(int(sp))&0xffffff));}
    static void projectPoll(Stage1Cpu&c,const P&p){switch(p.stage()){
        case P::Stage::FirstMask:case P::Stage::Mask:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&p.mask());val(c,c.reg8(8));break;
        case P::Stage::ResetRetries:sub16(c,uint16_t(c.reg16(8)),uint16_t(c.reg16(8)));c.reg16(8)=0;break;
        case P::Stage::DecrementRetries:{const int old=c.reg16(8),v=old-1;val(c,v);if((old&~v)&0x8000)c.ccr|=2;c.reg16(8)=int16_t(v);break;}
        case P::Stage::DispatchFault:call(c,0x14e6,uint32_t(c.getPC())+3);break;
        default:break;}
    }
    static uint32_t pollBoundary(uint32_t base,P::Stage p){constexpr unsigned offsets[]{0,8,10,12,14,22,24,26,28,30,0,34,36};return p==P::Stage::AwaitFault?0:base+offsets[unsigned(p)];}
    static uint32_t boundary(const Job&j){
        if(j.kind==Kind::Word){const bool addressed=j.word.addressed();switch(j.word.stage()){
            case W::Stage::SelectPort:return 0x165e;case W::Stage::ReadCache:return 0x1664;case W::Stage::CompareCache:return 0x166a;case W::Stage::SelectAddress:return 0x166c;case W::Stage::DispatchAddress:return 0x166e;
            case W::Stage::PollReady:return pollBoundary(addressed?0x1672:0x16ba,j.word.poll().stage());
            case W::Stage::PrepareWord:return addressed?0x1696:0x16de;case W::Stage::High:return addressed?0x1698:0x16e0;case W::Stage::Middle:return addressed?0x16a0:0x16e8;case W::Stage::Low:return addressed?0x16a8:0x16f0;
            case W::Stage::AdvanceAddress:return 0x16b0;case W::Stage::PublishAddress:return 0x16b2;case W::Stage::Return:return addressed?0x16b8:0x16f8;default:return 0;}}
        if(j.kind==Kind::Address){const bool receive=j.address.receives();switch(j.address.stage()){
            case A::Stage::SelectPort:return 0x16fa;case A::Stage::PublishCache:return 0x1700;
            case A::Stage::PollReady:return pollBoundary(receive?0x1706:0x15a6,j.address.poll().stage());
            case A::Stage::PrepareAssert:return receive?0x172a:0x15ca;case A::Stage::Assert:return receive?0x172c:0x15cc;
            case A::Stage::PollAsserted:return pollBoundary(receive?0x1734:0x15d4,j.address.poll().stage());
            case A::Stage::PrepareRelease:return receive?0x1758:0x15f8;case A::Stage::Release:return receive?0x175a:0x15fa;case A::Stage::PrepareCommand:return 0x1762;
            case A::Stage::WriteCommand:return receive?0x1764:0x1602;case A::Stage::WriteAddressHigh:return receive?0x176c:0x160a;case A::Stage::WriteAddressLow:return receive?0x1774:0x1612;
            case A::Stage::PollResponse:return pollBoundary(receive?0x177c:0x161a,j.address.poll().stage());
            case A::Stage::ReadHigh:return 0x17a0;case A::Stage::ReadMiddle:return 0x17a8;case A::Stage::ReadLow:return 0x17b0;case A::Stage::Return:return receive?0x17b8:0x163e;default:return 0;}}
        if(j.kind==Kind::Fault){switch(j.fault.stage()){case F::Stage::DispatchLatch:return 0x14e6;case F::Stage::ReturnWrapper:return 0x14ea;case F::Stage::RetainValue:return 0x20f6;case F::Stage::ReadPins:return 0x20f8;case F::Stage::ToggleFirst:return 0x20fa;case F::Stage::ToggleSecond:return 0x20fc;case F::Stage::WritePins:return 0x20fe;case F::Stage::RestoreValue:return 0x2100;case F::Stage::ReturnLatch:return 0x2102;default:return 0;}}
        if(j.kind==Kind::Invalidate){constexpr uint32_t sites[]{0x1640,0x1646,0x164a,0x1650};return j.setup<4?sites[j.setup]:0;}
        return j.setup==0?0x1652:j.setup==1?0x1658:j.setup==2?0x165c:0;
    }
    static unsigned length(const Job&j){
        if(j.kind==Kind::Word){switch(j.word.stage()){case W::Stage::SelectPort:case W::Stage::ReadCache:case W::Stage::PublishAddress:return 6;case W::Stage::DispatchAddress:return 4;case W::Stage::High:case W::Stage::Middle:case W::Stage::Low:return 8;case W::Stage::PollReady:return pollLength(j.word.poll());default:return 2;}}
        if(j.kind==Kind::Address){if(j.address.polling())return pollLength(j.address.poll());switch(j.address.stage()){case A::Stage::SelectPort:case A::Stage::PublishCache:return 6;case A::Stage::Assert:case A::Stage::Release:case A::Stage::WriteCommand:case A::Stage::WriteAddressHigh:case A::Stage::WriteAddressLow:case A::Stage::ReadHigh:case A::Stage::ReadMiddle:case A::Stage::ReadLow:return 8;default:return 2;}}
        if(j.kind==Kind::Fault)return j.fault.stage()==F::Stage::DispatchLatch?4:2;
        if(j.kind==Kind::Invalidate)return j.setup==0||j.setup==2?6:j.setup==1?4:2;
        return j.setup==0?6:j.setup==1?4:2;
    }
    static unsigned pollLength(const P&p){return p.stage()==P::Stage::FirstRead||p.stage()==P::Stage::Read?8:p.stage()==P::Stage::DispatchFault?4:2;}
    static unsigned setup(Stage1Cpu&c,Job&j,const DspTransferCredits&cr){
        if(j.kind==Kind::Invalidate){switch(j.setup++){case 0:c.reg32(5)=0x600000;val(c,c.reg32(5));return 9;case 1:c.reg16(0)=-1;val(c,c.reg16(0));return 6;case 2:c.write16(c.reg16(0),0xc598);val(c,c.reg16(0));return cr.writeCache;default:ret(c);return cr.ret;}}
        if(j.setup==0){c.write16(c.reg16(4),0xc598);val(c,c.reg16(4));j.setup=1;return cr.writeCache;}
        if(j.setup==1){call(c,0x15a6,0x165c);return cr.call;}
        ret(c);j.setup=4;return cr.ret;
    }
    std::array<Job,12> jobs_{};unsigned depth_=0;
};
}
