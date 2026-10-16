#include "ksStateInitCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{namespace{StateInitPartialTest partialTest=nullptr;using T=StateInitTask;using S=T::Stage;using R=T::Read;using W=T::Write;
struct Point{unsigned location,length;};Point point(S stage){switch(stage){
 case S::BindReset:return {0x41e2,6};
 case S::SetResetCount:return {0x41e8,4};
 case S::ClearResetValue:return {0x41ec,2};
 case S::PublishReset:return {0x41ee,2};
 case S::AdvanceReset:return {0x41f0,2};
 case S::TickReset:return {0x41f2,2};
 case S::AdmitReset:return {0x41f4,2};
 case S::SetDefault:return {0x41f6,2};
 case S::PublishDefault:return {0x41f8,6};
 case S::DispatchPanelDefault:return {0x41fe,4};
 case S::DispatchTimer0:return {0x4202,4};
 case S::DispatchTimer1:return {0x4206,4};
 case S::DispatchTimer2:return {0x420a,4};
 case S::DispatchTimer3:return {0x420e,4};
 case S::DispatchTimer4:return {0x4212,4};
 case S::DispatchBanks:return {0x4216,4};
 case S::DispatchNotes:return {0x421a,4};
 case S::DispatchMidi:return {0x421e,4};
 case S::DispatchPanelTimers:return {0x4222,4};
 case S::SetPinRequest:return {0x4226,2};
 case S::DispatchPin:return {0x4228,4};
 case S::ShiftPin:return {0x422c,2};
 case S::CapturePin:return {0x422e,2};
 case S::DispatchPanelValue:return {0x4230,4};
 case S::DispatchPanelState:return {0x4234,4};
 case S::DispatchGlobalBank:return {0x4238,4};
 case S::BindGlobal:return {0x423c,6};
 case S::PublishGlobal:return {0x4242,8};
 case S::ClearPatchControls:return {0x424a,2};
 case S::PublishPatch0:return {0x424c,8};
 case S::PublishPatch1:return {0x4254,8};
 case S::PublishPatch2:return {0x425c,8};
 case S::PublishPreviewGate:return {0x4264,6};
 case S::SetPatchBias:return {0x426a,2};
 case S::PublishPatchBias:return {0x426c,8};
 case S::ReadClock0High:return {0x4274,8};
 case S::ReadClock0Low:return {0x427c,8};
 case S::PublishClock0:return {0x4284,6};
 case S::ReadClock1High:return {0x428a,8};
 case S::ReadClock1Low:return {0x4292,8};
 case S::PublishClock1:return {0x429a,6};
 case S::ReadActiveMask:return {0x42a0,8};
 case S::PublishActiveMask:return {0x42a8,6};
 case S::ReadReceiveMask:return {0x42ae,8};
 case S::PublishReceiveMask:return {0x42b6,6};
 case S::SetSentinel:return {0x42bc,2};
 case S::PublishSentinel0:return {0x42be,6};
 case S::PublishSentinel1:return {0x42c4,6};
 case S::SetSingleMode:return {0x42ca,2};
 case S::PublishSingleMode:return {0x42cc,6};
 case S::DispatchSingleMode:return {0x42d2,4};
 case S::DispatchSingle:return {0x42d6,4};
 case S::SetMultiMode:return {0x42da,2};
 case S::PublishMultiMode:return {0x42dc,6};
 case S::DispatchMultiMode:return {0x42e2,4};
 case S::DispatchMulti:return {0x42e6,4};
 case S::RebindGlobal:return {0x42ea,6};
 case S::ReadPreferredMode:return {0x42f0,8};
 case S::PublishPreferredMode:return {0x42f8,6};
 case S::SetPreferredSentinel:return {0x42fe,2};
 case S::PublishPreferredSentinel0:return {0x4300,6};
 case S::PublishPreferredSentinel1:return {0x4306,6};
 case S::DispatchPreferredMode:return {0x430c,4};
 case S::ReadSelectedMode:return {0x4310,6};
 case S::AdmitSelectedMode:return {0x4316,2};
 case S::DispatchPreferredSingle:return {0x4318,4};
 case S::SkipPreferredMulti:return {0x431c,2};
 case S::DispatchPreferredMulti:return {0x431e,4};
 case S::DispatchConfiguration:return {0x4322,4};
 case S::DispatchDisplay:return {0x4326,4};
 case S::DispatchClock:return {0x432a,4};
 case S::PublishClock:return {0x432e,6};
 case S::DispatchHardware:return {0x4334,4};
 case S::Complete:return {0x4338,0};default:throw std::runtime_error("Invalid native initialization stage");}}
