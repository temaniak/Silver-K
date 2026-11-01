#include "ksStateFactoryServicesCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateFactoryServicesTask;using S=T::Stage;StateFactoryServicesPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,19> points{{{0x66dc,6},{0x66e2,2},{0x66e4,2},{0x66e6,4},{0x66ea,6},{0x66f0,6},{0x66f6,4},{0x66fa,2},{0x66fc,2},{0x66fe,2},{0x6700,2},{0x6702,2},{0x6704,6},{0x670a,6},{0x6710,4},{0x6714,4},{0x6718,6},{0x671e,4},{0x6722,4}}};Point point(const T& t){if(t.awaitingTail())return{t.service()==T::Service::Load?0x69c6u:0x48ccu,0};return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateFactoryServicesPartialForTests(StateFactoryServicesPartialTest f){partialTest=f;}
bool StateFactoryServicesCalls::retireReturns(ControllerCpu& h){bool retired=false;for(auto& c:calls)if(c&&c->token==h.getSP()&&c->task.awaitingTail()&&point(c->task).location==h.getPC()){c->task.retireTail();c.reset();++completed;retired=true;}return retired;}
bool StateFactoryServicesCalls::step(ControllerCpu& h){using namespace controllerDetail;retireReturns(h);const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x66dc||pc==0x66ea){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native factory-service capacity exceeded");selected->emplace(pc==0x66dc?T::Kind::Provided:T::Kind::Factory,h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();const unsigned physical=0x167fc+task.byteOffset();const unsigned storeClocks=(h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](unsigned offset){return observed=uint8_t(h.read8(0x81fe00+offset));};auto write=[&](unsigned offset,uint8_t value){f.store8(0x167fc+offset,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native factory-service admission differs");switch(stage){case S::SetCommitPage:{f.move(r[6].er,0x81ff00);break;}
case S::CaptureSource:{f.move(r[5].er,r[1].er);break;}
case S::StoreProvided:{f.call(0x668c);break;}
case S::TailLoad:{h.clockI(2);break;}
case S::SetFactorySource:{f.move(r[0].er,0x81fe00);break;}
case S::SetScratch:{f.move(r[1].er,0x167fc);break;}
case S::SetCount:{f.move(r[2].e,0x100);break;}
case S::ReadFactory:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteFactory:{;break;}
case S::AdvanceScratch:{f.inc(r[1].er,1);break;}
case S::DecrementCount:{f.inc(r[2].e,-1);break;}
case S::AdmitMore:{;break;}
case S::SetScratchSource:{f.move(r[5].er,0x167fc);break;}
case S::SetFactoryPage:{f.move(r[6].er,0x81ff00);break;}
case S::StoreFactory:{f.call(0x668c);break;}
case S::LoadFactory:{f.call(0x69c6);break;}
case S::SetMessage:{f.move(r[6].er,0x14edf);break;}
case S::DisplayMessage:{f.call(0x55b8);break;}
case S::TailPanel:{h.clockI(2);break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native factory-service clocks differ");h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
