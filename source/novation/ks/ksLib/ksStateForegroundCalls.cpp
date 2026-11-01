#include "ksStateForegroundCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateForegroundPartialTest partialTest=nullptr;using T=StateForegroundTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,74> points{{{0x437a,2},{0x437c,6},{0x4382,4},{0x4386,4},{0x438a,6},{0x4390,2},{0x4392,2},{0x4394,6},{0x439a,4},{0x439e,4},{0x43a2,4},{0x43a6,2},{0x4406,4},{0x440a,2},{0x440c,2},{0x440e,6},{0x4414,2},{0x4416,2},{0x4418,2},{0x441a,6},{0x4420,2},{0x4422,2},{0x4424,2},{0x4426,4},{0x442a,2},{0x442c,2},{0x442e,2},{0x4430,2},{0x4432,2},{0x4434,4},{0x4438,2},{0x443a,4},{0x443e,2},{0x4440,4},{0x4444,2},{0x4446,2},{0x4448,6},{0x444e,2},{0x4450,4},{0x4454,2},{0x4456,4},{0x445a,6},{0x4460,4},{0x4464,6},{0x446a,2},{0x446c,2},{0x446e,2},{0x4470,4},{0x4474,4},{0x4478,4},{0x447c,2},{0x447e,2},{0x4480,4},{0x4484,2},{0x4486,4},{0x448a,4},{0x448e,4},{0x4492,2},{0x4494,4},{0x4498,2},{0x449a,6},{0x44a0,2},{0x44a2,4},{0x44a6,2},{0x44a8,6},{0x44ae,4},{0x44b2,4},{0x44b6,4},{0x44ba,4},{0x44be,2},{0x44c0,2},{0x44c2,4},{0x44c6,4},{0x44ca,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){return at<0x400000&&!(previous&1)&&at==(unsigned(previous)|1u)?0:3;}
}
void setStateForegroundPartialForTests(StateForegroundPartialTest value){partialTest=value;}
bool StateForegroundCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x437a:kind=T::Kind::Refresh;break;case 0x4406:kind=T::Kind::Controls;break;case 0x443a:kind=T::Kind::Encoder;break;case 0x4486:kind=T::Kind::Reset;break;case 0x4494:kind=T::Kind::Tick;break; default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native foreground capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[6].r),bool(h.ccr&4));const auto stage=task.stage();
 const unsigned target=stage==S::WriteControlValue?0xffeeae + task.index():0x1679c;const unsigned store=byteClocks(target,h.lastwrite);
 const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint16_t{switch(field){case T::Read::RefreshFlags:return uint8_t(h.read8(0x167d3));case T::Read::Dirty:return uint8_t(h.read8(0x1679c));case T::Read::ControlBias:return uint8_t(h.read8(0x1678d));case T::Read::ControlValue:return uint8_t(h.read8(0xffeeae + task.index()));case T::Read::Page:return uint8_t(h.read8(0x16790));case T::Read::SpecialEdit:return uint8_t(h.read8(0x1679e));case T::Read::ControlEdit:return uint8_t(h.read8(0x16792));case T::Read::LastTick:return h.read16(0x16780);}return 0;};
 auto write=[&](T::Write field,uint16_t value){if(field==T::Write::LastTick)f.store16(0x16780,int16_t(value));else f.store8(field==T::Write::Dirty?0x1679c:0xffeeae + task.index(),int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native foreground admission differs");
 switch(stage){case S::ClearRefresh:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::ReadRefreshFlags:f.move(r[1].rh,task.refresh());break;
case S::SelectRefreshWord:f.move(r[4].r,0x8056);break;
case S::SendRefreshWord:f.call(0x24c6);break;
case S::ReadDirty:f.move(r[0].rl,task.dirty());break;
case S::ClearDirty:f.move(r[0].rl,0x0);break;
case S::RefreshDirty:f.call(0x6c3c);break;
case S::RefreshParts:f.call(0x737a);break;
case S::RefreshClock:f.call(0x267a);break;
case S::ReturnRefresh:f.ret();break;
case S::PopControl:f.call(0x1f12);break;
case S::ClearControlIndex:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::CaptureControlIndex:f.move(r[1].rl,r[6].rl);break;
case S::LocateControl:r[1].er=h.add32(r[1].er,0xffeeae);break;
case S::TestControlKind:f.move(r[6].rh,r[6].rh);break;
case S::ReadControlBias:f.move(r[0].rl,task.bias());break;
case S::ApplyControlBias:r[6].rl=h.add8(r[6].rl,r[0].rl);break;
case S::DispatchControlPress:f.call(0x2b38);break;
case S::ReturnControl:f.ret();break;
case S::NormalizeControlRelease:r[6].rh&=0x7f;value(h,r[6].rh);break;
case S::ReadControlValue:f.move(r[6].rl,task.event());break;
case S::DispatchControlRelease:f.call(0x2b0e);break;
case S::CheckEncoderContext:f.call(0x590a);break;
case S::ReadEncoder:f.call(0x1886);break;
case S::TestEncoder:f.move(r[6].rl,r[6].rl);break;
case S::ReadPage:f.move(r[0].rl,task.page());break;
case S::CompareNamePage:h.sub8(r[0].rl,0x2);break;
case S::SelectNamePage:h.clockI(2);if(task.complete())f.next=0x75be;break;
case S::CompareStorePage:h.sub8(r[0].rl,0x3);break;
case S::SelectStorePage:h.clockI(2);if(task.complete())f.next=0x752c;break;
case S::ReadSpecialEdit:f.move(r[0].rl,task.special());break;
case S::SelectSpecialEdit:h.clockI(2);if(task.complete())f.next=0xb716;break;
case S::ReadControlEdit:f.move(r[0].rl,task.control());break;
case S::BindControlDelta:f.move(r[6].rh,r[6].rl);break;
case S::BindControlKind:f.move(r[6].rl,0xfffffff0);break;
case S::DispatchControlEdit:h.clockI(2);if(task.complete())f.next=0x4ea4;break;
case S::CheckParameterEdit:f.call(0x4efe);break;
case S::SelectParameterEdit:h.clockI(2);if(task.complete())f.next=0x59a6;break;
case S::BindParameterDelta:f.move(r[6].rh,r[6].rl);break;
case S::BindParameterKind:f.move(r[6].rl,0xfffffff0);break;
case S::DispatchParameterEdit:h.clockI(2);if(task.complete())f.next=0x5db2;break;
case S::ReturnEncoder:f.ret();break;
case S::ResetButtons:f.call(0x13fe);break;
case S::ResetControls:f.call(0x1578);break;
case S::ResetNotes:f.call(0x1efe);break;
case S::ReturnReset:f.ret();break;
case S::ReadTick:f.call(0x12fa);break;
case S::RetainTick:f.move(r[6].e,r[6].r);break;
case S::ReadLastTick:f.move(r[5].r,task.last());break;
case S::ElapsedTick:r[6].r=h.sub16(r[6].r,r[5].r);break;
case S::CompareTick:h.sub16(r[6].r,0x19);break;
case S::ScanButtons:f.call(0x1456);break;
case S::StartAdc:f.call(0x1624);break;
case S::ScanEncoder:f.call(0x443a);break;
case S::ScanPedal:f.call(0x13be);break;
case S::TestPedal:f.move(r[6].rh,r[6].rh);break;
case S::SendPedal:f.call(0x2a76);break;
case S::RefreshPanel:f.call(0x437a);break;
case S::ReturnTick:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native foreground clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
