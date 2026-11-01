#pragma once
#include "ksSysExNativeCoordinator.h"
#include "ksStateCentralActionsAbi.h"
namespace ks {namespace sysexBoundary {
using namespace centralActionsBoundary;
struct SavedReport {uint32_t record=0,payload=0,value=0;};
template<class A>void upper(A&a,unsigned n,uint16_t x){a.values[n]=(a.values[n]&65535u)|(uint32_t(x)<<16);}
template<class A>void moveUpper(A&a,unsigned n,uint16_t x){upper(a,n,x);value16(a,x);}
template<class A>void cmp16(A&a,uint16_t x,uint16_t y){a.flags=controlArithmetic::subtract(int16_t(x),int16_t(y),a.flags).flags;}
template<class A>uint16_t sub16(A&a,uint16_t x,uint16_t y){const auto r=controlArithmetic::subtract(int16_t(x),int16_t(y),a.flags);a.flags=r.flags;return uint16_t(r.value);}
template<class A>uint16_t inc16(A&a,uint16_t x,int delta){const int32_t old=int16_t(x),result=int32_t(uint32_t(old)+uint32_t(delta));value32(a,uint32_t(result));flag(a,2,bool((delta>0?(~uint32_t(old)&uint32_t(result)):(uint32_t(old)&~uint32_t(result)))&0x8000));return uint16_t(result);}
template<class A>uint32_t inc32(A&a,uint32_t x,int delta){const uint32_t result=x+uint32_t(delta);value32(a,result);flag(a,2,bool((delta>0?(~x&result):(x&~result))&0x80000000u));return result;}
template<class A>void prefix(A&a,SysExFrameTask::Stage stage,uint32_t readData,const SavedReport&saved){using S=SysExFrameTask::Stage;auto&v=a.values;(void)v;(void)readData;(void)saved;switch(stage){
case S::BeginReadState:{move8(a,0,int8_t(readData));break;}
case S::RestartReadState:{move8(a,0,int8_t(readData));break;}
case S::RestartTickState:{low(a,0,inc8(a,uint8_t(v[0]),1));break;}
case S::RestartPublishState:{value8(a,uint8_t(v[0]));break;}
case S::RestartClearCount:{word(a,0,sub16(a,uint16_t(v[0]),uint16_t(v[0])));break;}
case S::RestartPublishCount:{value16(a,uint16_t(v[0]));break;}
case S::RestartBindPayload:{move32(a,0,0x15c38);break;}
case S::RestartPublishCursor:{value32(a,v[0]);break;}
case S::DispatchReadState:{move8(a,0,int8_t(readData));break;}
case S::DispatchCompareHeader:{cmp8(a,uint8_t(v[0]),0x1);break;}
case S::DispatchBindModel:{move32(a,1,0x1b0);break;}
case S::DispatchReadModel:{move8(a,1,int8_t(readData));break;}
case S::DispatchCompareCommand:{cmp8(a,uint8_t(v[0]),0x2);break;}
case S::DispatchReadCommand:{moveHigh(a,0,int8_t(readData));break;}
case S::DispatchCompareModel:{cmp8(a,uint8_t(v[0]>>8),uint8_t(v[1]));break;}
case S::DispatchComparePatchRequest:{cmp8(a,uint8_t(v[0]>>8),0x40);break;}
case S::DispatchCompareMultiRequest:{cmp8(a,uint8_t(v[0]>>8),0x41);break;}
case S::DispatchComparePayload:{cmp8(a,uint8_t(v[0]),0x3);break;}
case S::HeaderReadInput:{move8(a,0,int8_t(readData));break;}
case S::HeaderCompareCount:{cmp16(a,uint16_t(v[0]),0x6);break;}
case S::AppendReadCursor:{move32(a,1,int32_t(readData));break;}
case S::AppendPublishInput:{value8(a,uint8_t(v[0]));break;}
case S::AppendTickCursor:{v[1]=uint32_t(int32_t(uint32_t(v[1])+0x1));break;}
case S::AppendPublishCursor:{value32(a,v[1]);break;}
case S::AppendReadCount:{move16(a,0,int16_t(readData));break;}
case S::AppendTickCount:{word(a,0,inc16(a,uint16_t(v[0]),1));break;}
case S::AppendPublishCount:{value16(a,uint16_t(v[0]));break;}
case S::ValidateBindHeader:{move32(a,0,0x15c38);break;}
case S::ValidateBindModel:{move32(a,1,0x1b0);break;}
case S::ValidateSetCount:{moveUpper(a,2,0x4);break;}
case S::ValidateReadHeader:{move8(a,2,int8_t(readData));v[0]+=1;break;}
case S::ValidateReadModel:{moveHigh(a,2,int8_t(readData));v[1]+=1;break;}
case S::ValidateCompareByte:{cmp8(a,uint8_t(v[2]),uint8_t(v[2]>>8));break;}
case S::ValidateTickCount:{upper(a,2,inc16(a,uint16_t(v[2]>>16),-1));break;}
case S::ValidateReadCommand:{move8(a,2,int8_t(readData));v[0]+=1;break;}
case S::ValidatePublishCommand:{value8(a,uint8_t(v[2]));break;}
case S::ValidateReadModelCommand:{moveHigh(a,2,int8_t(readData));v[1]+=1;break;}
case S::ValidateComparePatch:{cmp8(a,uint8_t(v[2]),0x40);break;}
case S::ValidateCompareMulti:{cmp8(a,uint8_t(v[2]),0x41);break;}
case S::ValidateCompareModel:{cmp8(a,uint8_t(v[2]>>8),uint8_t(v[2]));break;}
case S::ValidateReadDevice:{move8(a,0,int8_t(readData));break;}
case S::ValidateCompareBroadcast:{cmp8(a,uint8_t(v[0]),0x7f);break;}
case S::ValidateBindPatch:{move32(a,3,0x168fc);break;}
case S::ValidateReadDeviceId:{moveHigh(a,0,int8_t(readData));break;}
case S::ValidateCompareDevice:{cmp8(a,uint8_t(v[0]),uint8_t(v[0]>>8));break;}
case S::ValidateBindCursor:{move32(a,0,0x1603c);break;}
case S::ValidatePublishCursor:{value32(a,v[0]);break;}
case S::CancelClear:{move8(a,0,0x0);break;}
case S::CancelPublishState:{value8(a,uint8_t(v[0]));break;}
case S::CancelPublishRemaining:{value8(a,uint8_t(v[0]));break;}
case S::BankReadInput:{move8(a,0,int8_t(readData));break;}
case S::BankCompareCount:{cmp16(a,uint16_t(v[0]),0x6);break;}
case S::BankReadType:{move8(a,0,int8_t(readData));break;}
case S::BankSmallSize:{move16(a,1,0x80);break;}
case S::BankCompareSmall:{cmp8(a,uint8_t(v[0]),0x1);break;}
case S::BankLargeSize:{move16(a,1,0x100);break;}
case S::BankCompareLarge:{cmp8(a,uint8_t(v[0]),0x2);break;}
case S::LegacyReadType:{move8(a,0,int8_t(readData));break;}
case S::LegacyComparePatch:{cmp8(a,uint8_t(v[0]),0x0);break;}
case S::LegacyComparePatchBank:{cmp8(a,uint8_t(v[0]),0x1);break;}
case S::LegacyCompareMulti:{cmp8(a,uint8_t(v[0]),0x2);break;}
case S::ModelReadInput:{move8(a,0,int8_t(readData));break;}
case S::ModelCompareCount:{cmp16(a,uint16_t(v[0]),0x6);break;}
case S::ModelReadType:{move8(a,0,int8_t(readData));break;}
case S::ModelCompareRequest:{cmp8(a,uint8_t(v[0]),0x40);break;}
case S::ModelSmallSize:{move16(a,1,0x100);break;}
case S::ModelCompareSmall:{cmp8(a,uint8_t(v[0]),0x1);break;}
case S::ModelCompareGlobal:{cmp8(a,uint8_t(v[0]),0x4);break;}
case S::ModelLargeSize:{move16(a,1,0x400);break;}
case S::ModelCompareLarge:{cmp8(a,uint8_t(v[0]),0x3);break;}
case S::PublishSize:{value16(a,uint16_t(v[1]));break;}
case S::PayloadReadInput:{move8(a,0,int8_t(readData));break;}
case S::PayloadReadSize:{move16(a,1,int16_t(readData));break;}
case S::PayloadCompareSize:{cmp16(a,uint16_t(v[0]),uint16_t(v[1]));break;}
case S::PayloadBindModel:{move32(a,1,0x1b0);break;}
case S::PayloadReadModel:{move8(a,1,int8_t(readData));break;}
case S::PayloadReadCommand:{move8(a,0,int8_t(readData));break;}
case S::PayloadCompareModel:{cmp8(a,uint8_t(v[0]),uint8_t(v[1]));break;}
case S::PayloadReadType:{move8(a,0,int8_t(readData));break;}
case S::PayloadComparePatch:{cmp8(a,uint8_t(v[0]),0x0);break;}
case S::PayloadComparePatchBank:{cmp8(a,uint8_t(v[0]),0x1);break;}
case S::PayloadCompareMulti:{cmp8(a,uint8_t(v[0]),0x2);break;}
case S::PayloadCompareMultiBank:{cmp8(a,uint8_t(v[0]),0x3);break;}
case S::PayloadCompareGlobal:{cmp8(a,uint8_t(v[0]),0x4);break;}
case S::RequestComparePatch:{cmp8(a,uint8_t(v[0]),0x40);break;}
case S::RequestComparePerformance:{cmp8(a,uint8_t(v[0]),0x44);break;}
case S::RequestReadSlot:{move8(a,5,int8_t(readData));break;}
case S::RequestReadBank:{moveHigh(a,6,int8_t(readData));break;}
case S::RequestCompareBank:{cmp8(a,uint8_t(v[6]>>8),0x4);break;}
case S::RequestReadProgram:{move8(a,6,int8_t(readData));break;}
case S::RequestCompareProgram:{cmp8(a,uint8_t(v[6]),0x64);break;}
case S::RequestCompareBankDump:{cmp8(a,uint8_t(v[0]),0x41);break;}
default:break;}}
template<class A>void prefix(A&a,SysExActionTask::Stage stage,uint32_t readData,const SavedReport&saved){using S=SysExActionTask::Stage;auto&v=a.values;(void)v;(void)readData;(void)saved;switch(stage){
case S::PatchReadMode:{move8(a,0,int8_t(readData));break;}
case S::PatchBindPayload:{move32(a,0,0x15c38);break;}
case S::PatchObserveActive:{move16(a,0,uint16_t(v[0]));break;}
case S::PatchMessage:{move32(a,6,0xf349);break;}
case S::MultiReadMode:{move8(a,0,int8_t(readData));break;}
case S::MultiBindPayload:{move32(a,0,0x15c38);break;}
case S::MultiObserveActive:{move16(a,0,uint16_t(v[0]));break;}
case S::MultiMessage:{move32(a,6,0xf35e);break;}
case S::GlobalBindPayload:{move32(a,1,0x15c38);break;}
case S::GlobalMessage:{move32(a,6,0xf373);break;}
case S::PatchBankReadBank:{moveHigh(a,2,int8_t(readData));break;}
case S::PatchBankCompareBank:{cmp8(a,uint8_t(v[2]>>8),0x8);break;}
case S::PatchBankLargeLimit:{move8(a,3,0x64);break;}
case S::PatchBankCompareLimit:{cmp8(a,uint8_t(v[2]>>8),0x4);break;}
case S::PatchBankSmallLimit:{move8(a,3,0x32);break;}
case S::PatchBankReadProgram:{move8(a,2,int8_t(readData));break;}
case S::PatchBankCompareProgram:{cmp8(a,uint8_t(v[2]),uint8_t(v[3]));break;}
case S::MultiBankBindRecord:{move32(a,0,0x16040);break;}
case S::MultiBankBindPayload:{move32(a,1,0x15c38);break;}
case S::MultiBankSetBank:{move8(a,2,0x1);break;}
case S::MultiBankPublishBank:{value8(a,uint8_t(v[2]));break;}
case S::MultiBankReadProgram:{move8(a,2,int8_t(readData));break;}
case S::MultiBankCompareProgram:{cmp8(a,uint8_t(v[2]),0x64);break;}
case S::ResolveBindRecord:{move32(a,0,0x16040);break;}
case S::ResolveBindPayload:{move32(a,1,0x15c38);break;}
case S::ResolveReadSlot:{move8(a,2,int8_t(readData));break;}
case S::ResolveReadFallback:{move8(a,2,int8_t(readData));break;}
case S::ResolvePublishFallback:{value8(a,uint8_t(v[2]));break;}
case S::PatchReportRetainRecord:{value32(a,v[0]);break;}
case S::PatchReportRetainPayload:{value32(a,v[1]);break;}
case S::PatchReportRetainValue:{value32(a,v[6]);break;}
case S::PatchReportMessage:{move32(a,6,0xf385);break;}
case S::PatchReportReadBank:{move8(a,6,int8_t(readData));break;}
case S::PatchReportDigit:{low(a,6,add8(a,uint8_t(v[6]),0x30));break;}
case S::PatchReportCaptureProgram:{move8(a,0,uint8_t(v[2]));break;}
case S::PatchReportReleaseValue:{move32(a,6,saved.value);break;}
case S::PatchReportReleasePayload:{move32(a,1,saved.payload);break;}
case S::PatchReportReleaseRecord:{move32(a,0,saved.record);break;}
case S::MultiReportRetainRecord:{value32(a,v[0]);break;}
case S::MultiReportRetainPayload:{value32(a,v[1]);break;}
case S::MultiReportRetainValue:{value32(a,v[6]);break;}
case S::MultiReportMessage:{move32(a,6,0xf398);break;}
case S::MultiReportReadBank:{move8(a,6,int8_t(readData));break;}
case S::MultiReportDigit:{low(a,6,add8(a,uint8_t(v[6]),0x30));break;}
case S::MultiReportCaptureProgram:{move8(a,0,uint8_t(v[2]));break;}
case S::MultiReportReleaseValue:{move32(a,6,saved.value);break;}
case S::MultiReportReleasePayload:{move32(a,1,saved.payload);break;}
case S::MultiReportReleaseRecord:{move32(a,0,saved.record);break;}
case S::HeaderBindDestination:{move32(a,1,0x16044);break;}
case S::HeaderBindSource:{move32(a,0,0x1b0);break;}
case S::HeaderSetCount:{moveUpper(a,2,0xc);break;}
case S::HeaderRead:{move8(a,2,int8_t(readData));v[0]+=1;break;}
case S::HeaderPublish:{value8(a,uint8_t(v[2]));break;}
case S::HeaderAdvance:{v[1]=uint32_t(inc32(a,v[1],1));break;}
case S::HeaderTick:{upper(a,2,inc16(a,uint16_t(v[2]>>16),-1));break;}
case S::SendRead:{moveHigh(a,0,int8_t(readData));v[6]+=1;break;}
case S::SendMask:{high(a,0,uint8_t(v[0]>>8)&0x7f);value8(a,uint8_t(v[0]>>8));break;}
case S::SendTick:{word(a,5,inc16(a,uint16_t(v[5]),-1));break;}
case S::SendHeaderStatus:{moveHigh(a,0,uint8_t(0xf0));break;}
case S::SendHeaderBind:{move32(a,6,0x16044);break;}
case S::SendHeaderCount:{move16(a,5,0xc);break;}
default:break;}}
template<class A>void prefix(A&a,LegacySysExTask::Stage stage,uint32_t readData,const SavedReport&saved){using S=LegacySysExTask::Stage;auto&v=a.values;(void)v;(void)readData;(void)saved;switch(stage){
case S::ApplyReadMode:{move8(a,0,int8_t(readData));break;}
case S::ApplyBindExpand:{move32(a,5,0x15c38);break;}
case S::ApplyBindPayload:{move32(a,0,0x15c38);break;}
case S::ApplyObserveActive:{move16(a,0,uint16_t(v[0]));break;}
case S::ApplyMessage:{move32(a,6,0xf349);break;}
case S::PatchReadBank:{moveHigh(a,2,int8_t(readData));break;}
case S::PatchCompareBank:{cmp8(a,uint8_t(v[2]>>8),0x4);break;}
case S::PatchReadProgram:{move8(a,2,int8_t(readData));break;}
case S::PatchCompareProgram:{cmp8(a,uint8_t(v[2]),0x64);break;}
case S::PatchBindExpand:{move32(a,5,0x15c38);break;}
case S::MultiReadBank:{move8(a,2,int8_t(readData));break;}
case S::MultiCompareBank:{cmp8(a,uint8_t(v[2]),0x4);break;}
case S::MultiReadProgram:{move8(a,2,int8_t(readData));break;}
case S::MultiCaptureParity:{moveHigh(a,2,uint8_t(v[2]));break;}
case S::MultiMaskParity:{high(a,2,uint8_t(v[2]>>8)&0x1);value8(a,uint8_t(v[2]>>8));break;}
case S::MultiCompareProgram:{cmp8(a,uint8_t(v[2]),0x62);break;}
case S::PairBindBackup:{move32(a,6,0x16050);break;}
case S::PairBindSource:{move32(a,5,0x15cb8);break;}
case S::PairCount:{moveUpper(a,4,0x80);break;}
case S::PairReadSource:{move8(a,4,int8_t(readData));v[5]+=1;break;}
case S::PairPublishBackup:{value8(a,uint8_t(v[4]));break;}
case S::PairAdvanceBackup:{v[6]=uint32_t(inc32(a,v[6],1));break;}
case S::PairTickBackup:{upper(a,4,inc16(a,uint16_t(v[4]>>16),-1));break;}
case S::PairBindFirst:{move32(a,5,0x15c38);break;}
case S::PairRebindBackup:{move32(a,6,0x16050);break;}
case S::PairBindSecond:{move32(a,5,0x15c38);break;}
case S::PairSecondCount:{moveUpper(a,4,0x80);break;}
case S::PairReadBackup:{move8(a,4,int8_t(readData));v[6]+=1;break;}
case S::PairPublishSource:{value8(a,uint8_t(v[4]));break;}
case S::PairAdvanceSource:{v[5]=uint32_t(inc32(a,v[5],1));break;}
case S::PairTickSource:{upper(a,4,inc16(a,uint16_t(v[4]>>16),-1));break;}
case S::PairBindFinal:{move32(a,5,0x15c38);break;}
case S::PairReadProgram:{move8(a,2,int8_t(readData));break;}
case S::PairAdvanceProgram:{low(a,2,inc8(a,uint8_t(v[2]),1));break;}
case S::PairPublishProgram:{value8(a,uint8_t(v[2]));break;}
default:break;}}

// This walk occurs only after the complete external transaction has returned.
// It composes retained semantic outcomes into the real caller ABI once.
// No ABI values are stored by an active job or passed to a running child.
template<class A,class Project>void node(A&a,const SysExNativeCoordinator&service,unsigned index,Project&project){const auto&o=service.node(index);if(!o.finished)throw std::logic_error("SysEx projection before completed node");auto task=SysExNativeCoordinator::initialTask(o.kind,o.input);const SavedReport saved{a.values[0],a.values[1],a.values[6]};unsigned reads=0,children=0,steps=0;
 std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;for(;;){if(++steps>4096)throw std::logic_error("SysEx completion projection bound");const auto stage=t.stage();uint32_t readData=0;const auto read=[&](auto)->uint32_t{if(reads==o.observations)throw std::logic_error("Missing SysEx domain observation");const auto&v=o.reads[reads++];if(v.stage!=unsigned(stage))throw std::logic_error("SysEx domain observation order");return readData=v.value;};const auto write=[](auto,auto){};const auto used=t.advanceBefore(t.nextClocks(),read,write,[](auto){return 3u;});if(!used)throw std::logic_error("SysEx completion phase absent");prefix(a,stage,readData,saved);
  if(children<o.children&&o.outcomes[children].stage==unsigned(stage)){const auto&child=o.outcomes[children++];if(child.node!=255)node(a,service,child.node,project);else project(child.result.outcome,a);if(child.tail)break;const auto&r=child.result;
   if constexpr(std::is_same_v<T,SysExFrameTask>){if(t.child()==T::Child::Append)t.resumeAppend(r.count);else t.resumeChild();}
   else if constexpr(std::is_same_v<T,SysExActionTask>){if(t.child()==T::Child::Active)t.resumeActive(r.active);else if(t.child()==T::Child::Resolve)t.resumeResolve(r.record);else if(t.child()==T::Child::UiText||t.child()==T::Child::UiByte)t.resumeUi(t.record(),t.program());else t.resumeChild();}
   else{if(t.child()==T::Child::Active)t.resumeActive(r.active);else if(t.child()==T::Child::Resolve)t.resumeResolve(r.record);else t.resumeChild();}
  }
  if(t.complete())break;
 }},task);if(reads!=o.observations||children!=o.children)throw std::logic_error("Unused SysEx completion facts");
}
}
template<class A,class Project>void projectSysExNativePolicy(A&a,const SysExNativeCoordinator&s,Project&&p){if(!s.complete())throw std::logic_error("Incomplete SysEx caller projection");sysexBoundary::node(a,s,0,p);}
}
