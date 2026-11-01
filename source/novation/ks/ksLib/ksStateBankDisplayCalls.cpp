#include "ksStateBankDisplayCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateBankDisplayTask;using S=T::Stage;StateBankDisplayPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,74> points{{{0x7354,6},{0x735a,6},{0x7360,4},{0x73e2,6},{0x73e8,4},{0x73ec,6},{0x73f2,6},{0x73f8,2},{0x73fa,6},{0x7400,2},{0x7402,2},{0x7404,6},{0x740a,4},{0x740e,6},{0x7414,4},{0x7418,8},{0x7420,2},{0x7422,4},{0x7426,6},{0x742c,2},{0x742e,2},{0x7430,2},{0x7432,6},{0x7438,4},{0x743c,2},{0x743e,4},{0x7442,2},{0x7444,4},{0x7448,6},{0x744e,4},{0x7452,2},{0x7454,4},{0x7458,6},{0x745e,2},{0x7460,4},{0x7464,4},{0x7468,4},{0x746c,2},{0x746e,2},{0x7470,2},{0x7472,2},{0x7474,2},{0x7476,6},{0x747c,2},{0x747e,4},{0x7482,2},{0x7484,4},{0x7488,4},{0x748c,2},{0x748e,2},{0x7490,2},{0x7492,2},{0x7494,2},{0x7496,2},{0x7498,2},{0x749a,2},{0x749c,2},{0x749e,4},{0x74a2,6},{0x74a8,4},{0x74ac,2},{0x74ae,2},{0x74b0,2},{0x74b2,4},{0x74b6,2},{0x74b8,8},{0x74c0,4},{0x74c4,2},{0x74c6,4},{0x74ca,2},{0x74cc,2},{0x74ce,2},{0x74d0,2},{0x74d2,2}}};Point point(const T& t){if(t.awaitingTail())return{t.tail()==T::Tail::Refresh?0x9662u:t.tail()==T::Tail::String?0x9b8u:0x976u,0};return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateBankDisplayPartialForTests(StateBankDisplayPartialTest f){partialTest=f;}
bool StateBankDisplayCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingTail()&&point(c->task).location==h.getPC()){c->task.retireTail();c.reset();++completed;retired=true;}return retired;}
bool StateBankDisplayCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x7354||pc==0x73e2){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native bank-display capacity exceeded");selected->emplace(pc==0x7354?T::Entry::Select:T::Entry::Display,h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){if(task.child()==T::Child::Table)call.table=uint32_t(r[0].er);task.resumeChild();}const auto stage=task.stage();const unsigned storeClocks=(h.lastwrite&1)||0x17526!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::SelectedPart:return observed=uint8_t(h.read8(0x16798));
case T::Read::MessageMode:return observed=uint8_t(h.read8(0x17492));
case T::Read::HeaderKind:return observed=uint8_t(h.read8(0x1679f));
case T::Read::Program:return observed=uint8_t(h.read8(0x1679e));
case T::Read::SelectedAddress:return observed=uint32_t(h.read32(0x1748e));
case T::Read::LateKind:return observed=uint8_t(h.read8(0x1679f));
case T::Read::NameCursor:return observed=uint8_t(h.read8(0x167d7));
case T::Read::RowProgram:return observed=uint8_t(h.read8(call.table+task.row()*2));
case T::Read::RowBank:return observed=uint8_t(h.read8(call.table+task.row()*2+1));
case T::Read::RowLookup:return observed=uint8_t(h.read8(0x10d6e + task.rowProgram()));}return 0;};auto write=[&](uint8_t value){f.store8(0x17526,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native bank-display admission differs");switch(stage){case S::ReadSelectedPart:{f.move(r[0].rl,observed);break;}
case S::WriteSelectedPart:{;break;}
case S::TailSelectedPart:{h.clockI(2);break;}
case S::ReadMessageMode:{f.move(r[0].rl,observed);break;}
case S::AdmitMessageMode:{h.clockI(2);break;}
case S::SetMainHeader:{f.move(r[6].er,0x15064);break;}
case S::ReadHeaderKind:{f.move(r[0].rl,observed);break;}
case S::AdmitHeaderKind:{;break;}
case S::SetPartHeader:{f.move(r[6].er,0x150d4);break;}
case S::DecrementHeaderKind:{f.inc(r[0].rl,-1);break;}
case S::AdmitPartHeader:{;break;}
case S::SetOtherHeader:{f.move(r[6].er,0x150e8);break;}
case S::PrintHeader:{f.call(0x9b8);break;}
case S::ReadProgram:{f.move(r[0].rl,observed);break;}
case S::PrintProgram:{f.call(0x9dc);break;}
case S::ReadSelectedAddress:{f.move(r[0].er,observed);break;}
case S::SetNameCursor:{f.move(r[6].rl,0xffffffc0);break;}
case S::SendNameCursor:{f.call(0x976);break;}
case S::ReadLateKind:{f.move(r[1].rl,observed);break;}
case S::AdmitLateKind:{;break;}
case S::ComparePartName:{h.sub8(r[1].rl,0x1);break;}
case S::AdmitPartName:{;break;}
case S::SetPartName:{f.move(r[6].er,0x1733c);break;}
case S::TailPartName:{h.clockI(2);break;}
case S::SetPrefix:{f.move(r[6].rl,0xfffffffc);break;}
case S::SendPrefix:{f.call(0x976);break;}
case S::SetQuoteBefore:{f.move(r[6].rl,0x27);break;}
case S::SendQuoteBefore:{f.call(0x976);break;}
case S::SetNameSource:{f.move(r[6].er,0x1733c);break;}
case S::PrintName:{f.call(0x9b8);break;}
case S::SetQuoteAfter:{f.move(r[6].rl,0x27);break;}
case S::SendQuoteAfter:{f.call(0x976);break;}
case S::ReadNameCursor:{f.move(r[6].rl,observed);break;}
case S::AddNameCursor:{r[6].rl=h.add8(r[6].rl,0xffffffc1);break;}
case S::TailNameCursor:{h.clockI(2);break;}
case S::ReadPatchName:{f.call(0x6e5e);break;}
case S::PrintPatchName:{f.call(0x9b8);break;}
case S::ReturnName:{f.ret();break;}
case S::DecrementMessageMode:{f.inc(r[0].rl,-1);break;}
case S::SetLabelStride:{f.move(r[0].rh,0x16);break;}
case S::ScaleLabel:{r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;}
case S::ExtendLabel:{r[0].er&=0xffff;value(h,r[0].er);break;}
case S::SetLabelBase:{f.move(r[6].er,0x15078);break;}
case S::AddLabelBase:{r[6].er=h.add32(r[6].er,r[0].er);break;}
case S::PrintLabel:{f.call(0x9b8);break;}
case S::SetLabelPrefix:{f.move(r[6].rl,0xfffffffc);break;}
case S::SendLabelPrefix:{f.call(0x976);break;}
case S::ResolveTable:{f.call(0xb6c0);break;}
case S::CaptureTable:{f.move(r[2].er,r[0].er);break;}
case S::SetRowCount:{f.move(r[6].rh,0x4);break;}
case S::SetRowCursor:{f.move(r[6].rl,0xffffffc1);break;}
case S::BindTableNames:{f.call(0x74d4);break;}
case S::ClearRowProgram:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::ReadRowProgram:{f.move(r[1].rl,observed);r[2].er+=1;h.clockI(2);break;}
case S::ReadRowBank:{f.move(r[0].rl,observed);r[2].er+=1;h.clockI(2);break;}
case S::AdmitRowBank:{;break;}
case S::RetainEmptyCursor:{f.push(r[6].r);break;}
case S::SendEmptyCursor:{f.call(0x976);break;}
case S::SetEmptyString:{f.move(r[6].er,0x150d0);break;}
case S::PrintEmptyString:{f.call(0x9b8);break;}
case S::RestoreEmptyCursor:{f.pop(r[6].r);break;}
case S::SkipRowFormat:{;break;}
case S::DecrementRowBank:{f.inc(r[0].rl,-1);break;}
case S::RetainTable:{f.push(r[2].er);break;}
case S::RetainRowCursor:{f.push(r[6].r);break;}
case S::ReadRowLookup:{f.move(r[1].rl,observed);break;}
case S::FormatRow:{f.call(0x5ae2);break;}
case S::RestoreRowCursor:{f.pop(r[6].r);break;}
case S::RestoreTable:{f.pop(r[2].er);break;}
case S::AdvanceRowCursor:{r[6].rl=h.add8(r[6].rl,0x5);break;}
case S::DecrementRowCount:{f.inc(r[6].rh,-1);break;}
case S::AdmitRows:{;break;}
case S::RestoreNameTables:{f.call(0x7500);break;}
case S::ReturnRows:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native bank-display clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
