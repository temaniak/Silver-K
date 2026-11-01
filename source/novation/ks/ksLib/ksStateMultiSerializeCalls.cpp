#include "ksStateMultiSerializeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateMultiSerializeTask;using S=T::Stage;StateMultiSerializePartialTest partialTest=nullptr;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::ReadGlobalField:case S::WriteGlobalField:{constexpr unsigned starts[]{0x71d2,0x71e2,0x71f2,0x7202,0x7212,0x7222,0x7232};return {starts[t.globalField()]+(t.stage()==S::WriteGlobalField?8u:0u),8};}case S::ReadSelection:return {t.selection()?0x7250u:0x7242u,6};case S::WriteSelection:return {t.selection()?0x7256u:0x7248u,8};case S::AwaitPart:return {0x727e,6};case S::AwaitPacking:return {0x72ba,2};case S::ReadOriginal:return {0x71ba,8};case S::RetainOriginal:return {0x71c2,4};case S::SetMulti:return {0x71c6,6};case S::SetGlobal:return {0x71cc,6};case S::SetProgramTable:return {0x725e,6};case S::SetPart:return {0x7264,2};case S::SetPartCount:return {0x7266,2};case S::RetainMulti:return {0x7268,4};case S::RetainProgram:return {0x726c,4};case S::RetainPart:return {0x7270,2};case S::RetainNameTarget:return {0x7272,4};case S::SnapshotNameTarget:return {0x7276,2};case S::SelectPart:return {0x7278,2};case S::DispatchPart:return {0x727a,4};case S::AddNameSource:return {0x727e,6};case S::AddNameTarget:return {0x7284,6};case S::SetNameCount:return {0x728a,4};case S::ReadName:return {0x728e,2};case S::WriteName:return {0x7290,2};case S::NextNameTarget:return {0x7292,2};case S::DecrementName:return {0x7294,2};case S::AdmitName:return {0x7296,2};case S::RestoreNameTarget:return {0x7298,4};case S::ReadProgram:return {0x729c,2};case S::WriteBank:return {0x729e,8};case S::WriteProgram:return {0x72a6,8};case S::SelectCurrent:return {0x72ae,8};case S::DispatchPacking:return {0x72b6,4};case S::RestorePart:return {0x72ba,2};case S::RestoreProgram:return {0x72bc,4};case S::RestoreMulti:return {0x72c0,4};case S::NextMulti:return {0x72c4,6};case S::NextProgram:return {0x72ca,2};case S::NextPart:return {0x72cc,2};case S::DecrementPartCount:return {0x72ce,2};case S::AdmitPart:return {0x72d0,2};case S::RestoreOriginal:return {0x72d2,4};case S::SelectOriginal:return {0x72d6,8};case S::ReturnMulti:return {0x72de,2}; default:return {0,0};}}}
void setStateMultiSerializePartialForTests(StateMultiSerializePartialTest f){partialTest=f;}
bool StateMultiSerializeCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x71ba){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native multiset capacity exceeded");selected->emplace(h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){if(task.child()==T::Child::PartReference)call.partSource=uint32_t(r[0].er);task.resumeChild();}const auto stage=task.stage();uint32_t address=stage==S::WriteGlobalField?0x169fc+task.targetGlobalOffset():stage==S::WriteSelection?0x169fc+189+task.selection():stage==S::WriteName?0x169fc+task.part()*256+179+task.nameOffset():0x169fc+task.part()*256+(stage==S::WriteBank?187:188);const unsigned physical=address&0xffffff;const unsigned storeClocks=physical<0x400000?((h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{switch(what){case T::Read::Original:return observed=call.original=uint32_t(h.read32(0x167bc));case T::Read::GlobalField:return observed=uint8_t(h.read8(0x168fc+task.sourceGlobalOffset()));case T::Read::Selection:return observed=uint8_t(h.read8(task.selection()?0x167d3:0x167d2));case T::Read::Name:return observed=uint8_t(h.read8(call.partSource+9+task.nameOffset()));case T::Read::Program:return observed=uint16_t(h.read16(0x167b0+task.part()*2));}return 0;};
 auto write=[&](T::Write what,uint32_t value){if(what==T::Write::SelectCurrent||what==T::Write::SelectOriginal){f.store32(0x167bc,what==T::Write::SelectCurrent?0x169fc+task.part()*256:call.original);return;}f.store8(address,uint8_t(value));};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native multiset admission differs");switch(pc){case 0x71ba:{f.move(r[0].er,observed);break;}
case 0x71c2:{f.push(r[0].er);break;}
case 0x71c6:{f.move(r[0].er,0x169fc);break;}
case 0x71cc:{f.move(r[1].er,0x168fc);break;}
case 0x71d2:{f.move(r[2].rl,observed);break;}
case 0x71da:{;break;}
case 0x71e2:{f.move(r[2].rl,observed);break;}
case 0x71ea:{;break;}
case 0x71f2:{f.move(r[2].rl,observed);break;}
case 0x71fa:{;break;}
case 0x7202:{f.move(r[2].rl,observed);break;}
case 0x720a:{;break;}
case 0x7212:{f.move(r[2].rl,observed);break;}
case 0x721a:{;break;}
case 0x7222:{f.move(r[2].rl,observed);break;}
case 0x722a:{;break;}
case 0x7232:{f.move(r[2].rl,observed);break;}
case 0x723a:{;break;}
case 0x7242:{f.move(r[1].rl,observed);break;}
case 0x7248:{;break;}
case 0x7250:{f.move(r[1].rl,observed);break;}
case 0x7256:{;break;}
case 0x725e:{f.move(r[1].er,0x167b0);break;}
case 0x7264:{f.move(r[2].rl,0x1);break;}
case 0x7266:{f.move(r[2].rh,0x4);break;}
case 0x7268:{f.push(r[0].er);break;}
case 0x726c:{f.push(r[1].er);break;}
case 0x7270:{f.push(r[2].r);break;}
case 0x7272:{f.push(r[0].er);break;}
case 0x7276:{f.move(r[3].er,r[0].er);break;}
case 0x7278:{f.move(r[0].rl,r[2].rl);break;}
case 0x727a:{f.call(0xa670);break;}
case 0x727e:{r[0].er=h.add32(r[0].er,0x9);break;}
case 0x7284:{r[3].er=h.add32(r[3].er,0xb3);break;}
case 0x728a:{f.move(r[2].e,0x8);break;}
case 0x728e:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case 0x7290:{;break;}
case 0x7292:{f.inc(r[3].er,1);break;}
case 0x7294:{f.inc(r[2].e,-1);break;}
case 0x7296:{;break;}
case 0x7298:{f.pop(r[0].er);break;}
case 0x729c:{f.move(r[3].r,observed);break;}
case 0x729e:{;break;}
case 0x72a6:{;break;}
case 0x72ae:{;break;}
case 0x72b6:{f.call(0x687e);break;}
case 0x72ba:{f.pop(r[2].r);break;}
case 0x72bc:{f.pop(r[1].er);break;}
case 0x72c0:{f.pop(r[0].er);break;}
case 0x72c4:{r[0].er=h.add32(r[0].er,0x100);break;}
case 0x72ca:{r[1].er=int32_t(uint32_t(r[1].er)+0x2);break;}
case 0x72cc:{f.inc(r[2].rl,1);break;}
case 0x72ce:{f.inc(r[2].rh,-1);break;}
case 0x72d0:{;break;}
case 0x72d2:{f.pop(r[0].er);call.original=uint32_t(r[0].er);break;}
case 0x72d6:{;break;}
case 0x72de:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native multiset clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
