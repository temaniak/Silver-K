#pragma once
#include <cstdint>
namespace ks {
// Screen descriptor selection, bounded page navigation and label/value lifecycle.
// Firmware data references remain opaque; observation and publication stay live.
class StateScreenTask final {
public:
 enum class Kind { Descriptor, Set, Next, Previous, Refresh, Gate, Render, Label };
 enum class Stage { CheckDescriptorIndex, SelectDescriptorIndex, ReadDescriptorMode, SelectDescriptorMode, IncrementDescriptorIndex, DoubleDescriptorIndex, QuadrupleDescriptorIndex, ClearDescriptorWord, ClearDescriptorLong, LocatePrimaryTable, ReadPrimaryPointer, ReadDescriptorHardware, SelectDescriptorHardware, CompareHardwareDescriptor, SelectHardwareDescriptor, RemapHardwareDescriptor, ReadHeader, CompareSentinel, SelectSentinel, ReadPatchPointer, ReadVariant, RetainVariant, ScaleVariant, ClearVariantWord, ClearVariantLong, LocateSecondaryTable, ReadSecondaryPointer, CompareDynamicVariant, SelectDynamicVariant, BindDynamicCount, ReadDynamicMode, SelectDynamicMode, IncrementDynamicCount, WriteDynamicCount, DecrementDynamicCount, ReadDynamicPage, CompareDynamicPage, SelectDynamicPage, ClampDynamicPage, ReadCount, ReadCurrent, ReturnDescriptor, WriteScreen, BindScreen, LookupSet, BindResetBase, ReadReset, SelectReset, ResetPage, JoinSet, ReadNextScreen, LookupNext, IncrementPage, ComparePage, SelectNextLimit, BindPageLimit, DecrementPageLimit, JoinNext, ReadPreviousScreen, LookupPrevious, DecrementPage, SelectPreviousLimit, FloorPage, WriteCurrent, IncrementCurrentAddress, WriteUiPage, RenderSelection, RefreshGate, SelectRefreshGate, InitializeCalibration, ReturnRefresh, ClearGate, ReadGateHardware, SelectGateHardware, ReadGateScreen, ReadGatePage, CompareGateLocation, SelectGateLocation, CheckCalibration, SelectCalibration, AcceptGate, TestGate, ReturnGate, BindHeading, DisplayHeading, ReadRenderScreen, LookupRender, IncrementRecordBase, WriteRenderPage, BindRecordSize, MultiplyRecordIndex, ClearRecordIndex, LocateRecord, WriteRecordPointer, ReadParameter, ClearParameterWord, ClearParameterLong, BindParameter, DisplayLabel, ClearParameterBase, ReturnRender, ReadRecordPointer, BindLabelPosition, SetLabelPosition, BindLabelLength, ReadCharacter, CompareCharacter, SelectCharacter, DisplayCharacter, DecrementLabelLength, RepeatLabel, BindSeparator, DisplaySeparator, ReadLabelParameter, ReadFormat, DisplayValue, ReturnLabel, AwaitChild, Complete };
 enum class Read { Mode, Hardware, PrimaryPointer, SecondaryPointer, DescriptorByte, PatchPointer, Variant, ResetFlag, Screen, UiPage, RecordPointer };
 enum class Write { DescriptorByte, Screen, UiPage, RecordPointer };
 struct Reply { uint32_t descriptor;uint8_t count,page;bool nonzero; };
 explicit StateScreenTask(Kind kind,uint8_t index=0):index_(index){switch(kind){case Kind::Descriptor:stage_=Stage::CheckDescriptorIndex;break;case Kind::Set:stage_=Stage::WriteScreen;break;case Kind::Next:stage_=Stage::ReadNextScreen;break;case Kind::Previous:stage_=Stage::ReadPreviousScreen;break;case Kind::Refresh:stage_=Stage::RefreshGate;break;case Kind::Gate:stage_=Stage::ClearGate;break;case Kind::Render:stage_=Stage::BindHeading;break;case Kind::Label:stage_=Stage::ReadRecordPointer;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(Reply reply){if(!awaitingChild())return false;switch(resume_){case Stage::BindResetBase:case Stage::IncrementPage:case Stage::DecrementPage:case Stage::IncrementRecordBase:pointer_=reply.descriptor;count_=reply.count;page_=reply.page;break;case Stage::SelectRefreshGate:case Stage::SelectCalibration:accepted_=reply.nonzero;break;default:break;}stage_=resume_;return true;}
 uint8_t index()const{return index_;}uint8_t mode()const{return mode_;}uint8_t hardware()const{return hardware_;}uint32_t pointer()const{return pointer_;}uint32_t patch()const{return patch_;}uint8_t header()const{return header_;}uint8_t variant()const{return variant_;}uint8_t count()const{return count_;}uint8_t page()const{return page_;}uint8_t reset()const{return reset_;}uint8_t screen()const{return screen_;}bool accepted()const{return accepted_;}uint8_t parameter()const{return parameter_;}uint8_t character()const{return character_;}uint8_t format()const{return format_;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadDescriptorMode:case Stage::ReadPrimaryPointer:case Stage::ReadDescriptorHardware:case Stage::ReadSecondaryPointer:case Stage::ReadDynamicMode:case Stage::ReadNextScreen:case Stage::ReadPreviousScreen:case Stage::ReadGateHardware:case Stage::ReadGateScreen:case Stage::ReadGatePage:case Stage::ReadRenderScreen:return 12; case Stage::LocatePrimaryTable:case Stage::CompareHardwareDescriptor:case Stage::RemapHardwareDescriptor:case Stage::LocateSecondaryTable:case Stage::ReadDynamicPage:case Stage::BindResetBase:case Stage::BindHeading:case Stage::ReadFormat:return 9; case Stage::ReadHeader:case Stage::ReadCurrent:case Stage::CompareGateLocation:return 6; case Stage::ReadPatchPointer:case Stage::WriteRecordPointer:case Stage::ReadRecordPointer:return 18; case Stage::ReadVariant:case Stage::ReadReset:case Stage::RenderSelection:case Stage::RefreshGate:case Stage::MultiplyRecordIndex:case Stage::ReadParameter:case Stage::DisplayLabel:return 15; case Stage::WriteDynamicCount:case Stage::WriteCurrent:return 3+store; case Stage::ClampDynamicPage:return 6+store; case Stage::ReadCount:case Stage::ReadCharacter:case Stage::ReadLabelParameter:return 8; case Stage::ReturnDescriptor:case Stage::ReturnRefresh:case Stage::ReturnGate:case Stage::ReturnRender:case Stage::ReturnLabel:return 17; case Stage::WriteScreen:case Stage::WriteUiPage:case Stage::WriteRenderPage:return 9+store; case Stage::LookupSet:case Stage::LookupNext:case Stage::LookupPrevious:case Stage::InitializeCalibration:case Stage::CheckCalibration:case Stage::DisplayHeading:case Stage::LookupRender:case Stage::BindParameter:case Stage::ClearParameterBase:case Stage::SetLabelPosition:case Stage::DisplayCharacter:case Stage::DisplaySeparator:case Stage::DisplayValue:return 20; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReturnDescriptor:stage_=Stage::Complete;break;
case Stage::LookupSet:await(Stage::BindResetBase);break;
case Stage::LookupNext:await(Stage::IncrementPage);break;
case Stage::LookupPrevious:await(Stage::DecrementPage);break;
case Stage::RenderSelection:await(Stage::RefreshGate);break;
case Stage::RefreshGate:await(Stage::SelectRefreshGate);break;
case Stage::InitializeCalibration:await(Stage::ReturnRefresh);break;
case Stage::ReturnRefresh:stage_=Stage::Complete;break;
case Stage::CheckCalibration:await(Stage::SelectCalibration);break;
case Stage::ReturnGate:stage_=Stage::Complete;break;
case Stage::DisplayHeading:await(Stage::ReadRenderScreen);break;
case Stage::LookupRender:await(Stage::IncrementRecordBase);break;
case Stage::BindParameter:await(Stage::DisplayLabel);break;
case Stage::DisplayLabel:await(Stage::ClearParameterBase);break;
case Stage::ClearParameterBase:await(Stage::ReturnRender);break;
case Stage::ReturnRender:stage_=Stage::Complete;break;
case Stage::SetLabelPosition:await(Stage::BindLabelLength);break;
case Stage::DisplayCharacter:await(Stage::DecrementLabelLength);break;
case Stage::DisplaySeparator:await(Stage::ReadLabelParameter);break;
case Stage::DisplayValue:await(Stage::ReturnLabel);break;
case Stage::ReturnLabel:stage_=Stage::Complete;break;
case Stage::SelectDescriptorIndex:stage_=index_!=12?Stage::DoubleDescriptorIndex:Stage::ReadDescriptorMode;break;
case Stage::ReadDescriptorMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectDescriptorMode:stage_=mode_?Stage::DoubleDescriptorIndex:Stage::IncrementDescriptorIndex;break;
case Stage::IncrementDescriptorIndex:++index_;next();break;
case Stage::DoubleDescriptorIndex:index_=uint8_t(index_<<1);next();break;
case Stage::QuadrupleDescriptorIndex:index_=uint8_t(index_<<1);next();break;
case Stage::ReadPrimaryPointer:pointer_=read(Read::PrimaryPointer,index_);next();break;
case Stage::ReadDescriptorHardware:hardware_=uint8_t(read(Read::Hardware,0));next();break;
case Stage::SelectDescriptorHardware:stage_=hardware_?Stage::CompareHardwareDescriptor:Stage::ReadHeader;break;
case Stage::SelectHardwareDescriptor:stage_=pointer_==0x10517?Stage::RemapHardwareDescriptor:Stage::ReadHeader;break;
case Stage::RemapHardwareDescriptor:pointer_=0x10569;next();break;
case Stage::ReadHeader:header_=uint8_t(read(Read::DescriptorByte,pointer_));next();break;
case Stage::SelectSentinel:stage_=header_==255?Stage::ReadPatchPointer:Stage::ReadCount;break;
case Stage::ReadPatchPointer:patch_=read(Read::PatchPointer,0);next();break;
case Stage::ReadVariant:variant_=uint8_t(read(Read::Variant,patch_));next();break;
case Stage::ScaleVariant:index_=uint8_t(variant_<<2);next();break;
case Stage::ReadSecondaryPointer:pointer_=read(Read::SecondaryPointer,index_);next();break;
case Stage::SelectDynamicVariant:stage_=variant_==6?Stage::BindDynamicCount:Stage::ReadCount;break;
case Stage::BindDynamicCount:count_=3;next();break;
case Stage::ReadDynamicMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectDynamicMode:stage_=mode_?Stage::IncrementDynamicCount:Stage::WriteDynamicCount;break;
case Stage::IncrementDynamicCount:++count_;next();break;
case Stage::WriteDynamicCount:write(Write::DescriptorByte,pointer_,count_);next();break;
case Stage::DecrementDynamicCount:--count_;next();break;
case Stage::ReadDynamicPage:page_=uint8_t(read(Read::DescriptorByte,pointer_+1));next();break;
case Stage::SelectDynamicPage:stage_=page_<=count_?Stage::ReadCount:Stage::ClampDynamicPage;break;
case Stage::ClampDynamicPage:write(Write::DescriptorByte,pointer_+1,count_);next();break;
case Stage::ReadCount:count_=uint8_t(read(Read::DescriptorByte,pointer_++));next();break;
case Stage::ReadCurrent:page_=uint8_t(read(Read::DescriptorByte,pointer_));next();break;
case Stage::WriteScreen:write(Write::Screen,0,index_);next();break;
case Stage::ReadReset:reset_=uint8_t(read(Read::ResetFlag,0));next();break;
case Stage::SelectReset:stage_=reset_?Stage::ResetPage:Stage::JoinSet;break;
case Stage::ResetPage:page_=0;next();break;
case Stage::JoinSet:stage_=Stage::WriteCurrent;break;
case Stage::ReadNextScreen:index_=uint8_t(read(Read::Screen,0));next();break;
case Stage::IncrementPage:++page_;next();break;
case Stage::SelectNextLimit:stage_=page_<count_?Stage::JoinNext:Stage::BindPageLimit;break;
case Stage::BindPageLimit:page_=count_;next();break;
case Stage::DecrementPageLimit:--page_;next();break;
case Stage::JoinNext:stage_=Stage::WriteCurrent;break;
case Stage::ReadPreviousScreen:index_=uint8_t(read(Read::Screen,0));next();break;
case Stage::DecrementPage:previousNegative_=page_==0||page_>=128;--page_;next();break;
case Stage::SelectPreviousLimit:stage_=previousNegative_?Stage::FloorPage:Stage::WriteCurrent;break;
case Stage::FloorPage:page_=0;next();break;
case Stage::WriteCurrent:write(Write::DescriptorByte,pointer_,page_);next();break;
case Stage::IncrementCurrentAddress:++pointer_;next();break;
case Stage::WriteUiPage:write(Write::UiPage,0,page_);next();break;
case Stage::SelectRefreshGate:stage_=accepted_?Stage::InitializeCalibration:Stage::ReturnRefresh;break;
case Stage::ClearGate:accepted_=false;next();break;
case Stage::ReadGateHardware:hardware_=uint8_t(read(Read::Hardware,0));next();break;
case Stage::SelectGateHardware:stage_=hardware_?Stage::TestGate:Stage::ReadGateScreen;break;
case Stage::ReadGateScreen:screen_=uint8_t(read(Read::Screen,0));next();break;
case Stage::ReadGatePage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectGateLocation:stage_=screen_==9&&page_==2?Stage::CheckCalibration:Stage::TestGate;break;
case Stage::SelectCalibration:stage_=accepted_?Stage::AcceptGate:Stage::TestGate;break;
case Stage::AcceptGate:accepted_=true;next();break;
case Stage::ReadRenderScreen:index_=uint8_t(read(Read::Screen,0));next();break;
case Stage::IncrementRecordBase:++pointer_;next();break;
case Stage::WriteRenderPage:write(Write::UiPage,0,page_);next();break;
case Stage::LocateRecord:pointer_+=unsigned(page_)*20;next();break;
case Stage::WriteRecordPointer:write(Write::RecordPointer,0,pointer_);next();break;
case Stage::ReadParameter:parameter_=uint8_t(read(Read::DescriptorByte,pointer_+16));next();break;
case Stage::ReadRecordPointer:pointer_=read(Read::RecordPointer,0);next();break;
case Stage::BindLabelLength:remaining_=16;next();break;
case Stage::ReadCharacter:character_=uint8_t(read(Read::DescriptorByte,pointer_++));next();break;
case Stage::SelectCharacter:stage_=character_==255?Stage::DecrementLabelLength:Stage::DisplayCharacter;break;
case Stage::DecrementLabelLength:--remaining_;next();break;
case Stage::RepeatLabel:stage_=remaining_?Stage::ReadCharacter:Stage::BindSeparator;break;
case Stage::ReadLabelParameter:parameter_=uint8_t(read(Read::DescriptorByte,pointer_++));next();break;
case Stage::ReadFormat:format_=uint8_t(read(Read::DescriptorByte,pointer_+2));next();break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t pointer_=0,patch_=0;uint8_t index_=0,mode_=0,hardware_=0,header_=0,variant_=0,count_=0,page_=0,reset_=0,screen_=0,parameter_=0,character_=0,format_=0,remaining_=0;bool accepted_=false,previousNegative_=false;
};
}
