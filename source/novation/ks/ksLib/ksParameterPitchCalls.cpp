#include "ksParameterPitchCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
using S=ParameterPitchTask::Stage;ParameterPitchPartialTest partialTest=nullptr;
struct Point{unsigned location,length;};struct Binding{unsigned entry,end;std::array<Point,63> points;};
constexpr std::array<Binding,3> bindings{{{0x7e8c,0x7f66,{{{0x7e8c,6},{0x7e92,8},{0x7e9a,2},{0x7e9c,2},{0x7e9e,2},{0x7ea0,2},{0x7ea2,2},{0x7ea4,2},{0x7ea6,2},{0x7ea8,2},{0x7eaa,4},{0x7eae,2},{0x7eb0,8},{0x7eb8,2},{0x7eba,2},{0x7ebc,6},{0x7ec2,8},{0x7eca,8},{0x7ed2,2},{0x7ed4,6},{0x7eda,2},{0x7edc,2},{0x7ede,2},{0x7ee0,4},{0x7ee4,8},{0x7eec,2},{0x7eee,2},{0x7ef0,2},{0x7ef2,4},{0x7ef6,8},{0x7efe,2},{0x7f00,2},{0x7f02,2},{0x7f04,2},{0x7f06,2},{0x7f08,2},{0x7f0a,2},{0x7f0c,2},{0x7f0e,4},{0x7f12,10},{0x7f1c,8},{0x7f24,4},{0x7f28,2},{0x7f2a,2},{0x7f2c,2},{0x7f2e,2},{0x7f30,2},{0x7f32,2},{0x7f34,4},{0x7f38,4},{0x7f3c,2},{0x7f3e,8},{0x7f46,2},{0x7f48,2},{0x7f4a,2},{0x7f4c,6},{0x7f52,4},{0x7f56,4},{0x7f5a,2},{0x7f5c,4},{0x7f60,2},{0x0,0},{0x7f62,4}}}},{0x8100,0x81da,{{{0x8100,6},{0x8106,8},{0x810e,2},{0x8110,2},{0x8112,2},{0x8114,2},{0x8116,2},{0x8118,2},{0x811a,2},{0x811c,2},{0x811e,4},{0x8122,2},{0x8124,8},{0x812c,2},{0x812e,2},{0x8130,6},{0x8136,8},{0x813e,8},{0x8146,2},{0x8148,6},{0x814e,2},{0x8150,2},{0x8152,2},{0x8154,4},{0x8158,8},{0x8160,2},{0x8162,2},{0x8164,2},{0x8166,4},{0x816a,8},{0x8172,2},{0x8174,2},{0x8176,2},{0x8178,2},{0x817a,2},{0x817c,2},{0x817e,2},{0x8180,2},{0x8182,4},{0x8186,10},{0x8190,8},{0x8198,4},{0x819c,2},{0x819e,2},{0x81a0,2},{0x81a2,2},{0x81a4,2},{0x81a6,2},{0x81a8,4},{0x81ac,4},{0x81b0,2},{0x81b2,8},{0x81ba,2},{0x81bc,2},{0x81be,2},{0x81c0,6},{0x81c6,4},{0x81ca,4},{0x81ce,2},{0x81d0,4},{0x81d4,2},{0x0,0},{0x81d6,4}}}},{0x8374,0x844e,{{{0x8374,6},{0x837a,8},{0x8382,2},{0x8384,2},{0x8386,2},{0x8388,2},{0x838a,2},{0x838c,2},{0x838e,2},{0x8390,2},{0x8392,4},{0x8396,2},{0x8398,8},{0x83a0,2},{0x83a2,2},{0x83a4,6},{0x83aa,8},{0x83b2,8},{0x83ba,2},{0x83bc,6},{0x83c2,2},{0x83c4,2},{0x83c6,2},{0x83c8,4},{0x83cc,8},{0x83d4,2},{0x83d6,2},{0x83d8,2},{0x83da,4},{0x83de,8},{0x83e6,2},{0x83e8,2},{0x83ea,2},{0x83ec,2},{0x83ee,2},{0x83f0,2},{0x83f2,2},{0x83f4,2},{0x83f6,4},{0x83fa,10},{0x8404,8},{0x840c,4},{0x8410,2},{0x8412,2},{0x8414,2},{0x8416,2},{0x8418,2},{0x841a,2},{0x841c,4},{0x8420,4},{0x8424,2},{0x8426,8},{0x842e,2},{0x8430,2},{0x8432,2},{0x8434,6},{0x843a,4},{0x843e,4},{0x8442,2},{0x8444,4},{0x8448,2},{0x0,0},{0x844a,4}}}}}};
Point point(const ParameterPitchTask& task,unsigned b){if(task.awaitingWord())return {bindings[b].end,0};return bindings[b].points[task.awaitingVoice()?29:unsigned(task.stage())];}
}
void setParameterPitchPartialForTests(ParameterPitchPartialTest fn){partialTest=fn;}
void ParameterPitchCalls::retireReturns(ControllerCpu& h){const unsigned pc=unsigned(h.getPC());for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingWord()&&point(c->task,c->binding).location==pc){c->task.resumeWord();c.reset();++completed;}}
bool ParameterPitchCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(selected&&(*selected)->task.awaitingWord()&&point((*selected)->task,(*selected)->binding).location==pc){(*selected)->task.resumeWord();selected->reset();selected=nullptr;++completed;}
 for(unsigned b=0;b<bindings.size();++b)if(pc==bindings[b].entry){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native oscillator-pitch capacity exceeded");selected->emplace(h.getSP(),b);++started;break;}
 if(!selected||point((*selected)->task,(*selected)->binding).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;
 if(task.awaitingVoice()){const auto pointer=uint32_t(r[3].er);if(pointer<0x17560||pointer>=0x17560+20*256||(pointer-0x17560)%20)throw std::runtime_error("Invalid oscillator-pitch voice binding");task.resumeVoice(uint8_t((pointer-0x17560)/20));}
 const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task,call.binding).length);uint32_t observed=0;
 auto read=[&](ParameterPitchTask::Read what)->uint32_t{using R=ParameterPitchTask::Read;switch(what){
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

case S::DispatchWord:{if((uint32_t(r[1].er)>>8)!=task.dspWord())throw std::runtime_error("Native oscillator-pitch payload projection differs");f.call(0x2520);break;}
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native oscillator-pitch clocks differ");
 if(stage==S::DispatchWord||stage==S::DispatchVoice){h.pc=h.makepc(f.next);return true;}

 h.pc=h.makepc(point(task,call.binding).location);return true;
}
}
