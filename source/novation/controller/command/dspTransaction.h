#pragma once

#include "controllerEvent.h"
#include "controllerTime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace novation::controller::command
{
    enum class DspMilestone : uint8_t { Transfer, Observation };
    enum class DspStatus : uint8_t { Queued, Transferred, Observed, Cancelled, Poisoned };
    enum class DspAdmission : uint8_t
    {
        Accepted, StaleSession, StaleOperation, InvalidDraft, Full, Poisoned, SequenceExhausted
    };
    enum class DspOfferStatus : uint8_t
    {
        Ready, Empty, Early, DependencyPending, DependencyFailed, AwaitingObservation,
        Poisoned, StaleSession, InvalidEndpoint
    };
    enum class DspCancellation : uint8_t { Rejected, Cancelled, Poisoned };
    enum class DspAddressMode : uint8_t { None, Write, Read };

    struct DspAddress
    {
        DspAddressMode mode = DspAddressMode::None;
        uint16_t address = 0;
    };

    struct DspTicket
    {
        Session session{};
        uint64_t sequence = 0;
        Endpoint endpoint = Endpoint::A;
        uint8_t slot = 0;
    };

    struct DspDependency
    {
        static constexpr size_t NoDraft = std::numeric_limits<size_t>::max();
        DspTicket ticket{};
        size_t earlierDraft = NoDraft;
        DspMilestone milestone = DspMilestone::Transfer;
    };

    struct DspTransactionDraft
    {
        Model model = Model::K;
        Endpoint endpoint = Endpoint::A;
        NativeFrame earliest{};
        uint64_t barrier = 0;
        DspDependency dependency{};
        DspMilestone releaseEndpoint = DspMilestone::Transfer;
        const uint32_t* words = nullptr;
        size_t wordCount = 0;
        DspAddress address{};
    };

    struct DspTransactionProgress
    {
        DspTicket ticket{};
        Model model = Model::K;
        NativeFrame earliest{};
        uint64_t barrier = 0;
        DspTicket prerequisite{};
        DspMilestone required = DspMilestone::Transfer;
        DspMilestone releaseEndpoint = DspMilestone::Transfer;
        size_t wordCount = 0;
        size_t accepted = 0;
        DspStatus status = DspStatus::Queued;
        DspAddress address{};
    };

    struct DspOffer
    {
        DspOfferStatus status = DspOfferStatus::Empty;
        DspTicket ticket{};
        NativeFrame earliest{};
        size_t offset = 0;
        uint32_t word = 0;
    };

    // A single controller worker owns this queue. It copies draft payloads and retains the
    // resulting records until explicit retirement. Physical I/O and observation are reported
    // by the endpoint adapter. A barrier identifies a model-defined fence; a dependency names
    // the precise predecessor and milestone that must have been reached before publication.
    class DspTransactionQueue
    {
    public:
        static constexpr size_t MaxWords = 128;
        static constexpr size_t EndpointCapacity = 16;
        static constexpr size_t BatchCapacity = 2 * EndpointCapacity;

        DspTransactionQueue() = default;
        DspTransactionQueue(const DspTransactionQueue&) = delete;
        DspTransactionQueue& operator=(const DspTransactionQueue&) = delete;

        // Use only after the physical endpoints have been cold reset.
        bool coldReset(Session session, Model model) noexcept
        {
            if (!session.owner || session.generation <= m_session.generation ||
                (m_session.owner && session.owner != m_session.owner) ||
                (model != Model::K && model != Model::KS))
                return false;

            for (auto& lane : m_lanes)
            {
                for (auto& slot : lane.slots)
                    slot.progress.ticket.sequence = 0;
                lane.head = EndpointCapacity;
                lane.poisoned = false;
                lane.retained = lane.highWater = 0;
            }
            m_session = session;
            m_model = model;
            m_lastSequence = 0;
            m_lastOperation = 0;
            m_running = true;
            return true;
        }

        bool stop(Session session) noexcept
        {
            if (!matches(session))
                return false;

            for (auto& lane : m_lanes)
            {
                if (lane.head != EndpointCapacity && lane.slots[lane.head].touched)
                    lane.poisoned = true;
            }
            m_running = false;
            return true;
        }

        bool poisoned(Endpoint endpoint) const noexcept
        {
            return validEndpoint(endpoint) && m_lanes[index(endpoint)].poisoned;
        }

        size_t highWater(Endpoint endpoint) const noexcept
        {
            return validEndpoint(endpoint) ? m_lanes[index(endpoint)].highWater : 0;
        }

        size_t available(Endpoint endpoint) const noexcept
        {
            return validEndpoint(endpoint) ? EndpointCapacity-m_lanes[index(endpoint)].retained : 0;
        }

        // Pass a dispatcher operation ID, increasing in commit order within the session.
        // On rejection, the queue, optional ticket output and musical state stay intact.
        // The noexcept commit executes once after publication and must not re-enter the queue.
        template<class Commit>
        DspAdmission submit(Session session, uint64_t operation, const DspTransactionDraft* drafts,
                            size_t count, DspTicket* tickets, Commit&& commit) noexcept
        {
            static_assert(std::is_nothrow_invocable_v<Commit&&>);
            if (!matches(session))
                return DspAdmission::StaleSession;
            if (operation <= m_lastOperation)
                return DspAdmission::StaleOperation;
            if (!drafts || count == 0 || count > BatchCapacity)
                return DspAdmission::InvalidDraft;
            if (count > std::numeric_limits<uint64_t>::max() - m_lastSequence)
                return DspAdmission::SequenceExhausted;

            std::array<DspTicket, BatchCapacity> reserved{};
            std::array<size_t, 2> nextSlot{};
            for (size_t i = 0; i < count; ++i)
            {
                const auto& draft = drafts[i];
                if (!validDraft(draft, i))
                    return DspAdmission::InvalidDraft;

                const auto endpoint = index(draft.endpoint);
                const auto& lane = m_lanes[endpoint];
                if (lane.poisoned)
                    return DspAdmission::Poisoned;

                auto& slot = nextSlot[endpoint];
                while (slot < EndpointCapacity && lane.slots[slot].progress.ticket.sequence)
                    ++slot;
                if (slot == EndpointCapacity)
                    return DspAdmission::Full;
                reserved[i] = {session, m_lastSequence + i + 1, draft.endpoint, static_cast<uint8_t>(slot++)};
            }

            for (size_t i = 0; i < count; ++i)
            {
                const auto& draft = drafts[i];
                const auto& ticket = reserved[i];
                auto& lane = m_lanes[index(ticket.endpoint)];
                auto& slot = lane.slots[ticket.slot];
                const auto prerequisite = draft.dependency.earlierDraft == DspDependency::NoDraft
                    ? draft.dependency.ticket : reserved[draft.dependency.earlierDraft];
                slot.progress = {ticket, draft.model, draft.earliest, draft.barrier, prerequisite,
                                 draft.dependency.milestone, draft.releaseEndpoint,
                                 draft.wordCount + (draft.address.mode != DspAddressMode::None), 0,
                                 DspStatus::Queued, draft.address};
                if (draft.wordCount)
                    std::copy_n(draft.words, draft.wordCount, slot.words.begin());
                slot.touched = false;
                slot.resolvedDependency = DependencyStatus::Pending;
                if (lane.head == EndpointCapacity)
                    lane.head = ticket.slot;
                lane.highWater = std::max(lane.highWater, ++lane.retained);
            }
            if (tickets)
                std::copy_n(reserved.begin(), count, tickets);
            m_lastSequence += count;
            m_lastOperation = operation;
            std::forward<Commit>(commit)();
            return DspAdmission::Accepted;
        }

        DspOffer offer(Session session, Endpoint endpoint, NativeFrame now) const noexcept
        {
            if (!matches(session))
                return {DspOfferStatus::StaleSession};
            if (!validEndpoint(endpoint))
                return {DspOfferStatus::InvalidEndpoint};
            const auto& lane = m_lanes[index(endpoint)];
            if (lane.poisoned)
                return {DspOfferStatus::Poisoned};
            if (lane.head == EndpointCapacity)
                return {DspOfferStatus::Empty};

            const auto& slot = lane.slots[lane.head];
            const auto& progress = slot.progress;
            DspOffer next{DspOfferStatus::Ready, progress.ticket, progress.earliest, progress.accepted};
            if (progress.status == DspStatus::Transferred)
                next.status = DspOfferStatus::AwaitingObservation;
            else if (now < progress.earliest)
                next.status = DspOfferStatus::Early;
            else if (const auto dependency = dependencyStatus(slot); dependency != DependencyStatus::Ready)
                next.status = dependency == DependencyStatus::Pending
                    ? DspOfferStatus::DependencyPending : DspOfferStatus::DependencyFailed;
            else
                next.word = wireWord(slot, progress.accepted);
            return next;
        }

        // An address, flag or other protocol side effect can precede the first accepted word.
        // Record it before cancellation can intervene so a half-written transaction keeps its lease.
        bool recordSideEffect(const DspOffer& offer) noexcept
        {
            auto* slot = offeredSlot(offer);
            if (!slot)
                return false;
            slot->touched = true;
            return true;
        }

        // Report only a word that the physical endpoint accepted. Busy has no state transition.
        bool recordAccepted(const DspOffer& offer) noexcept
        {
            auto* slot = offeredSlot(offer);
            if (!slot)
                return false;

            slot->touched = true;
            auto& progress = slot->progress;
            if (++progress.accepted == progress.wordCount)
            {
                progress.status = DspStatus::Transferred;
                if (progress.releaseEndpoint == DspMilestone::Transfer)
                    advanceHead(m_lanes[index(progress.ticket.endpoint)]);
            }
            return true;
        }

        bool recordObserved(DspTicket ticket) noexcept
        {
            auto* slot = find(ticket);
            if (!slot || slot->progress.status != DspStatus::Transferred)
                return false;

            slot->progress.status = DspStatus::Observed;
            if (slot->progress.releaseEndpoint == DspMilestone::Observation)
                advanceHead(m_lanes[index(ticket.endpoint)]);
            return true;
        }

        DspCancellation cancel(DspTicket ticket) noexcept
        {
            auto* slot = find(ticket);
            if (!slot || !holdsEndpoint(slot->progress))
                return DspCancellation::Rejected;

            auto& lane = m_lanes[index(ticket.endpoint)];
            if (slot->touched)
            {
                slot->progress.status = DspStatus::Poisoned;
                lane.poisoned = true;
                advanceHead(lane);
                return DspCancellation::Poisoned;
            }

            slot->progress.status = DspStatus::Cancelled;
            if (lane.head == ticket.slot)
                advanceHead(lane);
            return DspCancellation::Cancelled;
        }

        bool progress(DspTicket ticket, DspTransactionProgress& result) const noexcept
        {
            const auto* slot = find(ticket);
            if (!slot)
                return false;
            result = slot->progress;
            return true;
        }

        bool started(DspTicket ticket) const noexcept
        {
            const auto* slot = find(ticket);
            return slot && slot->touched;
        }

        // Preserve known outcomes for already-admitted dependents when releasing a record.
        // An outstanding observation dependency keeps the predecessor record alive.
        bool retire(DspTicket ticket) noexcept
        {
            auto* slot = find(ticket);
            if (!slot || holdsEndpoint(slot->progress))
                return false;

            for (auto& lane : m_lanes)
            {
                for (auto& dependent : lane.slots)
                {
                    const auto& progress = dependent.progress;
                    if (!progress.ticket.sequence || progress.status != DspStatus::Queued ||
                        progress.prerequisite.sequence != ticket.sequence)
                        continue;
                    const auto outcome = reached(slot->progress, progress.required);
                    if (outcome == DependencyStatus::Pending)
                        return false;
                    dependent.resolvedDependency = outcome;
                }
            }
            slot->progress.ticket.sequence = 0;
            --m_lanes[index(ticket.endpoint)].retained;
            return true;
        }

    private:
        enum class DependencyStatus : uint8_t { Pending, Ready, Failed };

        struct Slot
        {
            DspTransactionProgress progress{};
            std::array<uint32_t, MaxWords> words{};
            DependencyStatus resolvedDependency = DependencyStatus::Pending;
            bool touched = false;
        };

        struct Lane
        {
            std::array<Slot, EndpointCapacity> slots{};
            size_t head = EndpointCapacity;
            bool poisoned = false;
            size_t retained = 0;
            size_t highWater = 0;
        };

        static size_t index(Endpoint endpoint) noexcept { return static_cast<size_t>(endpoint); }

        bool validEndpoint(Endpoint endpoint) const noexcept
        {
            return endpoint == Endpoint::A || (m_model == Model::KS && endpoint == Endpoint::B);
        }

        bool matches(Session session) const noexcept
        {
            return m_running && m_session == session;
        }

        static bool validMilestone(DspMilestone milestone) noexcept
        {
            return milestone == DspMilestone::Transfer || milestone == DspMilestone::Observation;
        }

        bool validDraft(const DspTransactionDraft& draft, size_t i) const noexcept
        {
            if (draft.model != m_model || !validEndpoint(draft.endpoint) ||
                draft.wordCount > MaxWords || (draft.wordCount && !draft.words) ||
                !validMilestone(draft.releaseEndpoint) || !validMilestone(draft.dependency.milestone))
                return false;
            switch (draft.address.mode)
            {
            case DspAddressMode::None:
                if (!draft.wordCount)
                    return false;
                break;
            case DspAddressMode::Write:
                if (!draft.wordCount && draft.releaseEndpoint != DspMilestone::Observation)
                    return false;
                break;
            case DspAddressMode::Read:
                if (draft.wordCount || draft.releaseEndpoint != DspMilestone::Observation)
                    return false;
                break;
            default:
                return false;
            }

            const auto& dependency = draft.dependency;
            if (dependency.earlierDraft != DspDependency::NoDraft)
            {
                if (dependency.ticket.sequence || dependency.earlierDraft >= i)
                    return false;
            }
            else if (dependency.ticket.sequence)
            {
                const auto* predecessor = find(dependency.ticket);
                if (!predecessor || reached(predecessor->progress, dependency.milestone) == DependencyStatus::Failed)
                    return false;
            }

            return !draft.wordCount || std::all_of(draft.words, draft.words + draft.wordCount,
                                                    [](uint32_t word) { return word <= 0x00ffffff; });
        }

        static uint32_t wireWord(const Slot& slot, size_t offset) noexcept
        {
            const auto address = slot.progress.address;
            if (address.mode == DspAddressMode::None)
                return slot.words[offset];
            if (!offset)
                return (address.mode == DspAddressMode::Read ? 0x800000u : 0u) | address.address;
            return slot.words[offset - 1];
        }

        static bool holdsEndpoint(const DspTransactionProgress& progress) noexcept
        {
            return progress.status == DspStatus::Queued ||
                   (progress.status == DspStatus::Transferred && progress.releaseEndpoint == DspMilestone::Observation);
        }

        static void advanceHead(Lane& lane) noexcept
        {
            lane.head = EndpointCapacity;
            uint64_t first = 0;
            for (size_t i = 0; i < EndpointCapacity; ++i)
            {
                const auto& progress = lane.slots[i].progress;
                const auto sequence = progress.ticket.sequence;
                if (sequence && holdsEndpoint(progress) && (!first || sequence < first))
                {
                    first = sequence;
                    lane.head = i;
                }
            }
        }

        const Slot* find(DspTicket ticket) const noexcept
        {
            if (!matches(ticket.session) || !validEndpoint(ticket.endpoint) ||
                !ticket.sequence || ticket.slot >= EndpointCapacity)
                return nullptr;
            const auto& slot = m_lanes[index(ticket.endpoint)].slots[ticket.slot];
            return slot.progress.ticket.sequence == ticket.sequence ? &slot : nullptr;
        }

        Slot* find(DspTicket ticket) noexcept
        {
            return const_cast<Slot*>(std::as_const(*this).find(ticket));
        }

        DependencyStatus reached(const DspTransactionProgress& progress, DspMilestone milestone) const noexcept
        {
            if (progress.status == DspStatus::Observed ||
                (progress.accepted == progress.wordCount && milestone == DspMilestone::Transfer))
                return DependencyStatus::Ready;
            if (progress.status == DspStatus::Cancelled || progress.status == DspStatus::Poisoned ||
                (progress.status == DspStatus::Queued && poisoned(progress.ticket.endpoint)))
                return DependencyStatus::Failed;
            return DependencyStatus::Pending;
        }

        DependencyStatus dependencyStatus(const Slot& slot) const noexcept
        {
            const auto& progress = slot.progress;
            if (!progress.prerequisite.sequence)
                return DependencyStatus::Ready;
            if (slot.resolvedDependency != DependencyStatus::Pending)
                return slot.resolvedDependency;
            const auto* predecessor = find(progress.prerequisite);
            return predecessor ? reached(predecessor->progress, progress.required) : DependencyStatus::Failed;
        }

        Slot* offeredSlot(const DspOffer& offer) noexcept
        {
            if (offer.status != DspOfferStatus::Ready)
                return nullptr;
            auto* slot = find(offer.ticket);
            if (!slot)
                return nullptr;
            const auto& lane = m_lanes[index(offer.ticket.endpoint)];
            if (lane.poisoned || lane.head != offer.ticket.slot || slot->progress.status != DspStatus::Queued ||
                slot->progress.accepted != offer.offset || wireWord(*slot, offer.offset) != offer.word ||
                dependencyStatus(*slot) != DependencyStatus::Ready)
                return nullptr;
            return slot;
        }

        std::array<Lane, 2> m_lanes{};
        Session m_session{};
        Model m_model = Model::K;
        uint64_t m_lastSequence = 0;
        uint64_t m_lastOperation = 0;
        bool m_running = false;
    };
}
