#include "ksFlashPageCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
FlashPagePartialTest partialTest = nullptr;
struct Point { unsigned location, length; };
constexpr std::array<Point,19> comparePoints{{
    {0x1272,4},{0x1276,4},{0x127a,4},{0x127e,6},{0x1284,4},
    {0x1288,2},{0x128a,2},{0x128c,2},{0x128e,2},{0x1290,2},
    {0x1292,2},{0x1294,4},{0x1298,4},{0x129c,2},{0x129e,4},
    {0x12a2,4},{0x12a6,4},{0x12aa,4},{0x12ae,2}
}};
constexpr std::array<Point,59> programPoints{{
    {0x11ae,4},{0x11b2,4},{0x11b6,2},{0x11b8,4},{0x11bc,2},
    {0x11be,2},{0x11c0,6},{0x11c6,2},{0x11c8,4},{0x11cc,6},
    {0x11d2,2},{0x11d4,6},{0x11da,6},{0x11e0,4},{0x11e4,2},
    {0x11e6,4},{0x11ea,4},{0x11ee,2},{0x11f0,4},{0x11f4,4},
    {0x11f8,6},{0x11fe,2},{0x1200,6},{0x1206,2},{0x1208,6},
    {0x120e,2},{0x1210,4},{0x1214,2},{0x1216,4},{0x121a,2},
    {0x121c,4},{0x1220,2},{0x1222,2},{0x1224,2},{0x1226,4},
    {0x122a,4},{0x122e,2},{0x1230,2},{0x1232,2},{0x1234,2},
    {0x1236,2},{0x1238,4},{0x123c,2},{0x123e,8},{0x1246,8},
    {0x124e,2},{0x1250,2},{0x1252,2},{0x1254,2},{0x1256,2},
    {0x1258,2},{0x125a,2},{0x125c,2},{0x125e,4},{0x1262,4},
    {0x1266,2},{0x1268,4},{0x126c,4},{0x1270,2}
}};

// An aligned instruction fetch leaves the read bus at its last byte. The
// native job asks for the following data-transfer cost without doing IO.
unsigned dataClocks(uint32_t address, unsigned previous) {
    address &= 0xffffff;
    if (address < 0x400000)
        return (previous & 1) || address != (previous | 1) ? 3 : 0;
    if (address >= 0xfffd10 && address < 0xffffa0)
        return (previous & 1) || address != (previous | 1) ? 1 : 0;
    if (address >= 0xffffa0) return 1;
    return 3;
}
template<class Calls>
auto select(Calls& calls, uint32_t stack) -> typename Calls::value_type* {
    for (auto& c : calls) if (c && c->token == stack) return &c;
    return nullptr;
}
template<class Calls>
auto allocate(Calls& calls) -> typename Calls::value_type* {
    for (auto& c : calls) if (!c) return &c;
    throw std::runtime_error("Native Flash-page call capacity exceeded");
}
}

void setFlashPagePartialForTests(FlashPagePartialTest value) { partialTest = value; }
bool FlashPageCalls::step(ControllerCpu& h) {
    return unsigned(h.getPC()) >= 0x1272 ? compare(h) : program(h);
}

