#pragma once

#include "commandSession.h"

#include <bitset>

namespace novation::controller::command {
    enum class GlobalSyncMode : uint8_t { FirstNote, Program, SongStart, Off = 255 };

    inline GlobalSyncMode globalSyncMode(uint8_t value) noexcept {
        return value <= 2 ? GlobalSyncMode(value) : GlobalSyncMode::Off;
    }

    class GlobalSyncKeys final {
    public:
        bool press(uint8_t note, GlobalSyncMode mode) noexcept {
            if (note >= keys_.size()) return false;
            const bool first = keys_.none();
            keys_.set(note);
            const bool program = programArmed_;
            programArmed_ = false;
            return (mode == GlobalSyncMode::FirstNote && first) || (mode == GlobalSyncMode::Program && program);
        }
        void release(uint8_t note) noexcept { if (note < keys_.size()) keys_.reset(note); }
        void clear() noexcept { keys_.reset(); }
        void programApplied(GlobalSyncMode mode) noexcept {
            clear();
            programArmed_ = mode == GlobalSyncMode::Program;
        }

    private:
        std::bitset<128> keys_;
        bool programArmed_ = false;
    };

    struct GlobalSyncPatch {
        uint8_t effectPositions;
        uint8_t lfoFlags;
        uint8_t lfoShapes;
    };

    inline constexpr uint16_t effectPhaseAddress(Model model, size_t effect, uint8_t part = 0) noexcept {
        constexpr std::array<uint16_t, 3> k{0x8181, 0x8184, 0x8180};
        constexpr std::array<uint16_t, 3> ks{0x824d, 0x8250, 0x824c};
        return model == Model::KS ? uint16_t(ks[effect] + 5 * part) : k[effect];
    }

    inline constexpr uint16_t lfoPhaseAddress(Model model, size_t lfo) noexcept {
        constexpr std::array<uint16_t, 2> k{0x8053, 0x8059};
        constexpr std::array<uint16_t, 2> ks{0x8086, 0x8089};
        return model == Model::KS ? ks[lfo] : k[lfo];
    }

    // These addressed words are phase accumulators in the DSP firmware.
    // A tempo change uses the independent rate words in DspMusicalClock.
    class DspGlobalSync final {
    public:
        explicit DspGlobalSync(Model model) noexcept : model_(model) {}
        bool pending() const noexcept { return count_ != 0; }
        size_t size() const noexcept { return count_; }

        bool append(GlobalSyncPatch patch, uint8_t part = 0) noexcept {
            if (active_ || (model_ == Model::K ? part != 0 : part >= 4) || writes_.size() - count_ < 5)
                return false;
            constexpr std::array<uint32_t, 8> lfoPhase{
                0x400000, 0x800000, 0x000000, 0x7fffff, 0x000000, 0x000000, 0x800000, 0x000000};
            const bool ks = model_ == Model::KS;
            for (unsigned i = 0; i < 3; ++i) {
                const auto position = (patch.effectPositions >> (2 * i)) & 3;
                if (position)
                    writes_[count_++] = {Endpoint::A, effectPhaseAddress(model_, i, part),
                                         uint32_t(position - 1) << 22};
            }
            for (unsigned i = 0; i < 2; ++i) {
                const auto flags = (patch.lfoFlags >> (3 * i)) & 7;
                if ((flags & 6) == 6) {
                    const auto shape = (patch.lfoShapes >> (2 + 2 * i)) & 3;
                    writes_[count_++] = {ks ? Endpoint::B : Endpoint::A, lfoPhaseAddress(model_, i),
                                         lfoPhase[4 * (flags & 1) + shape]};
                }
            }
            return true;
        }

        std::optional<DspTicket> ticket() const noexcept {
            return active_ ? std::optional<DspTicket>{tickets_[next_]} : std::nullopt;
        }

        DspAdmission admit(CommandSession& session, Session owner, uint64_t operation, NativeFrame at) noexcept {
            if (active_ || !pending() || session.model() != model_)
                return DspAdmission::InvalidDraft;
            std::array<DspTransactionDraft, Capacity> drafts{};
            for (size_t i = 0; i < count_; ++i) {
                const auto& write = writes_[i];
                auto& draft = drafts[i];
                draft.model = model_;
                draft.endpoint = write.endpoint;
                draft.earliest = at;
                draft.address = {DspAddressMode::Write, write.address};
                draft.words = &write.value;
                draft.wordCount = 1;
                if (i)
                    draft.dependency.earlierDraft = i - 1;
            }
            return session.transactions().submit(owner, operation, drafts.data(), count_, tickets_.data(), [&]() noexcept {
                active_ = true;
                next_ = 0;
            });
        }

        bool transferred(CommandSession& session, DspTicket completed) noexcept {
            const auto current = ticket();
            DspTransactionProgress progress;
            if (!current || completed.session != current->session || completed.sequence != current->sequence ||
                completed.endpoint != current->endpoint || completed.slot != current->slot ||
                !session.transactions().progress(completed, progress) || progress.status != DspStatus::Transferred ||
                !session.transactions().retire(completed))
                return false;
            if (++next_ == count_) {
                active_ = false;
                count_ = 0;
            }
            return true;
        }

    private:
        static constexpr size_t Capacity = 4 * 5;
        struct Write { Endpoint endpoint; uint16_t address; uint32_t value; };
        Model model_;
        std::array<Write, Capacity> writes_{};
        std::array<DspTicket, Capacity> tickets_{};
        size_t count_ = 0, next_ = 0;
        bool active_ = false;
    };
}