Point point(const T&t){return point(t.awaiting()?t.resumeStage():t.stage());}
unsigned address(W f,unsigned index){switch(f){case W::ResetCell:return 0x16782+index;case W::Default:return 0x167a4;case W::ActiveGlobalPatch:return 0x167bc;
case W::Patch0:return 0x168fc+229;case W::Patch1:return 0x168fc+230;case W::Patch2:return 0x168fc+231;case W::PreviewGate:return 0x167d5;case W::PatchBias:return 0x168fc+228;
case W::Clock0:return 0x167aa;case W::Clock1:return 0x167ae;case W::ActiveMask:return 0x167d0;case W::ReceiveMask:return 0x167d1;case W::Sentinel0:return 0x167bb;case W::Sentinel1:return 0x167d4;case W::SelectedMode:return 0x167ba;case W::LastClock:return 0x16780;}return 0;}
}
void setStateInitPartialForTests(StateInitPartialTest fn){partialTest=fn;}
bool StateInitCalls::step(ControllerCpu&h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x41e2){if(!selected)for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native initialization capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto&task=(*selected)->task;auto&v=h.regs;if(task.awaiting())task.resumeChild();if(task.complete()){selected->reset();++completed;return false;}
 const auto stage=task.stage();const auto before=h.cycles;Boundary f(h,pc,point(task).length);
 auto timing=[&](W field,unsigned index){unsigned a=address(field,index);return (a&1)&&h.lastwrite==int(a-1)?0u:3u;};
 auto read=[&](R what)->unsigned{switch(what){case R::HardwarePin:return uint8_t(v[6].rl);case R::Clock0High:return uint8_t(h.read8(0x168fc+234));case R::Clock0Low:return uint8_t(h.read8(0x168fc+235));case R::Clock1High:return uint8_t(h.read8(0x168fc+236));case R::Clock1Low:return uint8_t(h.read8(0x168fc+237));case R::ActiveMask:return uint8_t(h.read8(0x168fc+189));case R::ReceiveMask:return uint8_t(h.read8(0x168fc+190));case R::PreferredMode:return uint8_t(h.read8(0x168fc+238));case R::SelectedMode:return uint8_t(h.read8(0x167ba));case R::CurrentClock:return uint16_t(v[6].r);}return 0;};
 auto write=[&](W field,unsigned index,unsigned data){const auto a=address(field,index);if(field==W::ActiveGlobalPatch)h.write32(0x168fc,a);else if(field==W::Clock0||field==W::Clock1||field==W::LastClock)h.write16(int16_t(data),a);else h.write8(int8_t(data),a);};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationField(),task.position()):3u);if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native initialization admission mismatch");
 switch(stage){
 case S::BindReset:f.move(v[0].er,0x16782);break;
 case S::SetResetCount:f.move(v[1].e,0x28);break;
 case S::ClearResetValue:f.move(v[1].rl,0x0);break;
 case S::PublishReset:value(h,v[1].rl);break;
 case S::AdvanceReset:f.inc(v[0].er,1);break;
 case S::TickReset:f.inc(v[1].e,-1);break;
 case S::SetDefault:f.move(v[0].rl,0x5);break;
 case S::PublishDefault:value(h,v[0].rl);break;
 case S::DispatchPanelDefault:f.call(0x6ec4);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchTimer0:f.call(0x4016);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchTimer1:f.call(0x4072);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchTimer2:f.call(0x40ce);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchTimer3:f.call(0x412a);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchTimer4:f.call(0x4186);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchBanks:f.call(0x601a);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchNotes:f.call(0x36d2);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchMidi:f.call(0x2a18);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchPanelTimers:f.call(0x5666);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::SetPinRequest:f.move(v[6].rl,0x10);break;
 case S::DispatchPin:f.call(0x15b8);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::ShiftPin:f.shift(v[6].rl,1,false);break;
 case S::CapturePin:f.move(v[6].rh,v[6].rl);break;
 case S::DispatchPanelValue:f.call(0x4df2);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchPanelState:f.call(0x7500);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchGlobalBank:f.call(0x69c6);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::BindGlobal:f.move(v[0].er,0x168fc);break;
 case S::PublishGlobal:value(h,v[0].er);break;
 case S::ClearPatchControls:f.move(v[1].rl,0x0);break;
 case S::PublishPatch0:value(h,v[1].rl);break;
 case S::PublishPatch1:value(h,v[1].rl);break;
 case S::PublishPatch2:value(h,v[1].rl);break;
 case S::PublishPreviewGate:value(h,v[1].rl);break;
 case S::SetPatchBias:f.move(v[1].rl,0x40);break;
 case S::PublishPatchBias:value(h,v[1].rl);break;
 case S::ReadClock0High:v[1].rh=int8_t(task.byte());value(h,v[1].rh);break;
 case S::ReadClock0Low:v[1].rl=int8_t(task.byte());value(h,v[1].rl);break;
 case S::PublishClock0:value(h,v[1].r);break;
 case S::ReadClock1High:v[1].rh=int8_t(task.byte());value(h,v[1].rh);break;
 case S::ReadClock1Low:v[1].rl=int8_t(task.byte());value(h,v[1].rl);break;
 case S::PublishClock1:value(h,v[1].r);break;
 case S::ReadActiveMask:v[1].rl=int8_t(task.byte());value(h,v[1].rl);break;
 case S::PublishActiveMask:value(h,v[1].rl);break;
 case S::ReadReceiveMask:v[1].rl=int8_t(task.byte());value(h,v[1].rl);break;
 case S::PublishReceiveMask:value(h,v[1].rl);break;
 case S::SetSentinel:f.move(v[0].rl,0xffffffff);break;
 case S::PublishSentinel0:value(h,v[0].rl);break;
 case S::PublishSentinel1:value(h,v[0].rl);break;
 case S::SetSingleMode:f.move(v[0].rl,0x0);break;
 case S::PublishSingleMode:value(h,v[0].rl);break;
 case S::DispatchSingleMode:f.call(0x6ec6);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchSingle:f.call(0x6382);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::SetMultiMode:f.move(v[0].rl,0x1);break;
 case S::PublishMultiMode:value(h,v[0].rl);break;
 case S::DispatchMultiMode:f.call(0x6ec6);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchMulti:f.call(0x6ff4);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::RebindGlobal:f.move(v[0].er,0x168fc);break;
 case S::ReadPreferredMode:v[1].rl=int8_t(task.byte());value(h,v[1].rl);break;
 case S::PublishPreferredMode:value(h,v[1].rl);break;
 case S::SetPreferredSentinel:f.move(v[0].rl,0xffffffff);break;
 case S::PublishPreferredSentinel0:value(h,v[0].rl);break;
 case S::PublishPreferredSentinel1:value(h,v[0].rl);break;
 case S::DispatchPreferredMode:f.call(0x6ec6);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::ReadSelectedMode:v[0].rl=int8_t(task.selectedMode());value(h,v[0].rl);break;
 case S::DispatchPreferredSingle:f.call(0x6382);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchPreferredMulti:f.call(0x6ff4);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchConfiguration:f.call(0x559e);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchDisplay:f.call(0x5340);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::DispatchClock:f.call(0x12fa);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 case S::PublishClock:value(h,v[6].r);break;
 case S::DispatchHardware:f.call(0x4486);h.pc=h.makepc(f.next);if(h.cycles-before!=cost)throw std::runtime_error("Native initialization child clocks differ");return true;
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native initialization clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
