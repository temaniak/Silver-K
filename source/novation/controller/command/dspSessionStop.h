#pragma once

#include "dspWordTransport.h"

namespace novation::controller::command {

enum class DspDrain : uint8_t { Idle, Cancelled, Completed, ResetRequired };
inline constexpr unsigned DspStopPumpLimit = 32768;

struct DspStopReport {
    DspDrain drain = DspDrain::Idle;
    bool stopped = false;
    unsigned pumps = 0;
    uint64_t elapsedCycles = 0;
};

// The owner has detached host ingress and is on its worker boundary. Finish
// only the addressed transfer that already touched a port. Its reply is never
// dispatched as a musical completion. The owner destroys or resets the DSP
// after this call, including when the physical deadline expires.
template<class Advance, class Native, class Pending>
DspStopReport stopDspSession(CommandSession& session, Session owner, DspWordTransport* transport,
                             BusCycle now, uint64_t cycleBudget, Advance advance, Native native,
                             Pending pending) noexcept {
    const auto snapshot = session.snapshot();
    if (snapshot.session != owner || snapshot.lifecycle != Lifecycle::Running)
        return {};

    auto at = now;
    const BusCycle deadline{now.ticks + std::min(cycleBudget, UINT64_MAX - now.ticks)};
    DspStopReport report;
    const auto ticket = transport ? transport->activeTicket() : std::nullopt;
    if (ticket && !session.transactions().started(*ticket)) {
        report.drain = transport->cancel(session) == DspCancellation::Cancelled
            ? DspDrain::Cancelled : DspDrain::ResetRequired;
    } else if (ticket) {
        report.drain = DspDrain::ResetRequired;
        // Busy polls count towards the access bound. KS DSP B can need
        // dozens of readiness polls per word while receiving a voice image.
        try {
            for (; report.pumps < DspStopPumpLimit; ) {
                const auto next = transport->nextBusAccess().value_or(at);
                if (deadline < next)
                    break;
                if (at < next) {
                    advance(next);
                    at = next;
                }
                ++report.pumps;
                const auto status = transport->pump(session, at, native());
                if (status == HdiWordStatus::WordAccepted || status == HdiWordStatus::ReplyQueued ||
                    status == HdiWordStatus::AddressSelected) {
                    report.drain = DspDrain::Completed;
                    break;
                }
                if (status != HdiWordStatus::Progress && status != HdiWordStatus::WaitingForReadiness)
                    break;
            }
        } catch (...) {
            // The physical endpoint will be reset by the owner.
        }
        if (transport->activeTicket())
            transport->cancel(session);
    }

    report.stopped = session.stop(owner);
    if (session.transactions().poisoned(Endpoint::A) || session.transactions().poisoned(Endpoint::B))
        report.drain = DspDrain::ResetRequired;

    // A final acknowledged strobe may have left one word in a physical RX
    // register. Give it bounded DSP progress without issuing another write.
    if (report.drain != DspDrain::ResetRequired) {
        try {
            while (pending() && at < deadline) {
                const BusCycle next{at.ticks + std::min(uint64_t{64}, deadline.ticks - at.ticks)};
                advance(next);
                at = next;
            }
            if (pending())
                report.drain = DspDrain::ResetRequired;
        } catch (...) {
            report.drain = DspDrain::ResetRequired;
        }
    }
    report.elapsedCycles = at.ticks - now.ticks;
    return report;
}

}
