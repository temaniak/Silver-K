#include "ksMidiControlCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {MidiControlPartialTest partialTest=nullptr;using T=MidiControlTask;using S=T::Stage;using K=T::Kind;
struct Point{unsigned location,length;};
Point point(const T& t){switch(t.stage()){
 case S::ReadPitchLow:return {0x349c,2};case S::ReadPitchHigh:return {0x349e,4};case S::DoublePitchLow:return {0x34a2,2};case S::DoublePitchWord:return {0x34a4,2};case S::CopyPitchHigh:return {0x34a6,2};case S::CapturePitch:return {0x34a8,2};case S::ClearPitchHigh:return {0x34aa,2};case S::DispatchPitch:return {0x34ac,4};case S::AwaitPitch:case S::FinishPitch:return {0x34b0,2};
 case S::ReadPressure:return {0x34b2,2};case S::CapturePressure:return {0x34b4,2};case S::DispatchPressure:return {0x34b6,4};case S::AwaitPressure:case S::FinishPressure:return {0x34ba,2};
 case S::ReadController:return {0x34bc,2};case S::ReadValue:return {0x34be,4};case S::CompareExpression:return {0x34c2,2};case S::AdmitExpression:return {0x34c4,2};case S::RemapExpression:return {0x34c6,8};case S::CompareWheel:return {0x34ce,2};case S::AdmitWheel:return {0x34d0,2};case S::CaptureWheel:return {0x34d2,2};case S::TailWheel:return {0x34d4,4};case S::CompareBreath:return {0x34d8,2};case S::AdmitBreath:return {0x34da,2};case S::CaptureBreath:return {0x34dc,2};case S::TailBreath:return {0x34de,4};
 case S::CompareBank:return {0x34e2,2};case S::AdmitBank:return {0x34e4,2};case S::CaptureBank:return {0x34e6,2};case S::DispatchPart:return {0x34e8,4};case S::AwaitPart:case S::PublishBank:return {0x34ec,8};case S::FinishBank:return {0x34f4,2};
 case S::CompareSound:return {0x34f6,2};case S::AdmitSound:return {0x34f8,2};case S::ObserveSoundValue:return {0x34fa,2};case S::AdmitSoundValue:return {0x34fc,2};case S::DispatchSound:return {0x34fe,4};case S::AwaitSound:case S::FinishSound:return {0x3502,2};
 case S::CompareReset:return {0x3504,2};case S::AdmitReset:return {0x3506,2};case S::FinishReset:return {0x3508,2};case S::CompareNotes:return {0x350a,2};case S::AdmitNotes:return {0x350c,2};case S::CompareOmniOff:return {0x350e,2};case S::AdmitOmniOff:return {0x3510,2};case S::CompareOmniOn:return {0x3512,2};case S::AdmitOmniOn:return {0x3514,2};case S::CompareMono:return {0x3516,2};case S::AdmitMono:return {0x3518,2};case S::ComparePoly:return {0x351a,2};case S::AdmitPoly:return {0x351c,2};
 case S::ObserveNotesValue:return {0x351e,2};case S::AdmitNotesValue:return {0x3520,2};case S::DispatchRelease:return {0x3522,4};case S::AwaitRelease:case S::DispatchHeldReset:return {0x3526,4};case S::AwaitHeldReset:case S::FinishNotes:return {0x352a,2};case S::HandoffPedal:return {0x352c,0};case S::HandoffWheel:return {0x9e62,0};case S::HandoffBreath:return {0x9e82,0};default:throw std::runtime_error("Invalid native MIDI control stage");}}
}
void setMidiControlPartialForTests(MidiControlPartialTest fn){partialTest=fn;}
bool MidiControlCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x349c||pc==0x34b2||pc==0x34bc){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI control capacity exceeded");selected->emplace(pc==0x349c?K::Pitch:pc==0x34b2?K::Pressure:K::Control,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;if(task.awaitingPart()){const auto address=uint32_t(v[0].er);if(address<0x160d2||address>=0x160d2+256*334||(address-0x160d2)%334)throw std::runtime_error("Invalid native MIDI bank part binding");task.resumePart(uint8_t((address-0x160d2)/334));}if(task.awaitingChild())task.resumeChild();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned base=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](unsigned offset){const auto address=base+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(331):3);
 auto read=[&](T::Read field)->unsigned{switch(field){case T::Read::EventFirst:return uint8_t(h.read8(uint32_t(v[4].er)));case T::Read::EventSecond:return uint8_t(h.read8(uint32_t(v[4].er)+1));case T::Read::ExpressionController:return uint8_t(h.read8(uint32_t(v[5].er)+177));default:return uint8_t(v[0].rl);}};
 auto write=[&](unsigned offset,unsigned data){h.write8(int8_t(data),base+offset);};if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native MIDI control deadline mismatch");
 switch(stage){
 case S::ReadPitchLow:case S::ReadPressure:v[0].rl=int8_t(task.first());value(h,v[0].rl);break;case S::ReadPitchHigh:v[0].rh=int8_t(task.second());value(h,v[0].rh);break;
 case S::DoublePitchLow:clocks.shift(v[0].rl,1,true);break;case S::DoublePitchWord:clocks.shift(v[0].r,1,true);break;case S::CopyPitchHigh:v[0].rl=int8_t(task.value());value(h,v[0].rl);break;
 case S::CapturePitch:case S::CapturePressure:case S::CaptureWheel:case S::CaptureBreath:case S::CaptureBank:v[5].rl=int8_t(task.argument());value(h,v[5].rl);break;case S::ClearPitchHigh:v[5].rh=h.sub8(v[5].rh,v[5].rh);break;
 case S::DispatchPitch:case S::DispatchPressure:case S::DispatchPart:case S::DispatchSound:case S::DispatchRelease:case S::DispatchHeldReset:clocks.call(stage==S::DispatchPitch?0x9e42:stage==S::DispatchPressure?0x9ea2:stage==S::DispatchPart?0x4000:stage==S::DispatchSound?0x9484:stage==S::DispatchRelease?0x9374:0x3074);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI control dispatch clocks differ");return true;
 case S::ReadController:v[0].rh=int8_t(task.first());value(h,v[0].rh);break;case S::ReadValue:v[0].rl=int8_t(task.second());value(h,v[0].rl);break;case S::RemapExpression:v[0].rh=int8_t(task.controller());value(h,v[0].rh);break;
 case S::CompareExpression:h.sub8(v[0].rh,11);break;case S::CompareWheel:h.sub8(v[0].rh,1);break;case S::CompareBreath:h.sub8(v[0].rh,2);break;case S::CompareBank:h.sub8(v[0].rh,32);break;case S::CompareSound:h.sub8(v[0].rh,120);break;case S::CompareReset:h.sub8(v[0].rh,121);break;case S::CompareNotes:h.sub8(v[0].rh,123);break;case S::CompareOmniOff:h.sub8(v[0].rh,124);break;case S::CompareOmniOn:h.sub8(v[0].rh,125);break;case S::CompareMono:h.sub8(v[0].rh,126);break;case S::ComparePoly:h.sub8(v[0].rh,127);break;
 case S::TailWheel:case S::TailBreath:h.clockI(2);break;case S::PublishBank:value(h,int8_t(task.argument()));break;case S::ObserveSoundValue:case S::ObserveNotesValue:value(h,v[0].rl);break;
 case S::FinishPitch:case S::FinishPressure:case S::FinishBank:case S::FinishSound:case S::FinishReset:case S::FinishNotes:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI control return clocks differ");selected->reset();++completed;return true;default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native MIDI control bus clocks differ");const auto next=point(task).location;if(task.handoff()){h.pc=h.makepc(next);selected->reset();++handedOff;}else h.pc=h.makepc(next);return true;
}
}
