#pragma once
#include "kstationSerialTxService.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>

namespace kstation {
// Temporary H8 caller projection around a CPU-independent SerialTxService.
// The pure service owns the decisions and continuation; this adapter preserves
// architectural observations while the remaining controller still executes H8.
class NativeSerialTx final {
    using S=SerialTxService::Stage;
public:
    struct CodeRange { uint32_t begin,end; };
    inline static constexpr std::array<CodeRange,2> CodeRanges{{{54*4,54*4+4},{0xb50,0xbb2}}};
    struct Result { bool stepped=false,entered=false,returned=false; };
    static bool candidate(uint32_t pc,uint64_t pending) { return pending||(pc>=0xb50&&pc<0xbb2); }
    bool active() const { return depth_!=0; }
    unsigned depth() const { return depth_; }
    void reset() { depth_=0; }
    Result step(Stage1Cpu& cpu) {
        if(cpu.interruptMode2||cpu.pending_irqs_high)return {};
        bool enter=false;
        if(cpu.pending_irqs&&!(cpu.ccr&0x80)) {
            // Reject every unsupported lowest pending IRQ BEFORE any IO.
            if((cpu.pending_irqs&((uint64_t(1)<<54)-1))||!(cpu.pending_irqs&(uint64_t(1)<<54)))return {};
            if(depth_==jobs_.size())throw std::logic_error("Native K TX continuation capacity");
            enter=true;
        } else if(!depth_||uint32_t(cpu.getPC())!=boundary(jobs_[depth_-1].stage())) {
            const auto pc=uint32_t(cpu.getPC());
            if(pc>=0xb50&&pc<0xbb2)throw std::logic_error("Native K TX unowned caller boundary");
            return {};
        }
        if(cpu.pending_irqs) {
            const bool ue=(cpu.read8(0xfffff2)&8)!=0;
            if(enter) {
                cpu.pending_irqs&=~(uint64_t(1)<<54);
                const auto saved=(uint32_t(cpu.getPC())&0xffffff)|(uint32_t(cpu.ccr)<<24);
                const auto sp=cpu.getSP()-4;
                cpu.write32(int32_t(saved),int(sp));cpu.setSP(sp);cpu.clockI(4);
                cpu.ccr|=0x80;if(!ue)cpu.ccr|=0x10;
                if((cpu.read32(54*4)&0xffffff)!=0xb50)throw std::logic_error("Native K TX vector binding changed");
                cpu.pc=cpu.makepc(0xb50);cpu.irqDepth(cpu.irqDepth()+1);
                jobs_[depth_++]=SerialTxService{};
            }
        }
        auto& task=jobs_[depth_-1];
        if(uint32_t(cpu.getPC())!=boundary(task.stage()))throw std::logic_error("Native K TX caller boundary differs");
        const auto phase=task.stage();const auto at=uint32_t(cpu.getPC());
        const unsigned length=bytes(phase);
        const SerialTxService::Credits credits{
            8+busCost(cpu.getSP()-4,4,cpu.writeHistory()),
            8+busCost(cpu.getSP(),4,int(at+length-1)),
            5+busCost(cpu.getSP(),4,int(at+length-1)),
            busCost(0xfffe73,1,cpu.writeHistory())};
        const auto cost=task.nextClocks(credits);const auto begin=cpu.cycles;
        cpu.fetchClocks(length);cpu.pc=cpu.makepc(int(at+1));
        project(cpu,phase);
        const auto used=task.advanceBefore(cost,credits,[&](SerialTxService::Read field)->uint32_t {
            switch(field) {
            case SerialTxService::Read::Realtime:cpu.reg8(8)=cpu.read8(0xfffe73);valueFlags(cpu,cpu.reg8(8));return uint8_t(cpu.reg8(8));
            case SerialTxService::Read::Consumer:cpu.reg16(0)=int16_t(cpu.read16(0xc494));valueFlags(cpu,cpu.reg16(0));return uint16_t(cpu.reg16(0));
            case SerialTxService::Read::Producer:cpu.reg16(8)=int16_t(cpu.read16(0xc492));valueFlags(cpu,cpu.reg16(8));return uint16_t(cpu.reg16(8));
            case SerialTxService::Read::Data:cpu.reg8(8)=cpu.read8(int(task.dataReference()));valueFlags(cpu,cpu.reg8(8));return uint8_t(cpu.reg8(8));
            case SerialTxService::Read::Status:cpu.reg8(8)=cpu.read8(0xffffb4);valueFlags(cpu,cpu.reg8(8));return uint8_t(cpu.reg8(8));
            }
            throw std::logic_error("Native K TX read kind");
        },[&](SerialTxService::Write field,uint16_t value) {
            switch(field) {
            case SerialTxService::Write::Consumer:cpu.write16(int16_t(value),0xc494);valueFlags(cpu,cpu.reg16(0));break;
            case SerialTxService::Write::Realtime:valueFlags(cpu,cpu.reg8(0));cpu.write8(int8_t(value),0xfffe73);break;
            case SerialTxService::Write::Control:valueFlags(cpu,cpu.reg8(8));cpu.write8(int8_t(value),0xffffb2);break;
            case SerialTxService::Write::Transmit:valueFlags(cpu,cpu.reg8(8));cpu.write8(int8_t(value),0xffffb3);break;
            case SerialTxService::Write::Status:valueFlags(cpu,cpu.reg8(8));cpu.write8(int8_t(value),0xffffb4);break;
            }
        });
        if(used!=cost||cpu.cycles-begin!=cost)throw std::logic_error("Native K TX phase clocks differ");
        const bool returned=task.complete();
        if(returned)--depth_;else cpu.pc=cpu.makepc(int(boundary(task.stage())));
        return {true,enter,returned};
    }
private:
    static unsigned busCost(uint32_t at,unsigned bytes,int previous) {
        unsigned cost=0;
        for(unsigned n=0;n<bytes;++n) {
            const auto a=(at+n)&0xffffff;
            if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)) {
                if((previous&1)||a!=uint32_t(previous|1))cost+=a<0x400000?3:1;
                previous=int(a);
            } else {cost+=a>=0xffffa0?1:3;previous=-1;}
        }
        return cost;
    }
    static void valueFlags(Stage1Cpu& cpu,int value) {cpu.ccr=uint8_t((cpu.ccr&~14)|(value<0?8:0)|(value==0?4:0));}
    static void project(Stage1Cpu& c,S phase) {
        switch(phase) {
        case S::RetainPrimary: case S::RetainRealtimeSource: case S::RetainQueuedSource: {
            const int r=phase==S::RetainPrimary?0:1;c.setSP(c.getSP()-4);c.write32(c.reg32(r),int(c.getSP()));valueFlags(c,c.reg32(r));c.clockI(2);break;}
        case S::RestoreEmpty: case S::RestorePrimary: case S::RestoreSource: {
            const int r=phase==S::RestoreSource?1:0;c.reg32(r)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);valueFlags(c,c.reg32(r));c.clockI(2);break;}
        case S::FinishEmpty: case S::Finish: {
            c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);const auto saved=c.read32(int(sp));c.ccr=uint8_t(saved>>24);c.pc=c.makepc(int(saved&0xffffff));break;}
        case S::ClearRealtimeValue:c.reg8(0)=0;valueFlags(c,0);break;
        case S::CompareQueue:{
            const int a=c.reg16(0),b=c.reg16(8),r=a-b;
            c.ccr=uint8_t((c.ccr&~0x2f)|((r&0xffff)?0:4)|((r&0x8000)?8:0)
                |((((~a&b)|(~a&r)|(b&r))&0x8000)?1:0)
                |((((~a&b)|(~a&r)|(b&r))&0x800)?32:0)
                |((((a&~b&~r)|(~a&b&r))&0x8000)?2:0));break;}
        case S::DisableValue:c.reg8(8)=0x70;valueFlags(c,c.reg8(8));break;
        case S::LocateData:c.reg32(1)=0xc496;valueFlags(c,c.reg32(1));break;
        case S::NormalizeConsumer:c.reg32(0)&=0xffff;valueFlags(c,c.reg32(0));break;
        case S::ResolveData:{
            const uint32_t a=uint32_t(c.reg32(1)),b=uint32_t(c.reg32(0)),r=a+b;
            c.ccr=uint8_t((c.ccr&~0x2f)|(r?0:4)|((r&0x80000000)?8:0)
                |((((a&b)|(a&~r)|(b&~r))&0x80000000)?1:0)
                |((((a&b)|(a&~r)|(b&~r))&0x8000000)?32:0)
                |((((a&b&~r)|(~a&~b&r))&0x80000000)?2:0));
            c.reg32(1)=int32_t(r);break;}
        case S::NextConsumer:{const int old=c.reg16(0),value=old+1;valueFlags(c,value);if((~old&value)&0x8000)c.ccr|=2;c.reg16(0)=int16_t(value);break;}
        case S::MaskConsumer:c.reg16(0)&=255;valueFlags(c,c.reg16(0));break;
        case S::MaskStatus:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&0x7f);valueFlags(c,c.reg8(8));break;
        default:break;
        }
    }
    static uint32_t boundary(S stage) {
        constexpr uint32_t locations[]{0xb50,0xb54,0xb5a,0xb5c,0xb5e,0xb64,0xb68,0xb6a,0xb70,0xb76,0xb78,0xb7a,0xb7c,0xb7e,0xb82,0xb84,0xb88,0xb8e,0xb90,0xb92,0xb94,0xb98,0xb9e,0xba0,0xba2,0xba4,0xba6,0xba8,0xbac,0xbb0};
        return stage==S::Complete?0:locations[unsigned(stage)];
    }
    static unsigned bytes(S s) {
        switch(s) {
        case S::RetainPrimary:case S::RetainRealtimeSource:case S::RetainQueuedSource:
        case S::RestoreEmpty:case S::RestoreSource:case S::RestorePrimary:case S::MaskConsumer:return 4;
        case S::ReadRealtime:case S::ClearRealtime:case S::ReadConsumer:case S::ReadProducer:case S::LocateData:case S::PublishConsumer:return 6;
        default:return 2;
        }
    }
    // K's IRQ body never unmasks itself. Fixed capacity also supports bounded
    // diagnostic nested admissions without allocating in the processing path.
    std::array<SerialTxService,8> jobs_{};
    unsigned depth_=0;
};
}
