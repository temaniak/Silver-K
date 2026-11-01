#pragma once
#include "kstationSerialRxService.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>

namespace kstation {
// CPU-independent receive decisions live in SerialRxService. This temporary
// adapter preserves the old caller observations while other K services use H8.
// The external-clock child is dispatched separately by NativeClock.
class NativeSerialRx final {
    using S=SerialRxService::Stage;
public:
    struct CodeRange { uint32_t begin,end; };
    inline static constexpr std::array<CodeRange,2> CodeRanges{{{53*4,53*4+4},{0xa64,0xac2}}};
    struct Result { bool stepped=false,entered=false,returned=false,clockRequested=false; };
    static bool candidate(uint32_t pc,uint64_t pending) { return pending||(pc>=0xa64&&pc<0xac2); }
    bool active() const { return depth_!=0; }
    unsigned depth() const { return depth_; }
    void reset() { depth_=0; }
    Result step(Stage1Cpu& cpu) {
        if(cpu.interruptMode2||cpu.pending_irqs_high)return {};
        bool enter=false;
        if(cpu.pending_irqs&&!(cpu.ccr&0x80)) {
            if((cpu.pending_irqs&((uint64_t(1)<<53)-1))||!(cpu.pending_irqs&(uint64_t(1)<<53)))return {};
            if(depth_==jobs_.size())throw std::logic_error("Native K RX continuation capacity");
            enter=true;
        } else if(!depth_||!atBoundary(jobs_[depth_-1],uint32_t(cpu.getPC()))) {
            const auto pc=uint32_t(cpu.getPC());
            if(pc>=0xa64&&pc<0xac2)throw std::logic_error("Native K RX unowned caller boundary");
            return {};
        }
        if(cpu.pending_irqs) {
            const bool ue=(cpu.read8(0xfffff2)&8)!=0;
            if(enter) {
                cpu.pending_irqs&=~(uint64_t(1)<<53);
                const auto saved=(uint32_t(cpu.getPC())&0xffffff)|(uint32_t(cpu.ccr)<<24);
                const auto sp=cpu.getSP()-4;cpu.write32(int32_t(saved),int(sp));cpu.setSP(sp);cpu.clockI(4);
                cpu.ccr|=0x80;if(!ue)cpu.ccr|=0x10;
                if((cpu.read32(53*4)&0xffffff)!=0xa64)throw std::logic_error("Native K RX vector binding changed");
                cpu.pc=cpu.makepc(0xa64);cpu.irqDepth(cpu.irqDepth()+1);jobs_[depth_++]=SerialRxService{};
            }
        }
        auto& task=jobs_[depth_-1];
        if(task.awaitingClock()&&cpu.getPC()==0xa80)task.resumeClock();
        if(uint32_t(cpu.getPC())!=boundary(task.stage()))throw std::logic_error("Native K RX caller boundary differs");
        const auto phase=task.stage();const auto at=uint32_t(cpu.getPC());const auto length=bytes(phase);
        const SerialRxService::Credits credits{
            8+busCost(cpu.getSP()-4,4,cpu.writeHistory()),
            8+busCost(cpu.getSP(),4,int(at+length-1)),
            5+busCost(cpu.getSP(),4,int(at+length-1)),
            8+busCost(cpu.getSP()-4,4,cpu.writeHistory()),
            busCost(task.dataReference(),1,cpu.writeHistory())};
        const auto cost=task.nextClocks(credits);const auto begin=cpu.cycles;
        cpu.fetchClocks(length);cpu.pc=cpu.makepc(int(at+1));project(cpu,phase);
        const auto used=task.advanceBefore(cost,credits,[&](SerialRxService::Read field)->uint32_t {
            switch(field) {
            case SerialRxService::Read::Status:cpu.reg8(8)=cpu.read8(0xffffb4);valueFlags(cpu,cpu.reg8(8));return uint8_t(cpu.reg8(8));
            case SerialRxService::Read::Receive:cpu.reg8(10)=cpu.read8(0xffffb5);valueFlags(cpu,cpu.reg8(10));return uint8_t(cpu.reg8(10));
            case SerialRxService::Read::Producer:cpu.reg16(0)=int16_t(cpu.read16(0xfffd2c));valueFlags(cpu,cpu.reg16(0));return uint16_t(cpu.reg16(0));
            case SerialRxService::Read::Consumer:cpu.reg16(1)=int16_t(cpu.read16(0xfffd2e));valueFlags(cpu,cpu.reg16(1));return uint16_t(cpu.reg16(1));
            }
            throw std::logic_error("Native K RX read kind");
        },[&](SerialRxService::Write field,uint16_t value) {
            switch(field) {
            case SerialRxService::Write::Status:valueFlags(cpu,cpu.reg8(8));cpu.write8(int8_t(value),0xffffb4);break;
            case SerialRxService::Write::Data:cpu.write8(int8_t(value),int(task.dataReference()));valueFlags(cpu,cpu.reg8(10));break;
            case SerialRxService::Write::Producer:cpu.write16(int16_t(value),0xfffd2c);valueFlags(cpu,cpu.reg16(0));break;
            case SerialRxService::Write::Error:valueFlags(cpu,cpu.reg8(9));cpu.write8(int8_t(value),0xfffe30);break;
            }
        });
        if(used!=cost||cpu.cycles-begin!=cost)throw std::logic_error("Native K RX phase clocks differ");
        const bool returned=task.complete();
        if(returned)--depth_;
        else if(task.awaitingClock())cpu.pc=cpu.makepc(0x19f4);
        else cpu.pc=cpu.makepc(int(boundary(task.stage())));
        return {true,enter,returned,phase==S::DispatchClock};
    }
private:
    static bool atBoundary(const SerialRxService& task,uint32_t pc) {
        return pc==(task.awaitingClock()?0xa80:boundary(task.stage()));
    }
    static unsigned busCost(uint32_t at,unsigned bytes,int previous) {
        unsigned cost=0;for(unsigned n=0;n<bytes;++n) {
            const auto a=(at+n)&0xffffff;
            if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)) {
                if((previous&1)||a!=uint32_t(previous|1))cost+=a<0x400000?3:1;
                previous=int(a);
            } else {cost+=a>=0xffffa0?1:3;previous=-1;}
        }
        return cost;
    }
    static void valueFlags(Stage1Cpu& c,int value) {c.ccr=uint8_t((c.ccr&~14)|(value<0?8:0)|(value==0?4:0));}
    static void project(Stage1Cpu& c,S phase) {
        switch(phase) {
        case S::RetainPrimary:case S::RetainAddress:case S::RetainData:{
            const int r=phase==S::RetainPrimary?0:phase==S::RetainAddress?1:2;
            c.setSP(c.getSP()-4);c.write32(c.reg32(r),int(c.getSP()));valueFlags(c,c.reg32(r));c.clockI(2);break;}
        case S::RestoreData:case S::RestoreAddress:case S::RestorePrimary:{
            const int r=phase==S::RestoreData?2:phase==S::RestoreAddress?1:0;
            c.reg32(r)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);valueFlags(c,c.reg32(r));c.clockI(2);break;}
        case S::Finish:{
            c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);const auto saved=c.read32(int(sp));c.ccr=uint8_t(saved>>24);c.pc=c.makepc(int(saved&0xffffff));break;}
        case S::MaskStatus:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&0xbf);valueFlags(c,c.reg8(8));break;
        case S::CompareClock:{
            const int a=c.reg8(10),b=-8,r=a-b;
            c.ccr=uint8_t((c.ccr&~0x2f)|((r&255)?0:4)|((r&0x80)?8:0)
                |((((~a&b)|(~a&r)|(b&r))&0x80)?1:0)
                |((((~a&b)|(~a&r)|(b&r))&8)?32:0)
                |((((a&~b&~r)|(~a&b&r))&0x80)?2:0));break;}
        case S::DispatchClock:{
            const auto sp=c.getSP()-4;c.write32(0xa80,int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);c.clockI(2);break;}
        case S::NormalizeProducer:c.reg32(0)&=0xffff;valueFlags(c,c.reg32(0));break;
        case S::LocateData:c.reg32(1)=0xfffd30;valueFlags(c,c.reg32(1));break;
        case S::ResolveData:{
            const uint32_t a=uint32_t(c.reg32(1)),b=uint32_t(c.reg32(0)),r=a+b;
            c.ccr=uint8_t((c.ccr&~0x2f)|(r?0:4)|((r&0x80000000)?8:0)
                |((((a&b)|(a&~r)|(b&~r))&0x80000000)?1:0)
                |((((a&b)|(a&~r)|(b&~r))&0x8000000)?32:0)
                |((((a&b&~r)|(~a&~b&r))&0x80000000)?2:0));c.reg32(1)=int32_t(r);break;}
        case S::NextProducer:{const int old=c.reg16(0),value=old+1;valueFlags(c,value);if((~old&value)&0x8000)c.ccr|=2;c.reg16(0)=int16_t(value);break;}
        case S::MaskProducer:c.reg16(0)&=255;valueFlags(c,c.reg16(0));break;
        case S::CompareFull:{
            const int a=c.reg16(1),b=c.reg16(0),r=a-b;
            c.ccr=uint8_t((c.ccr&~0x2f)|((r&0xffff)?0:4)|((r&0x8000)?8:0)
                |((((~a&b)|(~a&r)|(b&r))&0x8000)?1:0)
                |((((~a&b)|(~a&r)|(b&r))&0x800)?32:0)
                |((((a&~b&~r)|(~a&b&r))&0x8000)?2:0));break;}
        case S::SetError:c.reg8(9)=2;valueFlags(c,c.reg8(9));break;
        default:break;
        }
    }
    static uint32_t boundary(S stage) {
        constexpr uint32_t locations[]{0xa64,0xa68,0xa6c,0xa70,0xa72,0xa74,0xa76,0xa78,0xa7a,0xa7c,0xa80,0xa82,0xa88,0xa8a,0xa90,0xa92,0xa94,0xa96,0xa9a,0xaa0,0xaa2,0xaa4,0xaaa,0xaac,0xaae,0xab4,0xab8,0xabc,0xac0};
        return stage==S::AwaitClock||stage==S::Complete?0:locations[unsigned(stage)];
    }
    static unsigned bytes(S stage) {
        switch(stage) {
        case S::RetainPrimary:case S::RetainAddress:case S::RetainData:case S::DispatchClock:
        case S::MaskProducer:case S::RestoreData:case S::RestoreAddress:case S::RestorePrimary:return 4;
        case S::ReadProducer:case S::LocateData:case S::ReadConsumer:case S::PublishProducer:case S::WriteError:return 6;
        default:return 2;
        }
    }
    std::array<SerialRxService,8> jobs_{};
    unsigned depth_=0;
};
}
