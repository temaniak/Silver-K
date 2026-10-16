#include "ksStateDspFactoryTransferCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateDspFactoryTransferTask;using S=T::Stage;StateDspFactoryTransferPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,24> points{{{0x77a0,4},{0x77a4,4},{0x77a8,2},{0x77aa,2},{0x77ac,2},{0x77ae,4},{0x77b2,4},{0x77b6,2},{0x77b8,2},{0x77ba,2},{0x77bc,2},{0x77be,2},{0x77c0,4},{0x77c4,2},{0x77c6,2},{0x77c8,2},{0x77ca,2},{0x77cc,2},{0x77ce,2},{0x77d0,2},{0x77d2,4},{0x77d6,2},{0x77d8,2},{0x77da,2}}};Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}}
void setStateDspFactoryTransferPartialForTests(StateDspFactoryTransferPartialTest f){partialTest=f;}
bool StateDspFactoryTransferCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x77a0||pc==0x77ae)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native DSPfactory-transfer capacity exceeded");selected->emplace(pc==0x77a0?T::Kind::Poll:T::Kind::Stream,uint32_t(h.regs[1].er),uint16_t(h.regs[3].e),h.getSP(),uint32_t(h.regs[2].er));++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[4].rh));const auto stage=task.stage();const unsigned address=(call.source+task.byteOffset())&0xffffff;const unsigned wordReadClocks=address<0x400000&&!(address&1)?3:6;const auto cost=task.nextClocks(wordReadClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](bool byte)->uint32_t{return observed=byte?uint8_t(h.read8(call.source+task.byteOffset())):uint16_t(h.read16(call.source+task.byteOffset()));};if(task.advanceBefore(cost,read,wordReadClocks)!=cost)throw std::runtime_error("Native DSPfactory-transfer admission differs");switch(stage){case S::SetPollAddress:{f.move(r[4].r,0x803e);break;}
case S::ReadStatus:{f.call(0x2560);break;}
case S::CompareReady:{h.sub8(r[4].rh,0xffffff80);break;}
case S::AdmitReady:{;break;}
case S::ReturnPoll:{f.ret();break;}
case S::SetStreamAddress:{f.move(r[4].r,0x8c00);break;}
case S::PrimeStream:{f.call(0x24b2);break;}
case S::ClearTrailer:{f.move(r[1].rh,0x0);break;}
case S::CopyDescriptor:{f.move(r[0].r,r[1].e);break;}
case S::TestByteMode:{flag(h,4,(uint8_t(r[0].rh)&(1u<<0x3))==0);break;}
case S::AdmitByteMode:{;break;}
case S::ReadWord:{f.move(r[1].e,observed);r[2].er+=2;h.clockI(2);break;}
case S::WriteWord:{f.call(0x2520);break;}
case S::DecrementWord:{f.inc(r[3].e,-1);break;}
case S::AdmitWord:{;break;}
case S::ReturnWords:{f.ret();break;}
case S::ReadByte:{f.move(r[0].rh,observed);r[2].er+=1;h.clockI(2);break;}
case S::BiasByte:{r[0].rh=h.add8(r[0].rh,0xffffff80);break;}
case S::ClearLowByte:{f.move(r[0].rl,0x0);break;}
case S::CaptureByte:{f.move(r[1].e,r[0].r);break;}
case S::WriteByte:{f.call(0x2520);break;}
case S::DecrementByte:{f.inc(r[3].e,-1);break;}
case S::AdmitByte:{;break;}
case S::ReturnBytes:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native DSPfactory-transfer clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingChild()?f.next:point(task).location);return true;}}
