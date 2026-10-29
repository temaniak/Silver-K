#pragma once

#include "kstationHdiWait.h"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace kstation
{
// Stage 2 closes two complete IRQ services. The bridge projects their typed
// phases into the existing caller frame; all other IRQs still execute H8.
class NativeIrq final
{
public:
    enum class Kind { None, Timer3, SerialError };
    struct Result
    {
        bool stepped = false, entered = false, returned = false;
        Kind kind = Kind::None;
    };
    struct CodeRange { std::uint32_t begin, end; };
    inline static constexpr std::array<CodeRange, 4> CodeRanges{{
        {38 * 4, 38 * 4 + 4}, {52 * 4, 52 * 4 + 4},
        {0x092c, 0x0946}, {0x0a50, 0x0a64}
    }};

    static bool candidate(std::uint32_t pc, std::uint64_t pending)
    { return pending || bodyCandidate(pc); }

    static Result step(Stage1Cpu& cpu)
    {
        const auto at = static_cast<std::uint32_t>(cpu.getPC());
        // The common foreground path pays no IRQ selection or task dispatch.
        if(!cpu.pending_irqs && !bodyCandidate(at)) return {};
        if(cpu.interruptMode2 || cpu.pending_irqs_high) return {};
        auto task = locate(at);
        int vector = -1;
        if(cpu.pending_irqs && !(cpu.ccr & 0x80))
        {
            vector = firstPending(cpu.pending_irqs);
            if(vector != 38 && vector != 52) return {};
        }
        else if(task.kind == Kind::None) return {};

        bool entered = false;
        if(cpu.pending_irqs)
        {
            // Even a masked IRQ reads SYSCR in the original mode-0 arbiter.
            // It runs before fetching the opcode and therefore observes old PC.
            const bool ue = (cpu.read8(0xfffff2) & 8) != 0;
            if(!(cpu.ccr & 0x80))
            {
                cpu.pending_irqs ^= std::uint64_t(1) << vector;
                const auto saved = (static_cast<std::uint32_t>(cpu.getPC()) & 0xffffff)
                    | (std::uint32_t(cpu.ccr) << 24);
                const auto stack = cpu.getSP() - 4;
                // IRQ pushPC keeps old SP visible until all four writes finish.
                cpu.write32(static_cast<std::int32_t>(saved), static_cast<int>(stack));
                cpu.setSP(stack);
                cpu.clockI(4);
                cpu.ccr |= 0x80;
                if(!ue) cpu.ccr |= 0x10;
                const auto target = cpu.read32(vector * 4) & 0xffffff;
                const auto expected = vector == 38 ? 0x092cu : 0x0a50u;
                if(target != expected)
                    throw std::logic_error("Native K IRQ vector binding changed");
                cpu.pc = cpu.makepc(static_cast<int>(target));
                cpu.irqDepth(cpu.irqDepth() + 1);
                task = {vector == 38 ? Kind::Timer3 : Kind::SerialError, Phase::SaveWork};
                entered = true;
            }
        }
        return {true, entered, execute(cpu, task), task.kind};
    }

private:
    enum class Phase
    {
        SaveWork, ReadStatus, ZeroStatus, MaskStatus, WriteStatus,
        ReadCount, IncrementCount, WriteCount, SetError, WriteError,
        RestoreWork, Return
    };
    struct Task { Kind kind = Kind::None; Phase phase = Phase::SaveWork; };
    static bool bodyCandidate(std::uint32_t at)
    { return (at >= 0x092c && at < 0x0946) || (at >= 0x0a50 && at < 0x0a64); }
    static Task locate(std::uint32_t at)
    {
        switch(at)
        {
        case 0x092c: return {Kind::Timer3, Phase::SaveWork};
        case 0x092e: return {Kind::Timer3, Phase::ReadStatus};
        case 0x0930: return {Kind::Timer3, Phase::ZeroStatus};
        case 0x0932: return {Kind::Timer3, Phase::WriteStatus};
        case 0x0934: return {Kind::Timer3, Phase::ReadCount};
        case 0x093a: return {Kind::Timer3, Phase::IncrementCount};
        case 0x093c: return {Kind::Timer3, Phase::WriteCount};
        case 0x0942: return {Kind::Timer3, Phase::RestoreWork};
        case 0x0944: return {Kind::Timer3, Phase::Return};
        case 0x0a50: return {Kind::SerialError, Phase::SaveWork};
        case 0x0a52: return {Kind::SerialError, Phase::ReadStatus};
        case 0x0a54: return {Kind::SerialError, Phase::MaskStatus};
        case 0x0a56: return {Kind::SerialError, Phase::WriteStatus};
        case 0x0a58: return {Kind::SerialError, Phase::SetError};
        case 0x0a5a: return {Kind::SerialError, Phase::WriteError};
        case 0x0a60: return {Kind::SerialError, Phase::RestoreWork};
        case 0x0a62: return {Kind::SerialError, Phase::Return};
        default: return {};
        }
    }
    static int firstPending(std::uint64_t bits)
    {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctzll(bits);
#else
        int vector = 0;
        while(!(bits & 1)) { bits >>= 1; ++vector; }
        return vector;
#endif
    }
    static void valueFlags(Stage1Cpu& cpu, int value)
    { cpu.ccr = std::uint8_t((cpu.ccr & ~14) | (value < 0 ? 8 : 0) | (value == 0 ? 4 : 0)); }
    static bool execute(Stage1Cpu& cpu, Task task)
    {
        const auto at = static_cast<std::uint32_t>(cpu.getPC());
        const unsigned length = task.phase == Phase::ReadCount || task.phase == Phase::WriteCount
            || task.phase == Phase::WriteError ? 6 : 2;
        cpu.fetchClocks(length);
        // H8 handle_instr advances member PC by the first opcode byte; the
        // operand handler advances its own local pointer while performing IO.
        cpu.pc = cpu.makepc(static_cast<int>(at + 1));
        const auto status = task.kind == Kind::Timer3 ? 0xffff85 : 0xffffb4;
        switch(task.phase)
        {
        case Phase::SaveWork:
            // Opcode PUSH.W decrements SP BEFORE writing, unlike IRQ pushPC.
            cpu.setSP(cpu.getSP() - 2);
            cpu.write16(cpu.reg16(0), static_cast<int>(cpu.getSP()));
            cpu.clockI(2);
            valueFlags(cpu, cpu.reg16(0));
            break;
        case Phase::ReadStatus:
            cpu.reg8(8) = cpu.read8(status);
            valueFlags(cpu, cpu.reg8(8));
            break;
        case Phase::ZeroStatus:
            cpu.reg8(8) = 0; valueFlags(cpu, 0); break;
        case Phase::MaskStatus:
            cpu.reg8(8) = std::int8_t(std::uint8_t(cpu.reg8(8)) & 0xc7);
            valueFlags(cpu, cpu.reg8(8)); break;
        case Phase::WriteStatus:
            // Byte MOV sets flags BEFORE the mapped store.
            valueFlags(cpu, cpu.reg8(8));
            cpu.write8(cpu.reg8(8), status); break;
        case Phase::ReadCount:
            cpu.reg16(0) = static_cast<std::int16_t>(cpu.read16(0xfffd28));
            valueFlags(cpu, cpu.reg16(0)); break;
        case Phase::IncrementCount:
        {
            // Preserve the reference full-int INC flags before word narrowing.
            const int old = cpu.reg16(0), value = old + 1;
            valueFlags(cpu, value);
            if((~old & value) & 0x8000) cpu.ccr |= 2;
            cpu.reg16(0) = static_cast<std::int16_t>(value);
            break;
        }
        case Phase::WriteCount:
            // MOV.W instead sets flags AFTER both data bytes are published.
            cpu.write16(cpu.reg16(0), 0xfffd28);
            valueFlags(cpu, cpu.reg16(0)); break;
        case Phase::SetError:
            cpu.reg8(8) = 2; valueFlags(cpu, 2); break;
        case Phase::WriteError:
            valueFlags(cpu, cpu.reg8(8));
            cpu.write8(cpu.reg8(8), 0xfffe30); break;
        case Phase::RestoreWork:
            // Opcode POP.W keeps old SP visible while reading; I2 and flags
            // follow SP increment, exactly as handle_movwaep in the oracle.
            cpu.reg16(0) = static_cast<std::int16_t>(cpu.read16(static_cast<int>(cpu.getSP())));
            cpu.setSP(cpu.getSP() + 2);
            cpu.clockI(2);
            valueFlags(cpu, cpu.reg16(0)); break;
        case Phase::Return:
        {
            cpu.irqDepth(cpu.irqDepth() - 1);
            cpu.clockI(2);
            const auto stack = cpu.getSP();
            // RTE/popPC advances SP BEFORE the four saved-frame reads.
            cpu.setSP(stack + 4);
            const auto saved = cpu.read32(static_cast<int>(stack));
            cpu.ccr = static_cast<std::uint8_t>(saved >> 24);
            cpu.pc = cpu.makepc(static_cast<int>(saved & 0xffffff));
            return true;
        }
        }
        cpu.pc = cpu.makepc(static_cast<int>(at + length));
        return false;
    }
};
}
