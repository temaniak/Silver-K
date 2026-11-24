#pragma once

#include "../../../controller/command/commandSession.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace novation::controller::command
{
    struct KSRefreshWrite
    {
        Endpoint endpoint;
        uint16_t address;
        const uint32_t* words;
        size_t count;
    };

    // Each entry has an explicit address selection. A multiword entry is a
    // genuine contiguous stream on that address cursor.
    class KSRefreshPacket final
    {
    public:
        static constexpr size_t MaxWrites = 96;
        // All fourteen physical voice images, four tracking writes and up to 63 global/FX
        // words, including the prior-effect-status transition.
        static constexpr size_t MaxWords = 14*100 + 4 + 63;

        bool canAppend(size_t writes, size_t words) const noexcept
        {
            return writes <= MaxWrites - writes_ && words <= MaxWords - words_;
        }

        bool append(Endpoint endpoint, uint16_t address, const uint32_t* data, size_t count) noexcept
        {
            if (endpoint > Endpoint::B || !data || !count || count > DspTransactionQueue::MaxWords ||
                !canAppend(1, count) ||
                !std::all_of(data, data + count, [](uint32_t value) { return value <= 0xffffff; }))
                return false;
            std::copy_n(data, count, payload_.data() + words_);
            layout_[writes_++] = {endpoint, address, uint16_t(words_), uint16_t(count)};
            words_ += count;
            return true;
        }

        bool appendWord(Endpoint endpoint, uint16_t address, uint32_t value) noexcept
        {
            return append(endpoint, address, &value, 1);
        }

        std::optional<KSRefreshWrite> at(size_t index) const noexcept
        {
            if (index >= writes_)
                return std::nullopt;
            const auto& entry = layout_[index];
            return KSRefreshWrite{entry.endpoint, entry.address, payload_.data() + entry.offset, entry.count};
        }

        size_t size() const noexcept { return writes_; }
        size_t wordCount() const noexcept { return words_; }

    private:
        struct Entry
        {
            Endpoint endpoint;
            uint16_t address;
            uint16_t offset;
            uint16_t count;
        };

        std::array<Entry, MaxWrites> layout_{};
        std::array<uint32_t, MaxWords> payload_{};
        size_t writes_ = 0;
        size_t words_ = 0;
    };

    enum class KSRefreshProgress : uint8_t { Ready, WaitingForTransfer, Complete, Fault };

    // The packet is a bounded collection of physical transfers. Only the
    // currently leased endpoint can progress the global packet cursor.
    class KSRefreshTransfer final
    {
    public:
        KSRefreshTransfer(Session owner, KSRefreshPacket packet) noexcept
            : owner_(owner), packet_(std::move(packet)) {}

        KSRefreshProgress progress(CommandSession& session) noexcept
        {
            const auto snapshot = session.snapshot();
            if (snapshot.session != owner_ || snapshot.lifecycle != Lifecycle::Running)
                return KSRefreshProgress::Fault;
            if (ticket_)
            {
                DspTransactionProgress progress;
                auto& transactions = session.transactions();
                if (!transactions.progress(*ticket_, progress) ||
                    (progress.status != DspStatus::Queued && progress.status != DspStatus::Transferred))
                    return KSRefreshProgress::Fault;
                if (progress.status == DspStatus::Queued)
                    return KSRefreshProgress::WaitingForTransfer;
                if (!transactions.retire(*ticket_))
                    return KSRefreshProgress::Fault;
                ticket_.reset();
                ++next_;
            }
            return next_ == packet_.size() ? KSRefreshProgress::Complete : KSRefreshProgress::Ready;
        }

        DspAdmission admit(CommandSession& session, uint64_t operation, NativeFrame at) noexcept
        {
            if (ticket_)
                return DspAdmission::Full;
            const auto write = packet_.at(next_);
            if (!write)
                return DspAdmission::InvalidDraft;
            DspTransactionDraft draft;
            draft.model = Model::KS;
            draft.endpoint = write->endpoint;
            draft.earliest = at;
            draft.words = write->words;
            draft.wordCount = write->count;
            draft.address = {DspAddressMode::Write, write->address};
            DspTicket ticket;
            const auto result = session.transactions().submit(owner_, operation, &draft, 1, &ticket, []() noexcept {});
            if (result == DspAdmission::Accepted)
                ticket_ = ticket;
            return result;
        }

        std::optional<DspTicket> ticket() const noexcept { return ticket_; }

    private:
        Session owner_;
        KSRefreshPacket packet_;
        std::optional<DspTicket> ticket_;
        size_t next_ = 0;
    };
}
