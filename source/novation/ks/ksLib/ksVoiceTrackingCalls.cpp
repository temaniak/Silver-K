#include "ksVoiceTrackingCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
VoiceTrackingPartialTest partialTest=nullptr;using S=VoiceTrackingTask::Stage;using K=VoiceTrackingTask::Kind;using W=VoiceTrackingTask::Write;
struct Point {unsigned location,length;};
Point point(const VoiceTrackingTask& task){
 if(task.kind()==K::Observe){if(task.complete())return {task.sent()?0xa1f8u:0xa1e0u,2};switch(task.stage()){
 case S::CheckPatch:return {0xa1b0,6};
 case S::AdmitPatch:return {0xa1b6,2};
 case S::ReadPart:return {0xa1b8,6};
 case S::ReadActive:return {0xa1be,6};
 case S::ComparePart:return {0xa1c4,2};
 case S::AdmitPart:return {0xa1c6,2};
 case S::ReadVoice:return {0xa1c8,6};
 case S::ReadTracked:return {0xa1ce,6};
 case S::CompareVoice:return {0xa1d4,2};
 case S::AdmitVoice:return {0xa1d6,2};
 case S::Invalidate:return {0xa1d8,2};
 case S::PublishInvalid:return {0xa1da,6};
 case S::SetAbsoluteOffset:return {0xa1e2,4};
 case S::ReadSendVoice:return {0xa1e6,6};
 case S::PublishVoice:return {0xa1ec,6};
 case S::ClearAbsoluteHigh:return {0xa1f2,2};
 case S::DispatchAbsolute:return {0xa1f4,4};
 case S::AwaitAbsolute:return {0xa1f8,2};
 default:break;}}
 if(task.kind()==K::Auxiliary){if(task.complete())return {0xa206,2};switch(task.stage()){
 case S::SetAbsoluteOffset:return {0xa1fa,4};
 case S::ReadAuxiliary:return {0xa1fe,2};
 case S::ClearAbsoluteHigh:return {0xa200,2};
 case S::DispatchAbsolute:return {0xa202,4};
 case S::AwaitAbsolute:return {0xa206,2};
 default:break;}}
 if(task.kind()==K::Refresh){const bool second=task.effect()!=0;switch(task.stage()){
 case S::ReadPart:return {0xa208,6};
 case S::ReadActive:return {0xa20e,6};
 case S::ComparePart:return {0xa214,2};
 case S::AdmitPart:return {0xa216,4};
 case S::ReadRefreshTracked:return {0xa21a,6};
 case S::AdmitTracked:return {0xa220,4};
 case S::PublishIndex:return {0xa224,6};
 case S::DispatchSelect:return {0xa22a,4};
 case S::ReadPatch:return {0xa22e,8};
 case S::AwaitSelect:return {0xa22e,8};
 case S::AwaitSecond:return {second?0xa336u:0xa2b6u,second?2u:8u};
 case S::Complete:return {0xa336,2};
 case S::ReadSample:return {second?0xa2b6u:0xa236u,8};
 case S::AdmitSample:return {second?0xa2beu:0xa23eu,2};
 case S::DecrementSample:return {second?0xa2cau:0xa24au,2};
 case S::ShiftSampleOne:return {second?0xa2ccu:0xa24cu,2};
 case S::ShiftSampleTwo:return {second?0xa2ceu:0xa24eu,2};
 case S::NarrowSampleWord:return {second?0xa2d0u:0xa250u,2};
 case S::NarrowSampleAddress:return {second?0xa2d2u:0xa252u,2};
 case S::LocateTable:return {second?0xa2d4u:0xa254u,6};
 case S::ReadTable:return {second?0xa2dau:0xa25au,4};
 case S::JumpSecond:return {second?0xa2deu:0xa25eu,2};
 case S::ReadTime:return {second?0xa2e0u:0xa260u,8};
 case S::SetTimeLow:return {second?0xa2e8u:0xa268u,2};
 case S::RetainTime:return {second?0xa2eau:0xa26au,2};
 case S::HalfTime:return {second?0xa2ecu:0xa26cu,2};
 case S::RateMultiplier:return {second?0xa2eeu:0xa26eu,4};
 case S::MultiplyRate:return {second?0xa2f2u:0xa272u,2};
 case S::CaptureRate:return {second?0xa2f4u:0xa274u,2};
 case S::BiasRate:return {second?0xa2f6u:0xa276u,4};
 case S::ReadBase:return {second?0xa2fau:0xa27au,6};
 case S::BiasBase:return {second?0xa300u:0xa280u,4};
 case S::AddBase:return {second?0xa304u:0xa284u,2};
 case S::DispatchCoefficient:return {second?0xa306u:0xa286u,4};
 case S::RestoreTime:return {second?0xa30au:0xa28au,2};
 case S::CompareScale:return {second?0xa30cu:0xa28cu,2};
 case S::AdmitScale:return {second?0xa30eu:0xa28eu,2};
 case S::CapScale:return {second?0xa310u:0xa290u,2};
 case S::ShiftScaleOne:return {second?0xa312u:0xa292u,2};
 case S::ShiftScaleTwo:return {second?0xa314u:0xa294u,2};
 case S::CaptureCoefficientHigh:return {second?0xa316u:0xa296u,2};
 case S::MultiplyHigh:return {second?0xa318u:0xa298u,2};
 case S::MultiplyLow:return {second?0xa31au:0xa29au,2};
 case S::CaptureLowHigh:return {second?0xa31cu:0xa29cu,2};
 case S::ClearLowHigh:return {second?0xa31eu:0xa29eu,2};
 case S::AddProducts:return {second?0xa320u:0xa2a0u,2};
 case S::DoublePacked:return {second?0xa322u:0xa2a2u,2};
 case S::ZeroSecondWord:return {second?0xa32cu:0xa2acu,2};
 case S::SetSecondOffset:return {second?0xa32eu:0xa2aeu,4};
 case S::DispatchSecond:return {second?0xa332u:0xa2b2u,4};
 case S::ZeroFirstWord:return {second?0xa2c0u:0xa240u,2};
 case S::SetFirstOffset:return {second?(task.sample()?0xa2c2u:0xa324u):(task.sample()?0xa242u:0xa2a4u),4};
 case S::DispatchFirst:return {second?(task.sample()?0xa2c6u:0xa328u):(task.sample()?0xa246u:0xa2a8u),4};
 case S::AwaitFirst:return {second?(task.sample()?0xa2cau:0xa32cu):(task.sample()?0xa24au:0xa2acu),2};
 case S::AwaitCoefficient:return {second?0xa30au:0xa28au,2};
 default:break;}}
 throw std::runtime_error("Invalid native tracking stage");}
