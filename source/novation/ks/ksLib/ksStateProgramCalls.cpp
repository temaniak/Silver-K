#include "ksStateProgramCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateProgramTask;using S=T::Stage;StateProgramPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,40> points{{{0x6198,2},{0x619a,2},{0x619c,2},{0x619e,2},{0x61a0,2},{0x61a2,2},{0x61a4,2},{0x61a6,2},{0x61a8,2},{0x61aa,2},{0x61ac,2},{0x61ae,2},{0x61b0,2},{0x61b2,2},{0x61b4,2},{0x61b6,2},{0x61b8,2},{0x61ba,2},{0x61bc,2},{0x61be,2},{0x61c0,2},{0x61c2,2},{0x61c4,2},{0x61c6,4},{0x61ca,2},{0x61cc,2},{0x61ce,2},{0x61d0,4},{0x61d4,2},{0x61d6,6},{0x61dc,2},{0x61de,2},{0x61e0,6},{0x61e6,4},{0x61ea,2},{0x628c,2},{0x628e,6},{0x6294,2},{0x6296,6},{0x629c,2}}};Point point(const T& t){return t.awaitingMode()?Point{0x61ea,2}:points[unsigned(t.stage())];}}
void setStateProgramPartialForTests(StateProgramPartialTest f){partialTest=f;}
bool StateProgramCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x6198||pc==0x61c2||pc==0x628c){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native program capacity exceeded");selected->emplace(pc==0x6198?T::Kind::ReadIndex:pc==0x61c2?T::Kind::WriteIndex:T::Kind::PartSlot,uint16_t(h.regs[0].r),h.getSP(),uint32_t(h.regs[0].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingMode())task.resumeMode();const auto stage=task.stage();const unsigned address=stage==S::WriteProgram?0x167ad:0x167ac;const unsigned storeClocks=(h.lastwrite&1)||int(address)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{return observed=uint8_t(h.read8(what==T::Read::Bank?call.source:what==T::Read::Program?call.source+1:0x17526));};auto write=[&](T::Write what,uint8_t v){f.store8(what==T::Write::Bank?0x167ac:0x167ad,v);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native program admission differs");switch(stage){case S::RetainBank:{f.push(r[1].r);break;}
case S::RetainExtra:{f.push(r[2].r);break;}
case S::ReadBank:{f.move(r[1].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::PreviousBank:{f.inc(r[1].rl,-1);break;}
case S::ClearExtra:{f.move(r[2].rl,0x0);break;}
case S::CompareBank:{h.sub8(r[1].rl,0x4);break;}
case S::AdmitBank:{;break;}
case S::CopyExtra:{f.move(r[2].rl,r[1].rl);break;}
case S::BiasExtra:{r[2].rl=h.add8(r[2].rl,0xfffffffc);break;}
case S::ClampBank:{f.move(r[1].rl,0x4);break;}
case S::SetBankSize:{f.move(r[1].rh,0x64);break;}
case S::MultiplyBank:{r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rh));h.clockI(12);break;}
case S::SetExtraSize:{f.move(r[2].rh,0x32);break;}
case S::MultiplyExtra:{r[2].r=uint8_t(r[2].r)*uint32_t(uint8_t(r[2].rh));h.clockI(12);break;}
case S::ReadProgram:{f.move(r[0].rl,observed);break;}
case S::NormalizeProgram:{r[0].r&=0xff;value(h,r[0].r);break;}
case S::AddBank:{r[0].r=h.add16(r[0].r,r[1].r);break;}
case S::AddExtra:{r[0].r=h.add16(r[0].r,r[2].r);break;}
case S::RestoreExtra:{f.pop(r[2].r);break;}
case S::RestoreBank:{f.pop(r[1].r);break;}
case S::ReturnIndex:{if(uint16_t(r[0].r)!=task.index())throw std::runtime_error("Native program index differs");f.ret();break;}
case S::ClearBias:{f.move(r[1].rh,0x0);break;}
case S::SetDivisor:{f.move(r[1].rl,0x64);break;}
case S::CompareIndex:{h.sub16(r[0].r,0x1c2);break;}
case S::AdmitIndex:{;break;}
case S::SetBias:{f.move(r[1].rh,0x4);break;}
case S::SetLargeDivisor:{f.move(r[1].rl,0x32);break;}
case S::SubtractLargeBase:{r[0].r=h.sub16(r[0].r,0x190);break;}
case S::DivideIndex:{f.divideUnsigned(r[1].rl,r[0].r);break;}
case S::WriteProgram:{;break;}
case S::AddBias:{r[0].rl=h.add8(r[0].rl,r[1].rh);break;}
case S::IncrementBank:{f.inc(r[0].rl,1);break;}
case S::WriteBank:{;break;}
case S::RebindMode:{f.call(0x6ec6);break;}
case S::ReturnProgram:{f.ret();break;}
case S::ClearSlot:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::ReadPart:{f.move(r[0].rl,observed);break;}
case S::DoublePart:{f.shift(r[0].rl,1,true);break;}
case S::AddSlotBase:{r[0].er=h.add32(r[0].er,0x167ae);break;}
case S::ReturnSlot:{if(uint32_t(r[0].er)!=task.slotAddress())throw std::runtime_error("Native program slot differs");f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native program clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingMode())call.token=h.getSP();h.pc=h.makepc(task.awaitingMode()?f.next:point(task).location);return true;}}
