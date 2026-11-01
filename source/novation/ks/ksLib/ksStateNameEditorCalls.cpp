#include "ksStateNameEditorCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateNameEditorTask;using S=T::Stage;StateNameEditorPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,70> points{{{0x752c,6},{0x7532,2},{0x7534,2},{0x7536,2},{0x7538,2},{0x753a,2},{0x753c,2},{0x753e,6},{0x7544,4},{0x7548,2},{0x754a,6},{0x7550,2},{0x7552,2},{0x7554,6},{0x755a,2},{0x755c,2},{0x755e,2},{0x7560,2},{0x7562,2},{0x7564,2},{0x7566,4},{0x756a,2},{0x756c,6},{0x7572,4},{0x7576,4},{0x757a,2},{0x757c,6},{0x7582,2},{0x7584,2},{0x7586,6},{0x758c,2},{0x758e,2},{0x7590,4},{0x7594,2},{0x7596,2},{0x7598,2},{0x759a,4},{0x759e,2},{0x75a0,2},{0x75a2,2},{0x75a4,2},{0x75a6,2},{0x75a8,6},{0x75ae,6},{0x75b4,2},{0x75b6,2},{0x75b8,2},{0x75ba,4},{0x75be,2},{0x75c0,2},{0x75c2,2},{0x75c4,6},{0x75ca,2},{0x75cc,2},{0x75ce,2},{0x75d0,2},{0x75d2,2},{0x75d4,2},{0x75d6,2},{0x75d8,2},{0x75da,6},{0x75e0,2},{0x75e2,2},{0x75e4,4},{0x75fa,2},{0x75fc,2},{0x75fe,6},{0x7604,2},{0x7606,6},{0x760c,2}}};Point point(const T& t){if(t.awaitingTail())return {t.tail()==T::Tail::Display?0x5340u:0x75e0u,0};return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateNameEditorPartialForTests(StateNameEditorPartialTest f){partialTest=f;}
bool StateNameEditorCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingTail()&&point(c->task).location==h.getPC()){c->task.retireTail();c.reset();++completed;retired=true;}return retired;}
bool StateNameEditorCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x752c||pc==0x7548||pc==0x756a||pc==0x757c||pc==0x75a0||pc==0x75a4||pc==0x75a6||pc==0x75be||pc==0x75e0||pc==0x75fa)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native name-editor capacity exceeded");T::Kind kind;switch(pc){case 0x752c:kind=T::Kind::Category;break;case 0x7548:kind=T::Kind::ClearSuffix;break;case 0x756a:kind=T::Kind::Begin;break;case 0x757c:kind=T::Kind::Move;break;case 0x75a0:kind=T::Kind::InsertSpace;break;case 0x75a4:kind=T::Kind::InsertDigit;break;case 0x75a6:kind=T::Kind::InsertRaw;break;case 0x75be:kind=T::Kind::Adjust;break;case 0x75e0:kind=T::Kind::Commit;break;case 0x75fa:kind=T::Kind::Toggle;break; default:kind=T::Kind::Commit;break;}selected->emplace(kind,uint8_t(h.regs[6].rl),h.getSP(),uint32_t(h.regs[0].er));++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){if(task.child()==T::Child::Read)call.name=uint32_t(r[0].er);task.resumeChild(uint8_t(r[1].rl));}const auto stage=task.stage();unsigned address=0;switch(stage){case S::WriteCategory:address=0x167da;break;case S::WriteSpace:address=0x1733c+task.cursor()+task.suffixOffset();break;case S::WriteBegin:address=0x167d7;break;case S::WriteMoveCursor:address=0x167d7;break;case S::WriteInsert:address=0x1733c+task.cursor();break;case S::WriteIndex:address=0x167d9;break;case S::WriteName:address=call.name;break;case S::WriteCase:address=0x167d8;break; default:break;}address&=0xffffff;const unsigned storeClocks=address<0x400000?((h.lastwrite&1)||int(address)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Category:return observed=uint8_t(h.read8(0x167da));
case T::Read::SuffixCursor:return observed=uint8_t(h.read8(0x167d7));
case T::Read::MoveCursor:return observed=uint8_t(h.read8(0x167d7));
case T::Read::InsertCursor:return observed=uint8_t(h.read8(0x167d7));
case T::Read::PreviousIndex:return observed=uint8_t(h.read8(0x167d9));
case T::Read::Case:return observed=uint8_t(h.read8(0x167d8));}return 0;};auto write=[&](T::Write what,uint8_t value){switch(what){case T::Write::Category:f.store8(0x167da,value);break;
case T::Write::Suffix:f.store8(0x1733c+task.cursor()+task.suffixOffset(),value);break;
case T::Write::Cursor:f.store8(0x167d7,value);break;
case T::Write::Insert:f.store8(0x1733c+task.cursor(),value);break;
case T::Write::Index:f.store8(0x167d9,value);break;
case T::Write::Name:f.store8(call.name,value);break;
case T::Write::Case:f.store8(0x167d8,value);break;}};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native name-editor admission differs");switch(stage){case S::ReadCategory:{f.move(r[0].rl,observed);break;}
case S::AddCategory:{r[0].rl=h.add8(r[0].rl,r[6].rl);break;}
case S::AdmitCategorySign:{;break;}
case S::ClearCategory:{f.move(r[0].rl,0x0);break;}
case S::CompareCategory:{h.sub8(r[0].rl,0x18);break;}
case S::AdmitCategoryLimit:{;break;}
case S::SetLastCategory:{f.move(r[0].rl,0x17);break;}
case S::WriteCategory:{;break;}
case S::TailCategory:{h.clockI(2);break;}
case S::ClearSuffixBase:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::ReadSuffixCursor:{f.move(r[0].rl,observed);break;}
case S::SetSuffixLength:{f.move(r[1].rl,0x10);break;}
case S::SubtractSuffix:{r[1].rl=h.sub8(r[1].rl,r[0].rl);break;}
case S::AddSuffixBase:{r[0].er=h.add32(r[0].er,0x1733c);break;}
case S::ExtendSuffix:{r[1].r&=0xff;value(h,r[1].r);break;}
case S::SetSpaceByte:{f.move(r[2].rl,0x20);break;}
case S::WriteSpace:{;break;}
case S::AdvanceSpace:{f.inc(r[0].er,1);break;}
case S::DecrementSpace:{f.inc(r[1].r,-1);break;}
case S::AdmitSpace:{;break;}
case S::TailSuffix:{h.clockI(2);break;}
case S::ClearBegin:{f.move(r[0].rl,0x0);break;}
case S::WriteBegin:{;break;}
case S::ReadBegin:{f.call(0x75e8);break;}
case S::DecodeBegin:{f.call(0x760e);break;}
case S::ReturnBegin:{f.ret();break;}
case S::ReadMoveCursor:{f.move(r[0].rl,observed);break;}
case S::AddMoveCursor:{r[0].rl=h.add8(r[0].rl,r[6].rl);break;}
case S::MaskMoveCursor:{r[0].rl&=0xf;value(h,r[0].rl);break;}
case S::WriteMoveCursor:{;break;}
case S::SetCursorCommand:{f.move(r[6].rl,0xffffffc1);break;}
case S::AddCursorCommand:{r[6].rl=h.add8(r[6].rl,r[0].rl);break;}
case S::SendCursor:{f.call(0x976);break;}
case S::ReadMoved:{f.call(0x75e8);break;}
case S::CompareMoved:{h.sub8(r[1].rl,0x20);break;}
case S::AdmitMoved:{;break;}
case S::DecodeMoved:{f.call(0x760e);break;}
case S::ReturnMove:{f.ret();break;}
case S::SetSpaceInsert:{f.move(r[6].rl,0x20);break;}
case S::SkipSpaceInsert:{;break;}
case S::AddDigitInsert:{r[6].rl=h.add8(r[6].rl,0x30);break;}
case S::ClearInsertBase:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::ReadInsertCursor:{f.move(r[0].rl,observed);break;}
case S::AddInsertBase:{r[0].er=h.add32(r[0].er,0x1733c);break;}
case S::WriteInsert:{;break;}
case S::SetMoveNext:{f.move(r[6].rl,0x1);break;}
case S::MoveNext:{f.call(0x757c);break;}
case S::TailInsert:{h.clockI(2);break;}
case S::ReadAdjusted:{f.call(0x75e8);break;}
case S::CompareAdjusted:{h.sub8(r[1].rl,0x20);break;}
case S::AdmitAdjusted:{;break;}
case S::ReadPreviousIndex:{f.move(r[1].rl,observed);break;}
case S::SkipPreviousIndex:{;break;}
case S::DecodeAdjusted:{f.call(0x760e);break;}
case S::AddIndex:{r[1].rl=h.add8(r[1].rl,r[6].rl);break;}
case S::AdmitIndexSign:{;break;}
case S::ClearIndex:{f.move(r[1].rl,0x0);break;}
case S::CompareIndex:{h.sub8(r[1].rl,0x3a);break;}
case S::AdmitIndexLimit:{;break;}
case S::SetLastIndex:{f.move(r[1].rl,0x39);break;}
case S::WriteIndex:{;break;}
case S::EncodeIndex:{f.call(0x7644);break;}
case S::WriteName:{;break;}
case S::TailName:{h.clockI(2);break;}
case S::ReadToggle:{f.call(0x75e8);break;}
case S::DecodeToggle:{f.call(0x760e);break;}
case S::ReadCase:{f.move(r[2].rl,observed);break;}
case S::ToggleCase:{r[2].rl^=0x1;value(h,r[2].rl);break;}
case S::WriteCase:{;break;}
case S::TailCommit:{;break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native name-editor clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
