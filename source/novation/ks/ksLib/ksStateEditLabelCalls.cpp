#include "ksStateEditLabelCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StateEditLabelPartialTest partialTest=nullptr;using T=StateEditLabelTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,77> points{{{0x5012,2},{0x5014,4},{0x5018,2},{0x501a,2},{0x501c,2},{0x501e,2},{0x5020,6},{0x5026,6},{0x502c,4},{0x5030,2},{0x5032,2},{0x5034,2},{0x5036,2},{0x5038,2},{0x503a,4},{0x503e,2},{0x5040,6},{0x5046,4},{0x504a,8},{0x5052,2},{0x5054,2},{0x5056,8},{0x505e,2},{0x5060,6},{0x5066,2},{0x5068,2},{0x506a,6},{0x5070,6},{0x5076,2},{0x5078,2},{0x507a,2},{0x507c,8},{0x5084,2},{0x5086,4},{0x508a,2},{0x508c,2},{0x508e,2},{0x5090,8},{0x5098,2},{0x509a,4},{0x509e,2},{0x50a0,2},{0x50a2,8},{0x50aa,2},{0x50ac,2},{0x50ae,2},{0x50b0,6},{0x50b6,6},{0x50bc,4},{0x50c0,2},{0x50c2,2},{0x50c4,2},{0x50c6,2},{0x50c8,2},{0x50ca,4},{0x50ce,8},{0x50d6,2},{0x50d8,2},{0x50da,2},{0x50dc,2},{0x50de,2},{0x50e0,2},{0x50e2,8},{0x50ea,2},{0x50ec,2},{0x50ee,2},{0x50f0,2},{0x50f2,6},{0x50f8,6},{0x50fe,4},{0x5102,2},{0x5104,2},{0x5106,2},{0x5108,2},{0x510a,2},{0x510c,4},{0x5110,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStateEditLabelPartialForTests(StateEditLabelPartialTest value){partialTest=value;}
bool StateEditLabelCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x5012:kind=T::Kind::Label;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StateEditLabel capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(h.regs[5].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 auto accessCost=[&](uint32_t ref,unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){const unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};unsigned store=3;switch(stage){case S::ReadLabelCharacter:store=accessCost(uint32_t(r[6].er),1,-1);break;case S::WriteLabelCharacter:store=accessCost(uint32_t(r[5].er),1,h.lastwrite);break;case S::WriteLabelTerminator:store=accessCost(uint32_t(r[5].er),2,h.lastwrite);break;case S::ReadLabelKind:store=accessCost(uint32_t(r[6].er+4),1,-1);break;case S::ReadLabelPatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadModulationSource:store=accessCost(uint32_t(r[5].er+224),1,-1);break;case S::WriteModulationWord:store=accessCost(uint32_t(r[6].er+8),4,h.lastwrite);break;case S::ReadOscillatorSource:store=accessCost(uint32_t(r[5].er+222),1,-1);break;case S::WriteOscillatorDigit:store=accessCost(uint32_t(r[6].er+4),1,h.lastwrite);break;case S::ReadLfoSource:store=accessCost(uint32_t(r[5].er+225),1,-1);break;case S::WriteLfoDigit:store=accessCost(uint32_t(r[6].er+4),1,h.lastwrite);break;case S::ReadSourceKind:store=accessCost(uint32_t(r[5].er+223),1,-1);break;case S::ReadSourceCharacter:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::WriteSourceCharacter:store=accessCost(uint32_t(r[5].er),1,h.lastwrite);break;case S::ReadEnvelopeKind:store=accessCost(uint32_t(r[6].er+6),1,-1);break;case S::ReadEnvelopePatch:store=accessCost(uint32_t(0x167bc),4,-1);break;case S::ReadEnvelopeSource:store=accessCost(uint32_t(r[5].er+139),1,-1);break;case S::ReadEnvelopeCharacter:store=accessCost(uint32_t(r[0].er),1,-1);break;case S::WriteEnvelopeCharacter:store=accessCost(uint32_t(r[5].er),1,h.lastwrite);break;default:break;} const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{return field==T::Read::Patch?uint32_t(h.read32(0x167bc)):uint8_t(h.read8(ref));};auto write=[&](T::Write field,uint32_t ref,uint32_t value){if(field==T::Write::Byte)f.store8(ref,int8_t(value));else if(field==T::Write::Word)f.store16(ref,int16_t(value));else f.store32(ref,int32_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StateEditLabel admission differs");
 switch(stage){case S::BindLabelPosition:f.move(r[6].rl,0xffffff80);break;
case S::ShowLabelPosition:f.call(0x976);break;
case S::BindLabelIndex:f.move(r[6].rl,r[5].rl);break;
case S::BindLabelStride:f.move(r[6].rh,0x10);break;
case S::ScaleLabelIndex:r[6].r=uint8_t(r[6].r)*uint32_t(uint8_t(r[6].rh));h.clockI(12);break;
case S::NormalizeLabelIndex:r[6].er&=0xffff;value(h,r[6].er);break;
case S::LocateLabel:r[6].er=h.add32(r[6].er,0xfede);break;
case S::BindLabelBuffer:f.move(r[5].er,0x167dc);break;
case S::BindLabelLength:f.move(r[0].e,0x10);break;
case S::ReadLabelCharacter:f.move(r[0].rl,task.character());r[6].er+=1;h.clockI(2);break;
case S::NextLabelDestination:f.inc(r[5].er,1);break;
case S::TickLabelLength:f.inc(r[0].e,-1);break;
case S::BindLabelTerminator:f.move(r[0].r,0x2000);break;
case S::BindLabelReference:f.move(r[6].er,0x167dc);break;
case S::ReadLabelKind:f.move(r[0].rl,task.kind());break;
case S::ReadLabelPatch:f.move(r[5].er,task.patch());break;
case S::TestModulationLabel:h.sub8(r[0].rl,0x50);break;
case S::ReadModulationSource:f.move(r[0].rh,task.variant());break;
case S::BindLfoWord:f.move(r[1].er,0x4d6c666f);break;
case S::TickModulationSource:f.inc(r[0].rh,-1);break;
case S::BindEnvelopeWord:f.move(r[1].er,0x4d656e76);break;
case S::TestOscillatorLabel:h.sub8(r[0].rl,0x4f);break;
case S::ReadOscillatorSource:f.move(r[0].rh,task.variant());break;
case S::AddOscillatorDigit:r[0].rh=h.add8(r[0].rh,0x31);break;
case S::TestLfoLabel:h.sub8(r[0].rl,0x4c);break;
case S::ReadLfoSource:f.move(r[0].rh,task.variant());break;
case S::AddLfoDigit:r[0].rh=h.add8(r[0].rh,0x31);break;
case S::TestSourceLabel:h.sub8(r[0].rl,0x53);break;
case S::ReadSourceKind:f.move(r[0].rh,task.variant());break;
case S::BindSourceStride:f.move(r[0].rl,0x6);break;
case S::ScaleSourceKind:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::NormalizeSourceKind:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateSourceName:r[0].er=h.add32(r[0].er,0x10162);break;
case S::BindSourceBuffer:f.move(r[5].er,0x167dc);break;
case S::BindSourceLength:f.move(r[1].e,0x6);break;
case S::ReadSourceCharacter:f.move(r[1].rl,task.character());r[0].er+=1;h.clockI(2);break;
case S::NextSourceDestination:f.inc(r[5].er,1);break;
case S::TickSourceLength:f.inc(r[1].e,-1);break;
case S::ReadEnvelopeKind:f.move(r[0].rl,task.kind());break;
case S::ReadEnvelopePatch:f.move(r[5].er,task.patch());break;
case S::BindEnvelopeLetter:f.move(r[0].rh,0x41);break;
case S::SubtractEnvelopeLetter:r[0].rl=h.sub8(r[0].rl,r[0].rh);break;
case S::TestEnvelopeKind:h.sub8(r[0].rl,0x3);break;
case S::DoubleEnvelopeKind:f.shift(r[0].rl,1,true);break;
case S::ReadEnvelopeSource:f.move(r[0].rh,task.variant());break;
case S::AddEnvelopeSource:r[0].rl=h.add8(r[0].rl,r[0].rh);break;
case S::BindEnvelopeStride:f.move(r[0].rh,0x6);break;
case S::ScaleEnvelopeKind:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::NormalizeEnvelopeKind:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocateEnvelopeName:r[0].er=h.add32(r[0].er,0x10174);break;
case S::BindEnvelopeBuffer:f.move(r[5].er,0x167e2);break;
case S::BindEnvelopeLength:f.move(r[1].e,0x6);break;
case S::ReadEnvelopeCharacter:f.move(r[1].rl,task.character());r[0].er+=1;h.clockI(2);break;
case S::NextEnvelopeDestination:f.inc(r[5].er,1);break;
case S::TickEnvelopeLength:f.inc(r[1].e,-1);break;
case S::PrintLabel:f.call(0x9b8);break;
case S::ReturnLabel:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StateEditLabel clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
