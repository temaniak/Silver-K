#include "ksStateFieldMapCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {using S=StateFieldMapTask::Stage;StateFieldMapPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,45> points{{{0x6da4,2},{0x6da6,2},{0x6da8,2},{0x6daa,2},{0x6dac,2},{0x6dae,2},{0x6db0,2},{0x6db2,2},{0x6db4,2},{0x6db6,2},{0x6db8,10},{0x6dc2,2},{0x6dc4,2},{0x6dc6,2},{0x6dc8,2},{0x6dca,2},{0x6dcc,2},{0x6dce,2},{0x6dd0,2},{0x6dd2,2},{0x6dd4,2},{0x6dd6,2},{0x6dd8,2},{0x6dda,2},{0x6ddc,2},{0x6dde,2},{0x6de0,2},{0x6de2,2},{0x6de4,2},{0x6de6,2},{0x6de8,2},{0x6dea,2},{0x6dec,2},{0x6dee,2},{0x6df0,2},{0x6df2,2},{0x6df4,2},{0x6df6,8},{0x6dfe,2},{0x6e00,2},{0x6e02,2},{0x6e04,2},{0x6e06,2},{0x6e08,2},{0x6e0a,2}}};
Point point(const StateFieldMapTask& t){return points[unsigned(t.stage())];}}
void setStateFieldMapPartialForTests(StateFieldMapPartialTest fn){partialTest=fn;}
bool StateFieldMapCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x6da4){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native state-field capacity exceeded");selected->emplace(uint8_t(h.regs[6].rl),uint8_t(h.regs[6].rh),uint32_t(h.regs[1].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](StateFieldMapTask::Read what)->uint32_t{using R=StateFieldMapTask::Read;switch(what){case R::Descriptor:return observed=uint32_t(h.read32(72134+task.descriptorNumber()*4));case R::Base:return observed=uint32_t(h.read32(0x167bc));case R::Current:return observed=uint8_t(h.read8(int32_t(call.base+task.field())));default:return observed=uint8_t(h.read8(int32_t(call.descriptor+task.descriptorOffset())));}};
 if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native state-field admission mismatch");
 switch(stage){case S::CompareFirst:{h.sub8(r[6].rl,0x68);break;}
case S::AdmitFirst:{;break;}
case S::CompareLast:{h.sub8(r[6].rl,0x6b);break;}
case S::AdmitLast:{;break;}
case S::MarkUnmapped:{r[0].rl=h.sub8(r[0].rl,r[0].rl);break;}
case S::ReturnUnmapped:{if(uint32_t(r[1].er)!=task.field()||uint8_t(r[6].rh)!=task.value())throw std::runtime_error("Native state-field result projection differs");f.ret();break;}
case S::ClearDescriptorIndex:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::CaptureParameter:{f.move(r[0].rl,r[6].rl);break;}
case S::BiasDescriptorIndex:{r[0].rl=h.add8(r[0].rl,0xffffff98);break;}
case S::ScaleDescriptorIndex:{f.shift(r[0].rl,2,true);break;}
case S::ReadDescriptor:{f.move(r[0].er,observed);call.descriptor=observed;break;}
case S::ClearInput:{r[2].er=h.sub32(r[2].er,r[2].er);break;}
case S::CaptureInput:{f.move(r[2].rl,r[6].rh);break;}
case S::ReadRightCount:{f.move(r[1].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::AdmitRightCount:{;break;}
case S::ShiftInput:{f.shift(r[2].rl,1,false);break;}
case S::DecrementRight:{f.inc(r[1].rl,-1);break;}
case S::AdmitMoreRight:{;break;}
case S::ReadLimit:{f.move(r[1].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::CompareLimit:{h.sub8(r[1].rl,r[2].rl);break;}
case S::AdmitLimit:{;break;}
case S::MarkLimit:{f.move(r[0].rl,0x1);break;}
case S::ReturnLimit:{if(uint32_t(r[1].er)!=task.field()||uint8_t(r[6].rh)!=task.value())throw std::runtime_error("Native state-field result projection differs");f.ret();break;}
case S::ReadMask:{f.move(r[6].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::MaskValue:{r[6].rh&=r[6].rl;value(h,r[6].rh);break;}
case S::InvertMask:{r[6].rl=~r[6].rl;value(h,r[6].rl);break;}
case S::ReadSpare:{f.move(r[1].rh,observed);r[0].er+=1;h.clockI(2);break;}
case S::MakePairIndex:{r[2].rl&=0xfffffffe;value(h,r[2].rl);break;}
case S::AddPairIndex:{r[0].er=h.add32(r[0].er,r[2].er);break;}
case S::ReadLeftCount:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::AdmitLeftCount:{;break;}
case S::ShiftValue:{f.shift(r[6].rh,1,true);break;}
case S::RotateMask:{f.rotate(r[6].rl,1,true,false);break;}
case S::DecrementLeft:{f.inc(r[2].rl,-1);break;}
case S::AdmitMoreLeft:{;break;}
case S::ClearField:{r[1].er=h.sub32(r[1].er,r[1].er);break;}
case S::ReadField:{f.move(r[1].rl,observed);break;}
case S::ReadBase:{f.move(r[0].er,observed);call.base=observed;break;}
case S::AddFieldBase:{r[0].er=h.add32(r[0].er,r[1].er);break;}
case S::ReadCurrent:{f.move(r[0].rl,observed);break;}
case S::MaskCurrent:{r[0].rl&=r[6].rl;value(h,r[0].rl);break;}
case S::MergeCurrent:{r[6].rh|=r[0].rl;value(h,r[6].rh);break;}
case S::MarkMapped:{r[0].rl=h.sub8(r[0].rl,r[0].rl);break;}
case S::SetMapped:{f.inc(r[0].rl,1);break;}
case S::ReturnMapped:{if(uint32_t(r[1].er)!=task.field()||uint8_t(r[6].rh)!=task.value())throw std::runtime_error("Native state-field result projection differs");f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native state-field clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(point(task).location);return true;
}
}
