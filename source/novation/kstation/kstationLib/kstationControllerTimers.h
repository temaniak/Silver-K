#pragma once

#include "kstationControllerDeviceState.h"
#include <array>
#include <cstdint>

namespace kstation
{
    // K-only equivalent of the original Timers plus the KStationTimers MDF
    // adapter. Register writes do not advance time. tick() publishes the same
    // coalesced pending IRQ bits at the caller's original instruction boundary.
    class ControllerTimers final
    {
    public:
        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t),
                      "The reference timer increment must remain 32-bit");
        // Same valid-channel precondition (0..4) as the original peripheral.
        void tick_extclock(int which) { ++channels_[static_cast<unsigned>(which)].gra; }

        void tick(ControllerDeviceState& state)
        {
            const auto now = state.cycles;
            for(unsigned i = 0; i < channels_.size(); ++i)
            {
                if(!(activeMask_ & (1u << i))) continue;
                auto& c = channels_[i];
                const auto shift = c.tcr & 3;
                // The old peripheral deliberately narrows the shifted delta
                // to unsigned int. Keep its 32-bit truncation, including wrap.
                const auto count = static_cast<std::uint32_t>(
                    (now >> shift) - (lastCycles_ >> shift));
                if(!count) continue;
                const auto flags = advance(c, count);
                c.tsr |= flags;
                const auto pending = flags & c.tier;
                if(pending & 1) state.requestInterrupt(24 + static_cast<int>(i) * 4);
                if(pending & 2) state.requestInterrupt(25 + static_cast<int>(i) * 4);
                if(pending & 4) state.requestInterrupt(26 + static_cast<int>(i) * 4);
            }
            // Halted, PWM, externally clocked and MDF channels do not retain
            // elapsed clocks for a later enable; the original always updates it.
            lastCycles_ = now;
        }

        std::uint8_t read(std::uint32_t address)
        {
            if(address < Base || address >= Base + 64) return 0;
            auto offset = address - Base;
            switch(offset)
            {
            case 0: return tstr_;
            case 1: return tsnc_;
            case 2: return tmdr_;
            case 3: return tfcr_;
            default: break;
            }
            const auto index = channelIndex(offset);
            if(index < 0) return space_[offset];
            const auto& c = channels_[static_cast<unsigned>(index)];
            switch(offset)
            {
            case 0: return c.tcr;
            case 1: return 0; // TIOR is ignored by the reference peripheral.
            case 2: return c.tier;
            case 3: return c.tsr;
            case 4: return static_cast<std::uint8_t>(c.tcnt >> 8);
            case 5: return static_cast<std::uint8_t>(c.tcnt);
            case 6: return static_cast<std::uint8_t>(c.gra >> 8);
            case 7: return static_cast<std::uint8_t>(c.gra);
            case 8: return static_cast<std::uint8_t>(c.grb >> 8);
            case 9: return static_cast<std::uint8_t>(c.grb);
            default: return 0;
            }
        }

        void write(std::uint32_t address, std::uint8_t value)
        {
            if(address < Base || address >= Base + 64) return;
            auto offset = address - Base;
            switch(offset)
            {
            case 0: tstr_ = value; updateActiveMask(); return;
            case 1: tsnc_ = value; return;
            case 2: tmdr_ = value; updateActiveMask(); return;
            case 3: tfcr_ = value; return;
            default: break;
            }
            const auto index = channelIndex(offset);
            if(index < 0) { space_[offset] = value; return; }
            auto& c = channels_[static_cast<unsigned>(index)];
            switch(offset)
            {
            case 0: c.tcr = value; updateActiveMask(); return;
            case 1: return;
            case 2: c.tier = value; return;
            case 3: c.tsr = value; return;
            case 4: c.tcnt = static_cast<std::uint16_t>((c.tcnt & 0xffu) | (unsigned(value) << 8)); return;
            case 5: c.tcnt = static_cast<std::uint16_t>((c.tcnt & 0xff00u) | value); return;
            case 6: c.gra = static_cast<std::uint16_t>((c.gra & 0xffu) | (unsigned(value) << 8)); return;
            case 7: c.gra = static_cast<std::uint16_t>((c.gra & 0xff00u) | value); return;
            case 8: c.grb = static_cast<std::uint16_t>((c.grb & 0xffu) | (unsigned(value) << 8)); return;
            case 9: c.grb = static_cast<std::uint16_t>((c.grb & 0xff00u) | value); return;
            default: return;
            }
        }

    private:
        static constexpr std::uint32_t Base = 0xffff60;
        struct Channel
        {
            std::uint16_t tcnt = 0, gra = 0xffff, grb = 0xffff;
            std::uint8_t tcr = 0x80, tsr = 0xf8, tier = 0xf8;
        };

        static int channelIndex(std::uint32_t& offset)
        {
            constexpr std::array<unsigned, 5> starts{{4, 14, 24, 34, 50}};
            for(unsigned i = 0; i < starts.size(); ++i)
                if(offset >= starts[i] && offset < starts[i] + 10)
                { offset -= starts[i]; return static_cast<int>(i); }
            return -1;
        }

        void updateActiveMask()
        {
            unsigned mask = tstr_ & ~tmdr_ & 0x1fu;
            if(tmdr_ & 0x40) mask &= ~(1u << 2);
            for(unsigned i = 0; i < channels_.size(); ++i)
                if(channels_[i].tcr & 4) mask &= ~(1u << i);
            activeMask_ = static_cast<std::uint8_t>(mask);
        }

        static std::uint8_t advance(Channel& c, std::uint32_t count)
        {
            const auto start = c.tcnt;
            if(count == 1)
            {
                auto next = static_cast<std::uint16_t>(start + 1);
                std::uint8_t flags = 0;
                if(start == c.gra)
                {
                    flags |= 1;
                    if((c.tcr & 0x60) == 0x20) next = 0;
                }
                if(start == c.grb)
                {
                    flags |= 2;
                    if((c.tcr & 0x60) == 0x40) next = 0;
                }
                if(!start) flags |= 4;
                c.tcnt = next;
                return flags;
            }
            // Events inspect the old counter. Equality leaves the event at
            // the next counter, outside this interval; including zero also
            // excludes wraparound before the last increment.
            if(count <= static_cast<std::uint16_t>(c.gra - start)
               && count <= static_cast<std::uint16_t>(c.grb - start)
               && count <= static_cast<std::uint16_t>(0u - start))
            {
                c.tcnt = static_cast<std::uint16_t>(std::uint32_t(start) + count);
                return 0;
            }
            const auto mode = c.tcr & 0x60;
            const bool clears = mode == 0x20 || mode == 0x40;
            const auto clearAt = mode == 0x20 ? c.gra : c.grb;
            // Each event tests the OLD counter before the increment. After
            // the first clear, subsequent cycles visit 0..clearAt inclusive.
            const auto first = static_cast<std::uint16_t>(clearAt - start);
            const auto visits = [&](std::uint16_t value)
            {
                const auto distance = static_cast<std::uint16_t>(value - start);
                if(!clears) return std::uint32_t(distance) < count;
                if(distance <= first && std::uint32_t(distance) < count) return true;
                const auto prefix = std::uint32_t(first) + 1;
                return count > prefix && value <= clearAt
                    && std::uint32_t(value) < count - prefix;
            };
            const auto flags = static_cast<std::uint8_t>(
                (visits(c.gra) ? 1 : 0) | (visits(c.grb) ? 2 : 0)
                | (visits(0) ? 4 : 0));
            const auto prefix = std::uint32_t(first) + 1;
            if(clears && count >= prefix)
                c.tcnt = static_cast<std::uint16_t>(
                    (count - prefix) % (std::uint32_t(clearAt) + 1));
            else c.tcnt = static_cast<std::uint16_t>(std::uint32_t(start) + count);
            return flags;
        }

        std::uint8_t activeMask_ = 0;
        std::uint64_t lastCycles_ = 0;
        std::array<std::uint8_t, 64> space_{};
        std::uint8_t tstr_ = 0xc0, tsnc_ = 0xc0, tmdr_ = 0x80, tfcr_ = 0xc0;
        std::array<Channel, 5> channels_{};
    };
}
