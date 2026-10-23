#include "ksMidiOutputCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
MidiOutputPartialTest partialTest=nullptr;using T=MidiOutputTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,162> points{{{0x2a58,4},{0x2a5c,2},{0x2a5e,2},{0x2a60,4},{0x2a64,2},{0x2a66,4},{0x2a6a,2},{0x2a6c,4},{0x2a70,2},{0x2a72,4},{0x2a76,2},{0x2a78,4},{0x2a7c,2},{0x2a7e,4},{0x2a82,2},{0x2a84,4},{0x2a88,2},{0x2a8a,2},{0x2a8c,2},{0x2a8e,2},{0x2a90,4},{0x2a94,2},{0x2a96,2},{0x2a98,2},{0x2a9a,2},{0x2a9c,2},{0x2a9e,2},{0x2aa0,2},{0x2aa2,2},{0x2aa4,4},{0x2aa8,2},{0x2aaa,2},{0x2aac,2},{0x2aae,2},{0x2ab0,2},{0x2ab2,2},{0x2ab4,2},{0x2ab6,2},{0x2ab8,2},{0x2aba,2},{0x2abc,2},{0x2abe,2},{0x2ac0,2},{0x2ac2,2},{0x2ac4,2},{0x2ac6,2},{0x2ac8,4},{0x2acc,2},{0x2ace,2},{0x2ad0,2},{0x2ad2,2},{0x2ad4,2},{0x2ad6,2},{0x2ad8,2},{0x2ada,2},{0x2adc,4},{0x2ae0,2},{0x2ae2,2},{0x2ae4,2},{0x2ae6,2},{0x2ae8,2},{0x2aea,4},{0x2aee,6},{0x2af4,8},{0x2afc,2},{0x2afe,2},{0x2b00,4},{0x2b04,4},{0x2b08,4},{0x2b0c,2},{0x2b0e,4},{0x2b12,6},{0x2b18,8},{0x2b20,2},{0x2b22,2},{0x2b24,2},{0x2b26,2},{0x2b28,2},{0x2b2a,4},{0x2b2e,2},{0x2b30,2},{0x2b32,4},{0x2b36,2},{0x2b38,4},{0x2b3c,6},{0x2b42,8},{0x2b4a,2},{0x2b4c,2},{0x2b4e,2},{0x2b50,2},{0x2b52,2},{0x2b54,2},{0x2b56,2},{0x2b58,2},{0x2b5a,4},{0x2b5e,2},{0x2b60,2},{0x2b62,4},{0x2b66,2},{0x2b68,2},{0x2b6a,2},{0x2b6c,4},{0x2b70,2},{0x2b72,4},{0x2b76,2},{0x2b78,4},{0x2b7c,2},{0x2b7e,4},{0x2b82,2},{0x2b84,4},{0x2b88,2},{0x2b8a,2},{0x2b8c,2},{0x2b8e,4},{0x2b92,2},{0x2b94,4},{0x2b98,2},{0x2b9a,4},{0x2b9e,2},{0x2ba0,4},{0x2ba4,2},{0x2ba6,4},{0x2baa,6},{0x2bb0,2},{0x2bb2,4},{0x2bb6,8},{0x2bbe,2},{0x2bc0,2},{0x2bc2,2},{0x2bc4,2},{0x2bc6,2},{0x2bc8,4},{0x2bcc,8},{0x2bd4,2},{0x2bd6,4},{0x2bda,2},{0x2bdc,4},{0x2be0,2},{0x2be2,4},{0x2be6,2},{0x2be8,2},{0x2bea,2},{0x2bec,2},{0x2bee,4},{0x2bf2,6},{0x2bf8,8},{0x2c00,4},{0x2c04,2},{0x2c06,2},{0x2c08,4},{0x2c0c,2},{0x2c0e,4},{0x2c12,2},{0x2c14,4},{0x2c18,2},{0x2c1a,4},{0x2c1e,2},{0x2c20,4},{0x2c24,2},{0x2c26,4},{0x2c2a,2},{0x2c2c,4}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned address,int previous){address&=0xffffff;if(address<0x400000)return !(previous&1)&&address==(unsigned(previous)|1u)?0:3;if(address>=0xfffd10&&address<0xffffa0)return !(previous&1)&&address==(unsigned(previous)|1u)?0:1;return address>=0xffffa0?1:3;}
}
void setMidiOutputPartialForTests(MidiOutputPartialTest value){partialTest=value;}
bool MidiOutputCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
 for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x2a58:kind=T::Kind::NoteOff;break;case 0x2a60:kind=T::Kind::NoteOn;break;case 0x2a76:kind=T::Kind::Pedal;break;case 0x2a8e:kind=T::Kind::Expression;break;case 0x2aa2:kind=T::Kind::PitchBend;break;case 0x2ac6:kind=T::Kind::Modulation;break;case 0x2ada:kind=T::Kind::Pressure;break;case 0x2aea:kind=T::Kind::Byte;break;case 0x2b0c:kind=T::Kind::Empty;break;case 0x2b0e:kind=T::Kind::RouteOff;break;case 0x2b38:kind=T::Kind::RouteOn;break;case 0x2b60:kind=T::Kind::QueueOff;break;case 0x2b6a:kind=T::Kind::QueueOn;break;case 0x2b82:kind=T::Kind::TransmitOff;break;case 0x2b8c:kind=T::Kind::TransmitOn;break;case 0x2ba4:kind=T::Kind::Local;break;case 0x2bc8:kind=T::Kind::Panic;break;case 0x2be8:kind=T::Kind::Control;break;case 0x2c06:kind=T::Kind::RawControl;break;case 0x2c1e:kind=T::Kind::Program;break; default:break;}
 if(kind){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native MIDI output capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[6].r),uint16_t(h.regs[5].r),uint8_t(h.regs[2].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[6].r),uint32_t(r[0].er),bool(h.ccr&4));const auto stage=task.stage();
 const unsigned dataAddress=(task.address()+(stage==S::ReadLocalFlags?327:11))&0xffffff;
 const unsigned dataRead=byteClocks(dataAddress,int(pc+point(task).length-1)),partWrite=byteClocks(0x17526,h.lastwrite);
 const auto cost=task.nextClocks(dataRead,partWrite);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(field==T::Read::SendMode?0x169ed:field==T::Read::ControlChannel?0x169b1:dataAddress));};
 auto write=[&](T::Write,uint8_t value){f.store8(0x17526,int8_t(value));};
 if(task.advanceBefore(cost,read,write,dataRead,partWrite)!=cost)throw std::runtime_error("Native MIDI output admission differs");
 switch(stage){case S::SelectOffChannel:f.call(0x43e8);break;
case S::OffStatus:r[6].rl|=0xffffff80;value(h,r[6].rl);break;
case S::SelectOnChannel:f.call(0x43e8);break;
case S::OnStatus:r[6].rl|=0xffffff90;value(h,r[6].rl);break;
case S::SendNoteStatus:f.call(0x1104);break;
case S::BindNote:f.move(r[6].rl,r[5].rl);break;
case S::SendNote:f.call(0x1104);break;
case S::BindVelocity:f.move(r[6].rl,r[5].rh);break;
case S::FinishNote:h.clockI(2);f.next=0x1104;break;
case S::SavePedal:f.push(r[6].r);break;
case S::SelectPedalChannel:f.call(0x43e8);break;
case S::PedalStatus:r[6].rl|=0xffffffb0;value(h,r[6].rl);break;
case S::SendPedalStatus:f.call(0x2aea);break;
case S::BindPedalController:f.move(r[6].rl,0x40);break;
case S::SendPedalController:f.call(0x2aea);break;
case S::RestorePedal:f.pop(r[6].r);break;
case S::BindPedalValue:f.move(r[6].rl,r[6].rh);break;
case S::FinishPedal:f.next=0x2aea;break;
case S::SaveExpression:f.push(r[6].r);break;
case S::SelectExpressionChannel:f.call(0x43e8);break;
case S::ExpressionStatus:r[6].rl|=0xffffffb0;value(h,r[6].rl);break;
case S::SendExpressionStatus:f.call(0x2aea);break;
case S::BindExpressionController:f.move(r[6].rl,0xb);break;
case S::SendExpressionController:f.call(0x2aea);break;
case S::RestoreExpression:f.pop(r[6].r);break;
case S::BindExpressionValue:f.move(r[6].rl,r[6].rh);break;
case S::FinishExpression:f.next=0x2aea;break;
case S::SaveBend:f.push(r[6].r);break;
case S::SelectBendChannel:f.call(0x43e8);break;
case S::BendStatus:r[6].rl|=0xffffffe0;value(h,r[6].rl);break;
case S::SendBendStatus:f.call(0x2aea);break;
case S::RestoreBend:f.pop(r[6].r);break;
case S::ClearBendLow:f.move(r[6].rl,0x0);break;
case S::HalfBend:f.shift(r[6].rh,1,false);break;
case S::SetBendLow:f.move(r[6].rl,0x40);break;
case S::CompareBendEnd:h.sub8(r[6].rh,0x7f);break;
case S::SetBendEnd:f.move(r[6].rl,0x7f);break;
case S::SaveBendHigh:f.push(r[6].r);break;
case S::SendBendLow:f.call(0x2aea);break;
case S::RestoreBendHigh:f.pop(r[6].r);break;
case S::BindBendHigh:f.move(r[6].rl,r[6].rh);break;
case S::FinishBend:f.next=0x2aea;break;
case S::SaveModulation:f.push(r[6].r);break;
case S::SelectModulationChannel:f.call(0x43e8);break;
case S::ModulationStatus:r[6].rl|=0xffffffb0;value(h,r[6].rl);break;
case S::SendModulationStatus:f.call(0x2aea);break;
case S::BindModulationController:f.move(r[6].rl,0x1);break;
case S::SendModulationController:f.call(0x2aea);break;
case S::RestoreModulation:f.pop(r[6].r);break;
case S::BindModulationValue:f.move(r[6].rl,r[6].rh);break;
case S::FinishModulation:f.next=0x2aea;break;
case S::SavePressure:f.push(r[6].r);break;
case S::SelectPressureChannel:f.call(0x43e8);break;
case S::PressureStatus:r[6].rl|=0xffffffd0;value(h,r[6].rl);break;
case S::SendPressureStatus:f.call(0x2aea);break;
case S::RestorePressure:f.pop(r[6].r);break;
case S::BindPressureValue:f.move(r[6].rl,r[6].rh);break;
case S::FinishPressure:f.next=0x2aea;break;
case S::SaveByteContext:f.push(r[0].er);break;
case S::SelectByteGlobals:f.move(r[0].er,0x168fc);break;
case S::ReadByteMode:f.move(r[0].rl,task.mode());break;
case S::CompareByteMode:h.sub8(r[0].rl,0x3f);break;
case S::QueueByte:f.call(0x2c88);break;
case S::RestoreByteContext:f.pop(r[0].er);break;
case S::FinishByte:h.clockI(2);f.next=0x1104;break;
case S::ReturnEmpty:f.ret();break;
case S::SaveOffContext:f.push(r[0].er);break;
case S::SelectOffGlobals:f.move(r[0].er,0x168fc);break;
case S::ReadOffMode:f.move(r[0].rl,task.mode());break;
case S::CompareOffMode:h.sub8(r[0].rl,0x3f);break;
case S::SaveOffEvent:f.push(r[6].r);break;
case S::QueueOffEvent:f.call(0x2b60);break;
case S::RestoreOffEvent:f.pop(r[6].r);break;
case S::CheckOffLocal:f.call(0x2ba4);break;
case S::SendOffEvent:f.call(0x2b82);break;
case S::RestoreOffContext:f.pop(r[0].er);break;
case S::ReturnOff:f.ret();break;
case S::SaveOnContext:f.push(r[0].er);break;
case S::SelectOnGlobals:f.move(r[0].er,0x168fc);break;
case S::ReadOnMode:f.move(r[0].rl,task.mode());break;
case S::CompareOnMode:h.sub8(r[0].rl,0x3f);break;
case S::SaveOnEvent:f.push(r[6].r);break;
case S::QueueOnEvent:f.call(0x2b6a);break;
case S::RestoreOnEvent:f.pop(r[6].r);break;
case S::CheckOnLocal:f.call(0x2ba4);break;
case S::SendOnEvent:f.call(0x2b8c);break;
case S::RestoreOnContext:f.pop(r[0].er);break;
case S::ReturnOn:f.ret();break;
case S::SaveQueueOff:f.push(r[6].r);break;
case S::SelectQueueOffChannel:f.call(0x43e8);break;
case S::QueueOffStatus:r[6].rl|=0xffffff80;value(h,r[6].rl);break;
case S::SaveQueueOn:f.push(r[6].r);break;
case S::SelectQueueOnChannel:f.call(0x43e8);break;
case S::QueueOnStatus:r[6].rl|=0xffffff90;value(h,r[6].rl);break;
case S::QueueNoteStatus:f.call(0x2c88);break;
case S::RestoreQueueNote:f.pop(r[6].r);break;
case S::QueueNote:f.call(0x2c88);break;
case S::BindQueueVelocity:f.move(r[6].rl,r[6].rh);break;
case S::FinishQueueNote:h.clockI(2);f.next=0x2c88;break;
case S::SaveTransmitOff:f.push(r[6].r);break;
case S::SelectTransmitOffChannel:f.call(0x43e8);break;
case S::TransmitOffStatus:r[6].rl|=0xffffff80;value(h,r[6].rl);break;
case S::SaveTransmitOn:f.push(r[6].r);break;
case S::SelectTransmitOnChannel:f.call(0x43e8);break;
case S::TransmitOnStatus:r[6].rl|=0xffffff90;value(h,r[6].rl);break;
case S::TransmitNoteStatus:f.call(0x1104);break;
case S::RestoreTransmitNote:f.pop(r[6].r);break;
case S::TransmitNote:f.call(0x1104);break;
case S::BindTransmitVelocity:f.move(r[6].rl,r[6].rh);break;
case S::FinishTransmitNote:h.clockI(2);f.next=0x1104;break;
case S::SaveLocalEvent:f.push(r[6].r);break;
case S::SelectLocalPart:f.call(0x43e8);break;
case S::RestoreLocalEvent:f.pop(r[6].r);break;
case S::LocateLocalControls:f.call(0x4000);break;
case S::ReadLocalFlags:f.move(r[0].rl,task.flags());break;
case S::TestLocalEnable:flag(h,4,(uint8_t(r[0].rl)&(1u<<0x2))==0);break;
case S::MaskLocalFlags:r[0].rl&=0x60;value(h,r[0].rl);break;
case S::ReturnLocal:f.ret();break;
case S::LocatePanicControls:f.call(0xa66a);break;
case S::ReadPanicChannel:f.move(r[6].rl,task.low());break;
case S::PanicStatus:r[6].rl|=0xffffffb0;value(h,r[6].rl);break;
case S::SendPanicStatus:f.call(0x1104);break;
case S::BindPanicController:f.move(r[6].rl,0x7b);break;
case S::SendPanicController:f.call(0x1104);break;
case S::BindPanicValue:f.move(r[6].rl,0x0);break;
case S::SendPanicValue:f.call(0x1104);break;
case S::ReturnPanic:f.ret();break;
case S::SaveControlEvent:f.push(r[6].r);break;
case S::CompareSpecialControl:h.sub8(r[2].rl,0x7e);break;
case S::SaveControlContext:f.push(r[0].er);break;
case S::SelectControlGlobals:f.move(r[0].er,0x168fc);break;
case S::ReadControlChannel:f.move(r[6].rl,task.low());break;
case S::RestoreControlContext:f.pop(r[0].er);break;
case S::SaveRawControlEvent:f.push(r[6].r);break;
case S::SelectRawControlChannel:f.call(0x43e8);break;
case S::ControlStatus:r[6].rl|=0xffffffb0;value(h,r[6].rl);break;
case S::SendControlStatus:f.call(0x1104);break;
case S::RestoreControlEvent:f.pop(r[6].r);break;
case S::SendControlIndex:f.call(0x1104);break;
case S::BindControlValue:f.move(r[6].rl,r[6].rh);break;
case S::FinishControl:h.clockI(2);f.next=0x1104;break;
case S::SaveProgramEvent:f.push(r[6].r);break;
case S::SelectProgramChannel:f.call(0x43e8);break;
case S::ProgramStatus:r[6].rl|=0xffffffc0;value(h,r[6].rl);break;
case S::SendProgramStatus:f.call(0x1104);break;
case S::RestoreProgramEvent:f.pop(r[6].r);break;
case S::FinishProgram:h.clockI(2);f.next=0x1104;break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native MIDI output clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
