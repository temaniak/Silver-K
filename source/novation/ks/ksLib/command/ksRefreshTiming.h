#pragma once

#include "ksMusicalState.h"
#include "ksRefreshPacket.h"

namespace novation::controller::command
{
    // Research approximation retained by the existing diagnostic candidate.
    // It projects ideal 25,000-cycle ticks; real H8 publication occurs after
    // IRQ admission/work, not at the compare edge. A causal resource producer
    // must use KSObservedRefreshClock instead of treating these as commits.
    class KSRefreshClock final
    {
    public:
        static constexpr uint64_t CounterPeriod = 25000;
        static constexpr uint16_t RefreshTicks = 25;

        static std::optional<KSRefreshClock> prepare(BusCycle origin, uint16_t counter, uint16_t lastRefresh,
                                                    uint64_t untilNextTick) noexcept
        {
            if (!untilNextTick || untilNextTick > CounterPeriod)
                return std::nullopt;
            return KSRefreshClock(origin, counter, lastRefresh, untilNextTick);
        }

        std::optional<BusCycle> nextDeadline(BusCycle now) const noexcept
        {
            if (now < origin_)
                return std::nullopt;
            const auto sample = observe(now);
            const auto elapsed = uint16_t(sample.counter - lastRefresh_);
            if (elapsed >= RefreshTicks)
                return now;
            const auto delay = sample.untilNext + (RefreshTicks - elapsed - 1) * CounterPeriod;
            if (delay > std::numeric_limits<uint64_t>::max() - now.ticks)
                return std::nullopt;
            return BusCycle{now.ticks + delay};
        }

        // The admitted tick and the mandatory signal become visible together.
        // Failure to reserve the actual transaction leaves the deadline due.
        DspAdmission admit(CommandSession& session, Session owner, uint64_t operation, BusCycle now,
                           NativeFrame at, uint8_t activeMask, DspTicket& signal) noexcept
        {
            const auto deadline = nextDeadline(now);
            if (!deadline || *deadline != now)
                return DspAdmission::InvalidDraft;
            const auto sample = observe(now);
            const uint32_t word = activeMask;
            DspTransactionDraft draft;
            draft.model = Model::KS;
            draft.earliest = at;
            draft.words = &word;
            draft.wordCount = 1;
            draft.address = {DspAddressMode::Write, 0x8056};
            return session.transactions().submit(owner, operation, &draft, 1, &signal,
                [&]() noexcept { lastRefresh_ = sample.counter; });
        }

    private:
        struct Sample
        {
            uint16_t counter;
            uint64_t untilNext;
        };

        KSRefreshClock(BusCycle origin, uint16_t counter, uint16_t lastRefresh, uint64_t untilNext) noexcept
            : origin_(origin), counter_(counter), lastRefresh_(lastRefresh), untilNext_(untilNext) {}

        Sample observe(BusCycle now) const noexcept
        {
            const auto delta = now.ticks - origin_.ticks;
            if (delta < untilNext_)
                return {counter_, untilNext_ - delta};
            const auto ticks = delta - untilNext_;
            return {uint16_t(uint64_t(counter_) + 1 + ticks / CounterPeriod), CounterPeriod - ticks % CounterPeriod};
        }

        BusCycle origin_;
        uint16_t counter_;
        uint16_t lastRefresh_;
        uint64_t untilNext_;
    };

    enum class KSDeferredSelection : uint8_t { NoEdit, Cooldown, Single, Performance, Unsupported, AlreadyOwned };

    // Called after the periodic signal and any independently selected aux
    // write have completed. The admitted dirty set stays owned until its last part transfers.
    class KSDeferredRefresh final
    {
    public:
        explicit KSDeferredRefresh(uint8_t cooldown = 0) noexcept : cooldown_(cooldown) {}

        KSDeferredSelection preview(const KSMusicalState& state) const noexcept
        {
            if (ownedParts_)
                return KSDeferredSelection::AlreadyOwned;
            if (cooldown_)
                return KSDeferredSelection::Cooldown;
            const auto mask = state.dirtySlots();
            if (mask.none())
                return KSDeferredSelection::NoEdit;
            const bool includesSingle = mask.test(size_t(KSStorageSlot::Single));
            if (state.layout() == KSLayout::Performance)
                return includesSingle && !state.part(KSStorageSlot::Single) ? KSDeferredSelection::Unsupported : KSDeferredSelection::Performance;
            return includesSingle && mask.count() == 1 ? KSDeferredSelection::Single : KSDeferredSelection::Unsupported;
        }

        bool commit(KSMusicalState& state, KSDeferredSelection selected) noexcept
        {
            if (selected != preview(state))
                return false;
            switch (selected)
            {
            case KSDeferredSelection::NoEdit:
                return true;
            case KSDeferredSelection::Cooldown:
                --cooldown_;
                return true;
            case KSDeferredSelection::Single:
            case KSDeferredSelection::Performance:
                ownedParts_ = uint8_t(state.takeDirtySlots().count());
                return true;
            default:
                return false;
            }
        }

        bool finish() noexcept
        {
            if (!ownedParts_)
                return false;
            cooldown_ = ownedParts_;
            ownedParts_ = 0;
            return true;
        }

    private:
        uint8_t cooldown_;
        uint8_t ownedParts_ = 0;
    };
}
