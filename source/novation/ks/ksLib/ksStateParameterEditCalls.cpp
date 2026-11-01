#include "ksStateParameterEditCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateParameterEditTask;using S=T::Stage;StateParameterEditPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,48> points{{{0x6cfe,2},{0x6d00,2},{0x6d02,2},{0x6d04,4},{0x6d08,2},{0x6d0a,2},{0x6d0c,2},{0x6d0e,2},{0x6d10,2},{0x6d12,2},{0x6d14,2},{0x6d16,8},{0x6d1e,8},{0x6d26,2},{0x6d28,2},{0x6d2a,8},{0x6d32,8},{0x6d3a,2},{0x6d3c,6},{0x6d42,2},{0x6d44,2},{0x6d46,4},{0x6d4a,4},{0x6d4e,4},{0x6d52,2},{0x6d54,8},{0x6d5c,2},{0x6d5e,2},{0x6d60,8},{0x6d68,2},{0x6d6a,2},{0x6d6c,2},{0x6d6e,4},{0x6d72,4},{0x6d76,4},{0x6d7a,2},{0x6d7c,6},{0x6d82,2},{0x6d84,6},{0x6d8a,2},{0x6d8c,2},{0x6d8e,2},{0x6d90,4},{0x6d94,2},{0x6d96,4},{0x6d9a,2},{0x6d9c,6},{0x6da2,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateParameterEditPartialForTests(StateParameterEditPartialTest f){partialTest=f;}
bool StateParameterEditCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if((pc==0x6cfe||pc==0x6d10)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native parameter-edit capacity exceeded");selected->emplace(pc==0x6cfe?T::Entry::Public:T::Entry::Internal,uint8_t(h.regs[6].rl),uint8_t(h.regs[6].rh),uint32_t(h.regs[1].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){const auto child=task.child();const auto outcome=(h.ccr&4)?StateFieldMapTask::Outcome::Unmapped:(h.ccr&1)?StateFieldMapTask::Outcome::AboveLimit:StateFieldMapTask::Outcome::Packed;task.resumeChild(outcome,uint32_t(r[1].er),uint8_t(r[6].rh),!(h.ccr&4));}
 const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Field:return observed=uint8_t(h.read8(71842+task.parameter()));case T::Read::Type:return observed=uint8_t(h.read8(int32_t(71586+task.field())));case T::Read::Inhibit:return observed=uint8_t(h.read8(0x15c23));case T::Read::CurrentBase:return observed=call.current=uint32_t(h.read32(0x167bc));case T::Read::LiveBase:return observed=call.live=uint32_t(h.read32(0x167c0));case T::Read::NotificationMode:return observed=uint8_t(h.read8(0x16792));case T::Read::NotificationField:return observed=uint8_t(h.read8(0x16794));}return 0;};
 auto write=[&](T::Write what,uint8_t value){uint32_t address=0;switch(what){case T::Write::Global:address=92412+task.field();break;case T::Write::GlobalMirror:address=93714+task.field();break;case T::Write::Current:address=call.current+task.field();break;case T::Write::Live:address=call.live+task.field();break;case T::Write::Edited:address=0x1679c;break;}f.store8(int32_t(address),value);};
 if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native parameter-edit admission differs");switch(stage){case S::ComparePublic:{h.sub8(r[6].rl,0x7f);break;}
case S::AdmitPublic:{;break;}
case S::ReturnPublic:{f.ret();break;}
case S::MapControl:{f.call(0x6da4);break;}
case S::AdmitMapping:{;break;}
case S::AdmitMapped:{;break;}
case S::ReturnRejected:{f.ret();break;}
case S::BankParameter:{r[6].rl=h.add8(r[6].rl,0xffffff80);break;}
case S::ClearParameter:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::CaptureParameter:{f.move(r[0].rl,r[6].rl);break;}
case S::ClearField:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::ReadField:{f.move(r[1].rl,observed);break;}
case S::ReadType:{f.move(r[0].rl,observed);break;}
case S::CompareType:{h.sub8(r[0].rl,0x7e);break;}
case S::AdmitType:{;break;}
case S::WriteGlobal:{;break;}
case S::WriteGlobalMirror:{;break;}
case S::SkipToNotification:{;break;}
case S::ReadInhibit:{f.move(r[0].rl,observed);break;}
case S::AdmitInhibit:{;break;}
case S::RetainRequest:{f.push(r[6].r);break;}
case S::RetainField:{f.push(r[1].er);break;}
case S::BindCurrent:{f.call(0x629e);break;}
case S::RestoreField:{f.pop(r[1].er);break;}
case S::RestoreRequest:{f.pop(r[6].r);break;}
case S::ReadCurrentBase:{f.move(r[0].er,observed);break;}
case S::AddCurrentField:{r[0].er=h.add32(r[0].er,r[1].er);break;}
case S::WriteCurrent:{;break;}
case S::ReadLiveBase:{f.move(r[0].er,observed);break;}
case S::AddLiveField:{r[0].er=h.add32(r[0].er,r[1].er);break;}
case S::WriteLive:{;break;}
case S::RetainNotifyField:{f.push(r[1].r);break;}
case S::ExpandFields:{f.call(0x6726);break;}
case S::RebindMode:{f.call(0x6ec6);break;}
case S::MarkDirty:{f.call(0x7364);break;}
case S::RestoreNotifyField:{f.pop(r[1].r);break;}
case S::ReadNotificationMode:{f.move(r[0].rl,observed);break;}
case S::AdmitNotificationMode:{;break;}
case S::ReadNotificationField:{f.move(r[0].rl,observed);break;}
case S::CompareNotificationField:{h.sub8(r[1].rl,r[0].rl);break;}
case S::AdmitNotificationField:{;break;}
case S::SkipDisplay:{;break;}
case S::ProbeDisplay:{f.call(0x4efe);break;}
case S::AdmitDisplay:{;break;}
case S::RefreshDisplay:{f.call(0x5340);break;}
case S::MarkEdited:{f.move(r[0].rl,0x1);break;}
case S::WriteEdited:{;break;}
case S::ReturnEdited:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native parameter-edit clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}
}