bool FlashPageCalls::compare(ControllerCpu& h) {
    using namespace controllerDetail;
    using T = FlashPageCompareTask;
    using S = T::Stage;
    const unsigned pc = unsigned(h.getPC());
    auto* selected = select(compares_, h.getSP());
    if (pc == 0x1272) {
        if (!selected) selected = allocate(compares_);
        selected->emplace(uint32_t(h.regs[5].er), uint32_t(h.regs[6].er), h.getSP());
        ++started;
    }
    if (!selected || comparePoints[unsigned((*selected)->task.stage())].location != pc) {
        if (selected) selected->reset();
        if (partialTest && partialTest(h)) { ++partialEntries; return true; }
        return false;
    }
    auto& call = **selected;
    auto& task = call.task;
    auto& r = h.regs;
    const auto stage = task.stage();
    const auto point = comparePoints[unsigned(stage)];
    unsigned readCost = 3;
    if (stage == S::ReadSource || stage == S::ReadTarget)
        readCost = dataClocks(stage == S::ReadSource ? uint32_t(r[5].er) : uint32_t(r[6].er), pc + point.length - 1);
    const auto cost = task.nextClocks(readCost);
    const auto before = h.cycles;
    RuntimeBoundary f(h, pc, point.length);
    auto read = [&](bool target, unsigned index) {
        const auto address = target ? (task.target() & 0x00ffff00u) + index : task.source() + index;
        return uint8_t(h.read8(int(address)));
    };
    if (task.advanceBefore(cost, read, readCost) != cost)
        throw std::runtime_error("Native Flash-page comparison admission differs");
    switch (stage) {
    case S::AdmitContext: f.push(r[0].er); break;
    case S::AdmitTarget: f.push(r[6].er); break;
    case S::AdmitSource: f.push(r[5].er); break;
    case S::AlignPage: r[6].er &= 0xffff00; value(h,r[6].er); break;
    case S::BeginScan: f.move(r[0].e,0x100); break;
    case S::ReadSource: f.move(r[0].rl,task.sourceByte()); r[5].er = int32_t(uint32_t(r[5].er)+1); h.clockI(2); break;
    case S::ReadTarget: f.move(r[0].rh,task.targetByte()); r[6].er = int32_t(uint32_t(r[6].er)+1); h.clockI(2); break;
    case S::Compare: h.sub8(r[0].rh,r[0].rl); break;
    case S::NextByte: f.inc(r[0].e,-1); break;
    case S::DiscardSource: case S::DiscardTarget: f.pop(r[0].er); break;
    case S::RestoreSource: f.pop(r[5].er); break;
    case S::RestoreTarget: f.pop(r[6].er); break;
    case S::MarkDifference: r[6].e |= int16_t(0x8000); value(h,r[6].e); break;
    case S::ReleaseContext: f.pop(r[0].er); break;
    case S::Return: f.ret(); break;
    default: break;
    }
    if (h.cycles-before != cost) throw std::runtime_error("Native Flash-page comparison clocks differ");
    if (task.complete()) {
        h.pc = h.makepc(f.next); selected->reset(); ++completed; return true;
    }
    call.token = h.getSP();
    h.pc = h.makepc(comparePoints[unsigned(task.stage())].location);
    return true;
}

