#pragma once
#include "kstationTimer1Service.h"
#include "kstationHdiWait.h"
#include <array>
#include <stdexcept>
namespace kstation {
// Temporary caller projection for the closed Timer1/encoder/panel graph.
// Only typed peripheral service continuations drive native control flow.
class NativeTimer1 final {
 using S=Timer1Service::Stage;using R=Timer1Service::Read;using W=Timer1Service::Write;
public:
 struct CodeRange{uint32_t begin,end;};
 inline static constexpr std::array<CodeRange,7> CodeRanges{{{28*4,28*4+4},{0x968,0x9aa},{0x1bd0,0x1e1c},{0x1e72,0x1ea4},{0x2186,0x21f0},{0x2284,0x22c0},{0x22fc,0x2312}}};
 struct Result{bool stepped=false,entered=false,returned=false;};
 static bool candidate(uint32_t pc,uint64_t pending){return pending||owned(pc);}
 bool active()const{return depth_!=0;}void reset(){depth_=0;}
 Result step(Stage1Cpu& c){
  if(c.interruptMode2||c.pending_irqs_high)return {};bool entered=false;
  if(c.pending_irqs&&!(c.ccr&0x80)){if((c.pending_irqs&((uint64_t(1)<<28)-1))||!(c.pending_irqs&(uint64_t(1)<<28)))return {};if(depth_==jobs_.size())throw std::logic_error("Native K Timer1 capacity");entered=true;}
  else if(!depth_)return {};else if(uint32_t(c.getPC())!=boundary(jobs_[depth_-1])){if(owned(uint32_t(c.getPC())))throw std::logic_error("Native K Timer1 caller differs");return {};}
  if(c.pending_irqs){const bool ue=(c.read8(0xfffff2)&8)!=0;if(entered){c.pending_irqs&=~(uint64_t(1)<<28);const auto saved=(uint32_t(c.getPC())&0xffffff)|(uint32_t(c.ccr)<<24);const auto sp=c.getSP()-4;c.write32(int32_t(saved),int(sp));c.setSP(sp);c.clockI(4);c.ccr|=0x80;if(!ue)c.ccr|=0x10;if((c.read32(28*4)&0xffffff)!=0x968)throw std::logic_error("Native K Timer1 vector changed");c.pc=c.makepc(0x968);c.irqDepth(c.irqDepth()+1);jobs_[depth_++]=Timer1Service{};}}
  auto&t=jobs_[depth_-1];if(t.awaitingClock()){if(uint32_t(c.getPC())!=0x98a)throw std::logic_error("Native K Timer1 clock resume differs");t.resumeClock();}const auto s=t.stage();const uint32_t at=uint32_t(c.getPC());if(at!=boundary(t))throw std::logic_error("Native K Timer1 boundary mismatch");const unsigned len=bytes(s);
  auto credits=timing(c,t,len);const auto cost=t.nextClocks(credits);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));project(c,t,at,len);
  const auto used=t.advanceBefore(cost,credits,[&](R field,uint32_t ref){return read(c,field,ref);},[&](W field,uint32_t ref,uint32_t value){write(c,field,ref,value);});
  if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K Timer1 phase cost mismatch");
  const bool done=t.complete();if(done)--depth_;else {const auto next=boundary(t);if(s==S::DispatchClock)c.pc=c.makepc(0x1970);else {if(isReturn(s)||(s==S::Transition&&transitionReturnBefore(s,at))){if(uint32_t(c.getPC())!=next)throw std::logic_error("Native K Timer1 child return differs");}c.pc=c.makepc(int(next));}}
  return {true,entered,done};
 }
