#pragma once

#include "h8s/h8s.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace kstation
{
// Only the five verified HDI status waits are native in this first stage.
// Every phase retains the original instruction boundary. The surrounding H8
// caller/IRQ frame is a temporary projection, not a complete native controller.
class HdiWaitTask final
{
public:
    enum class Phase { FirstRead, FirstMask, FirstTest, ClearCount,
                       Read, Mask, Test, Decrement, CountTest, Retry,
                       Ready, Timeout };
    struct State { Phase phase; uint8_t status, flags; uint16_t count; };

    template<class Read> static void advance(State& s, uint8_t mask, Read&& read)
    {
        switch(s.phase)
        {
        case Phase::FirstRead: case Phase::Read:
            s.status=read(); valueFlags(s,int8_t(s.status));
            s.phase=s.phase==Phase::FirstRead?Phase::FirstMask:Phase::Mask; break;
        case Phase::FirstMask: case Phase::Mask:
            s.status&=mask; valueFlags(s,int8_t(s.status));
            s.phase=s.phase==Phase::FirstMask?Phase::FirstTest:Phase::Test; break;
        case Phase::FirstTest:
            s.phase=(s.flags&4)?Phase::ClearCount:Phase::Ready; break;
        case Phase::ClearCount:
            s.count=0; s.flags=uint8_t((s.flags&~0x2f)|4); s.phase=Phase::Read; break;
        case Phase::Test:
            s.phase=(s.flags&4)?Phase::Decrement:Phase::Ready; break;
        case Phase::Decrement:
        {
            // Match the reference's full-width intermediate, including its
            // signed-word DEC flag behavior at 0x8000 (not hardware correction).
            const int old=int16_t(s.count), value=old-1;
            valueFlags(s,value);
            if((old&~value)&0x8000)s.flags|=2;
            s.count=uint16_t(value); s.phase=Phase::CountTest; break;
        }
        case Phase::CountTest: s.phase=(s.flags&4)?Phase::Timeout:Phase::Read; break;
        case Phase::Retry: s.phase=Phase::Read; break;
        case Phase::Ready: case Phase::Timeout: throw std::logic_error("Completed HDI wait phase");
        }
    }
private:
    static void valueFlags(State& s,int value)
    {
        s.flags=uint8_t((s.flags&~14)|(value<0?8:0)|(value==0?4:0));
    }
};

// No shared H8 core changes: expose only timing history for the verified K
// bridge and its differential tests. Opcode reads disappear; their clocks and
// read history remain exact. Ordinary data/MMIO reads still go through read8.
class Stage1Cpu final : public H8SEmulator
{
public:
    void fetchClocks(unsigned bytes)
    {
        const auto at=uint32_t(getPC());
        // All admitted K instructions are aligned and wholly inside low ROM.
        if((at&1)||at+bytes>0x10000||(bytes&1)||!bytes)
            throw std::logic_error("Invalid native K instruction boundary");
        cycles+=3*(bytes/2); lastread=int(at+bytes-1);
    }
    int readHistory()const{return lastread;}
    int writeHistory()const{return lastwrite;}
    void histories(int read,int write){lastread=read;lastwrite=write;}
    int irqDepth()const{return indent;}
    void irqDepth(int value){indent=value;}
};

class NativeHdiWait final
{
public:
    struct Site { uint32_t begin; uint8_t mask; };
    inline static constexpr std::array<Site,5> Sites{{
        {0x1672,2},{0x16ba,2},{0x1706,4},{0x1734,16},{0x177c,1}
    }};
    // The bridge never handles IRQ admission. Any pending IRQ, even masked,
    // uses the original arbiter because its SYSCR read itself costs a clock.
    static bool step(Stage1Cpu& cpu)
    {
        if(cpu.pending_irqs||cpu.pending_irqs_high||cpu.interruptMode2)return false;
        const auto pc=uint32_t(cpu.getPC());
        if(pc<0x1672||pc>0x179e)return false;
        for(const auto& site:Sites)
        {
            if(pc<site.begin||pc>site.begin+0x22)continue;
            HdiWaitTask::Phase phase;
            unsigned length=2;
            switch(pc-site.begin)
            {
            case 0: phase=HdiWaitTask::Phase::FirstRead; length=8; break;
            case 8: phase=HdiWaitTask::Phase::FirstMask; break;
            case 10: phase=HdiWaitTask::Phase::FirstTest; break;
            case 12: phase=HdiWaitTask::Phase::ClearCount; break;
            case 14: phase=HdiWaitTask::Phase::Read; length=8; break;
            case 22: phase=HdiWaitTask::Phase::Mask; break;
            case 24: phase=HdiWaitTask::Phase::Test; break;
            case 26: phase=HdiWaitTask::Phase::Decrement; break;
            case 28: phase=HdiWaitTask::Phase::CountTest; break;
            case 34: phase=HdiWaitTask::Phase::Retry; break;
            // The existing timeout subroutine and its return execute outside
            // the native wait; all other gaps are outside this task as well.
            default: return false;
            }
            if(uint32_t(cpu.reg32(5))!=0x600000)
                throw std::logic_error("Native HDI wait has no verified host port");
            HdiWaitTask::State s{phase,uint8_t(cpu.reg8(8)),cpu.ccr,uint16_t(cpu.reg16(8))};
            cpu.fetchClocks(length);
            // handle_instr has advanced its member PC by the first opcode byte
            // before the operand handler performs mapped data reads.
            cpu.pc=cpu.makepc(int(pc+1));
            HdiWaitTask::advance(s,site.mask,[&]{return uint8_t(cpu.read8(0x600004));});
            cpu.reg8(8)=int8_t(s.status); cpu.reg16(8)=int16_t(s.count); cpu.ccr=s.flags;
            cpu.pc=cpu.makepc(int(address(site,s.phase)));
            return true;
        }
        return false;
    }
private:
    static uint32_t address(Site site,HdiWaitTask::Phase phase)
    {
        switch(phase)
        {
        case HdiWaitTask::Phase::FirstRead: return site.begin;
        case HdiWaitTask::Phase::FirstMask: return site.begin+8;
        case HdiWaitTask::Phase::FirstTest: return site.begin+10;
        case HdiWaitTask::Phase::ClearCount: return site.begin+12;
        case HdiWaitTask::Phase::Read: return site.begin+14;
        case HdiWaitTask::Phase::Mask: return site.begin+22;
        case HdiWaitTask::Phase::Test: return site.begin+24;
        case HdiWaitTask::Phase::Decrement: return site.begin+26;
        case HdiWaitTask::Phase::CountTest: return site.begin+28;
        case HdiWaitTask::Phase::Timeout: return site.begin+30;
        case HdiWaitTask::Phase::Retry: return site.begin+34;
        case HdiWaitTask::Phase::Ready: return site.begin+36;
        }
        throw std::logic_error("Unknown HDI phase");
    }
};
}
