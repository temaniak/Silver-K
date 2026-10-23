#include "ksPanelAnalogCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
PanelAnalogPartialTest partialTest=nullptr;using T=PanelAnalogTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,99> points{{{0x1510,2},{0x1512,2},{0x1514,2},{0x1516,2},{0x1518,2},{0x151a,2},{0x151c,2},{0x151e,2},{0x1520,2},{0x1522,2},{0x1524,2},{0x1526,2},{0x1528,2},{0x152a,2},{0x152c,2},{0x20c8,2},{0x20ca,2},{0x20cc,2},{0x20ce,2},{0x20d0,4},{0x20d4,2},{0x20d6,2},{0x20d8,4},{0x20dc,2},{0x20de,4},{0x20e2,2},{0x20e4,2},{0x20e6,2},{0x20e8,2},{0x20ea,2},{0x20ec,2},{0x20ee,2},{0x20f0,2},{0x20f2,2},{0x20f4,2},{0x20f6,4},{0x20fa,2},{0x20fc,2},{0x20fe,2},{0x2100,2},{0x2102,2},{0x2104,2},{0x2106,4},{0x210a,2},{0x210c,4},{0x2110,2},{0x2112,4},{0x2116,6},{0x211c,2},{0x211e,2},{0x2120,4},{0x2124,2},{0x2126,4},{0x212a,2},{0x212c,4},{0x2130,6},{0x2136,2},{0x2138,2},{0x213a,2},{0x213c,2},{0x213e,4},{0x2142,2},{0x2144,4},{0x2148,4},{0x214c,2},{0x214e,2},{0x2150,2},{0x2152,2},{0x2154,2},{0x2156,2},{0x2158,4},{0x215c,2},{0x215e,2},{0x2160,2},{0x2162,2},{0x2164,4},{0x2168,2},{0x216a,2},{0x216c,2},{0x216e,2},{0x2170,2},{0x2172,2},{0x2174,2},{0x2176,4},{0x217a,2},{0x217c,2},{0x217e,2},{0x2180,2},{0x2182,2},{0x2184,2},{0x2186,4},{0x218a,2},{0x218c,2},{0x218e,2},{0x2190,2},{0x2192,2},{0x2194,2},{0x2196,2},{0x2198,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;return at>=0xffffa0?1:3;}
}
void setPanelAnalogPartialForTests(PanelAnalogPartialTest value){partialTest=value;}
bool PanelAnalogCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1510:kind=T::Kind::Initialize;break;case 0x151e:kind=T::Kind::Duty;break;case 0x1522:kind=T::Kind::Level;break;case 0x20c8:kind=T::Kind::Range;break;case 0x20de:kind=T::Kind::Coefficients;break;case 0x20fc:kind=T::Kind::Convert;break;case 0x2144:kind=T::Kind::Scale;break;case 0x218c:kind=T::Kind::Square;break; default:break;}
 if(kind){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native panel analog capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(pc==0x151e?h.regs[0].rl:pc==0x1522?h.regs[6].rl:h.regs[6].rh),uint8_t(h.regs[6].rl),uint16_t(h.regs[3].r),uint32_t(h.regs[1].er),uint32_t(h.regs[2].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[6].rh));const auto stage=task.stage();
 const unsigned history=call.history,coefficients=call.coefficients;
 unsigned memory=3;switch(stage){case S::ReadMinimum:memory=byteClocks(history,int(pc+point(task).length-1));break;
case S::WriteMinimum:memory=byteClocks(history,h.lastwrite);break;
case S::ReadMaximum:memory=byteClocks(history+1,int(pc+point(task).length-1));break;
case S::WriteMaximum:memory=byteClocks(history+1,h.lastwrite);break;
case S::ReadCoefficientMaximum:memory=byteClocks(history+1,int(pc+point(task).length-1));break;
case S::ReadCoefficientMinimum:memory=byteClocks(history,int(pc+point(task).length-1));break;
case S::WriteOffset:memory=byteClocks(coefficients,h.lastwrite);break;
case S::WriteFactor:memory=byteClocks(coefficients+1,h.lastwrite);break;
case S::ReadFactor:memory=byteClocks(coefficients+1,int(pc+point(task).length-1));break;
case S::ReadOffset:memory=byteClocks(coefficients,int(pc+point(task).length-1));break; default:break;}
 const auto cost=task.nextClocks(memory);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(field==T::Read::Minimum?history:field==T::Read::Maximum?history+1:field==T::Read::Offset?coefficients:coefficients+1));};
 auto write=[&](T::Write field,uint8_t value){unsigned at=field==T::Write::ControlMode?0xffffa6:field==T::Write::ControlCount?0xffffa4:field==T::Write::Duty?0xffffa5:field==T::Write::Minimum?history:field==T::Write::Maximum?history+1:field==T::Write::Offset?coefficients:coefficients+1;f.store8(at,int8_t(value));};
 if(task.advanceBefore(cost,read,write,memory)!=cost)throw std::runtime_error("Native panel analog admission differs");
 switch(stage){case S::BindControlMode:f.move(r[0].rl,0xffffffc0);break;
case S::ClearControlCount:f.move(r[0].rl,0x0);break;
case S::BindInitialDuty:f.move(r[0].rl,0xffffffc0);break;
case S::ReturnInitialize:f.ret();break;
case S::ReturnDuty:f.ret();break;
case S::BindLevelCenter:f.move(r[6].rh,0x40);break;
case S::SubtractLevel:r[6].rh=h.sub8(r[6].rh,r[6].rl);break;
case S::DoubleLevel:f.shift(r[6].rh,1,true);break;
case S::BiasLevel:r[6].rh=h.add8(r[6].rh,0x40);break;
case S::ReturnLevel:f.ret();break;
case S::ReadMinimum:f.move(r[0].rl,task.minimum());break;
case S::CompareMinimum:h.sub8(r[6].rh,r[0].rl);break;
case S::ReadMaximum:f.move(r[0].rl,task.maximum());break;
case S::CompareMaximum:h.sub8(r[6].rh,r[0].rl);break;
case S::ReturnRange:f.ret();break;
case S::ReadCoefficientMaximum:f.move(r[0].rl,task.maximum());break;
case S::ReadCoefficientMinimum:f.move(r[0].rh,task.minimum());break;
case S::ComputeSpan:r[0].rl=h.sub8(r[0].rl,r[0].rh);break;
case S::RetainSpan:f.push(r[0].r);break;
case S::HalfSpan:f.shift(r[0].rl,1,false);break;
case S::ComputeCenter:r[0].rl=h.add8(r[0].rl,r[0].rh);break;
case S::BindCenter:f.move(r[0].rh,0xffffff80);break;
case S::ComputeOffset:r[0].rh=h.sub8(r[0].rh,r[0].rl);break;
case S::RestoreSpan:f.pop(r[0].r);break;
case S::ComputeFactor:f.divideUnsigned(r[0].rl,r[3].r);break;
case S::ReturnCoefficients:f.ret();break;
case S::CompareFirstWheel:h.sub8(r[6].rl,0x22);break;
case S::CompareSecondWheel:h.sub8(r[6].rl,0x23);break;
case S::CompareSquare:h.sub8(r[6].rl,0x20);break;
case S::SelectSquare:h.clockI(2);if(task.complete())f.next=0x218c;break;
case S::ReturnUnchanged:f.ret();break;
case S::SaveFirstAddress:f.push(r[2].er);break;
case S::SaveFirstBias:f.push(r[3].r);break;
case S::SetFirstBias:f.move(r[3].r,0x400);break;
case S::SelectFirstCoefficients:f.move(r[2].er,0x158ec);break;
case S::ScaleFirst:f.call(0x2144);break;
case S::RestoreFirstBias:f.pop(r[3].r);break;
case S::RestoreFirstAddress:f.pop(r[2].er);break;
case S::ReturnFirst:f.ret();break;
case S::SaveSecondAddress:f.push(r[2].er);break;
case S::SaveSecondBias:f.push(r[3].r);break;
case S::SetSecondBias:f.move(r[3].r,0x0);break;
case S::SelectSecondCoefficients:f.move(r[2].er,0x158ee);break;
case S::ReverseSecond:r[6].rh=~r[6].rh;value(h,r[6].rh);break;
case S::ScaleSecond:f.call(0x2144);break;
case S::HalfSecond:f.shift(r[6].rh,1,false);break;
case S::RestoreSecondBias:f.pop(r[3].r);break;
case S::RestoreSecondAddress:f.pop(r[2].er);break;
case S::ReturnSecond:f.ret();break;
case S::SaveValue:f.push(r[0].er);break;
case S::ReadFactor:f.move(r[0].rl,task.factor());break;
case S::ClearFactorHigh:r[0].rh=h.sub8(r[0].rh,r[0].rh);break;
case S::RetainFactor:f.move(r[0].e,r[0].r);break;
case S::ReadOffset:f.move(r[0].rl,task.offset());break;
case S::AddInput:r[0].rl=h.add8(r[0].rl,r[6].rh);break;
case S::CenterInput:r[0].rl=h.add8(r[0].rl,0xffffff80);break;
case S::SignInput:r[0].r=int8_t(r[0].r);value(h,r[0].r);break;
case S::MultiplyInput:f.multiplySigned(r[0].e,r[0].er);break;
case S::SubtractBias:r[0].r=h.sub16(r[0].r,r[3].r);break;
case S::ClearPositive:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::BindPositiveLimit:f.move(r[0].e,0x3f80);break;
case S::ComparePositiveLimit:h.sub16(r[0].r,r[0].e);break;
case S::ClampPositive:f.move(r[0].r,r[0].e);break;
case S::AddBias:r[0].r=h.add16(r[0].r,r[3].r);break;
case S::ClearNegative:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::BindNegativeLimit:f.move(r[0].e,0xc000);break;
case S::CompareNegativeLimit:h.sub16(r[0].r,r[0].e);break;
case S::ClampNegative:f.move(r[0].r,r[0].e);break;
case S::DoubleResult:f.shift(r[0].r,1,true);break;
case S::BiasResult:r[0].rh=h.add8(r[0].rh,0xffffff80);break;
case S::PublishResult:f.move(r[6].rh,r[0].rh);break;
case S::RestoreValue:f.pop(r[0].er);break;
case S::ReturnValue:f.ret();break;
case S::SaveSquare:f.push(r[0].r);break;
case S::CopySquare:f.move(r[0].rl,r[6].rh);break;
case S::ComputeSquare:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::HalfSquare:f.shift(r[0].rh,1,false);break;
case S::PublishSquare:f.move(r[6].rh,r[0].rh);break;
case S::RestoreSquare:f.pop(r[0].r);break;
case S::ReturnSquare:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native panel analog clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
