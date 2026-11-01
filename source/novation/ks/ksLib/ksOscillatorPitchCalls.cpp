#include "ksOscillatorPitchCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
using S=OscillatorPitchTask::Stage;OscillatorPitchPartialTest partialTest=nullptr;
struct Point{unsigned location,length;};constexpr std::array<Point,63> points{{{0x0,6},{0x6,8},{0xe,2},{0x10,2},{0x12,2},{0x14,2},{0x16,2},{0x18,2},{0x1a,2},{0x1c,2},{0x1e,4},{0x22,2},{0x24,8},{0x2c,2},{0x2e,2},{0x30,6},{0x36,8},{0x3e,8},{0x46,2},{0x48,6},{0x4e,2},{0x50,2},{0x52,2},{0x54,4},{0x58,8},{0x60,2},{0x62,2},{0x64,2},{0x66,4},{0x6a,8},{0x72,2},{0x74,2},{0x76,2},{0x78,2},{0x7a,2},{0x7c,2},{0x7e,2},{0x80,2},{0x82,4},{0x86,10},{0x90,8},{0x98,4},{0x9c,2},{0x9e,2},{0xa0,2},{0xa2,2},{0xa4,2},{0xa6,2},{0xa8,4},{0xac,4},{0xb0,2},{0xb2,8},{0xba,2},{0xbc,2},{0xbe,2},{0xc0,6},{0xc6,4},{0xca,4},{0xce,2},{0xd0,4},{0xd4,2},{0xd6,4},{0xda,4}}};
Point point(const OscillatorPitchTask& task){if(task.stage()==S::Return)return {0x7de2,2};
 if(task.awaitingWord())return {0x7b48+0xde*(task.output()+1),0};
 const auto p=points[task.awaitingVoice()?29:unsigned(task.stage())];return {0x7b48+0xde*task.output()+p.location,p.length};}
}
void setOscillatorPitchPartialForTests(OscillatorPitchPartialTest fn){partialTest=fn;}
bool OscillatorPitchCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(selected&&(*selected)->task.awaitingWord()&&point((*selected)->task).location==pc)(*selected)->task.resumeWord();
 if(pc==0x7b48){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native oscillator-pitch capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;
 if(task.awaitingVoice()){const auto pointer=uint32_t(r[3].er);if(pointer<0x17560||pointer>=0x17560+20*256||(pointer-0x17560)%20)throw std::runtime_error("Invalid oscillator-pitch voice binding");task.resumeVoice(uint8_t((pointer-0x17560)/20));}
 const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](OscillatorPitchTask::Read what)->uint32_t{using R=OscillatorPitchTask::Read;switch(what){
 case R::StaticPitch:return observed=uint16_t(h.read16(0x15130));
 case R::Octaves:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+6)));
 case R::Coarse:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.coarseOffset())));
 case R::GlobalFine:return observed=uint8_t(h.read8(0x169f0));
 case R::PatchFine:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.fineOffset())));
 case R::WheelFine:return observed=uint8_t(h.read8(0x17521));
 case R::Flags:return observed=uint8_t(h.read8(r[6].er));
 case R::UnisonCounter:return observed=uint8_t(h.read8(0x17560+20*task.slot()+12));
 case R::SelectedPatch:return observed=uint32_t(h.read32(0x17560+20*task.slot()));
 case R::Spread:return observed=uint8_t(h.read8(int32_t(call.selectedPatch+1)));
 case R::PitchDepth:return observed=uint8_t(h.read8(int32_t(uint32_t(r[6].er)+task.depthOffset())));
 case R::Wheel:return observed=uint16_t(h.read16(0x17516));}return 0;};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native oscillator-pitch admission mismatch");
 switch(stage){case S::ReadStaticPitch:{f.move(r[1].r,int16_t(observed));break;}
case S::ReadOctaves:{f.move(r[0].rl,int8_t(observed));break;}
case S::SetOctaveShift:{f.move(r[0].rh,int8_t(task.output()));break;}
case S::AdmitShift:{;break;}
case S::ShiftOctaves:{f.shift(r[0].rl,2,false);break;}
case S::DecrementShift:{f.inc(r[0].rh,-1);break;}
case S::AdmitMoreShift:{;break;}
case S::MaskOctaves:{r[0].rl&=0x3;value(h,r[0].rl);break;}
case S::BiasOctave:{f.inc(r[0].rl,-1);break;}
case S::SetSemitoneScale:{f.move(r[0].rh,0xc);break;}
case S::MultiplyOctave:{f.multiplySigned(r[0].rh,r[0].r);break;}
case S::AddOctave:{r[1].rh=h.add8(r[1].rh,r[0].rl);break;}
case S::ReadCoarse:{f.move(r[0].rl,int8_t(observed));break;}
case S::BiasCoarse:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
case S::AddCoarse:{r[1].rh=h.add8(r[1].rh,r[0].rl);break;}
case S::BindGlobals:{f.move(r[3].er,0x168fc);break;}
case S::ReadGlobalFine:{f.move(r[0].rl,int8_t(observed));break;}
case S::ReadPatchFine:{f.move(r[0].rh,int8_t(observed));break;}
case S::AddFine:{r[0].rl=h.add8(r[0].rl,r[0].rh);break;}
case S::ReadWheelFine:{f.move(r[0].rh,int8_t(observed));break;}
case S::AddWheelFine:{r[0].rl=h.add8(r[0].rl,r[0].rh);break;}
case S::ClearCarry:{f.move(r[0].rh,0x0);break;}
case S::CaptureCarry:{r[0].rh=h.add8(r[0].rh,r[0].rh,h.ccr,true);break;}
case S::BiasFine:{r[0].r=h.sub16(r[0].r,0xc0);break;}
case S::ReadFlags:{f.move(r[2].rl,int8_t(observed));break;}
case S::MaskUnison:{r[2].rl&=0x7;value(h,r[2].rl);break;}
case S::AdmitUnison:{;break;}
case S::ScaleUnison:{f.shift(r[2].rl,2,true);break;}
case S::DispatchVoice:{f.call(0xa656);break;}
case S::ReadUnisonCounter:{f.move(r[2].rh,int8_t(observed));break;}
case S::MaskUnisonCounter:{r[2].rh&=0x7;value(h,r[2].rh);break;}
case S::DoubleCounter0:{f.shift(r[2].rh,1,true);break;}
case S::DoubleCounter1:{f.shift(r[2].rh,1,true);break;}
case S::DoubleCounter2:{f.shift(r[2].rh,1,true);break;}
case S::BiasCounter:{r[2].rh=h.add8(r[2].rh,0x1);break;}
case S::CenterCounter:{r[2].rh=h.sub8(r[2].rh,r[2].rl);break;}
case S::BiasDivisor:{r[2].rl=h.add8(r[2].rl,0xc);break;}
case S::DoubleDivisor:{f.shift(r[2].rl,1,true);break;}
case S::DivideUnison:{f.divideSigned(r[2].rl,r[2].r);break;}
case S::ReadSelectedPatch:{f.move(r[3].er,int32_t(observed));call.selectedPatch=observed;break;}
case S::ReadSpread:{f.move(r[2].rh,int8_t(observed));break;}
case S::MultiplySpread:{f.multiplySigned(r[2].rh,r[2].r);break;}
case S::CaptureSpread:{f.move(r[2].rl,r[2].rh);break;}
case S::SignSpread:{r[2].r=int8_t(r[2].r);value(h,r[2].r);break;}
case S::AddSpread:{r[0].r=h.add16(r[0].r,r[2].r);break;}
case S::ScaleFine0:{f.shift(r[0].r,1,true);break;}
case S::ScaleFine1:{f.shift(r[0].r,1,true);break;}
case S::ScaleFine2:{f.shift(r[0].r,1,true);break;}
case S::SetFineScale:{f.move(r[0].e,0x51eb);break;}
case S::MultiplyFine:{f.multiplySigned(r[0].e,r[0].er);break;}
case S::AddScaledFine:{r[1].r=h.add16(r[1].r,r[0].e);break;}
case S::ReadPitchDepth:{f.move(r[0].rl,int8_t(observed));break;}
case S::BiasPitchDepth:{r[0].rl=h.add8(r[0].rl,0xffffffc0);break;}
case S::SignPitchDepth:{r[0].r=int8_t(r[0].r);value(h,r[0].r);break;}
case S::CapturePitchDepth:{f.move(r[0].e,r[0].r);break;}
case S::ReadWheel:{f.move(r[0].r,int16_t(observed));break;}
case S::BiasWheel:{r[0].r=h.sub16(r[0].r,0x100);break;}
case S::MultiplyWheel:{f.multiplySigned(r[0].e,r[0].er);break;}
case S::AddWheel:{r[1].r=h.add16(r[1].r,r[0].r);break;}
case S::SetPitchScale:{f.move(r[1].e,0xaaab);break;}
case S::MultiplyPitch:{r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20);break;}
case S::SetOffset:{f.move(r[4].r,int16_t(task.offset()));break;}
case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native oscillator-pitch payload projection differs");f.call(0x785a);break;}
case S::Return:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native oscillator-pitch clocks differ");
 if(stage==S::DispatchWord||stage==S::DispatchVoice){h.pc=h.makepc(f.next);return true;}
 if(stage==S::Return){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}
 h.pc=h.makepc(point(task).location);return true;
}
}
