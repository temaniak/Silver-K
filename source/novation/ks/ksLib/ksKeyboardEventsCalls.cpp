#include "ksKeyboardEventsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
KeyboardEventsPartialTest partialTest=nullptr;using T=KeyboardEventsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,120> points{{{0x1e16,2},{0x1e18,2},{0x1e1a,2},{0x1e1c,2},{0x1e1e,2},{0x1e20,2},{0x1e22,2},{0x1e24,2},{0x1e26,2},{0x1e28,2},{0x1e2a,2},{0x1e2c,2},{0x1e2e,2},{0x1e30,2},{0x1e32,2},{0x1e34,2},{0x1e36,6},{0x1e3c,4},{0x1e40,4},{0x1e44,2},{0x1e46,6},{0x1e4c,4},{0x1e50,4},{0x1e54,2},{0x1e56,4},{0x1e5a,2},{0x1e5c,4},{0x1e60,4},{0x1e64,2},{0x1e66,4},{0x1e6a,4},{0x1e6e,2},{0x1e70,6},{0x1e76,4},{0x1e7a,2},{0x1e7c,4},{0x1e80,2},{0x1e82,2},{0x1e84,2},{0x1e86,2},{0x1e88,4},{0x1e8c,4},{0x1e90,2},{0x1e92,4},{0x1e96,2},{0x1e98,6},{0x1e9e,4},{0x1ea2,2},{0x1ea4,2},{0x1ea6,4},{0x1eaa,4},{0x1eae,2},{0x1eb0,4},{0x1eb4,4},{0x1eb8,2},{0x1eba,4},{0x1ebe,4},{0x1ec2,4},{0x1ec6,2},{0x1ec8,4},{0x1ecc,4},{0x1ed0,2},{0x1ed2,2},{0x1ed4,2},{0x1ed6,2},{0x1ed8,2},{0x1eda,2},{0x1edc,2},{0x1ede,2},{0x1ee0,2},{0x1ee2,2},{0x1ee4,2},{0x1ee6,2},{0x1ee8,2},{0x1eea,2},{0x1eec,2},{0x1eee,2},{0x1ef0,2},{0x1ef2,2},{0x1ef4,4},{0x1ef8,4},{0x1efc,2},{0x1efe,2},{0x1f00,2},{0x1f02,6},{0x1f08,6},{0x1f0e,2},{0x1f10,2},{0x1f12,4},{0x1f16,6},{0x1f1c,6},{0x1f22,2},{0x1f24,2},{0x1f26,2},{0x1f28,4},{0x1f2c,2},{0x1f2e,4},{0x1f32,6},{0x1f38,2},{0x1f3a,2},{0x1f3c,2},{0x1f3e,4},{0x1f42,6},{0x1f48,2},{0x1f4a,4},{0x1f4e,4},{0x1f52,2},{0x1f54,4},{0x1f58,4},{0x1f5c,6},{0x1f62,2},{0x1f64,6},{0x1f6a,2},{0x1f6c,2},{0x1f6e,2},{0x1f70,4},{0x1f74,6},{0x1f7a,4},{0x1f7e,4},{0x1f82,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setKeyboardEventsPartialForTests(KeyboardEventsPartialTest value){partialTest=value;}
bool KeyboardEventsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1e16:kind=T::Kind::Transition0;break;case 0x1e18:kind=T::Kind::Transition1;break;case 0x1e1a:kind=T::Kind::Transition2;break;case 0x1e1c:kind=T::Kind::Transition3;break;case 0x1e1e:kind=T::Kind::Transition4;break;case 0x1e20:kind=T::Kind::Transition5;break;case 0x1e22:kind=T::Kind::Transition6;break;case 0x1e24:kind=T::Kind::Transition7;break;case 0x1e26:kind=T::Kind::Transition8;break;case 0x1e28:kind=T::Kind::Transition9;break;case 0x1e2a:kind=T::Kind::TransitionA;break;case 0x1e2c:kind=T::Kind::TransitionB;break;case 0x1e2e:kind=T::Kind::TransitionC;break;case 0x1e30:kind=T::Kind::TransitionD;break;case 0x1e32:kind=T::Kind::TransitionE;break;case 0x1e34:kind=T::Kind::TransitionF;break;case 0x1e36:kind=T::Kind::Press;break;case 0x1e46:kind=T::Kind::Release;break;case 0x1e56:kind=T::Kind::FixedOn;break;case 0x1e60:kind=T::Kind::FixedOff;break;case 0x1e6a:kind=T::Kind::Repeat;break;case 0x1e92:kind=T::Kind::ReleaseVelocity;break;case 0x1eb0:kind=T::Kind::Velocity;break;case 0x1efe:kind=T::Kind::Reset;break;case 0x1f12:kind=T::Kind::Pop;break;case 0x1f54:kind=T::Kind::Append;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native KeyboardEvents capacity exceeded");slot->emplace(*kind,h.getSP(),uint32_t(h.regs[3].er),uint16_t(h.regs[2].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[2].rh));const auto stage=task.stage();
 const unsigned address=stage==S::ReadPopEvent?0x156ec + task.consumer():stage==S::WriteAppendEvent?0x156ec + task.producer():task.record()+2;
 const bool wordRead=stage==S::ReadRepeatStart||stage==S::ReadVelocityStart||stage==S::ReadPopEvent;
 const bool wordWrite=stage==S::StorePressTick||stage==S::StoreReleaseTick||stage==S::WriteAppendEvent;
 const unsigned store=wordRead?(address<0x400000?3u+3u*(address&1):6u):wordWrite?byteClocks(address,h.lastwrite)+byteClocks(address+1,int(address)):byteClocks(task.record(),h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field)->uint16_t{switch(field){case T::Read::Tick:return uint16_t(h.read16(0xffee76));case T::Read::Start:return uint16_t(h.read16(task.record()+2));case T::Read::Flags:return uint8_t(h.read8(task.record()));case T::Read::Key:return uint8_t(h.read8(task.record()+1));case T::Read::Producer:return uint16_t(h.read16(0x156e8));case T::Read::Consumer:return uint16_t(h.read16(0x156ea));case T::Read::Event:return uint16_t(h.read16(0x156ec + task.consumer()));}return 0;};
 auto write=[&](T::Write field,uint16_t value){if(field==T::Write::Flags)h.write8(int8_t(value),task.record());else f.store16(field==T::Write::Start?task.record()+2:field==T::Write::Producer?0x156e8:field==T::Write::Consumer?0x156ea:0x156ec + task.producer(),int16_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native KeyboardEvents admission differs");
 switch(stage){case S::Transition0:f.ret();break;
case S::Transition2:f.ret();break;
case S::Transition5:f.ret();break;
case S::Transition6:f.ret();break;
case S::Transition8:f.ret();break;
case S::Transition9:f.ret();break;
case S::TransitionA:f.ret();break;
case S::TransitionB:f.ret();break;
case S::TransitionE:f.ret();break;
case S::TransitionF:f.ret();break;
case S::ReadPressTick:f.move(r[2].r,task.tick());break;
case S::ReturnPress:f.ret();break;
case S::ReadReleaseTick:f.move(r[2].r,task.tick());break;
case S::ReturnRelease:f.ret();break;
case S::ReadFixedOnKey:f.move(r[2].rl,task.key());break;
case S::BindFixedOnVelocity:f.move(r[2].rh,0x7f);break;
case S::AppendFixedOn:h.clockI(2);break;
case S::ReadFixedOffKey:f.move(r[2].rl,task.key());break;
case S::BindFixedOffVelocity:f.move(r[2].rh,0xffffffff);break;
case S::AppendFixedOff:h.clockI(2);break;
case S::ReadRepeatFlags:flag(h,4,(task.flags()&2)==0);break;
case S::ReadRepeatTick:f.move(r[2].r,task.tick());break;
case S::ReadRepeatStart:f.move(r[2].e,task.start());break;
case S::ElapsedRepeat:r[2].r=h.sub16(r[2].r,r[2].e);break;
case S::BiasRepeat:r[2].r=h.sub16(r[2].r,0xa);break;
case S::IncrementRepeat:f.inc(r[2].r,1);break;
case S::CalculateRepeat:f.call(0x1eb0);break;
case S::MarkRepeat:r[2].rh|=(1u<<0x7);break;
case S::ReadRepeatKey:f.move(r[2].rl,task.key());break;
case S::AppendRepeat:h.clockI(2);break;
case S::ReturnRepeat:f.ret();break;
case S::ReadReleaseFlags:flag(h,4,(task.flags()&2)==0);break;
case S::ReadVelocityTick:f.move(r[2].r,task.tick());break;
case S::ReadVelocityStart:f.move(r[2].e,task.start());break;
case S::ElapsedVelocity:r[2].r=h.sub16(r[2].r,r[2].e);break;
case S::CalculateRelease:f.call(0x1eb0);break;
case S::ReadReleaseKey:f.move(r[2].rl,task.key());break;
case S::AppendRelease:h.clockI(2);break;
case S::ReturnReleaseVelocity:f.ret();break;
case S::SaveVelocityValue:f.push(r[0].er);break;
case S::SaveVelocityIndex:f.push(r[1].er);break;
case S::RetainDelay:f.move(r[1].r,r[2].r);break;
case S::BindSlowPeriod:f.move(r[2].e,0x78);break;
case S::BindSlowBias:f.move(r[2].r,0x3);break;
case S::ReadVelocityFlags:flag(h,4,(task.flags()&1)==0);break;
case S::BindFastPeriod:f.move(r[2].e,0x1e);break;
case S::BindFastBias:f.move(r[2].r,0x2);break;
case S::SubtractDelayBias:r[1].r=h.sub16(r[1].r,r[2].r);break;
case S::ClearDelayFloor:r[1].r=h.sub16(r[1].r,r[1].r);break;
case S::ClearNumeratorLow:r[2].r=h.sub16(r[2].r,r[2].r);break;
case S::SquareDelay:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].r));h.clockI(20);break;
case S::BindDenominatorBias:f.move(r[0].r,r[2].e);break;
case S::NormalizeDenominatorBias:r[0].er&=0xffff;value(h,r[0].er);break;
case S::AddDenominatorBias:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::HalfNumerator:f.shift(r[2].er,1,false);break;
case S::TestDenominatorHigh:f.move(r[1].e,r[1].e);break;
case S::ShiftDenominator:f.shift(r[1].er,1,false);break;
case S::ShiftNumerator:f.shift(r[2].er,1,false);break;
case S::DivideVelocity:f.divideUnsigned(r[1].r,r[2].er);break;
case S::IncrementVelocity:f.inc(r[2].rh,1);break;
case S::ClampVelocity:f.move(r[2].rh,0x7f);break;
case S::RestoreVelocityIndex:f.pop(r[1].er);break;
case S::RestoreVelocityValue:f.pop(r[0].er);break;
case S::ReturnVelocity:f.ret();break;
case S::SaveResetEvent:f.push(r[6].r);break;
case S::ClearResetEvent:r[6].r=h.sub16(r[6].r,r[6].r);break;
case S::RestoreResetEvent:f.pop(r[6].r);break;
case S::ReturnReset:f.ret();break;
case S::SavePopValue:f.push(r[0].er);break;
case S::ReadPopConsumer:f.move(r[0].r,task.consumer());break;
case S::ReadPopProducer:f.move(r[0].e,task.producer());break;
case S::ComparePopIndices:h.sub16(r[0].r,r[0].e);break;
case S::ClearPopEvent:r[6].r=h.sub16(r[6].r,r[6].r);break;
case S::RestoreEmptyPopValue:f.pop(r[0].er);break;
case S::ReturnEmptyPop:f.ret();break;
case S::SavePopIndex:f.push(r[1].er);break;
case S::BindPopBase:f.move(r[1].er,0x156ec);break;
case S::NormalizePopIndex:r[0].er&=0xffff;value(h,r[0].er);break;
case S::LocatePopEvent:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::IncrementPopConsumer:f.inc(r[0].r,2);break;
case S::WrapPopConsumer:r[0].r&=0x1fe;value(h,r[0].r);break;
case S::ReadPopEvent:f.move(r[6].r,task.message());break;
case S::RestorePopIndex:f.pop(r[1].er);break;
case S::RestorePopValue:f.pop(r[0].er);break;
case S::ReturnPop:f.ret();break;
case S::SaveAppendIndex:f.push(r[1].er);break;
case S::SaveAppendValue:f.push(r[0].er);break;
case S::ReadAppendProducer:f.move(r[0].r,task.producer());break;
case S::NormalizeAppendIndex:r[0].er&=0xffff;value(h,r[0].er);break;
case S::BindAppendBase:f.move(r[1].er,0x156ec);break;
case S::LocateAppendEvent:r[1].er=h.add32(r[1].er,r[0].er);break;
case S::IncrementAppendProducer:f.inc(r[0].r,2);break;
case S::WrapAppendProducer:r[0].r&=0x1fe;value(h,r[0].r);break;
case S::RestoreAppendValue:f.pop(r[0].er);break;
case S::RestoreAppendIndex:f.pop(r[1].er);break;
case S::ReturnAppend:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native KeyboardEvents clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
