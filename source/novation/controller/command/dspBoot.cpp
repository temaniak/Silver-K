#include "dspBoot.h"

#include "baseLib/sha256.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace novation::controller::command {
namespace {
constexpr uint8_t TxEmpty = 0x02;
constexpr uint8_t FirmwareBusy = 0x10;

uint32_t big24(const uint8_t* bytes) {
    return (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | bytes[2];
}
}

std::vector<uint32_t> readDspBootProgram(const DspBootImage& image, const std::vector<uint8_t>& os) {
    const size_t size = 3 * (image.programWords + 2);
    if (image.programOffset > os.size() || size > os.size() - image.programOffset)
        throw std::runtime_error(std::string(image.name) + " DSP bootstrap exceeds OS image");
    const auto* bytes = os.data() + image.programOffset;
    if (big24(bytes) != image.programWords || big24(bytes + 3) != 0 ||
        baseLib::SHA256(bytes + 6, size - 6).toString() != image.programHash)
        throw std::runtime_error(std::string(image.name) + " DSP bootstrap verification failed");
    std::vector<uint32_t> words;
    words.reserve(image.programWords + 2);
    for (size_t i = 0; i < size; i += 3)
        words.push_back(big24(bytes + i));
    return words;
}

std::vector<uint32_t> readDspBootTable(const DspBootImage& image, const std::vector<uint8_t>& os) {
    std::vector<uint32_t> words;
    words.reserve(image.model == Model::K ? 512 : 1024);
    if (image.model == Model::K) {
        constexpr size_t Offset = 0xa211;
        if (Offset + 512 * 3 > os.size())
            throw std::runtime_error("K startup table exceeds OS image");
        for (size_t i = 0; i < 512; ++i)
            words.push_back(big24(os.data() + Offset + 3 * i));
    } else {
        uint32_t accumulator = 0x40000000;
        for (unsigned i = 0; i < 512; ++i) {
            words.push_back(accumulator >> 8);
            accumulator += (0xb191u * (accumulator >> 16)) >> 9;
        }
        for (uint32_t divisor = 512; divisor < 1024; ++divisor)
            words.push_back(uint32_t((uint64_t{0x800000} << 8) / divisor));
    }
    std::vector<uint8_t> encoded;
    encoded.reserve(words.size() * 3);
    for (const auto word : words) {
        encoded.push_back(uint8_t(word >> 16));
        encoded.push_back(uint8_t(word >> 8));
        encoded.push_back(uint8_t(word));
    }
    if (baseLib::SHA256(encoded).toString() != image.tableHash)
        throw std::runtime_error(std::string(image.name) + " DSP startup table verification failed");
    return words;
}

DspBootstrap::DspBootstrap(DspBootPort port, const DspBootImage& image, uint64_t busSpacing)
    : port_(port), image_(image), transport_(image.endpoint, port.hdi),
      timing_{busSpacing, busSpacing, 25000} {
    check(port_.advance && port_.bus && port_.frame && port_.verifyProgram && busSpacing, "invalid cold boot port");
    token_ = session_.coldReset(image.model, {}, port.frame(port.context), {48000}, {48000}, 0);
}

void DspBootstrap::check(bool condition, const char* phase) const {
    if (!condition)
        throw std::runtime_error(std::string(image_.name) + " cold boot: " + phase +
                                 " (bus cycle " + std::to_string(bus_.ticks) + ')');
}

void DspBootstrap::advance(uint64_t cycles) {
    bus_.ticks += cycles;
    port_.advance(port_.context, bus_);
}

void DspBootstrap::waitFlag(uint8_t mask, bool set, uint64_t budget) {
    const auto deadline = bus_.ticks + budget;
    do {
        if (bool(port_.hdi.read(HdiRegister::Status) & mask) == set)
            return;
        advance(timing_.pollInterval);
    } while (bus_.ticks <= deadline);
    check(false, set ? "HDI readiness flag timeout" : "HDI busy flag timeout");
}

void DspBootstrap::start(const std::vector<uint32_t>& program, const std::vector<uint32_t>& table) {
    bus_ = port_.bus(port_.context);
    check(!started_ && program.size() == image_.programWords + 2 &&
          program[0] == image_.programWords && program[1] == 0 && !table.empty(), "invalid startup payload");
    for (const auto word : program) {
        waitFlag(TxEmpty, true, timing_.readyTimeout);
        check(port_.hdi.write(HdiRegister::High, uint8_t(word >> 16)), "bootstrap high strobe refused");
        advance(timing_.strobeSpacing);
        check(port_.hdi.write(HdiRegister::Middle, uint8_t(word >> 8)), "bootstrap middle strobe refused");
        advance(timing_.strobeSpacing);
        check(port_.hdi.write(HdiRegister::Low, uint8_t(word)), "bootstrap low strobe refused");
        advance(timing_.strobeSpacing);
    }
    check(port_.verifyProgram(port_.context, program.data() + 2, image_.programWords, program[1]),
          "loaded DSP program readback failed");
    waitFlag(FirmwareBusy, true, timing_.readyTimeout);
    const auto deadline = bus_.ticks + 500000;
    while (port_.hdi.read(HdiRegister::Status) & FirmwareBusy) {
        check(bus_.ticks < deadline, "DSP firmware did not finish initialization");
        advance(128);
    }
    started_ = true;
    write(image_.tableAddress, table.data(), table.size());
}

void DspBootstrap::write(uint16_t address, const uint32_t* words, size_t size) {
    bus_ = port_.bus(port_.context);
    check(started_ && words && size, "invalid startup write");
    DspAddress selection{DspAddressMode::Write, address};
    while (size) {
        const auto count = std::min(size, DspTransactionQueue::MaxWords);
        transact(selection, words, count);
        selection = {};
        words += count;
        size -= count;
    }
    advance(timing_.strobeSpacing);
    waitFlag(TxEmpty, true, timing_.readyTimeout);
}

uint32_t DspBootstrap::transact(DspAddress address, const uint32_t* words, size_t size) {
    const bool reading = address.mode == DspAddressMode::Read;
    const auto frame = port_.frame(port_.context);
    DspTransactionDraft draft;
    draft.model = image_.model;
    draft.endpoint = image_.endpoint;
    draft.earliest = frame;
    draft.words = words;
    draft.wordCount = size;
    draft.address = address;
    draft.releaseEndpoint = reading ? DspMilestone::Observation : DspMilestone::Transfer;
    DspTicket ticket;
    auto& queue = session_.transactions();
    check(queue.submit(token_, ++operation_, &draft, 1, &ticket, []() noexcept {}) ==
          DspAdmission::Accepted, "startup transaction admission failed");
    const bool addressed = address.mode != DspAddressMode::None;
    DspTransactionProgress progress;
    do {
        advance(timing_.strobeSpacing);
        const auto now = port_.frame(port_.context);
        const auto offer = queue.offer(token_, image_.endpoint, now);
        check(addressed ? transport_.beginAddressed(session_, offer, now, bus_, timing_) :
                          transport_.beginWrite(session_, offer, now, bus_, timing_), "startup HDI admission failed");
        while (true) {
            const auto next = transport_.nextBusAccess();
            check(next && next->ticks >= bus_.ticks, "missing startup transaction deadline");
            advance(next->ticks - bus_.ticks);
            const auto status = transport_.pump(session_, bus_, port_.frame(port_.context));
            if (status == HdiWordStatus::WordAccepted || status == HdiWordStatus::ReplyQueued)
                break;
            check(status == HdiWordStatus::Progress || status == HdiWordStatus::WaitingForReadiness ||
                  status == HdiWordStatus::WaitingForBus, "startup HDI transaction failed");
        }
        check(queue.progress(ticket, progress), "startup transaction no longer owned");
    } while (!addressed && progress.status == DspStatus::Queued);
    DspReply reply{};
    if (reading) {
        const MusicalDispatch dispatch{&reply,
            [](void* target, CommandSession&, const ScheduledEvent& event, NativeFrame) noexcept {
                if (const auto* value = std::get_if<DspReply>(&event.event.payload))
                    *static_cast<DspReply*>(target) = *value;
                return DispatchDecision{};
            }};
        check(session_.runUntil(port_.frame(port_.context), 1, dispatch).applied == 1 &&
              reply.endpoint == ticket.endpoint && reply.transaction == ticket.sequence, "invalid DSP startup reply");
    }
    check(progress.status == (reading ? DspStatus::Observed : DspStatus::Transferred) && queue.retire(ticket),
          "startup transaction did not finish");
    return reply.word;
}

void DspBootstrap::waitResponse(uint16_t address, uint32_t mask, uint32_t expected,
                                uint64_t maxBusCycles, uint64_t pollSpacing) {
    bus_ = port_.bus(port_.context);
    check(started_ && pollSpacing, "invalid startup response request");
    const auto deadline = bus_.ticks + maxBusCycles;
    uint32_t response;
    do {
        response = transact({DspAddressMode::Read, address}, nullptr, 0);
        if ((response & mask) == expected)
            return;
        advance(std::min(pollSpacing, deadline - std::min(deadline, bus_.ticks)));
    } while (bus_.ticks < deadline);
    throw std::runtime_error(std::string(image_.name) + " DSP startup response timeout at address " +
                             std::to_string(address) + ", last word " + std::to_string(response));
}

}