unsigned outputAddress(W what){return what==W::Tracked?0x1755e:0x174a7;}
}
void setVoiceTrackingPartialForTests(VoiceTrackingPartialTest fn){partialTest=fn;}
bool VoiceTrackingCalls::step(ControllerCpu& h){using namespace controllerDetail;using C=VoiceTrackingTask::Child;
 const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0xa1b0||pc==0xa1fa||pc==0xa208){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native tracking capacity exceeded");selected->emplace(pc==0xa1b0?K::Observe:pc==0xa1fa?K::Auxiliary:K::Refresh,uint32_t(h.regs[6].er)<0x839000,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;auto& v=h.regs;const auto child=task.child();
 if(child!=C::None)task.resumeChild(child==C::Coefficient?uint32_t(v[0].er):0);
 const auto stage=task.stage();const auto before=h.cycles;Boundary clocks(h,pc,point(task).length);
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 auto timing=[&](W what){const auto address=outputAddress(what);return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};
 const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind()):3u);
 auto read=[&](VoiceTrackingTask::Read what)->uint32_t{using R=VoiceTrackingTask::Read;switch(what){
 case R::Part:return uint8_t(h.read8(0x17526));case R::ActivePart:return uint8_t(h.read8(0x16798));case R::Voice:return uint8_t(h.read8(0x174a7));
 case R::Tracked:return uint8_t(h.read8(0x1755e));case R::Auxiliary:return uint8_t(v[6].rh);
 case R::PatchReference:call.patch=uint32_t(h.read32(0x167bc));return 0;
 case R::Sample:return uint8_t(h.read8(int32_t(call.patch+(task.effect()?76:73))));
 case R::Time:return uint8_t(h.read8(int32_t(call.patch+(task.effect()?75:72))));case R::Base:return uint16_t(h.read16(0x15138));
 case R::Table:return uint32_t(h.read32(0x159f4 + task.sampleIndex()));}return 0;};
 auto write=[&](W what,uint32_t data){h.write8(int8_t(data),outputAddress(what));};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native tracking admission mismatch");
 auto dispatch=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);if(h.cycles-before!=cost)throw std::runtime_error("Native tracking dispatch clocks differ");};
 switch(stage){case S::CheckPatch:h.sub32(v[6].er,0x839000);break;
 case S::ReadPart:v[0].rl=int8_t(task.part());value(h,v[0].rl);break;case S::ReadActive:v[0].rh=int8_t(task.activePart());value(h,v[0].rh);break;
 case S::ComparePart:h.sub8(v[0].rh,v[0].rl);break;case S::AdmitPart:if(task.kind()==K::Refresh)h.clockI(2);break;
 case S::ReadVoice:v[0].rl=int8_t(task.voice());value(h,v[0].rl);break;case S::ReadTracked:v[0].rh=int8_t(task.tracked());value(h,v[0].rh);break;
 case S::CompareVoice:h.sub8(v[0].rh,v[0].rl);break;case S::Invalidate:v[0].rl=-1;value(h,v[0].rl);break;case S::PublishInvalid:value(h,-1);break;
 case S::SetAbsoluteOffset:v[4].r=int16_t(task.absoluteOffset());value(h,v[4].r);break;case S::ReadSendVoice:v[1].rh=int8_t(task.voice());value(h,v[1].rh);break;
 case S::PublishVoice:value(h,int8_t(task.voice()));break;case S::ReadAuxiliary:v[1].rh=int8_t(task.auxiliary());value(h,v[1].rh);break;
 case S::ClearAbsoluteHigh:v[1].e=h.sub16(v[1].e,v[1].e);break;case S::DispatchAbsolute:dispatch(0x24be);return true;
 case S::ReadRefreshTracked:v[0].rl=int8_t(task.tracked());value(h,v[0].rl);break;case S::AdmitTracked:h.clockI(2);break;
 case S::PublishIndex:value(h,int8_t(task.tracked()));break;case S::DispatchSelect:dispatch(0x77f4);return true;
 case S::ReadPatch:v[6].er=int32_t(call.patch);value(h,v[6].er);break;case S::ReadSample:v[2].rl=int8_t(task.sample());value(h,v[2].rl);break;
 case S::ZeroFirstWord:case S::ZeroSecondWord:v[1].er=h.sub32(v[1].er,v[1].er);break;
 case S::SetFirstOffset:v[4].r=int16_t(task.firstOffset());value(h,v[4].r);break;case S::SetSecondOffset:v[4].r=int16_t(task.secondOffset());value(h,v[4].r);break;
 case S::DispatchFirst:case S::DispatchSecond:dispatch(0x785a);return true;
 case S::DecrementSample:clocks.inc(v[2].rl,-1);break;case S::ShiftSampleOne:case S::ShiftSampleTwo:clocks.shift(v[2].rl,1,true);break;
 case S::NarrowSampleWord:v[2].r&=0xff;value(h,v[2].r);break;case S::NarrowSampleAddress:v[2].er&=0xffff;value(h,v[2].er);break;
 case S::LocateTable:v[2].er=h.add32(v[2].er,0x159f4);break;case S::ReadTable:v[1].er=int32_t(task.packed());value(h,v[1].er);break;
 case S::ReadTime:v[0].rh=int8_t(task.time());value(h,v[0].rh);break;case S::SetTimeLow:v[0].rl=0;value(h,v[0].rl);break;
 case S::RetainTime:clocks.push(v[0].r);call.depth+=2;break;case S::HalfTime:clocks.shift(v[0].r,1,false);break;
 case S::RateMultiplier:v[0].e=int16_t(0xaaab);value(h,v[0].e);break;
 case S::MultiplyRate:v[0].er=int32_t(uint16_t(v[0].er)*uint32_t(uint16_t(v[0].e)));h.clockI(20);break;
 case S::CaptureRate:v[0].r=v[0].e;value(h,v[0].r);break;case S::BiasRate:v[0].r=h.add16(v[0].r,0x4700);break;
 case S::ReadBase:v[0].e=int16_t(task.base());value(h,v[0].e);break;case S::BiasBase:v[0].e=h.sub16(v[0].e,0x4470);break;
 case S::AddBase:v[0].r=h.add16(v[0].r,v[0].e);break;case S::DispatchCoefficient:dispatch(0xa69e);return true;
 case S::RestoreTime:clocks.pop(v[2].r);call.depth-=2;break;case S::CompareScale:h.sub8(v[2].rh,0x20);break;case S::CapScale:v[2].rh=0x20;value(h,v[2].rh);break;
 case S::ShiftScaleOne:case S::ShiftScaleTwo:clocks.shift(v[2].rh,1,true);break;case S::CaptureCoefficientHigh:v[1].r=v[0].e;value(h,v[1].r);break;
 case S::MultiplyHigh:v[1].er=int32_t(task.highProduct());h.clockI(20);break;case S::MultiplyLow:v[0].er=int32_t(task.lowProduct());h.clockI(20);break;
 case S::CaptureLowHigh:v[0].r=v[0].e;value(h,v[0].r);break;case S::ClearLowHigh:v[0].e=h.sub16(v[0].e,v[0].e);break;
 case S::AddProducts:v[1].er=h.add32(v[1].er,v[0].er);break;case S::DoublePacked:clocks.shift(v[1].er,1,true);break;default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native tracking bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
