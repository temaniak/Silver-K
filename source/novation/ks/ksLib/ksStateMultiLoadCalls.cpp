#include "ksStateMultiLoadCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateMultiLoadTask;using S=T::Stage;StateMultiLoadPartialTest partialTest=nullptr;struct Point{unsigned location,length;};Point point(const T& t){if(t.awaitingChild()){T copy=t;copy.resumeChild();return point(copy);}switch(t.stage()){case S::ReadSelection:return {t.selection()?0x7032u:0x7024u,8};case S::WriteSelection:return {t.selection()?0x703au:0x702cu,6};case S::ReadGlobalField:case S::WriteGlobalField:{constexpr unsigned starts[]{0x7046,0x7056,0x7066,0x7076,0x7086,0x7096};return {starts[t.globalField()]+(t.stage()==S::WriteGlobalField?8u:0u),8};}case S::ReadSavedSelection:return {t.selection()?0x711eu:0x7112u,6};case S::WriteRestoredSelection:return {t.selection()?0x7124u:0x7118u,6};case S::ResolveSource:return {0x6ff4,4};case S::DispatchLoad:return {0x6ff8,2};case S::ReturnResolved:return {0x6ffa,2};case S::MarkUnknown:return {0x6ffc,2};case S::WritePreviousMode:return {0x6ffe,6};case S::WritePreviousPart:return {0x7004,6};case S::ClearCache:return {0x700a,2};case S::WriteDirtyState:return {0x700c,6};case S::WriteGateState:return {0x7012,6};case S::WriteBankState:return {0x7018,6};case S::WriteModeState:return {0x701e,6};case S::SetGlobalBase:return {0x7040,6};case S::SetProgramTable:return {0x70a6,6};case S::SetPartMask:return {0x70ac,2};case S::SetPartCount:return {0x70ae,2};case S::ReadPartBank:return {0x70b0,8};case S::ReadPartProgram:return {0x70b8,8};case S::WriteProgramNumber:return {0x70c0,2};case S::RetainSource:return {0x70c2,4};case S::RetainProgramTable:return {0x70c6,4};case S::RetainPart:return {0x70ca,2};case S::WriteActiveMask:return {0x70cc,6};case S::BindMode:return {0x70d2,4};case S::LoadPart:return {0x70d6,2};case S::RestorePart:return {0x70d8,2};case S::RestoreProgramTable:return {0x70da,4};case S::RestoreSource:return {0x70de,4};case S::NextSource:return {0x70e2,6};case S::NextProgramTable:return {0x70e8,2};case S::DoublePartMask:return {0x70ea,2};case S::DecrementPartCount:return {0x70ec,2};case S::AdmitPart:return {0x70ee,2};case S::SetVoiceGlobal:return {0x70f0,6};case S::SetVoicePart:return {0x70f6,6};case S::ReadVoiceMode:return {0x70fc,8};case S::WriteVoiceMode:return {0x7104,8};case S::CopyVoiceMode:return {0x710c,2};case S::RefreshVoiceMode:return {0x710e,4};case S::RestoreMode:return {0x712a,4};case S::RefreshPanel:return {0x712e,4};case S::ReturnLoaded:return {0x7132,2}; default:return {0,0};}}}
void setStateMultiLoadPartialForTests(StateMultiLoadPartialTest f){partialTest=f;}
bool StateMultiLoadCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x6ff4||pc==0x6ffc)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native multiload capacity exceeded");selected->emplace(pc==0x6ff4?T::Entry::Selected:T::Entry::Provided,h.getSP(),uint32_t(h.regs[0].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild()){if(task.child()==T::Child::Source)call.source=uint32_t(r[0].er);task.resumeChild();}const auto stage=task.stage();uint32_t writeAddress=stage==S::WriteSelection?0x167b8+task.selection():stage==S::WriteGlobalField?0x168fc+task.targetGlobalOffset():stage==S::WriteRestoredSelection?0x167d0+task.selection():stage==S::WriteVoiceMode?0x168fc+227:stage==S::WriteActiveMask?0x167d0:stage==S::WritePreviousMode?0x167bb:stage==S::WritePreviousPart?0x167d4:stage==S::WriteDirtyState?0x16783:stage==S::WriteGateState?0x167a3:stage==S::WriteBankState?0x1679e :0x167a0;const unsigned storeClocks=(h.lastwrite&1)||int(writeAddress)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{uint32_t address=what==T::Read::Selection?call.source+189+task.selection():what==T::Read::GlobalField?call.source+task.sourceGlobalOffset():what==T::Read::PartBank?call.source+task.part()*256+187:what==T::Read::PartProgram?call.source+task.part()*256+188:what==T::Read::VoiceMode?0x169fc+227:0x167b8+task.selection();return observed=uint8_t(h.read8(address));};auto write=[&](T::Write what,uint32_t value){if(what==T::Write::ProgramNumber){f.store16(0x167b0+task.part()*2,value);return;}f.store8(writeAddress,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native multiload admission differs");switch(pc){case 0x6ff4:{f.call(0x7190);break;}
case 0x6ff8:{f.call(0x6ffc);break;}
case 0x6ffa:{f.ret();break;}
case 0x6ffc:{f.move(r[1].rl,0xffffffff);break;}
case 0x6ffe:{;break;}
case 0x7004:{;break;}
case 0x700a:{f.move(r[1].rl,0x0);break;}
case 0x700c:{;break;}
case 0x7012:{;break;}
case 0x7018:{;break;}
case 0x701e:{;break;}
case 0x7024:{f.move(r[1].rl,observed);break;}
case 0x702c:{;break;}
case 0x7032:{f.move(r[1].rl,observed);break;}
case 0x703a:{;break;}
case 0x7040:{f.move(r[1].er,0x168fc);break;}
case 0x7046:{f.move(r[2].rl,observed);break;}
case 0x704e:{;break;}
case 0x7056:{f.move(r[2].rl,observed);break;}
case 0x705e:{;break;}
case 0x7066:{f.move(r[2].rl,observed);break;}
case 0x706e:{;break;}
case 0x7076:{f.move(r[2].rl,observed);break;}
case 0x707e:{;break;}
case 0x7086:{f.move(r[2].rl,observed);break;}
case 0x708e:{;break;}
case 0x7096:{f.move(r[2].rl,observed);break;}
case 0x709e:{;break;}
case 0x70a6:{f.move(r[1].er,0x167b0);break;}
case 0x70ac:{f.move(r[2].rl,0x1);break;}
case 0x70ae:{f.move(r[2].rh,0x4);break;}
case 0x70b0:{f.move(r[3].rh,observed);break;}
case 0x70b8:{f.move(r[3].rl,observed);break;}
case 0x70c0:{;break;}
case 0x70c2:{f.push(r[0].er);break;}
case 0x70c6:{f.push(r[1].er);break;}
case 0x70ca:{f.push(r[2].r);break;}
case 0x70cc:{;break;}
case 0x70d2:{f.call(0x6ec6);break;}
case 0x70d6:{f.call(0x714c);break;}
case 0x70d8:{f.pop(r[2].r);break;}
case 0x70da:{f.pop(r[1].er);break;}
case 0x70de:{f.pop(r[0].er);break;}
case 0x70e2:{r[0].er=h.add32(r[0].er,0x100);break;}
case 0x70e8:{r[1].er=int32_t(uint32_t(r[1].er)+0x2);break;}
case 0x70ea:{f.shift(r[2].rl,1,true);break;}
case 0x70ec:{f.inc(r[2].rh,-1);break;}
case 0x70ee:{;break;}
case 0x70f0:{f.move(r[0].er,0x168fc);break;}
case 0x70f6:{f.move(r[1].er,0x169fc);break;}
case 0x70fc:{f.move(r[2].rl,observed);break;}
case 0x7104:{;break;}
case 0x710c:{f.move(r[6].rh,r[2].rl);break;}
case 0x710e:{f.call(0x4c1e);break;}
case 0x7112:{f.move(r[0].rl,observed);break;}
case 0x7118:{;break;}
case 0x711e:{f.move(r[0].rl,observed);break;}
case 0x7124:{;break;}
case 0x712a:{f.call(0x6ec6);break;}
case 0x712e:{f.call(0x6c3c);break;}
case 0x7132:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native multiload clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