private:
 static bool owned(uint32_t pc){for(auto r:CodeRanges)if(pc>=r.begin&&pc<r.end)return true;return false;}
 static bool transitionReturnBefore(S,uint32_t at){const unsigned n=(at-0x1d34)/2;return n==0||n==2||n==5||n==6||n==8||n==9||n==10||n==11||n==14||n==15;}
 static unsigned bus(uint32_t at,unsigned count,int last){unsigned cost=0;for(unsigned n=0;n<count;++n){const auto a=(at+n)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
 static Timer1Service::Credits timing(const Stage1Cpu& c,const Timer1Service&t,unsigned len){Timer1Service::Credits v;const auto history=int(uint32_t(c.getPC())+len-1);switch(t.stage()){
 case S::RetainTimer:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadTimerStatus:v.read=bus(0xffff71,1,history);break;
 case S::PublishTimerStatus:v.write=bus(0xffff71,1,c.writeHistory());break;
 case S::DispatchScan:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadDivider:v.read=bus(0xfffd2a,1,history);break;
 case S::PublishDivider:v.write=bus(0xfffd2a,1,c.writeHistory());break;
 case S::DispatchClock:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadTick:v.read=bus(0xfffd20,4,history);break;
 case S::PublishTick:v.write=bus(0xfffd20,4,c.writeHistory());break;
 case S::DispatchPanel:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::RestoreTimer:v.stack=8+bus(c.getSP(),4,history);break;
 case S::FinishTimer:v.stack=5+bus(c.getSP(),4,history);break;
 case S::SaveScanIndex:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::SaveScanContacts:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::SaveScanRecord:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadScanTick:v.read=bus(0xfffe86,2,history);break;
 case S::PublishScanTick:v.write=bus(0xfffe86,2,c.writeHistory());break;
 case S::ReadPins:v.read=bus(0xffffd7,1,history);break;
 case S::PublishMux:v.write=bus(0xffffd6,1,c.writeHistory());break;
 case S::ReadHistory:v.read=bus(t.history(),1,history);break;
 case S::RestoreScanRecord:v.stack=8+bus(c.getSP(),4,history);break;
 case S::RestoreScanContacts:v.stack=8+bus(c.getSP(),4,history);break;
 case S::RestoreScanIndex:v.stack=8+bus(c.getSP(),4,history);break;
 case S::ReturnScan:v.stack=5+bus(c.getSP(),4,history);break;
 case S::DispatchChanges:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::PublishHistory:v.write=bus(t.history(),1,c.writeHistory());break;
 case S::SaveTransitionIndex:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::DispatchTransition:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::RestoreTransitionIndex:v.stack=8+bus(c.getSP(),4,history);break;
 case S::ReturnChanges:v.stack=5+bus(c.getSP(),4,history);break;
 case S::Transition:v.stack=5+bus(c.getSP(),4,history);break;
 case S::ReadPressTick:v.read=bus(0xfffe86,2,history);break;
 case S::StorePressTick:v.write=bus(t.record()+2,2,c.writeHistory());break;
 case S::SetPressed:v.read=bus(t.record(),1,history);v.write=bus(t.record(),1,c.writeHistory());break;
 case S::ReturnPress:v.stack=5+bus(c.getSP(),4,history);break;
 case S::ReadReleaseTick:v.read=bus(0xfffe86,2,history);break;
 case S::StoreReleaseTick:v.write=bus(t.record()+2,2,c.writeHistory());break;
 case S::ClearPressed:v.read=bus(t.record(),1,history);v.write=bus(t.record(),1,c.writeHistory());break;
 case S::ReturnRelease:v.stack=5+bus(c.getSP(),4,history);break;
 case S::ReadFixedOnKey:v.read=bus(t.record()+1,1,history);break;
 case S::ReadFixedOffKey:v.read=bus(t.record()+1,1,history);break;
 case S::ReadRepeatFlags:v.read=bus(t.record(),1,history);break;
 case S::ReadRepeatTick:v.read=bus(0xfffe86,2,history);break;
 case S::ReadRepeatStart:v.read=bus(t.record()+2,2,history);break;
 case S::CalculateRepeat:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadRepeatKey:v.read=bus(t.record()+1,1,history);break;
 case S::ReturnRepeat:v.stack=5+bus(c.getSP(),4,history);break;
 case S::ReadReleaseFlags:v.read=bus(t.record(),1,history);break;
 case S::ReadVelocityTick:v.read=bus(0xfffe86,2,history);break;
 case S::ReadVelocityStart:v.read=bus(t.record()+2,2,history);break;
 case S::CalculateRelease:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadReleaseKey:v.read=bus(t.record()+1,1,history);break;
 case S::ReturnReleaseSpeed:v.stack=5+bus(c.getSP(),4,history);break;
 case S::SaveSpeedValue:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::SaveSpeedIndex:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadSpeedFlags:v.read=bus(t.record(),1,history);break;
 case S::RestoreSpeedIndex:v.stack=8+bus(c.getSP(),4,history);break;
 case S::RestoreSpeedValue:v.stack=8+bus(c.getSP(),4,history);break;
 case S::ReturnSpeed:v.stack=5+bus(c.getSP(),4,history);break;
 case S::SaveAppendIndex:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::SaveAppendValue:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReadProducer:v.read=bus(0xc6aa,2,history);break;
 case S::PublishEvent:v.write=bus(t.queueRef(),2,c.writeHistory());break;
 case S::PublishProducer:v.write=bus(0xc6aa,2,c.writeHistory());break;
 case S::RestoreAppendValue:v.stack=8+bus(c.getSP(),4,history);break;
 case S::RestoreAppendIndex:v.stack=8+bus(c.getSP(),4,history);break;
 case S::ReturnAppend:v.stack=5+bus(c.getSP(),4,history);break;
 case S::ReadPanelCount:v.read=bus(0xfffe8f+t.panel(),1,history);break;
 case S::PublishPanelCount:v.write=bus(0xfffe8f+t.panel(),1,c.writeHistory());break;
 case S::RetainPanelPort:v.stack=5+bus(c.getSP()-2,2,c.writeHistory());break;
 case S::ReadPanelPort:v.read=bus(0xffffd2,1,history);break;
 case S::ReadPanelMirror:v.read=bus(0xfffe8e,1,history);break;
 case S::PublishPanelPort:v.write=bus(0xffffd2,1,c.writeHistory());break;
 case S::RestorePanelPort:v.stack=5+bus(c.getSP(),2,history);break;
 case S::RetainPanelCommand:v.stack=5+bus(c.getSP()-2,2,c.writeHistory());break;
 case S::DispatchPanelCommand:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::RestorePanelCommand:v.stack=5+bus(c.getSP(),2,history);break;
 case S::ReturnPanel:v.stack=5+bus(c.getSP(),4,history);break;
 case S::RetainCommand:v.stack=5+bus(c.getSP()-2,2,c.writeHistory());break;
 case S::DispatchReadyPoll:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::RestoreCommand:v.stack=5+bus(c.getSP(),2,history);break;
 case S::DispatchCommandWrite:v.stack=8+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::ReturnCommand:v.stack=5+bus(c.getSP(),4,history);break;
 case S::PollFirstRead:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::PollFinalRead:v.stack=3+bus(c.getSP()-4,4,c.writeHistory());break;
 case S::PollReturn:v.stack=5+bus(c.getSP(),4,history);break;
 case S::RetainRead:v.stack=5+bus(c.getSP()-2,2,c.writeHistory());break;
 case S::ReadDirection:v.write=bus(0xffffc5,1,c.writeHistory());break;
 case S::ReadSelect:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::ReadEnable:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::ReadLcdData:v.read=bus(0xffffc7,1,history);break;
 case S::ReadFinish:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::ReadRestoreDirection:v.write=bus(0xffffc5,1,c.writeHistory());break;
 case S::RestoreRead:v.stack=5+bus(c.getSP(),2,history);break;
 case S::ReturnRead:v.stack=5+bus(c.getSP(),4,history);break;
 case S::RetainWrite:v.stack=5+bus(c.getSP()-2,2,c.writeHistory());break;
 case S::WriteSelect:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::WriteEnable:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::WriteLcdData:v.write=bus(0xffffc7,1,c.writeHistory());break;
 case S::WriteFinish:v.write=bus(0xffffd3,1,c.writeHistory());break;
 case S::RestoreWrite:v.stack=5+bus(c.getSP(),2,history);break;
 case S::ReturnWrite:v.stack=5+bus(c.getSP(),4,history);break;
 default:break;}return v;}
 static unsigned bytes(S s){switch(s){
case S::RetainTimer: case S::DispatchScan: case S::DispatchClock: case S::DispatchPanel: case S::RestoreTimer: case S::SaveScanIndex: case S::SaveScanContacts: case S::SaveScanRecord: case S::RestoreScanRecord: case S::RestoreScanContacts: case S::RestoreScanIndex: case S::SaveTransitionIndex: case S::RestoreTransitionIndex: case S::StorePressTick: case S::SetPressed: case S::StoreReleaseTick: case S::ClearPressed: case S::ReadFixedOnKey: case S::AppendFixedOn: case S::ReadFixedOffKey: case S::AppendFixedOff: case S::ReadRepeatFlags: case S::ReadRepeatStart: case S::BiasRepeat: case S::ReadRepeatKey: case S::AppendRepeat: case S::ReadReleaseFlags: case S::ReadVelocityStart: case S::ReadReleaseKey: case S::AppendRelease: case S::SaveSpeedValue: case S::SaveSpeedIndex: case S::BindSlowPeriod: case S::BindSlowBias: case S::ReadSpeedFlags: case S::BindFastPeriod: case S::BindFastBias: case S::RestoreSpeedIndex: case S::RestoreSpeedValue: case S::SaveAppendIndex: case S::SaveAppendValue: case S::MaskProducer: case S::RestoreAppendValue: case S::RestoreAppendIndex: case S::DispatchPanelCommand: case S::DispatchCommandWrite:return 4;
case S::ReadDivider: case S::PublishDivider: case S::ReadScanTick: case S::PublishScanTick: case S::BindHistory: case S::BindChangedRecord: case S::BindTransition: case S::ReadPressTick: case S::ReadReleaseTick: case S::ReadRepeatTick: case S::ReadVelocityTick: case S::ReadProducer: case S::BindQueue: case S::PublishProducer: case S::ReadPanelCount: case S::PublishPanelCount: case S::ReadPanelMirror:return 6;
case S::ReadTick: case S::PublishTick:return 8;
 default:return 2;}}
 static uint32_t boundary(const Timer1Service&t){switch(t.stage()){
 case S::RetainTimer:return 0x968;
 case S::ReadTimerStatus:return 0x96c;
 case S::ClearTimerValue:return 0x96e;
 case S::PublishTimerStatus:return 0x970;
 case S::DispatchScan:return 0x972;
 case S::ReadDivider:return 0x976;
 case S::AdvanceDivider:return 0x97c;
 case S::PublishDivider:return 0x97e;
 case S::ChooseClock:return 0x984;
 case S::DispatchClock:return 0x986;
 case S::ReadTick:return 0x98a;
 case S::IncrementTick:return 0x992;
 case S::PublishTick:return 0x994;
 case S::MaskTick:return 0x99c;
 case S::ChoosePanel:return 0x99e;
 case S::DispatchPanel:return 0x9a0;
 case S::RestoreTimer:return 0x9a4;
 case S::FinishTimer:return 0x9a8;
 case S::SaveScanIndex:return 0x1bd0;
 case S::SaveScanContacts:return 0x1bd4;
 case S::SaveScanRecord:return 0x1bd8;
 case S::ReadScanTick:return 0x1bdc;
 case S::IncrementScanTick:return 0x1be2;
 case S::PublishScanTick:return 0x1be4;
 case S::BindHistory:return 0x1bea;
 case S::ReadPins:return 0x1bf0 + 12*t.page();
 case S::SelectMux:return 0x1bf2 + 12*t.page();
 case S::PublishMux:return 0x1bf4 + 12*t.page();
 case S::ReadHistory:return 0x1bf6 + 12*t.page();
 case S::ComparePins:return 0x1bf8 + 12*t.page();
 case S::ChooseChanges:return 0x1bfa + 12*t.page();
 case S::RestoreScanRecord:return 0x1c50;
 case S::RestoreScanContacts:return 0x1c54;
 case S::RestoreScanIndex:return 0x1c58;
 case S::ReturnScan:return 0x1c5c;
 case S::BindChangedRecord:return 0x1c5e + 10*t.page();
 case S::DispatchChanges:return 0x1c64 + 10*t.page();
 case S::ResumeScan:return 0x1c66 + 10*t.page();
 case S::PublishHistory:return 0x1cae;
 case S::RestoreHistoryCursor:return 0x1cb0;
 case S::SaveTransitionIndex:return 0x1cb2;
 case S::ClearTransition:return 0x1cb6 + 30*t.quad() + 0;
 case S::ShiftCurrentFirst:return 0x1cb6 + 30*t.quad() + 2;
 case S::AppendCurrentFirst:return 0x1cb6 + 30*t.quad() + 4;
 case S::ShiftCurrentSecond:return 0x1cb6 + 30*t.quad() + 6;
 case S::AppendCurrentSecond:return 0x1cb6 + 30*t.quad() + 8;
 case S::ShiftPreviousFirst:return 0x1cb6 + 30*t.quad() + 10;
 case S::AppendPreviousFirst:return 0x1cb6 + 30*t.quad() + 12;
 case S::ShiftPreviousSecond:return 0x1cb6 + 30*t.quad() + 14;
 case S::AppendPreviousSecond:return 0x1cb6 + 30*t.quad() + 16;
 case S::ScaleTransition:return 0x1cb6 + 30*t.quad() + 18;
 case S::BindTransition:return 0x1cb6 + 30*t.quad() + 20;
 case S::DispatchTransition:return 0x1cd0 + 30*t.quad();
 case S::NextTransitionRecord:return 0x1cd2 + 30*t.quad();
 case S::RestoreTransitionIndex:return 0x1d2e;
 case S::ReturnChanges:return 0x1d32;
 case S::Transition:return 0x1d34 + 2*t.transition();
 case S::ReadPressTick:return 0x1d54;
 case S::StorePressTick:return 0x1d5a;
 case S::SetPressed:return 0x1d5e;
 case S::ReturnPress:return 0x1d62;
 case S::ReadReleaseTick:return 0x1d64;
 case S::StoreReleaseTick:return 0x1d6a;
 case S::ClearPressed:return 0x1d6e;
 case S::ReturnRelease:return 0x1d72;
 case S::ReadFixedOnKey:return 0x1d74;
 case S::BindFixedOnSpeed:return 0x1d78;
 case S::AppendFixedOn:return 0x1d7a;
 case S::ReadFixedOffKey:return 0x1d7e;
 case S::BindFixedOffSpeed:return 0x1d82;
 case S::AppendFixedOff:return 0x1d84;
 case S::ReadRepeatFlags:return 0x1d88;
 case S::ChooseRepeat:return 0x1d8c;
 case S::ReadRepeatTick:return 0x1d8e;
 case S::ReadRepeatStart:return 0x1d94;
 case S::ElapsedRepeat:return 0x1d98;
 case S::BiasRepeat:return 0x1d9a;
 case S::ChooseEarlyRepeat:return 0x1d9e;
 case S::IncrementRepeat:return 0x1da0;
 case S::CalculateRepeat:return 0x1da2;
 case S::MarkRepeat:return 0x1da4;
 case S::ReadRepeatKey:return 0x1da6;
 case S::AppendRepeat:return 0x1daa;
 case S::ReturnRepeat:return 0x1dae;
 case S::ReadReleaseFlags:return 0x1db0;
 case S::ChooseRelease:return 0x1db4;
 case S::ReadVelocityTick:return 0x1db6;
 case S::ReadVelocityStart:return 0x1dbc;
 case S::ElapsedVelocity:return 0x1dc0;
 case S::CalculateRelease:return 0x1dc2;
 case S::ReadReleaseKey:return 0x1dc4;
 case S::AppendRelease:return 0x1dc8;
 case S::ReturnReleaseSpeed:return 0x1dcc;
 case S::SaveSpeedValue:return 0x1dce;
 case S::SaveSpeedIndex:return 0x1dd2;
 case S::RetainDelay:return 0x1dd6;
 case S::BindSlowPeriod:return 0x1dd8;
 case S::BindSlowBias:return 0x1ddc;
 case S::ReadSpeedFlags:return 0x1de0;
 case S::ChooseSpeedMode:return 0x1de4;
 case S::BindFastPeriod:return 0x1de6;
 case S::BindFastBias:return 0x1dea;
 case S::SubtractDelayBias:return 0x1dee;
 case S::ChooseDelayFloor:return 0x1df0;
 case S::ClearDelayFloor:return 0x1df2;
 case S::ClearNumerator:return 0x1df4;
 case S::SquareDelay:return 0x1df6;
 case S::BindDenominatorBias:return 0x1df8;
 case S::NormalizeDenominatorBias:return 0x1dfa;
 case S::AddDenominatorBias:return 0x1dfc;
 case S::HalfNumerator:return 0x1dfe;
 case S::TestDenominatorHigh:return 0x1e00;
 case S::ChooseDenominatorShift:return 0x1e02;
 case S::ShiftDenominator:return 0x1e04;
 case S::ShiftNumerator:return 0x1e06;
 case S::RepeatDenominator:return 0x1e08;
 case S::DivideSpeed:return 0x1e0a;
 case S::IncrementSpeed:return 0x1e0c;
 case S::ChooseSpeedLimit:return 0x1e0e;
 case S::ClampSpeed:return 0x1e10;
 case S::RestoreSpeedIndex:return 0x1e12;
 case S::RestoreSpeedValue:return 0x1e16;
 case S::ReturnSpeed:return 0x1e1a;
 case S::BiasEvent:return 0x1e72;
 case S::SaveAppendIndex:return 0x1e74;
 case S::SaveAppendValue:return 0x1e78;
 case S::ReadProducer:return 0x1e7c;
 case S::NormalizeProducer:return 0x1e82;
 case S::BindQueue:return 0x1e84;
 case S::LocateEvent:return 0x1e8a;
 case S::PublishEvent:return 0x1e8c;
 case S::IncrementProducer:return 0x1e8e;
 case S::MaskProducer:return 0x1e90;
 case S::PublishProducer:return 0x1e94;
 case S::RestoreAppendValue:return 0x1e9a;
 case S::RestoreAppendIndex:return 0x1e9e;
 case S::ReturnAppend:return 0x1ea2;
 case S::ReadPanelCount:return 0x2186 + 38*t.panel();
 case S::ChoosePanelActive:return 0x218c + 38*t.panel();
 case S::DecayPanel:return 0x218e + 38*t.panel();
 case S::PublishPanelCount:return 0x2190 + 38*t.panel();
 case S::ChoosePanelExpiry:return 0x2196 + 38*t.panel();
 case S::RetainPanelPort:return 0x2198 + 38*t.panel();
 case S::ReadPanelPort:return 0x219a + 38*t.panel();
 case S::MaskPanelPort:return 0x219c + 38*t.panel();
 case S::ReadPanelMirror:return 0x219e + 38*t.panel();
 case S::MaskPanelMirror:return 0x21a4 + 38*t.panel();
 case S::MergePanel:return 0x21a6 + 38*t.panel();
 case S::PublishPanelPort:return 0x21a8 + 38*t.panel();
 case S::RestorePanelPort:return 0x21aa + 38*t.panel();
 case S::RetainPanelCommand:return 0x21e4;
 case S::SetPanelCommand:return 0x21e6;
 case S::DispatchPanelCommand:return 0x21e8;
 case S::RestorePanelCommand:return 0x21ec;
 case S::ReturnPanel:return 0x21ee;
 case S::RetainCommand:return 0x2306;
 case S::DispatchReadyPoll:return 0x2308;
 case S::RestoreCommand:return 0x230a;
 case S::DispatchCommandWrite:return 0x230c;
 case S::ReturnCommand:return 0x2310;
 case S::PollFirstRead:return 0x22fc;
 case S::PollTest:return 0x22fe;
 case S::PollChoose:return 0x2300;
 case S::PollFinalRead:return 0x2302;
 case S::PollReturn:return 0x2304;
 case S::RetainRead:return 0x229e;
 case S::ReadDirectionValue:return 0x22a0;
 case S::ReadDirection:return 0x22a2;
 case S::ReadSelectValue:return 0x22a4;
 case S::ReadSelect:return 0x22a6;
 case S::ReadEnableValue:return 0x22a8;
 case S::ReadEnable:return 0x22aa;
 case S::ReadWaitFirst:return 0x22ac;
 case S::ReadWaitSecond:return 0x22ae;
 case S::ReadWaitThird:return 0x22b0;
 case S::ReadLcdData:return 0x22b2;
 case S::ReadFinishValue:return 0x22b4;
 case S::ReadFinish:return 0x22b6;
 case S::ReadRestoreDirectionValue:return 0x22b8;
 case S::ReadRestoreDirection:return 0x22ba;
 case S::RestoreRead:return 0x22bc;
 case S::ReturnRead:return 0x22be;
 case S::RetainWrite:return 0x2284;
 case S::WriteSelectValue:return 0x2286;
 case S::WriteSelect:return 0x2288;
 case S::WriteEnableValue:return 0x228a;
 case S::WriteEnable:return 0x228c;
 case S::WriteLcdData:return 0x228e;
 case S::WriteWaitFirst:return 0x2290;
 case S::WriteWaitSecond:return 0x2292;
 case S::WriteWaitThird:return 0x2294;
 case S::WriteFinishValue:return 0x2296;
 case S::WriteFinish:return 0x2298;
 case S::RestoreWrite:return 0x229a;
 case S::ReturnWrite:return 0x229c;
 case S::Complete:return 0;}return 0;}
 static bool isReturn(S s){switch(s){
case S::FinishTimer: case S::ReturnScan: case S::ReturnChanges: case S::ReturnPress: case S::ReturnRelease: case S::ReturnRepeat: case S::ReturnReleaseSpeed: case S::ReturnSpeed: case S::ReturnAppend: case S::ReturnPanel: case S::ReturnCommand: case S::PollReturn: case S::ReturnRead: case S::ReturnWrite:return true;default:return false;}}
 static void project(Stage1Cpu&c,const Timer1Service&t,uint32_t at,unsigned len){switch(t.stage()){
 case S::RetainTimer:{save32(c,0);break;}
 case S::ClearTimerValue:{c.reg8(8)=0;flags(c,0);break;}
 case S::DispatchScan:{call(c,at+len);break;}
 case S::AdvanceDivider:{c.reg8(8)=add8(c,c.reg8(8),-128);break;}
 case S::DispatchClock:{call(c,at+len);break;}
 case S::IncrementTick:{inc32(c,0,1);break;}
 case S::MaskTick:{c.reg8(8)&=7;flags(c,c.reg8(8));break;}
 case S::DispatchPanel:{call(c,at+len);break;}
 case S::RestoreTimer:{restore32(c,0);break;}
 case S::FinishTimer:{finish(c,true);break;}
 case S::SaveScanIndex:{save32(c,5);break;}
 case S::SaveScanContacts:{save32(c,4);break;}
 case S::SaveScanRecord:{save32(c,3);break;}
 case S::IncrementScanTick:{inc16(c,5,1);break;}
 case S::BindHistory:{c.reg32(5)=0xa856;flags(c,c.reg32(5));break;}
 case S::SelectMux:{c.reg8(12)=int8_t(t.muxValue());flags(c,c.reg8(12));break;}
 case S::ComparePins:{sub8(c,c.reg8(12),c.reg8(4));break;}
 case S::RestoreScanRecord:{restore32(c,3);break;}
 case S::RestoreScanContacts:{restore32(c,4);break;}
 case S::RestoreScanIndex:{restore32(c,5);break;}
 case S::ReturnScan:{finish(c,false);break;}
 case S::BindChangedRecord:{c.reg32(3)=int32_t(t.pageRecord());flags(c,c.reg32(3));break;}
 case S::DispatchChanges:{call(c,at+len);break;}
 case S::RestoreHistoryCursor:{c.reg32(5)=int32_t(uint32_t(c.reg32(5))+1);break;}
 case S::SaveTransitionIndex:{save32(c,2);break;}
 case S::ClearTransition:{c.reg32(2)=sub32(c,c.reg32(2),c.reg32(2));break;}
 case S::ShiftCurrentFirst:{shiftRight(c,4,1);break;}
 case S::AppendCurrentFirst:{rotateCarry(c,2);break;}
 case S::ShiftCurrentSecond:{shiftRight(c,4,1);break;}
 case S::AppendCurrentSecond:{rotateCarry(c,2);break;}
 case S::ShiftPreviousFirst:{shiftRight(c,12,1);break;}
 case S::AppendPreviousFirst:{rotateCarry(c,2);break;}
 case S::ShiftPreviousSecond:{shiftRight(c,12,1);break;}
 case S::AppendPreviousSecond:{rotateCarry(c,2);break;}
 case S::ScaleTransition:{shiftLeft32(c,2);break;}
 case S::BindTransition:{c.reg32(2)=int32_t(add32(c,uint32_t(c.reg32(2)),0x1d34));break;}
 case S::DispatchTransition:{call(c,at+len);break;}
 case S::NextTransitionRecord:{c.reg32(3)=int32_t(uint32_t(c.reg32(3))+4);break;}
 case S::RestoreTransitionIndex:{restore32(c,2);break;}
 case S::ReturnChanges:{finish(c,false);break;}
 case S::Transition:{if(t.transitionReturns())finish(c,false);break;}
 case S::ReturnPress:{finish(c,false);break;}
 case S::ReturnRelease:{finish(c,false);break;}
 case S::BindFixedOnSpeed:{c.reg8(2)=127;flags(c,c.reg8(2));break;}
 case S::AppendFixedOn:{c.clockI(2);break;}
 case S::BindFixedOffSpeed:{c.reg8(2)=-1;flags(c,c.reg8(2));break;}
 case S::AppendFixedOff:{c.clockI(2);break;}
 case S::ElapsedRepeat:{c.reg16(2)=sub16(c,c.reg16(2),c.reg16(10));break;}
 case S::BiasRepeat:{c.reg16(2)=sub16(c,c.reg16(2),10);break;}
 case S::IncrementRepeat:{inc16(c,2,1);break;}
 case S::CalculateRepeat:{call(c,at+len);break;}
 case S::MarkRepeat:{c.reg8(2)=int8_t(uint8_t(c.reg8(2))|128);break;}
 case S::AppendRepeat:{c.clockI(2);break;}
 case S::ReturnRepeat:{finish(c,false);break;}
 case S::ElapsedVelocity:{c.reg16(2)=sub16(c,c.reg16(2),c.reg16(10));break;}
 case S::CalculateRelease:{call(c,at+len);break;}
 case S::AppendRelease:{c.clockI(2);break;}
 case S::ReturnReleaseSpeed:{finish(c,false);break;}
 case S::SaveSpeedValue:{save32(c,0);break;}
 case S::SaveSpeedIndex:{save32(c,1);break;}
 case S::RetainDelay:{c.reg16(1)=c.reg16(2);flags(c,c.reg16(1));break;}
 case S::BindSlowPeriod:{c.reg16(10)=120;flags(c,c.reg16(10));break;}
 case S::BindSlowBias:{c.reg16(2)=3;flags(c,c.reg16(2));break;}
 case S::BindFastPeriod:{c.reg16(10)=30;flags(c,c.reg16(10));break;}
 case S::BindFastBias:{c.reg16(2)=2;flags(c,c.reg16(2));break;}
 case S::SubtractDelayBias:{c.reg16(1)=sub16(c,c.reg16(1),c.reg16(2));break;}
 case S::ClearDelayFloor:{c.reg16(1)=sub16(c,c.reg16(1),c.reg16(1));break;}
 case S::ClearNumerator:{c.reg16(2)=sub16(c,c.reg16(2),c.reg16(2));break;}
 case S::SquareDelay:{c.reg32(1)=int32_t(uint32_t(uint16_t(c.reg16(1)))*uint16_t(c.reg16(1)));c.clockI(20);break;}
 case S::BindDenominatorBias:{c.reg16(0)=c.reg16(10);flags(c,c.reg16(0));break;}
 case S::NormalizeDenominatorBias:{c.reg32(0)&=0xffff;flags(c,c.reg32(0));break;}
 case S::AddDenominatorBias:{c.reg32(1)=int32_t(add32(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(0))));break;}
 case S::HalfNumerator:{shiftRight(c,2,4);break;}
 case S::TestDenominatorHigh:{flags(c,c.reg16(9));break;}
 case S::ShiftDenominator:{shiftRight(c,1,4);break;}
 case S::ShiftNumerator:{shiftRight(c,2,4);break;}
 case S::DivideSpeed:{divide(c,1,2);break;}
 case S::IncrementSpeed:{inc8(c,2,1);break;}
 case S::ClampSpeed:{c.reg8(2)=127;flags(c,c.reg8(2));break;}
 case S::RestoreSpeedIndex:{restore32(c,1);break;}
 case S::RestoreSpeedValue:{restore32(c,0);break;}
 case S::ReturnSpeed:{finish(c,false);break;}
 case S::BiasEvent:{c.reg8(10)=add8(c,c.reg8(10),12);break;}
 case S::SaveAppendIndex:{save32(c,1);break;}
 case S::SaveAppendValue:{save32(c,0);break;}
 case S::NormalizeProducer:{c.reg32(0)&=0xffff;flags(c,c.reg32(0));break;}
 case S::BindQueue:{c.reg32(1)=0xc6ae;flags(c,c.reg32(1));break;}
 case S::LocateEvent:{c.reg32(1)=int32_t(add32(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(0))));break;}
 case S::IncrementProducer:{inc16(c,0,2);break;}
 case S::MaskProducer:{c.reg16(0)&=0x1fe;flags(c,c.reg16(0));break;}
 case S::RestoreAppendValue:{restore32(c,0);break;}
 case S::RestoreAppendIndex:{restore32(c,1);break;}
 case S::ReturnAppend:{finish(c,false);break;}
 case S::DecayPanel:{inc8(c,8,-1);break;}
 case S::RetainPanelPort:{save16(c,6);break;}
 case S::MaskPanelPort:{c.reg8(6)=int8_t(uint8_t(c.reg8(6))&(t.panel()?0xbf:0xef));flags(c,c.reg8(6));break;}
 case S::MaskPanelMirror:{c.reg8(14)&=t.panel()?0x20:0x10;flags(c,c.reg8(14));break;}
 case S::MergePanel:{c.reg8(6)=int8_t(uint8_t(c.reg8(6))|uint8_t(c.reg8(14)));flags(c,c.reg8(6));break;}
 case S::RestorePanelPort:{restore16(c,6);break;}
 case S::RetainPanelCommand:{save16(c,6);break;}
 case S::SetPanelCommand:{c.reg8(14)=12;flags(c,c.reg8(14));break;}
 case S::DispatchPanelCommand:{call(c,at+len);break;}
 case S::RestorePanelCommand:{restore16(c,6);break;}
 case S::ReturnPanel:{finish(c,false);break;}
 case S::RetainCommand:{save16(c,6);break;}
 case S::DispatchReadyPoll:{call(c,at+len);break;}
 case S::RestoreCommand:{restore16(c,6);break;}
 case S::DispatchCommandWrite:{call(c,at+len);break;}
 case S::ReturnCommand:{finish(c,false);break;}
 case S::PollFirstRead:{call(c,at+len);break;}
 case S::PollTest:{flags(c,c.reg8(14));break;}
 case S::PollFinalRead:{call(c,at+len);break;}
 case S::PollReturn:{finish(c,false);break;}
 case S::RetainRead:{save16(c,0);break;}
 case S::ReadDirectionValue:{c.reg8(8)=0;flags(c,0);break;}
 case S::ReadSelectValue:{c.reg8(8)=0x50;flags(c,c.reg8(8));break;}
 case S::ReadEnableValue:{c.reg8(8)=int8_t(0xd0);flags(c,c.reg8(8));break;}
 case S::ReadFinishValue:{c.reg8(8)=0x50;flags(c,c.reg8(8));break;}
 case S::ReadRestoreDirectionValue:{c.reg8(8)=-1;flags(c,c.reg8(8));break;}
 case S::RestoreRead:{restore16(c,0);break;}
 case S::ReturnRead:{finish(c,false);break;}
 case S::RetainWrite:{save16(c,0);break;}
 case S::WriteSelectValue:{c.reg8(8)=0x40;flags(c,c.reg8(8));break;}
 case S::WriteEnableValue:{c.reg8(8)=int8_t(0xc0);flags(c,c.reg8(8));break;}
 case S::WriteFinishValue:{c.reg8(8)=0x40;flags(c,c.reg8(8));break;}
 case S::RestoreWrite:{restore16(c,0);break;}
 case S::ReturnWrite:{finish(c,false);break;}
 default:break;}}
 static uint32_t read(Stage1Cpu&c,R field,uint32_t ref){switch(field){
 case R::TimerStatus:case R::Divider:case R::PanelCount:c.reg8(8)=c.read8(int(ref));flags(c,c.reg8(8));return uint8_t(c.reg8(8));
 case R::Tick:c.reg32(0)=int32_t(c.read32(int(ref)));flags(c,c.reg32(0));return uint32_t(c.reg32(0));
 case R::ScanTick:c.reg16(5)=int16_t(c.read16(int(ref)));flags(c,c.reg16(5));return uint16_t(c.reg16(5));
 case R::Pins:c.reg8(4)=c.read8(int(ref));flags(c,c.reg8(4));return uint8_t(c.reg8(4));
 case R::History:c.reg8(12)=c.read8(int(ref));c.reg32(5)=int32_t(uint32_t(c.reg32(5))+1);flags(c,c.reg8(12));c.clockI(2);return uint8_t(c.reg8(12));
 case R::EventTick:c.reg16(2)=int16_t(c.read16(int(ref)));flags(c,c.reg16(2));return uint16_t(c.reg16(2));
 case R::EventStart:c.reg16(10)=int16_t(c.read16(int(ref)));flags(c,c.reg16(10));return uint16_t(c.reg16(10));
 case R::EventKey:c.reg8(10)=c.read8(int(ref));flags(c,c.reg8(10));return uint8_t(c.reg8(10));
 case R::RecordFlagsKeep:return uint8_t(c.read8(int(ref)));
 case R::RecordFlagsTest:{const auto v=uint8_t(c.read8(int(ref)));bitFlag(c,4,!(v&2));return v;}
 case R::SpeedFlags:{const auto v=uint8_t(c.read8(int(ref)));bitFlag(c,4,!(v&1));return v;}
 case R::Producer:c.reg16(0)=int16_t(c.read16(int(ref)));flags(c,c.reg16(0));return uint16_t(c.reg16(0));
 case R::PanelPort:c.reg8(6)=c.read8(int(ref));flags(c,c.reg8(6));return uint8_t(c.reg8(6));
 case R::PanelMirror:case R::LcdData:c.reg8(14)=c.read8(int(ref));flags(c,c.reg8(14));return uint8_t(c.reg8(14));
 }throw std::logic_error("Native K Timer1 resource read");}
 static void write(Stage1Cpu&c,W field,uint32_t ref,uint32_t v){switch(field){
 case W::TimerStatus:case W::Divider:case W::PanelCount:flags(c,c.reg8(8));c.write8(int8_t(v),int(ref));break;
 case W::Tick:c.write32(int32_t(v),int(ref));flags(c,c.reg32(0));break;
 case W::ScanTick:c.write16(int16_t(v),int(ref));flags(c,c.reg16(5));break;
 case W::Mux:flags(c,c.reg8(12));c.write8(int8_t(v),int(ref));break;
 case W::History:c.reg32(5)=int32_t(uint32_t(c.reg32(5))-1);c.write8(int8_t(v),int(ref));flags(c,c.reg8(4));c.clockI(2);break;
 case W::RecordTick:c.write16(int16_t(v),int(ref));flags(c,c.reg16(2));break;
 case W::RecordFlags:c.write8(int8_t(v),int(ref));break;
 case W::Event:c.write16(int16_t(v),int(ref));flags(c,c.reg16(2));break;
 case W::Producer:c.write16(int16_t(v),int(ref));flags(c,c.reg16(0));break;
 case W::PanelPort:flags(c,c.reg8(6));c.write8(int8_t(v),int(ref));break;
 case W::LcdLow:flags(c,c.reg8(8));c.write8(int8_t(v),int(ref));break;
 case W::LcdCommand:flags(c,c.reg8(14));c.write8(int8_t(v),int(ref));break;
 }}
 static void flags(Stage1Cpu&c,int value){c.ccr=uint8_t((c.ccr&~14)|(value<0?8:0)|(value==0?4:0));}
 static void bitFlag(Stage1Cpu&c,unsigned bit,bool on){c.ccr=uint8_t((c.ccr&~bit)|(on?bit:0));}
 static int8_t add8(Stage1Cpu&c,int8_t a,int8_t b){const int r=int(a)+b;c.ccr=uint8_t((c.ccr&~0x2f)|((r&255)?0:4)|((r&128)?8:0)|((((a&b)|(a&~r)|(b&~r))&128)?1:0)|((((a&b)|(a&~r)|(b&~r))&8)?32:0)|((((a&b&~r)|(~a&~b&r))&128)?2:0));return int8_t(r);}
 static int8_t sub8(Stage1Cpu&c,int8_t a,int8_t b){const int r=int(a)-b;c.ccr=uint8_t((c.ccr&~0x2f)|((r&255)?0:4)|((r&128)?8:0)|((((~a&b)|(~a&r)|(b&r))&128)?1:0)|((((~a&b)|(~a&r)|(b&r))&8)?32:0)|((((a&~b&~r)|(~a&b&r))&128)?2:0));return int8_t(r);}
 static int16_t sub16(Stage1Cpu&c,int16_t a,int16_t b){const int r=int(a)-b;c.ccr=uint8_t((c.ccr&~0x2f)|((r&65535)?0:4)|((r&32768)?8:0)|((((~a&b)|(~a&r)|(b&r))&32768)?1:0)|((((~a&b)|(~a&r)|(b&r))&2048)?32:0)|((((a&~b&~r)|(~a&b&r))&32768)?2:0));return int16_t(r);}
 static uint32_t add32(Stage1Cpu&c,uint32_t a,uint32_t b){const auto r=a+b;c.ccr=uint8_t((c.ccr&~0x2f)|(r?0:4)|((r&0x80000000)?8:0)|((((a&b)|(a&~r)|(b&~r))&0x80000000)?1:0)|((((a&b)|(a&~r)|(b&~r))&0x8000000)?32:0)|((((a&b&~r)|(~a&~b&r))&0x80000000)?2:0));return r;}
 static int32_t sub32(Stage1Cpu&c,int32_t a,int32_t b){const uint32_t x=uint32_t(a),y=uint32_t(b),r=x-y;c.ccr=uint8_t((c.ccr&~0x2f)|(r?0:4)|((r&0x80000000)?8:0)|((((~x&y)|(~x&r)|(y&r))&0x80000000)?1:0)|((((~x&y)|(~x&r)|(y&r))&0x8000000)?32:0)|((((x&~y&~r)|(~x&y&r))&0x80000000)?2:0));return int32_t(r);}
 static void inc8(Stage1Cpu&c,int reg,int amount){const int old=c.reg8(reg),v=old+amount;flags(c,v);bitFlag(c,2,((amount>0?(~old&v):(old&~v))&128)!=0);c.reg8(reg)=int8_t(v);}
 static void inc16(Stage1Cpu&c,int reg,int amount){const int old=c.reg16(reg),v=old+amount;flags(c,v);bitFlag(c,2,((~old&v)&32768)!=0);c.reg16(reg)=int16_t(v);}
 static void inc32(Stage1Cpu&c,int reg,int amount){const uint32_t old=uint32_t(c.reg32(reg)),v=old+unsigned(amount);flags(c,int32_t(v));bitFlag(c,2,((~old&v)&0x80000000)!=0);c.reg32(reg)=int32_t(v);}
 static void shiftRight(Stage1Cpu&c,int reg,unsigned size){const uint32_t old=size==1?uint8_t(c.reg8(reg)):uint32_t(c.reg32(reg));bitFlag(c,1,old&1);const auto v=old>>1;flags(c,int32_t(v<<(8*(4-size))));if(size==1)c.reg8(reg)=int8_t(v);else c.reg32(reg)=int32_t(v);}
 static void shiftLeft32(Stage1Cpu&c,int reg){const auto old=uint32_t(c.reg32(reg));bitFlag(c,1,old&0x80000000);const auto v=old<<1;flags(c,int32_t(v));c.reg32(reg)=int32_t(v);}
 static void rotateCarry(Stage1Cpu&c,int reg){const auto old=uint32_t(c.reg32(reg));const auto v=(old<<1)|(c.ccr&1);bitFlag(c,1,old&0x80000000);flags(c,int32_t(v));c.reg32(reg)=int32_t(v);}
 static void divide(Stage1Cpu&c,int source,int target){const uint32_t a=uint16_t(c.reg16(source)),b=uint32_t(c.reg32(target)),q=a?b/a:0;bitFlag(c,8,a&32768);bitFlag(c,4,a==0);c.reg16(target)=int16_t(q);c.reg16(target+8)=int16_t(b-q*a);c.clockI(20);}
 static void save32(Stage1Cpu&c,int r){c.setSP(c.getSP()-4);c.write32(c.reg32(r),int(c.getSP()));flags(c,c.reg32(r));c.clockI(2);}
 static void restore32(Stage1Cpu&c,int r){c.reg32(r)=int32_t(c.read32(int(c.getSP())));c.setSP(c.getSP()+4);flags(c,c.reg32(r));c.clockI(2);}
 static void save16(Stage1Cpu&c,int r){c.setSP(c.getSP()-2);c.write16(c.reg16(r),int(c.getSP()));c.clockI(2);flags(c,c.reg16(r));}
 static void restore16(Stage1Cpu&c,int r){c.reg16(r)=int16_t(c.read16(int(c.getSP())));c.setSP(c.getSP()+2);c.clockI(2);flags(c,c.reg16(r));}
 static void call(Stage1Cpu&c,uint32_t ret){const auto sp=c.getSP()-4;c.write32(int32_t(ret),int(sp));c.setSP(sp);c.irqDepth(c.irqDepth()+1);if(ret-uint32_t(c.getPC())==3)c.clockI(2);}
 static void finish(Stage1Cpu&c,bool interrupt){c.irqDepth(c.irqDepth()-1);c.clockI(2);const auto sp=c.getSP();c.setSP(sp+4);const auto value=c.read32(int(sp));if(interrupt)c.ccr=uint8_t(value>>24);c.pc=c.makepc(int(value&0xffffff));}
 std::array<Timer1Service,8>jobs_{};unsigned depth_=0;
};
}
