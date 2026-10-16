#include "ksRuntimeController.h"
#include "ksRuntimeBoundary.h"

namespace ks {
using namespace controllerDetail;
// Periodic voice/DSP feedback, control updates, allocation and note/pedal release.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::voices(ControllerCpu& h){
    shared.voiceActions.globalFinishScalarCalls.retireReturns(h);
    shared.voiceActions.globalTimeTailCalls.retireReturns(h);
    shared.voiceActions.globalTailScalarCalls.retireReturns(h);
    shared.voiceActions.globalPairCalls.retireReturns(h);
    shared.voiceActions.globalClampedTimeCalls.retireReturns(h);
    shared.voiceActions.globalMatrixCalls.retireReturns(h);
    shared.voiceActions.globalMixCalls.retireReturns(h);
    shared.voiceActions.globalSetupCalls.retireReturns(h);
    shared.voiceActions.globalScalarCalls.retireReturns(h);
    shared.voiceActions.globalTimeCalls.retireReturns(h);
    shared.voiceActions.globalShapeCalls.retireReturns(h);
    shared.voiceActions.globalRangeCalls.retireReturns(h);
    if(GlobalMixCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalMixCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTableCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTableCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(SquareRootCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.squareRootCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalMatrixCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalMatrixCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalClampedTimeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalClampedTimeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalPairCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalPairCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTailScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTailScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTimeTailCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTimeTailCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalFinishScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalFinishScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalFinalCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalFinalCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(VoiceSweepCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.sweepCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(VoiceRefreshCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.refreshCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalSetupCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalSetupCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTimeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTimeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalShapeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalShapeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalRangeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalRangeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalEffectCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalEffectCalls.step(h))return true;
    if(PartModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partModulationCalls.step(h))return true;
    if(PartRouteCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partRouteCalls.step(h))return true;
    if(VoiceFeedbackCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.voiceFeedbackCalls.step(h))return true;
    if(PartEnableCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partEnableCalls.step(h))return true;
    if(PartSignalCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partSignalCalls.step(h))return true;
    auto& r=h.regs;
#define N(P,L,...) case P:{RuntimeBoundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
    switch(unsigned(h.getPC())){
    N(0x9fe4,4,f.push(r[6].er))
    N(0x9fe8,6,f.move(r[6].er,0x168fc))
    N(0x9fee,8,f.move(r[6].rl,h.read8(r[6].er+249)))
    N(0x9ff6,2,h.sub8(r[6].rl,0x1))
    N(0x9ff8,2,f.branch(!(h.ccr&4),0xa002))
    N(0x9ffa,2,f.move(r[6].rl,0x1))
    N(0x9ffc,6,f.store8(0x1755c,r[6].rl))
    N(0xa002,4,f.pop(r[6].er))
    N(0xa006,2,f.ret())
    N(0xa008,2,f.push(r[0].r))
    N(0xa00a,2,f.move(r[0].rl,0x0))
    N(0xa00c,2,f.branch(true,0xa02c))
    N(0xa00e,2,f.push(r[0].r))
    N(0xa010,6,f.move(r[0].rl,h.read8(0x1755c)))
    N(0xa016,2,f.branch(!(h.ccr&4),0xa01c))
    N(0xa018,2,f.pop(r[0].r))
    N(0xa01a,2,f.ret())
    N(0xa01c,2,f.move(r[0].rl,0x0))
    N(0xa01e,6,f.store8(0x1755c,r[0].rl))
    N(0xa024,2,f.move(r[0].rl,0x1))
    N(0xa026,2,f.branch(true,0xa02c))
    N(0xa028,2,f.push(r[0].r))
    N(0xa02a,2,f.move(r[0].rl,0x2))
    N(0xa02c,4,f.push(r[6].er))
    N(0xa030,6,f.move(r[6].er,0x168fc))
    N(0xa036,8,f.move(r[0].rh,h.read8(r[6].er+249)))
    N(0xa03e,2,h.sub8(r[0].rh,r[0].rl))
    N(0xa040,4,f.branch(!(h.ccr&4),0xa0bc))
    N(0xa044,2,f.push(r[0].e))
    N(0xa046,2,f.push(r[4].r))
    N(0xa048,4,f.push(r[1].er))
    N(0xa04c,4,f.push(r[5].er))
    N(0xa050,8,f.move(r[0].rl,h.read8(r[6].er+219)))
    N(0xa058,4,f.move(r[4].r,0x824d))
    N(0xa05c,2,f.call(0xa0c4))
    N(0xa05e,8,f.move(r[0].rl,h.read8(r[6].er+220)))
    N(0xa066,4,f.move(r[4].r,0x8250))
    N(0xa06a,2,f.call(0xa0c4))
    N(0xa06c,8,f.move(r[0].rl,h.read8(r[6].er+221)))
    N(0xa074,4,f.move(r[4].r,0x824c))
    N(0xa078,2,f.call(0xa0c4))
    N(0xa07a,8,f.move(r[0].rl,h.read8(r[6].er+79)))
    N(0xa082,8,f.move(r[1].rl,h.read8(r[6].er+78)))
    N(0xa08a,4,f.move(r[4].r,0x8086))
    N(0xa08e,2,f.call(0xa0dc))
    N(0xa090,8,f.move(r[0].rl,h.read8(r[6].er+79)))
    N(0xa098,2,f.shift(r[0].rl,1,false))
    N(0xa09a,2,f.shift(r[0].rl,1,false))
    N(0xa09c,2,f.shift(r[0].rl,1,false))
    N(0xa09e,8,f.move(r[1].rl,h.read8(r[6].er+78)))
    N(0xa0a6,2,f.shift(r[1].rl,1,false))
    N(0xa0a8,2,f.shift(r[1].rl,1,false))
    N(0xa0aa,4,f.move(r[4].r,0x8089))
    N(0xa0ae,2,f.call(0xa0dc))
    N(0xa0b0,4,f.pop(r[5].er))
    N(0xa0b4,4,f.pop(r[1].er))
    N(0xa0b8,2,f.pop(r[4].r))
    N(0xa0ba,2,f.pop(r[0].e))
    N(0xa0bc,4,f.pop(r[6].er))
    N(0xa0c0,2,f.pop(r[0].r))
    N(0xa0c2,2,f.ret())
    N(0xa0c4,2,f.move(r[0].rl,r[0].rl))
    N(0xa0c6,2,f.branch(bool(h.ccr&4),0xa0da))
    N(0xa0c8,2,f.inc(r[0].rl,-1))
    N(0xa0ca,2,r[1].e=h.sub16(r[1].e,r[1].e))
    N(0xa0cc,2,f.shift(r[0].rl,1,false))
    N(0xa0ce,2,f.rotate(r[1].e,1,false,true))
    N(0xa0d0,2,f.shift(r[0].rl,1,false))
    N(0xa0d2,2,f.rotate(r[1].e,1,false,true))
    N(0xa0d4,2,f.move(r[1].rh,0x0))
    N(0xa0d6,4,f.call(0x24c6))
    N(0xa0da,2,f.ret())
    N(0xa0dc,2,f.move(r[0].rh,r[0].rl))
    N(0xa0de,2,r[0].rh&=0x6;value(h,r[0].rh))
    N(0xa0e0,2,h.sub8(r[0].rh,0x6))
    N(0xa0e2,2,f.branch(!(h.ccr&4),0xa104))
    N(0xa0e4,2,f.shift(r[0].rl,1,true))
    N(0xa0e6,2,f.shift(r[0].rl,1,true))
    N(0xa0e8,2,f.shift(r[0].rl,1,true))
    N(0xa0ea,2,f.shift(r[0].rl,1,true))
    N(0xa0ec,2,r[0].rl&=0x10;value(h,r[0].rl))
    N(0xa0ee,2,r[1].rl&=0xc;value(h,r[1].rl))
    N(0xa0f0,2,r[0].rl|=r[1].rl;value(h,r[0].rl))
    N(0xa0f2,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa0f4,2,f.move(r[1].rl,r[0].rl))
    N(0xa0f6,6,r[1].er=h.add32(r[1].er,0x1532e))
    N(0xa0fc,4,f.move(r[1].er,h.read32(r[1].er)))
    N(0xa100,4,f.call(0x24be))
    N(0xa104,2,f.ret())
    N(0xa12c,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0xa132,2,f.push(r[0].r))
    N(0xa134,4,f.move(r[4].r,0x8041))
    N(0xa138,2,f.move(r[0].rl,0x0))
    N(0xa13a,2,f.move(r[0].rh,0x4))
    N(0xa13c,2,f.push(r[0].r))
    N(0xa13e,2,f.push(r[4].r))
    N(0xa140,4,f.call(0x2568))
    N(0xa144,2,h.sub8(r[0].rl,0x3))
    N(0xa146,2,f.branch(!(h.ccr&4),0xa16c))
    N(0xa148,2,f.pop(r[4].r))
    N(0xa14a,2,f.push(r[4].r))
    N(0xa14c,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa14e,4,f.call(0x24c6))
    N(0xa152,2,f.pop(r[4].r))
    N(0xa154,2,f.pop(r[0].r))
    N(0xa156,2,f.push(r[0].r))
    N(0xa158,2,f.push(r[4].r))
    N(0xa15a,6,f.move(r[0].rh,h.read8(0x167ba)))
    N(0xa160,2,r[0].rl=h.add8(r[0].rl,r[0].rh))
    N(0xa162,6,f.store8(0x17526,r[0].rl))
    N(0xa168,4,f.call(0x96d6))
    N(0xa16c,2,f.pop(r[4].r))
    N(0xa16e,2,f.pop(r[0].r))
    N(0xa170,2,f.inc(r[4].r,1))
    N(0xa172,2,f.inc(r[0].rl,1))
    N(0xa174,2,f.inc(r[0].rh,-1))
    N(0xa176,2,f.branch(!(h.ccr&4),0xa13c))
    N(0xa178,2,f.pop(r[0].r))
    N(0xa17a,6,f.store8(0x17526,r[0].rl))
    N(0xa180,2,f.ret())
    default:return shared.noteActions.step(h)||shared.voiceActions.step(h);
    }
#undef N
}
}
