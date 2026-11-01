#include "ksTpu0Service.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
bool measuring = false;
uint64_t allocations = 0;
void check(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
struct Config {
    uint8_t divider = 255, flags = 3;
    uint32_t counter = 7, phase = 0xffffffffu, increment = 1;
    unsigned pattern = 14, delay = 30, busyReads = 0;
    uint16_t producer = 45, consumer = 17;
    bool scan = true;
};
// Only the owned resource ranges exist here: there is no CPU, firmware,
// instruction decoder, execution register, guest stack or address-space image.
struct Resources {
    std::array<uint8_t, 0x1000> records{};
    std::array<uint8_t, 0x1000> queue{};
    std::array<uint8_t, 0x1400> io{};
    uint8_t& at(uint32_t address) {
        if (address >= 0xb000 && address < 0xc000) return records[address - 0xb000];
        if (address >= 0x15000 && address < 0x16000) return queue[address - 0x15000];
        check(address >= 0xffec00 && address <= 0xffffff, "Unexpected TPU0 resource address");
        return io[address - 0xffec00];
    }
    uint16_t word(uint32_t a) { return uint16_t((unsigned(at(a)) << 8) | at(a + 1)); }
    uint32_t wide(uint32_t a) { return (uint32_t(word(a)) << 16) | word(a + 2); }
    void word(uint32_t a, uint16_t v) { at(a) = uint8_t(v >> 8); at(a + 1) = uint8_t(v); }
    void wide(uint32_t a, uint32_t v) { word(a, uint16_t(v >> 16)); word(a + 2, uint16_t(v)); }
    bool operator==(const Resources& other) const {
        return records == other.records && queue == other.queue && io == other.io;
    }
};
struct Transfer {
    uint32_t address = 0;
    uint8_t value = 0;
    unsigned offset = 0;
    bool writing = false;
    bool operator==(const Transfer& other) const {
        return address == other.address && value == other.value &&
               offset == other.offset && writing == other.writing;
    }
};
struct Harness {
    ks::Tpu0Service task;
    Resources resource;
    std::array<Transfer, 8> phaseTransfers{};
    size_t phaseTransferCount = 0;
    uint64_t clocks = 0, phases = 0, digest = 0xcbf29ce484222325ull;
    unsigned phaseCost = 0, busyReads = 0, lcdReads = 0, lcdCommands = 0;
    unsigned consumerReads = 0, dataWrites = 0, muxWrites = 0, criticalPhases = 0;
    explicit Harness(const Config& config) : busyReads(config.busyReads) {
        resource.at(0xffffd5) = 0xa5;
        resource.at(0xffec14) = config.scan ? 0 : 1;
        resource.at(0xffee70) = config.divider;
        resource.wide(0xffec10, config.counter);
        resource.word(0xffee76, 0xffff);
        resource.word(0xffee78, 0xffff);
        resource.wide(0xffee7a, config.phase);
        resource.wide(0xffee7e, config.increment);
        resource.wide(0xffee82, 0xffffffffu);
        resource.wide(0xffee86, 0x34);
        resource.at(0xffee97) = 3;
        resource.at(0xffee9a) = 0xff;
        resource.word(0xffed6c, config.producer);
        resource.word(0xffed6e, config.consumer);
        resource.word(0x156e8, 0x1fe);
        resource.word(0x156ea, 0);
        const auto current = uint16_t(0x5555u * (config.pattern >> 2));
        const auto previous = uint16_t(0x5555u * (config.pattern & 3));
        resource.at(0xffff55) = uint8_t(current >> 8);
        resource.at(0xffff54) = uint8_t(((current >> 8) & 3) << 2);
        resource.at(0xffff59) = uint8_t(current & 0xe0);
        resource.at(0xffff50) = uint8_t(current & 0x1f);
        for (unsigned page = 0; page < 8; ++page) resource.word(0xb960 + 2 * page, previous);
        for (unsigned n = 0; n < 64; ++n) {
            const auto record = 0xb970 + n * 4;
            resource.at(record) = config.flags;
            resource.at(record + 1) = uint8_t(n + 36);
            resource.word(record + 2, uint16_t(0 - config.delay));
        }
        for (unsigned n = 0; n < 4; ++n) {
            resource.at(0xffec16 + n) = uint8_t(1u << n);
            resource.at(0xffec24 + n) = uint8_t(128u >> n);
        }
        resource.at(0xffec1b) = 0x10;
        resource.at(0xffec29) = 0x20;
        resource.at(0xffec23) = 1;
    }
    void transfer(uint32_t address, uint8_t value, unsigned offset, bool writing) {
        check(offset && offset <= phaseCost, "Callback crossed phase clock budget");
        if (phaseTransferCount) check(offset >= phaseTransfers[phaseTransferCount - 1].offset,
                                      "Callbacks ran in reverse clock order");
        check(phaseTransferCount < phaseTransfers.size(), "Unexpected transfer fanout");
        phaseTransfers[phaseTransferCount++] = {address, value, offset, writing};
        digest ^= uint64_t(address) | (uint64_t(value) << 24) | (uint64_t(writing) << 32);
        digest *= 1099511628211ull;
        digest ^= clocks + offset;
        digest *= 1099511628211ull;
    }
    unsigned step(unsigned available) {
        phaseTransferCount = 0;
        phaseCost = task.nextClocks();
        const auto used = task.advanceBefore(available,
            [&](uint32_t address, unsigned offset) {
                uint8_t value = resource.at(address);
                if (address == 0xffff51) {
                    ++lcdReads;
                    value = busyReads ? 0x80 : 0;
                    if (busyReads) --busyReads;
                }
                if (address == 0xffed6e) ++consumerReads;
                transfer(address, value, offset, false);
                return value;
            },
            [&](uint32_t address, uint8_t value, unsigned offset) {
                transfer(address, value, offset, true);
                if (address >= 0xffed70 && address < 0xffee70) ++dataWrites;
                if (address == 0xffff8a) {
                    check(task.critical(), "UART enable escaped the critical section");
                    check(value == 0xf0, "UART enable value differs");
                }
                if (address == 0xffff6d) {
                    check(value == uint8_t(1u << ((muxWrites + 1) & 7)), "Keyboard mux order differs");
                    ++muxWrites;
                }
                if (address == 0xffff61) {
                    check(value == 12, "Panel decay emitted a different latch command");
                    ++lcdCommands;
                }
                resource.at(address) = value;
            });
        check(!used || used == phaseCost, "Service crossed a logical phase boundary");
        if (used) {
            clocks += used;
            ++phases;
            criticalPhases += task.critical();
        }
        return used;
    }
};
uint64_t cases = 0, phases = 0, rejections = 0;
uint32_t components = 0;
void compare(const Harness& a, const Harness& b) {
    check(a.task.component() == b.task.component() &&
          a.task.components() == b.task.components() &&
          a.task.critical() == b.task.critical() &&
          a.task.complete() == b.task.complete() &&
          a.task.nextClocks() == b.task.nextClocks(), "Budget changed logical task continuation");
    check(a.clocks == b.clocks && a.digest == b.digest &&
          a.phaseTransferCount == b.phaseTransferCount, "Budget changed resource timing/order");
    for (size_t i = 0; i < a.phaseTransferCount; ++i)
        check(a.phaseTransfers[i] == b.phaseTransfers[i], "Budget changed callback contents");
}
void advance(Harness& a, Harness& b) {
    const auto cost = a.task.nextClocks();
    check(cost && a.phases < 20000, "TPU0 failed to make bounded progress");
    const auto component = a.task.component();
    const auto componentMask = a.task.components();
    const auto critical = a.task.critical();
    const auto oldDigest = a.digest;
    const auto oldLcdReads = a.lcdReads;
    const auto oldConsumerReads = a.consumerReads;
    for (auto rejectedBudget : {0u, cost - 1}) {
        check(!a.step(rejectedBudget) && !a.phaseTransferCount && a.digest == oldDigest &&
              a.lcdReads == oldLcdReads && a.consumerReads == oldConsumerReads &&
              a.task.component() == component && a.task.components() == componentMask &&
              a.task.critical() == critical && a.task.nextClocks() == cost,
              "Insufficient budget mutated resources or continuation");
        ++rejections;
    }
    check(a.step(cost) == cost && b.step(std::numeric_limits<unsigned>::max()) == cost,
          "Admitted phase did not finish exactly once");
    compare(a, b);
    ++phases;
}
void finish(Harness& a, Harness& b) {
    while (!a.task.complete()) advance(a, b);
    check(!a.task.critical() && !b.task.critical(), "Completed timer retained interrupt mask");
    check(!a.task.nextClocks() && !a.step(100000) && !a.phaseTransferCount,
          "Completed timer restarted or touched resources");
    check(a.resource == b.resource, "Resource state depends on phase budget");
    components |= a.task.components();
    ++cases;
}
void results(Harness& a, const Config& c) {
    const bool due = c.divider >= 128;
    const bool decay = due && !((c.counter + 1) & 7);
    const bool carry = uint64_t(c.phase) + c.increment > 0xffffffffu;
    check(a.resource.at(0xffffd5) == 0xc0 &&
          a.resource.at(0xffee70) == uint8_t(c.divider + 128), "Timer acknowledge/divider result differs");
    check(a.resource.wide(0xffec10) == c.counter + unsigned(due), "Half-rate counter cadence differs");
    check(a.resource.wide(0xffee7a) == (due ? c.phase + c.increment : c.phase), "Clock phase wrap differs");
    check(a.resource.word(0xffee78) == (due && carry ? 0 : 0xffff), "Clock pulse wrap differs");
    check(a.resource.wide(0xffee82) == (due ? 0x33u : 0xffffffffu), "Secondary phase wrap differs");
    check(a.resource.at(0xffee97) == (due ? 2 : 3) && a.resource.at(0xffee9a) == 0xff,
          "Timeout wrap/flag semantics differ");
    check(a.dataWrites == unsigned(due && carry), "Clock MIDI output was dropped or duplicated");
    if (due && carry) {
        check(a.resource.at(0xffed70 + c.producer) == 0xf8 &&
              a.resource.word(0xffed6c) == uint16_t((c.producer + 1) & 255) &&
              a.criticalPhases == 3, "Clock MIDI queue/mask result differs");
    }
    check(a.lcdCommands == unsigned(decay), "Latch command cadence differs");
    check(a.resource.at(0xffec23) == (decay ? 0 : 1), "Latch holdoff result differs");
    for (unsigned n = 0; n < 4; ++n) {
        check(a.resource.at(0xffec16 + n) == uint8_t((1u << n) >> unsigned(decay)) &&
              a.resource.at(0xffec24 + n) == uint8_t((128u >> n) >> unsigned(decay)),
              "Panel decay arithmetic differs");
    }
    check(a.resource.at(0xffec1b) == (decay ? 0x11 : 0x10) &&
          a.resource.at(0xffec29) == 0x20, "Decay pending flags differ");
    check(a.muxWrites == (c.scan ? 8u : 0u), "Keyboard pages were missed or repeated");
    check(a.resource.word(0xffee76) == (c.scan ? 0 : 0xffff), "Keyboard tick rollover differs");
    if (c.scan) for (unsigned page = 0; page < 8; ++page)
        check(a.resource.word(0xb960 + page * 2) == uint16_t(0x5555u * (c.pattern >> 2)),
              "Current keyboard page was not published");
}
void run(const Config& config) {
    Harness a(config), b(config);
    finish(a, b);
    results(a, config);
}
}
void* operator new(std::size_t size) {
    if (measuring) ++allocations;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
int main() {
    try {
        measuring = true;
        Config c;
        c.scan = false;
        for (unsigned divider = 0; divider < 256; ++divider)
            for (unsigned residue = 0; residue < 8; ++residue) {
                c.divider = uint8_t(divider);
                c.counter = residue;
                run(c);
            }
        constexpr std::array<uint32_t, 5> accumulators{{0, 1, 0x7fffffffu, 0x80000000u, 0xffffffffu}};
        constexpr std::array<uint32_t, 4> counters{{0, 7, 0xfffffffeu, 0xffffffffu}};
        c.divider = 255;
        for (auto phase : accumulators) for (auto increment : accumulators) for (auto counter : counters) {
            c.phase = phase; c.increment = increment; c.counter = counter;
            run(c);
        }
        c = Config{};
        constexpr std::array<unsigned, 10> delays{{0, 1, 2, 3, 9, 10, 11, 30, 120, 65535}};
        for (unsigned pattern = 0; pattern < 16; ++pattern)
            for (unsigned flags = 0; flags < 4; ++flags)
                for (auto delay : delays) {
                    c.pattern = pattern;
                    c.flags = uint8_t(flags);
                    c.delay = delay;
                    run(c);
                }
        // One complete release-velocity scan wraps the event producer once.
        c = Config{};
        Harness a(c), b(c);
        finish(a, b);
        check(a.task.components() == 0x7ff && a.resource.word(0x156e8) == 126,
              "Complete TPU0 graph/key event queue rollover was not exercised");
        for (unsigned n = 0; n < 64; ++n)
            check(a.resource.word(0x156ec + ((510 + n * 2) & 510)) == uint16_t(0x0500 | (n + 36)),
                  "Release velocity/key ordering differs");
        // The queue producer must wait on live consumer state, without masking
        // interrupts or re-emitting F8 while the queue remains full.
        c.scan = false; c.producer = 255; c.consumer = 0;
        Harness fullA(c), fullB(c);
        while (fullA.consumerReads < 4) advance(fullA, fullB);
        check(fullA.task.component() == ks::Tpu0Service::Component::Midi &&
              fullA.resource.word(0xffed6c) == 255 && fullA.dataWrites == 1 &&
              !fullA.task.critical(), "Full UART queue did not retain its live wait");
        fullA.resource.word(0xffed6e, 1); fullB.resource.word(0xffed6e, 1);
        finish(fullA, fullB); results(fullA, c);
        // Keep LCD busy for many complete polls, then release it from the
        // resource side. Polling must not publish the pending command early.
        c = Config{}; c.scan = false; c.busyReads = 1000;
        Harness busyA(c), busyB(c);
        while (busyA.lcdReads < 12) advance(busyA, busyB);
        check(!busyA.task.complete() && !busyA.lcdCommands && !busyA.task.critical(),
              "Busy LCD bypassed polling");
        busyA.busyReads = 0; busyB.busyReads = 0;
        finish(busyA, busyB); results(busyA, c);
        check(busyA.lcdReads == 14, "LCD did not re-read ready status and final reply");
        measuring = false;
        check(components == 0x7ff && allocations == 0, "Graph coverage or allocation contract failed");
        std::printf("PASS: %llu CPU-independent TPU0 cases; %llu phases; %llu rejected budgets; components=0x%03x; allocations=%llu; UART full/release, LCD busy/release, key queue wrap and velocity verified\n",
                    static_cast<unsigned long long>(cases), static_cast<unsigned long long>(phases),
                    static_cast<unsigned long long>(rejections), components,
                    static_cast<unsigned long long>(allocations));
        return 0;
    } catch (const std::exception& e) {
        measuring = false;
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
