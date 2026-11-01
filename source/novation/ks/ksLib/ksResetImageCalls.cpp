#include "ksResetImageCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {ResetImagePartialTest partialTest=nullptr;using T=ResetImageTask;using S=T::Stage;struct Point{unsigned location,length;};Point point(S s){switch(s){
case S::EnterWarm:return{0x180,4};
case S::EstablishStack:return{0x2ec,6};
case S::BindClear:return{0x2f2,6};
case S::SetClearCount:return{0x2f8,6};
case S::ClearWord:return{0x2fe,2};
case S::AdvanceClear:return{0x300,2};
case S::TickClear:return{0x302,2};
case S::RepeatClear:return{0x304,2};
case S::ConfigurePins:return{0x306,4};
case S::InitializePanel:return{0x30a,4};
case S::ConfigureRuntime:return{0x30e,4};
case S::RetainSettling:return{0x312,2};
case S::SetSettlingCount:return{0x314,4};
case S::RetainDelay:return{0x318,2};
case S::SetDelayCount:return{0x31a,4};
case S::TickDelay:return{0x31e,2};
case S::RepeatDelay:return{0x320,2};
case S::ReleaseDelay:return{0x322,2};
case S::TickSettling:return{0x324,2};
case S::RepeatSettling:return{0x326,2};
case S::ReleaseSettling:return{0x328,2};
case S::ConfigureDisplay:return{0x32a,4};
case S::VerifyImageBounds:return{0x32e,4};
case S::ConfigureRuntimeA:return{0x332,4};
case S::ConfigureRuntimeB:return{0x336,4};
case S::ClearMarker:return{0x33a,2};
case S::PublishMarker:return{0x33c,6};
case S::InitializeProbe:return{0x342,4};
case S::ReadProbeMarker:return{0x346,6};
case S::CompareProbeMarker:return{0x34c,2};
case S::AdmitProbeMarker:return{0x34e,2};
case S::BindBootSource:return{0x350,6};
case S::BindBootTarget:return{0x356,6};
case S::SetBootCount:return{0x35c,4};
case S::ReadBootByte:return{0x360,2};
case S::WriteBootByte:return{0x362,2};
case S::AdvanceBootTarget:return{0x364,2};
case S::TickBootCount:return{0x366,2};
case S::RepeatBootCopy:return{0x368,2};
case S::AdmitOverlay:return{0x36a,4};
case S::ReadWarmMarker:return{0x36e,6};
case S::CompareWarmMarker:return{0x374,2};
case S::AdmitWarmUpdate:return{0x376,4};
case S::EnterPlayback:return{0x37a,4};
case S::BeginVerification:return{0x37e,2};
case S::PrepareVerification:return{0x380,2};
case S::BindSignature:return{0x382,6};
case S::BindStoredSignature:return{0x388,6};
case S::SetSignatureCount:return{0x38e,4};
case S::ReadSignature:return{0x392,2};
case S::ReadStoredSignature:return{0x394,2};
case S::CompareSignature:return{0x396,2};
case S::AdmitSignature:return{0x398,2};
case S::TickSignature:return{0x39a,2};
case S::RepeatSignature:return{0x39c,2};
case S::AdmitVerifiedSignature:return{0x39e,2};
case S::BindStoredImage:return{0x3a0,6};
case S::ReadImageSize:return{0x3a6,10};
case S::AdmitImageSize:return{0x3b0,2};
case S::MarkEmptyImage:return{0x3b2,2};
case S::AdmitEmptyImage:return{0x3b4,2};
case S::CountImageWords:return{0x3b6,2};
case S::ClearWordUpper:return{0x3b8,2};
case S::ClearChecksum:return{0x3ba,2};
case S::ReadImageWord:return{0x3bc,2};
case S::WriteImageWord:return{0x3be,8};
case S::AccumulateChecksum:return{0x3c6,2};
case S::TickImageWords:return{0x3c8,2};
case S::RepeatImageWords:return{0x3ca,2};
case S::ReadExpectedChecksum:return{0x3cc,4};
case S::WriteChecksum:return{0x3d0,10};
case S::SetTailCount:return{0x3da,2};
case S::ClearTailValue:return{0x3dc,2};
case S::WriteTailWord:return{0x3de,8};
case S::AdvanceTail:return{0x3e6,2};
case S::TickTail:return{0x3e8,2};
case S::RepeatTail:return{0x3ea,2};
case S::CompareChecksum:return{0x3ec,2};
case S::AdmitChecksum:return{0x3ee,2};
case S::ResetErrorDisplay:return{0x3f0,4};
case S::BindErrorMessage:return{0x3f4,6};
case S::PublishErrorMessage:return{0x3fa,4};
case S::HoldError:return{0x3fe,2};
case S::MaskForOverlay:return{0x400,4};
case S::BindOverlayHeader:return{0x404,6};
case S::BindOverlayTarget:return{0x40a,6};
case S::SetOverlayCount:return{0x410,4};
case S::ReadOverlayByte:return{0x414,2};
case S::WriteOverlayByte:return{0x416,2};
case S::AdvanceOverlayTarget:return{0x418,2};
case S::TickOverlayCount:return{0x41a,2};
case S::RepeatOverlayCopy:return{0x41c,2};
case S::EnterRelocatedOverlay:return{0x41e,4};
case S::SwitchOverlay:return{0xffffec00,4};
case S::SetOverlayMode:return{0xffffec04,2};
case S::PublishOverlayMode:return{0xffffec06,4};
case S::RestartWarm:return{0xffffec0a,4};
case S::BindUpdateMessage:return{0x430,6};
case S::PublishUpdateMessage:return{0x436,4};
case S::BindUpdateTarget:return{0x43a,6};
case S::BindUpdateSource:return{0x440,6};
case S::UpdateFirmware:return{0x446,4};
case S::BindDoneMessage:return{0x44a,6};
case S::PublishDoneMessage:return{0x450,4};
case S::HoldUpdate:return{0x454,2};
default:throw std::runtime_error("Invalid reset image stage");}}}
void setResetImagePartialForTests(ResetImagePartialTest fn){partialTest=fn;}
bool ResetImageCalls::step(ControllerCpu&h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());
 if(pc==0x2ec||pc==0x180&&(!task_||point(task_->stage()).location!=pc)||pc==0xffffec00&&(!task_||point(task_->stage()).location!=pc)){if(task_)throw std::runtime_error("Overlapping reset image lifecycle");task_.emplace(pc==0x2ec?T::Entry::Cold:pc==0x180?T::Entry::Warm:T::Entry::Relocated);++started;}
 if(!task_||point(task_->stage()).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto&task=*task_;if(task.awaiting())task.resumeChild();const auto stage=task.stage();const auto before=h.cycles;auto&r=h.regs;RuntimeBoundary f(h,pc,point(stage).length);const auto cost=task.nextClocks();
 task.advanceBefore(cost,[&](S what,unsigned index)->uint32_t{switch(what){
 case S::ReadProbeMarker:case S::ReadWarmMarker:return uint8_t(h.read8(0xffec33));
 case S::ReadBootByte:return uint8_t(h.read8(0x800000+index));
 case S::ReadSignature:return uint8_t(h.read8(0x190+index));
 case S::ReadStoredSignature:return uint8_t(h.read8(0x804190+index));
 case S::ReadImageSize:return h.read32(0x804184);
 case S::ReadImageWord:return h.read16(0x804000+index);
 case S::ReadExpectedChecksum:return h.read32(0x804000+index);
 case S::ReadOverlayByte:return uint8_t(h.read8(0x422+index));
 case S::SwitchOverlay:return uint8_t(h.read8(0xffff6f));
 default:throw std::runtime_error("Invalid reset read");}},[&](S what,unsigned index,uint32_t value){switch(what){
 case S::ClearWord:h.write16(int16_t(value),0xffec00+index*2);break;
 case S::PublishMarker:h.write8(int8_t(value),0xffec33);break;
 case S::WriteBootByte:h.write8(int8_t(value),0xa00000+index);break;
 case S::WriteImageWord:case S::WriteTailWord:h.write16(int16_t(value),0xa00000+index);break;
 case S::WriteChecksum:h.write32(int32_t(value),0xa00000+index);break;
 case S::WriteOverlayByte:h.write8(int8_t(value),0xffec00+index);break;
 case S::SwitchOverlay:h.write8(int8_t(value),0xffff6f);break;
 case S::PublishOverlayMode:h.write8(int8_t(value),0xfffed1);break;
 default:throw std::runtime_error("Invalid reset publication");}});
 switch(stage){
case S::EnterWarm:{f.jump(0x36e,true);break;}
case S::EstablishStack:{f.move(r[7].er,0xfffc00);break;}
case S::BindClear:{f.move(r[6].er,0xffec00);break;}
case S::SetClearCount:{f.move(r[0].er,0x800);break;}
case S::ClearWord:{value(h,r[0].e);break;}
case S::AdvanceClear:{r[6].er=int32_t(uint32_t(r[6].er)+2);break;}
case S::TickClear:{f.inc(r[0].r,-1);break;}
case S::RepeatClear:{f.branch(!(h.ccr&4),0x2fe);break;}
case S::ConfigurePins:{f.call(0x456);break;}
case S::InitializePanel:{f.call(0x68e);break;}
case S::ConfigureRuntime:{f.call(0x4d6);break;}
case S::RetainSettling:{f.push(r[0].r);break;}
case S::SetSettlingCount:{f.move(r[0].r,10);break;}
case S::RetainDelay:{f.push(r[0].r);break;}
case S::SetDelayCount:{f.move(r[0].r,0xa67);break;}
case S::TickDelay:{f.inc(r[0].r,-1);break;}
case S::RepeatDelay:{f.branch(!(h.ccr&4),0x31e);break;}
case S::ReleaseDelay:{f.pop(r[0].r);break;}
case S::TickSettling:{f.inc(r[0].r,-1);break;}
case S::RepeatSettling:{f.branch(!(h.ccr&4),0x318);break;}
case S::ReleaseSettling:{f.pop(r[0].r);break;}
case S::ConfigureDisplay:{f.call(0x926);break;}
case S::VerifyImageBounds:{f.call(0x540);break;}
case S::ConfigureRuntimeA:{f.call(0x4f0);break;}
case S::ConfigureRuntimeB:{f.call(0x506);break;}
case S::ClearMarker:{f.move(r[0].rl,0);break;}
case S::PublishMarker:{value(h,r[0].rl);break;}
case S::InitializeProbe:{f.call(0xd0c);break;}
case S::ReadProbeMarker:{f.move(r[0].rl,task.marker());break;}
case S::CompareProbeMarker:{h.sub8(r[0].rl,0x55);break;}
case S::AdmitProbeMarker:{f.branch(!(h.ccr&4),0x37e);break;}
case S::BindBootSource:{f.move(r[0].er,0x800000);break;}
case S::BindBootTarget:{f.move(r[1].er,0xa00000);break;}
case S::SetBootCount:{f.move(r[2].e,0x4000);break;}
case S::ReadBootByte:{f.move(r[2].rl,task.datum());++r[0].er;h.clockI(2);break;}
case S::WriteBootByte:{value(h,r[2].rl);break;}
case S::AdvanceBootTarget:{f.inc(r[1].er,1);break;}
case S::TickBootCount:{f.inc(r[2].e,-1);break;}
case S::RepeatBootCopy:{f.branch(!(h.ccr&4),0x360);break;}
case S::AdmitOverlay:{f.branch(true,0x400);break;}
case S::ReadWarmMarker:{f.move(r[0].rl,task.marker());break;}
case S::CompareWarmMarker:{h.sub8(r[0].rl,0x55);break;}
case S::AdmitWarmUpdate:{f.branch(bool(h.ccr&4),0x430);break;}
case S::EnterPlayback:{f.branch(true,0x1306);break;}
case S::BeginVerification:{(void)0;break;}
case S::PrepareVerification:{(void)0;break;}
case S::BindSignature:{f.move(r[0].er,0x190);break;}
case S::BindStoredSignature:{f.move(r[1].er,0x804190);break;}
case S::SetSignatureCount:{f.move(r[2].e,0x28);break;}
case S::ReadSignature:{f.move(r[2].rl,task.signature());++r[0].er;h.clockI(2);break;}
case S::ReadStoredSignature:{f.move(r[2].rh,task.datum());++r[1].er;h.clockI(2);break;}
case S::CompareSignature:{h.sub8(r[2].rl,r[2].rh);break;}
case S::AdmitSignature:{f.branch(!(h.ccr&4),0x39e);break;}
case S::TickSignature:{f.inc(r[2].e,-1);break;}
case S::RepeatSignature:{f.branch(!(h.ccr&4),0x392);break;}
case S::AdmitVerifiedSignature:{f.branch(!(h.ccr&4),0x3f0);break;}
case S::BindStoredImage:{f.move(r[1].er,0x804000);break;}
case S::ReadImageSize:{f.move(r[0].er,task.imageSize());break;}
case S::AdmitImageSize:{f.branch(!(h.ccr&4),0x3b6);break;}
case S::MarkEmptyImage:{f.move(r[0].rl,1);break;}
case S::AdmitEmptyImage:{f.branch(true,0x3ee);break;}
case S::CountImageWords:{f.shift(r[0].er,1,false);break;}
case S::ClearWordUpper:{r[2].e=h.sub16(r[2].e,r[2].e);break;}
case S::ClearChecksum:{r[3].er=h.sub32(r[3].er,r[3].er);break;}
case S::ReadImageWord:{f.move(r[2].r,task.datum());r[1].er+=2;h.clockI(2);break;}
case S::WriteImageWord:{value(h,r[2].r);break;}
case S::AccumulateChecksum:{r[3].er=h.add32(r[3].er,r[2].er);break;}
case S::TickImageWords:{f.inc(r[0].er,-1);break;}
case S::RepeatImageWords:{f.branch(!(h.ccr&4),0x3bc);break;}
case S::ReadExpectedChecksum:{f.move(r[2].er,task.expectedChecksum());r[1].er+=4;h.clockI(2);break;}
case S::WriteChecksum:{value(h,r[3].er);break;}
case S::SetTailCount:{f.move(r[0].rl,0xffffff80);break;}
case S::ClearTailValue:{r[0].e=h.sub16(r[0].e,r[0].e);break;}
case S::WriteTailWord:{value(h,r[0].e);break;}
case S::AdvanceTail:{r[1].er=int32_t(uint32_t(r[1].er)+2);break;}
case S::TickTail:{f.inc(r[0].rl,-1);break;}
case S::RepeatTail:{f.branch(!(h.ccr&4),0x3de);break;}
case S::CompareChecksum:{h.sub32(r[3].er,r[2].er);break;}
case S::AdmitChecksum:{f.branch(bool(h.ccr&4),0x400);break;}
case S::ResetErrorDisplay:{f.call(0x926);break;}
case S::BindErrorMessage:{f.move(r[6].er,0x2c5);break;}
case S::PublishErrorMessage:{f.call(0x9b8);break;}
case S::HoldError:{f.branch(true,0x3fe);break;}
case S::MaskForOverlay:{h.exr=7;break;}
case S::BindOverlayHeader:{f.move(r[0].er,0x422);break;}
case S::BindOverlayTarget:{f.move(r[1].er,0xffec00);break;}
case S::SetOverlayCount:{f.move(r[2].e,14);break;}
case S::ReadOverlayByte:{f.move(r[2].rl,task.datum());++r[0].er;h.clockI(2);break;}
case S::WriteOverlayByte:{value(h,r[2].rl);break;}
case S::AdvanceOverlayTarget:{f.inc(r[1].er,1);break;}
case S::TickOverlayCount:{f.inc(r[2].e,-1);break;}
case S::RepeatOverlayCopy:{f.branch(!(h.ccr&4),0x414);break;}
case S::EnterRelocatedOverlay:{f.jump(0xffffec00,true);break;}
case S::SwitchOverlay:{;break;}
case S::SetOverlayMode:{f.move(r[0].rl,0xffffffd2);break;}
case S::PublishOverlayMode:{value(h,r[0].rl);break;}
case S::RestartWarm:{f.jump(0x180,true);break;}
case S::BindUpdateMessage:{f.move(r[6].er,0x2b2);break;}
case S::PublishUpdateMessage:{f.call(0x9b8);break;}
case S::BindUpdateTarget:{f.move(r[5].er,0xa04000);break;}
case S::BindUpdateSource:{f.move(r[6].er,0x804000);break;}
case S::UpdateFirmware:{f.call(0xdfa);break;}
case S::BindDoneMessage:{f.move(r[6].er,0x298);break;}
case S::PublishDoneMessage:{f.call(0x9b8);break;}
case S::HoldUpdate:{f.branch(true,0x454);break;}
 default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Reset image timing differs");
 if(task.playback()){h.pc=h.makepc(f.next);task_.reset();++completed;}
 else if(task.awaiting())h.pc=h.makepc(f.next);else h.pc=h.makepc(point(task.stage()).location);return true;
}}
