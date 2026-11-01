#include "ksStateDspFactoryResetCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateDspFactoryResetTask;using S=T::Stage;StateDspFactoryResetPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,16> points{{{0x7666,2},{0x7668,4},{0x766c,4},{0x7670,2},{0x7672,6},{0x7678,6},{0x767e,6},{0x7684,6},{0x768a,2},{0x768c,6},{0x7692,2},{0x7694,6},{0x769a,2},{0x769c,4},{0x76a0,4},{0x76a4,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateDspFactoryResetPartialForTests(StateDspFactoryResetPartialTest f){partialTest=f;}
bool StateDspFactoryResetCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x7666){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native DSPfactory-reset capacity exceeded");selected->emplace(h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[1].rl));const auto stage=task.stage();unsigned target=0;switch(stage){case S::WriteSelected:target=0x17526;break;case S::WriteVoiceBank:target=0x1755c;break;case S::WritePartFlags:target=0x17527;break;case S::WriteBusy:target=0x174a7;break;case S::WriteInvalid:target=0x1755e;break;case S::WriteVoiceMask:target=0x174a6;break; default:break;}const unsigned storeClocks=(h.lastwrite&1)||int(target)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);auto write=[&](T::Write,uint8_t value){f.store8(target,value);};if(task.advanceBefore(cost,write,storeClocks)!=cost)throw std::runtime_error("Native DSPfactory-reset admission differs");switch(stage){case S::UploadFactory:{f.call(0x76a6);break;}
case S::ResetParts:{f.call(0xa6d4);break;}
case S::ResetVoices:{f.call(0x9f50);break;}
case S::ClearFlags:{f.move(r[0].rl,0x0);break;}
case S::WriteSelected:{;break;}
case S::WriteVoiceBank:{;break;}
case S::WritePartFlags:{;break;}
case S::WriteBusy:{;break;}
case S::SetInvalid:{f.move(r[0].rl,0xffffffff);break;}
case S::WriteInvalid:{;break;}
case S::SetVoiceMask:{f.move(r[1].rh,0xe);break;}
case S::WriteVoiceMask:{;break;}
case S::ClearPayloadHigh:{r[1].e=h.sub16(r[1].e,r[1].e);break;}
case S::SetAddress:{f.move(r[4].r,0x2);break;}
case S::WriteControl:{f.call(0x24be);break;}
case S::Return:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native DSPfactory-reset clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
