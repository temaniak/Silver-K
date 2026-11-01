#pragma once
#include <cstdint>
namespace ks {
// Native reset, image admission/copy/checksum and overlay lifecycle. Numeric
// image offsets belong to the Board adapter; this task owns logical cursors,
// verification results and ordered service requests, without CPU state.
class ResetImageTask final {public:
 enum class Stage {EnterWarm,EstablishStack,BindClear,SetClearCount,ClearWord,AdvanceClear,TickClear,RepeatClear,ConfigurePins,InitializePanel,ConfigureRuntime,RetainSettling,SetSettlingCount,RetainDelay,SetDelayCount,TickDelay,RepeatDelay,ReleaseDelay,TickSettling,RepeatSettling,ReleaseSettling,ConfigureDisplay,VerifyImageBounds,ConfigureRuntimeA,ConfigureRuntimeB,ClearMarker,PublishMarker,InitializeProbe,ReadProbeMarker,CompareProbeMarker,AdmitProbeMarker,BindBootSource,BindBootTarget,SetBootCount,ReadBootByte,WriteBootByte,AdvanceBootTarget,TickBootCount,RepeatBootCopy,AdmitOverlay,ReadWarmMarker,CompareWarmMarker,AdmitWarmUpdate,EnterPlayback,BeginVerification,PrepareVerification,BindSignature,BindStoredSignature,SetSignatureCount,ReadSignature,ReadStoredSignature,CompareSignature,AdmitSignature,TickSignature,RepeatSignature,AdmitVerifiedSignature,BindStoredImage,ReadImageSize,AdmitImageSize,MarkEmptyImage,AdmitEmptyImage,CountImageWords,ClearWordUpper,ClearChecksum,ReadImageWord,WriteImageWord,AccumulateChecksum,TickImageWords,RepeatImageWords,ReadExpectedChecksum,WriteChecksum,SetTailCount,ClearTailValue,WriteTailWord,AdvanceTail,TickTail,RepeatTail,CompareChecksum,AdmitChecksum,ResetErrorDisplay,BindErrorMessage,PublishErrorMessage,HoldError,MaskForOverlay,BindOverlayHeader,BindOverlayTarget,SetOverlayCount,ReadOverlayByte,WriteOverlayByte,AdvanceOverlayTarget,TickOverlayCount,RepeatOverlayCopy,EnterRelocatedOverlay,SwitchOverlay,SetOverlayMode,PublishOverlayMode,RestartWarm,BindUpdateMessage,PublishUpdateMessage,BindUpdateTarget,BindUpdateSource,UpdateFirmware,BindDoneMessage,PublishDoneMessage,HoldUpdate,Playback};
 enum class Child {None,Pins,Panel,Runtime,Display,ImageBounds,RuntimeA,RuntimeB,Probe,ErrorMessage,UpdateMessage,FirmwareUpdate,DoneMessage};
 enum class Entry {Cold,Warm,Relocated};
 explicit ResetImageTask(Entry entry=Entry::Cold):stage_(entry==Entry::Cold?Stage::EstablishStack:entry==Entry::Warm?Stage::EnterWarm:Stage::SwitchOverlay){}
 Stage stage()const{return stage_;}Child child()const{return child_;}bool awaiting()const{return child_!=Child::None;}bool playback()const{return stage_==Stage::Playback;}
 bool resumeChild(){if(!awaiting())return false;child_=Child::None;return true;}
 unsigned clearIndex()const{return clearIndex_;}unsigned sourceIndex()const{return sourceIndex_;}unsigned targetIndex()const{return targetIndex_;}uint32_t remaining()const{return remaining_;}uint32_t datum()const{return data_;}uint32_t checksum()const{return checksum_;}uint32_t imageSize()const{return size_;}uint8_t marker()const{return marker_;}uint8_t signature()const{return signature_;}uint32_t expectedChecksum()const{return expected_;}
 unsigned nextClocks()const{if(awaiting())return 0;switch(stage_){
case Stage::EnterWarm:return 8;
case Stage::EstablishStack:return 9;
case Stage::BindClear:return 9;
case Stage::SetClearCount:return 9;
case Stage::ClearWord:return 9;
case Stage::AdvanceClear:return 3;
case Stage::TickClear:return 3;
case Stage::RepeatClear:return 3;
case Stage::ConfigurePins:return 20;
case Stage::InitializePanel:return 20;
case Stage::ConfigureRuntime:return 20;
case Stage::RetainSettling:return 11;
case Stage::SetSettlingCount:return 6;
case Stage::RetainDelay:return 11;
case Stage::SetDelayCount:return 6;
case Stage::TickDelay:return 3;
case Stage::RepeatDelay:return 3;
case Stage::ReleaseDelay:return 11;
case Stage::TickSettling:return 3;
case Stage::RepeatSettling:return 3;
case Stage::ReleaseSettling:return 11;
case Stage::ConfigureDisplay:return 20;
case Stage::VerifyImageBounds:return 20;
case Stage::ConfigureRuntimeA:return 20;
case Stage::ConfigureRuntimeB:return 20;
case Stage::ClearMarker:return 3;
case Stage::PublishMarker:return 12;
case Stage::InitializeProbe:return 20;
case Stage::ReadProbeMarker:return 12;
case Stage::CompareProbeMarker:return 3;
case Stage::AdmitProbeMarker:return 3;
case Stage::BindBootSource:return 9;
case Stage::BindBootTarget:return 9;
case Stage::SetBootCount:return 6;
case Stage::ReadBootByte:return 8;
case Stage::WriteBootByte:return 6;
case Stage::AdvanceBootTarget:return 3;
case Stage::TickBootCount:return 3;
case Stage::RepeatBootCopy:return 3;
case Stage::AdmitOverlay:return 8;
case Stage::ReadWarmMarker:return 12;
case Stage::CompareWarmMarker:return 3;
case Stage::AdmitWarmUpdate:return 8;
case Stage::EnterPlayback:return 8;
case Stage::BeginVerification:return 3;
case Stage::PrepareVerification:return 3;
case Stage::BindSignature:return 9;
case Stage::BindStoredSignature:return 9;
case Stage::SetSignatureCount:return 6;
case Stage::ReadSignature:return 8;
case Stage::ReadStoredSignature:return 8;
case Stage::CompareSignature:return 3;
case Stage::AdmitSignature:return 3;
case Stage::TickSignature:return 3;
case Stage::RepeatSignature:return 3;
case Stage::AdmitVerifiedSignature:return 3;
case Stage::BindStoredImage:return 9;
case Stage::ReadImageSize:return 27;
case Stage::AdmitImageSize:return 3;
case Stage::MarkEmptyImage:return 3;
case Stage::AdmitEmptyImage:return 3;
case Stage::CountImageWords:return 3;
case Stage::ClearWordUpper:return 3;
case Stage::ClearChecksum:return 3;
case Stage::ReadImageWord:return 11;
case Stage::WriteImageWord:return 18;
case Stage::AccumulateChecksum:return 3;
case Stage::TickImageWords:return 3;
case Stage::RepeatImageWords:return 3;
case Stage::ReadExpectedChecksum:return 20;
case Stage::WriteChecksum:return 27;
case Stage::SetTailCount:return 3;
case Stage::ClearTailValue:return 3;
case Stage::WriteTailWord:return 18;
case Stage::AdvanceTail:return 3;
case Stage::TickTail:return 3;
case Stage::RepeatTail:return 3;
case Stage::CompareChecksum:return 3;
case Stage::AdmitChecksum:return 3;
case Stage::ResetErrorDisplay:return 20;
case Stage::BindErrorMessage:return 9;
case Stage::PublishErrorMessage:return 20;
case Stage::HoldError:return 3;
case Stage::MaskForOverlay:return 6;
case Stage::BindOverlayHeader:return 9;
case Stage::BindOverlayTarget:return 9;
case Stage::SetOverlayCount:return 6;
case Stage::ReadOverlayByte:return 8;
case Stage::WriteOverlayByte:return 6;
case Stage::AdvanceOverlayTarget:return 3;
case Stage::TickOverlayCount:return 3;
case Stage::RepeatOverlayCopy:return 3;
case Stage::EnterRelocatedOverlay:return 8;
case Stage::SwitchOverlay:return 14;
case Stage::SetOverlayMode:return 6;
case Stage::PublishOverlayMode:return 13;
case Stage::RestartWarm:return 14;
case Stage::BindUpdateMessage:return 9;
case Stage::PublishUpdateMessage:return 20;
case Stage::BindUpdateTarget:return 9;
case Stage::BindUpdateSource:return 9;
case Stage::UpdateFirmware:return 20;
case Stage::BindDoneMessage:return 9;
case Stage::PublishDoneMessage:return 20;
case Stage::HoldUpdate:return 3;
case Stage::Playback:return 0;}return 0;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write){const auto cost=nextClocks();if(!cost||available<cost)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){
case Stage::EnterWarm:{next=Stage::ReadWarmMarker;break;}
case Stage::BindClear:{clearIndex_=0;break;}
case Stage::SetClearCount:{remaining_=2048;break;}
case Stage::ClearWord:{write(stage_,clearIndex_,0);break;}
case Stage::AdvanceClear:{++clearIndex_;break;}
case Stage::TickClear:{--remaining_;break;}
case Stage::RepeatClear:{if(remaining_)next=Stage::ClearWord;break;}
case Stage::SetSettlingCount:{settling_=10;break;}
case Stage::SetDelayCount:{delay_=2663;break;}
case Stage::TickDelay:{--delay_;break;}
case Stage::RepeatDelay:{if(delay_)next=Stage::TickDelay;break;}
case Stage::TickSettling:{--settling_;break;}
case Stage::RepeatSettling:{if(settling_)next=Stage::RetainDelay;break;}
case Stage::ClearMarker:{marker_=0;break;}
case Stage::PublishMarker:{write(stage_,0,marker_);break;}
case Stage::ReadProbeMarker:{marker_=uint8_t(read(stage_,0));break;}
case Stage::AdmitProbeMarker:{if(marker_!=0x55)next=Stage::BeginVerification;break;}
case Stage::BindBootSource:{sourceIndex_=0;break;}
case Stage::BindBootTarget:{targetIndex_=0;break;}
case Stage::SetBootCount:{remaining_=16384;break;}
case Stage::ReadBootByte:{data_=uint8_t(read(stage_,sourceIndex_));++sourceIndex_;break;}
case Stage::WriteBootByte:{write(stage_,targetIndex_,data_);break;}
case Stage::AdvanceBootTarget:{++targetIndex_;break;}
case Stage::TickBootCount:{--remaining_;break;}
case Stage::RepeatBootCopy:{if(remaining_)next=Stage::ReadBootByte;break;}
case Stage::AdmitOverlay:{next=Stage::MaskForOverlay;break;}
case Stage::ReadWarmMarker:{marker_=uint8_t(read(stage_,0));break;}
case Stage::AdmitWarmUpdate:{if(marker_==0x55)next=Stage::BindUpdateMessage;break;}
case Stage::EnterPlayback:{next=Stage::Playback;break;}
case Stage::BindSignature:{sourceIndex_=0;break;}
case Stage::BindStoredSignature:{targetIndex_=0;break;}
case Stage::SetSignatureCount:{remaining_=40;break;}
case Stage::ReadSignature:{signature_=uint8_t(read(stage_,sourceIndex_));++sourceIndex_;break;}
case Stage::ReadStoredSignature:{data_=uint8_t(read(stage_,targetIndex_));++targetIndex_;break;}
case Stage::CompareSignature:{verified_=signature_==uint8_t(data_);break;}
case Stage::AdmitSignature:{if(!verified_)next=Stage::AdmitVerifiedSignature;break;}
case Stage::TickSignature:{--remaining_;break;}
case Stage::RepeatSignature:{if(remaining_)next=Stage::ReadSignature;break;}
case Stage::AdmitVerifiedSignature:{if(!verified_)next=Stage::ResetErrorDisplay;break;}
case Stage::BindStoredImage:{sourceIndex_=0;break;}
case Stage::ReadImageSize:{size_=uint32_t(read(stage_,0));break;}
case Stage::AdmitImageSize:{if(size_)next=Stage::CountImageWords;break;}
case Stage::MarkEmptyImage:{checksumMatches_=false;break;}
case Stage::AdmitEmptyImage:{next=Stage::AdmitChecksum;break;}
case Stage::CountImageWords:{remaining_=size_>>1;break;}
case Stage::ClearChecksum:{checksum_=0;break;}
case Stage::ReadImageWord:{data_=uint16_t(read(stage_,sourceIndex_));sourceIndex_+=2;break;}
case Stage::WriteImageWord:{write(stage_,sourceIndex_-2,data_);break;}
case Stage::AccumulateChecksum:{checksum_+=uint16_t(data_);break;}
case Stage::TickImageWords:{--remaining_;break;}
case Stage::RepeatImageWords:{if(remaining_)next=Stage::ReadImageWord;break;}
case Stage::ReadExpectedChecksum:{expected_=uint32_t(read(stage_,sourceIndex_));sourceIndex_+=4;break;}
case Stage::WriteChecksum:{write(stage_,sourceIndex_-4,checksum_);break;}
case Stage::SetTailCount:{tail_=128;break;}
case Stage::WriteTailWord:{write(stage_,sourceIndex_,0);break;}
case Stage::AdvanceTail:{sourceIndex_+=2;break;}
case Stage::TickTail:{--tail_;break;}
case Stage::RepeatTail:{if(tail_)next=Stage::WriteTailWord;break;}
case Stage::CompareChecksum:{checksumMatches_=checksum_==expected_;break;}
case Stage::AdmitChecksum:{if(checksumMatches_)next=Stage::MaskForOverlay;break;}
case Stage::HoldError:{next=Stage::HoldError;break;}
case Stage::BindOverlayHeader:{sourceIndex_=0;break;}
case Stage::BindOverlayTarget:{targetIndex_=0;break;}
case Stage::SetOverlayCount:{remaining_=14;break;}
case Stage::ReadOverlayByte:{data_=uint8_t(read(stage_,sourceIndex_));++sourceIndex_;break;}
case Stage::WriteOverlayByte:{write(stage_,targetIndex_,data_);break;}
case Stage::AdvanceOverlayTarget:{++targetIndex_;break;}
case Stage::TickOverlayCount:{--remaining_;break;}
case Stage::RepeatOverlayCopy:{if(remaining_)next=Stage::ReadOverlayByte;break;}
case Stage::EnterRelocatedOverlay:{next=Stage::SwitchOverlay;break;}
case Stage::SwitchOverlay:{data_=uint8_t(read(stage_,0))&uint8_t(~1u);write(stage_,0,data_);break;}
case Stage::SetOverlayMode:{overlayMode_=0xd2;break;}
case Stage::PublishOverlayMode:{write(stage_,0,overlayMode_);break;}
case Stage::RestartWarm:{next=Stage::EnterWarm;break;}
case Stage::HoldUpdate:{next=Stage::HoldUpdate;break;}
case Stage::ConfigurePins:{child_=Child::Pins;break;}
case Stage::InitializePanel:{child_=Child::Panel;break;}
case Stage::ConfigureRuntime:{child_=Child::Runtime;break;}
case Stage::ConfigureDisplay:{child_=Child::Display;break;}
case Stage::VerifyImageBounds:{child_=Child::ImageBounds;break;}
case Stage::ConfigureRuntimeA:{child_=Child::RuntimeA;break;}
case Stage::ConfigureRuntimeB:{child_=Child::RuntimeB;break;}
case Stage::InitializeProbe:{child_=Child::Probe;break;}
case Stage::ResetErrorDisplay:{child_=Child::Display;break;}
case Stage::PublishErrorMessage:{child_=Child::ErrorMessage;break;}
case Stage::PublishUpdateMessage:{child_=Child::UpdateMessage;break;}
case Stage::UpdateFirmware:{child_=Child::FirmwareUpdate;break;}
case Stage::PublishDoneMessage:{child_=Child::DoneMessage;break;}
default:break;}stage_=next;return cost;}
private:Stage stage_;Child child_=Child::None;uint32_t clearIndex_=0,sourceIndex_=0,targetIndex_=0,remaining_=0,size_=0,data_=0,checksum_=0,expected_=0;uint16_t delay_=0;uint8_t settling_=0,tail_=0,marker_=0,signature_=0,overlayMode_=0;bool verified_=false,checksumMatches_=false;
};}