bool FlashPageCalls::program(ControllerCpu& h) {
    using namespace controllerDetail;
    using T = FlashPageProgramTask;
    using S = T::Stage;
    const unsigned pc = unsigned(h.getPC());
    auto* selected = select(programs_, h.getSP());
    if (pc == 0x11ae) {
        if (!selected) selected = allocate(programs_);
        selected->emplace(uint32_t(h.regs[5].er), uint32_t(h.regs[6].er), h.getSP());
        ++started;
    }
    const auto pointOf = [](const T& t) { return programPoints[unsigned(t.awaitingCompare() ? t.resumeStage() : t.stage())]; };
    if (!selected || pointOf((*selected)->task).location != pc) {
        if (selected) selected->reset();
        if (partialTest && partialTest(h)) { ++partialEntries; return true; }
        return false;
    }
    auto& call = **selected;
    auto& task = call.task;
    auto& r = h.regs;
    if (task.awaitingCompare()) task.resumeCompare(uint32_t(r[5].er),uint32_t(r[6].er));
    const auto stage = task.stage();
    const auto point = pointOf(task);
    unsigned byteCost = 3;
    if (stage == S::ReadSource) byteCost = dataClocks(task.source()+task.position(), pc+point.length-1);
    else if (stage == S::ReadReady) byteCost = dataClocks(task.target()+255, pc+point.length-1);
    else if (stage == S::WriteTarget) byteCost = dataClocks(task.target()+task.position(), unsigned(h.lastwrite));
    const auto cost = task.nextClocks(byteCost);
    const auto before = h.cycles;
    RuntimeBoundary f(h, pc, point.length);
    auto read = [&](T::Read what, unsigned index) -> uint32_t {
        switch (what) {
        case T::Read::Pending: return h.read16(0xffed6a);
        case T::Read::Source: return uint8_t(h.read8(int(task.source()+index)));
        case T::Read::StartTick: case T::Read::CurrentTick: return h.read32(0xffec10);
        case T::Read::Ready: return uint8_t(h.read8(int(task.target()+index)));
        }
        return 0;
    };
    auto write = [&](T::Write what, uint16_t value, unsigned index) {
        switch (what) {
        case T::Write::Pending: f.store16(0xffed6a,int16_t(value)); break;
        case T::Write::FirstCommand: case T::Write::ProgramCommand: f.store8(task.bank()+0x5555,int8_t(value)); break;
        case T::Write::SecondCommand: f.store8(task.bank()+0x2aaa,int8_t(value)); break;
        case T::Write::Byte: f.store8(task.target()+index,int8_t(value)); break;
        }
    };
    if (task.advanceBefore(cost,read,write,byteCost) != cost)
        throw std::runtime_error("Native Flash-page program admission differs");
    switch (stage) {
    case S::AdmitContext: f.push(r[0].er); break;
    case S::AdmitCommand: f.push(r[1].er); break;
    case S::AdmitByte: f.push(r[2].r); break;
    case S::AdmitClock: f.push(r[3].er); break;
    case S::SelectTarget: f.move(r[0].er,r[6].er); break;
    case S::SelectFlash: f.move(r[1].er,0x800000); break;
    case S::Offset: r[0].er=h.sub32(r[0].er,r[1].er); break;
    case S::AdmitFlash: case S::AdmitAlignment: case S::AdmitDifference: h.clockI(2); break;
    case S::CompareProtected: h.sub32(r[0].er,0x4000); break;
    case S::SkipSource: r[5].er=h.add32(r[5].er,0x100); break;
    case S::SkipTarget: r[6].er=h.add32(r[6].er,0x100); break;
    case S::FinishProtected: h.clockI(2); break;
    case S::CheckAlignment: f.move(r[6].rl,r[6].rl); break;
    case S::ComparePage: f.call(0x1272); break;
    case S::CheckDifference: f.move(r[6].e,r[6].e); break;
    case S::ClearDifference: r[6].e &= 0x7fff; value(h,r[6].e); break;
    case S::ReadPending: f.move(r[0].r,task.pending()); break;
    case S::IncrementPending: f.inc(r[0].r,1); break;
    case S::SelectBank: f.move(r[0].er,r[6].er); break;
    case S::NormalizeBank: r[0].er &= 0xf80000; value(h,r[0].er); break;
    case S::FirstCommand: f.move(r[1].rl,int8_t(0xaa)); break;
    case S::SecondCommand: f.move(r[1].rl,0x55); break;
    case S::ProgramCommand: f.move(r[1].rl,int8_t(0xa0)); break;
    case S::SelectWriteTarget: f.move(r[0].er,r[6].er); break;
    case S::SelectWriteSource: f.move(r[1].er,r[5].er); break;
    case S::BeginWrite: f.move(r[2].rh,0); break;
    case S::EnterWriteWindow: r[7].er -= 2; h.write16(h.exr,r[7].er); h.clockI(2); break;
    case S::RaiseWritePriority: h.exr |= 6; break;
    case S::ReadSource: f.move(r[2].rl,task.byte()); r[1].er=int32_t(uint32_t(r[1].er)+1); h.clockI(2); break;
    case S::NextTarget: f.inc(r[0].er,1); break;
    case S::NextByte: f.inc(r[2].rh,-1); break;
    case S::LeaveWriteWindow: h.exr=uint8_t(h.read16(r[7].er)); r[7].er += 2; h.clockI(2); break;
    case S::SelectPollTarget: f.inc(r[0].er,-1); break;
    case S::ReadStartTick: f.move(r[3].er,task.startTick()); break;
    case S::ReadCurrentTick: f.move(r[1].er,task.currentTick()); break;
    case S::Elapsed: r[1].rl=h.sub8(r[1].rl,r[3].rl); break;
    case S::CompareTimeout: h.sub8(r[1].rl,0xf); break;
    case S::ReadReady: f.move(r[2].rh,task.ready()); break;
    case S::CompareReady: h.sub8(r[2].rl,r[2].rh); break;
    case S::VerifyPage: f.call(0x1272); break;
    case S::Fail: r[6].e |= int16_t(0x8000); value(h,r[6].e); break;
    case S::ReleaseClock: f.pop(r[3].er); break;
    case S::ReleaseByte: f.pop(r[2].r); break;
    case S::ReleaseCommand: f.pop(r[1].er); break;
    case S::ReleaseContext: f.pop(r[0].er); break;
    case S::Return: f.ret(); break;
    default: break;
    }
    if (h.cycles-before != cost) throw std::runtime_error("Native Flash-page program clocks differ");
    if (task.complete()) {
        h.pc=h.makepc(f.next); selected->reset(); ++completed; return true;
    }
    if (!task.awaitingCompare()) call.token=h.getSP();
    h.pc=h.makepc(task.awaitingCompare() ? f.next : pointOf(task).location);
    return true;
}
}
