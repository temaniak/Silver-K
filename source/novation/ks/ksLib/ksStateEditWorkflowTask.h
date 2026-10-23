#pragma once
#include <cstdint>
namespace ks {
// Edit page restore/selection, live encoder routing, audition notes and descriptor-backed parameter dispatch. Handler continuations remain in the bridge.
class StateEditWorkflowTask final {
public:
 enum class Kind { Restore, Select, SelectNote, PanelEvent, Velocity, NoteOn, NoteOff, ResolveNote, DirectEdit, Publish, AutomaticEdit, AutomaticEvent, ManualEdit, EditActive, ApplyVelocity };enum class Stage { BindRestoreScreen, ReadRestoreScreen, WriteRestoreScreen, BindRestorePage, ReadRestorePage, WriteRestorePage, ReturnRestore, WriteSelectArmed, SelectArmed, ReadSelectOther, SelectOther, TestSelectOther, SelectOtherName, JoinSelectOther, ReadSelectPage, TestSelectPage, SelectPageName, SelectPageRange, ReadSelectGroup, TestSelectGroup, SelectGroupRange, ReadSelectRouting, WriteSelectRouting, RefreshSelectRouting, LoadSelectProgram, ResetSelectPlayback, ReturnSelect, ReadNoteSelectPage, TestNoteSelectPage, SelectNoteSelectPage, ReadNoteSelectArmed, SelectNoteSelectArmed, TestNoteSelectLow, SelectNoteSelectLow, TestNoteSelectHigh, SelectNoteSelectHigh, SubtractNoteSelectBase, WriteNoteSelect, RefreshNoteSelect, LoadNoteSelect, ResetNoteSelect, ReturnNoteSelect, ObservePanelCalibration, SelectPanelCalibration, ReadPanelEncoder, TestPanelEncoder, SelectPanelEncoder, TestPanelSpecialOne, SelectPanelSpecialOne, TestPanelSpecialTwo, SelectPanelSpecialTwo, TestPanelSpecialThree, SelectPanelSpecialThree, TestPanelSpecialFour, SelectPanelSpecialFour, ApplyPanelParameter, ReturnPanelEvent, ScalePanelSpecialTwo, EnterPanelSpecialTwo, SaveVelocityEvent, BindVelocityValue, SquareVelocityValue, DoubleVelocityValue, BindVelocityCurve, ClearVelocityCurveHigh, BindVelocityScale, ApplyVelocityScale, RestoreVelocityEvent, ReturnVelocity, ReadNoteOnPage, SelectNoteOnPage, CloseNoteOnPage, ResolveNoteOn, TestNoteOn, SelectNoteOn, ReadNoteOnFallback, AddNoteOnBase, BindNoteOnVelocity, StartNoteOn, ReturnNoteOn, ResolveNoteOff, TestNoteOff, SelectNoteOff, ReadNoteOffFallback, BindNoteOffNext, AddNoteOffNext, TestNoteOffNext, SelectNoteOffNext, ClearNoteOffNext, WriteNoteOffNext, AddNoteOffBase, BindNoteOffVelocity, StopNoteOff, ResetNoteOffMidi, ReadNoteOffPage, SelectNoteOffPage, ReturnNoteOff, ClearResolvedNote, ReadResolvedGroup, TestResolvedGroup, SelectResolvedGroup, ReadResolvedKey, TestResolvedKey, SelectResolvedKey, BindResolvedNote, AddResolvedBase, ReturnResolvedNote, SaveDirectEvent, TestDirectEvent, SelectDirectEvent, AddDirectIndex, ApplyDirectEdit, RestoreDirectEvent, ReturnDirect, BindPublishBase, ReadPublishEnabled, SelectPublishEnabled, BindPublishIndex, PublishParameter, ReturnPublish, ReadAutomaticIndex, SaveAutomaticEvent, BindAutomaticFlag, JoinAutomaticEdit, SaveManualEvent, BindManualFlag, SaveEditIndex, TestDynamicIndex, SelectDynamicIndex, ReadDynamicPatch, ReadDynamicVariant, AddDynamicIndex, BindDescriptorIndex, BindDescriptorStride, ScaleDescriptorIndex, NormalizeDescriptorIndex, LocateDescriptor, ReadDescriptorCategory, DoubleDescriptorCategory, QuadrupleDescriptorCategory, NormalizeCategoryWord, NormalizeCategoryLong, LocateHandlerTable, ReadEditHandler, DispatchEditHandler, ClearEditContext, WriteEditContext, RestoreEditEvent, ReturnEdit, ReadEditActive, ReturnEditActive, ApplyVelocity, ReturnVelocityApply, AwaitChild, Complete };
 enum class Read { PreviousScreen, PreviousPage, Other, UiPage, Group, Routing, Armed, Fallback, Key, Publish, Automatic, Patch, Edit, Byte, Handler };enum class Write { Screen, Page, Armed, Routing, Key, Fallback, Context };enum class Exit { None, Name, Calibration, SpecialOne, SpecialTwo, SpecialThree, SpecialFour, CloseNote };Exit exit()const{return exit_;}uint32_t originalKey()const{return originalKey_;}uint32_t routing()const{return routing_;}uint32_t value()const{return value_;}uint32_t offset()const{return offset_;}uint32_t armed()const{return armed_;}uint32_t other()const{return other_;}uint32_t patch()const{return patch_;}uint32_t page()const{return page_;}uint32_t edit()const{return edit_;}uint32_t key()const{return key_;}uint32_t category()const{return category_;}uint32_t variant()const{return variant_;}uint32_t curve()const{return curve_;}uint32_t screen()const{return screen_;}uint32_t publish()const{return publish_;}uint32_t group()const{return group_;}uint32_t storedKey()const{return storedKey_;}
 explicit StateEditWorkflowTask(Kind kind,uint32_t event=0){event_=event;key_=uint8_t(event);value_=uint8_t(event>>8);switch(kind){case Kind::Restore:stage_=Stage::BindRestoreScreen;break;case Kind::Select:stage_=Stage::WriteSelectArmed;break;case Kind::SelectNote:stage_=Stage::ReadNoteSelectPage;break;case Kind::PanelEvent:stage_=Stage::ObservePanelCalibration;break;case Kind::Velocity:stage_=Stage::SaveVelocityEvent;break;case Kind::NoteOn:stage_=Stage::ReadNoteOnPage;break;case Kind::NoteOff:stage_=Stage::ResolveNoteOff;break;case Kind::ResolveNote:stage_=Stage::ClearResolvedNote;break;case Kind::DirectEdit:stage_=Stage::SaveDirectEvent;break;case Kind::Publish:stage_=Stage::BindPublishBase;break;case Kind::AutomaticEdit:stage_=Stage::ReadAutomaticIndex;break;case Kind::AutomaticEvent:stage_=Stage::SaveAutomaticEvent;break;case Kind::ManualEdit:stage_=Stage::SaveManualEvent;break;case Kind::EditActive:stage_=Stage::ReadEditActive;break;case Kind::ApplyVelocity:stage_=Stage::ApplyVelocity;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool nonzero,uint32_t event){if(!awaitingChild())return false;childNonzero_=nonzero;if(resume_==Stage::TestPanelEncoder){event_=event;key_=uint8_t(event);value_=uint8_t(event>>8);}if(resume_==Stage::TestNoteOn||resume_==Stage::TestNoteOff)key_=uint8_t(event);stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindRestoreScreen:case Stage::ReadRestoreScreen:case Stage::BindRestorePage:case Stage::ReadRestorePage:case Stage::BindPublishBase:case Stage::LocateDescriptor:case Stage::LocateHandlerTable:return 9; case Stage::WriteRestoreScreen:case Stage::WriteRestorePage:return 3+store; case Stage::ReturnRestore:case Stage::ReturnSelect:case Stage::ReturnNoteSelect:case Stage::ReturnPanelEvent:case Stage::ReturnVelocity:case Stage::ReturnNoteOn:case Stage::ReturnNoteOff:case Stage::ReturnResolvedNote:case Stage::ReturnDirect:case Stage::ReturnPublish:case Stage::ReturnEdit:case Stage::ReturnEditActive:case Stage::ReturnVelocityApply:return 17; case Stage::WriteSelectArmed:case Stage::WriteNoteSelect:case Stage::WriteNoteOffNext:return 9+store; case Stage::ReadSelectOther:case Stage::ReadSelectPage:case Stage::ReadSelectGroup:case Stage::ReadSelectRouting:case Stage::WriteSelectRouting:case Stage::ReadNoteSelectPage:case Stage::ReadNoteSelectArmed:case Stage::ReadNoteOnPage:case Stage::ReadNoteOnFallback:case Stage::ReadNoteOffFallback:case Stage::ReadNoteOffPage:case Stage::ReadResolvedGroup:case Stage::ReadResolvedKey:case Stage::ReadAutomaticIndex:case Stage::ReadEditHandler:case Stage::ReadEditActive:return 12; case Stage::SelectOtherName:case Stage::SelectPageName:case Stage::SelectPanelCalibration:case Stage::SelectPanelSpecialOne:case Stage::SelectPanelSpecialThree:case Stage::SelectPanelSpecialFour:case Stage::EnterPanelSpecialTwo:case Stage::SelectNoteOffPage:return 8; case Stage::RefreshSelectRouting:case Stage::LoadSelectProgram:case Stage::ResetSelectPlayback:case Stage::RefreshNoteSelect:case Stage::LoadNoteSelect:case Stage::ResetNoteSelect:case Stage::ObservePanelCalibration:case Stage::ReadPanelEncoder:case Stage::ApplyPanelParameter:case Stage::ApplyVelocityScale:case Stage::CloseNoteOnPage:case Stage::StartNoteOn:case Stage::StopNoteOff:case Stage::ResetNoteOffMidi:case Stage::PublishParameter:case Stage::ApplyVelocity:return 20; case Stage::SaveVelocityEvent:case Stage::RestoreVelocityEvent:case Stage::SaveDirectEvent:case Stage::RestoreDirectEvent:case Stage::SaveAutomaticEvent:case Stage::SaveManualEvent:case Stage::RestoreEditEvent:return 11; case Stage::SquareVelocityValue:case Stage::ResolveNoteOn:case Stage::ResolveNoteOff:case Stage::ApplyDirectEdit:case Stage::ReadPublishEnabled:case Stage::ReadDynamicVariant:case Stage::ScaleDescriptorIndex:case Stage::DispatchEditHandler:return 15; case Stage::BindVelocityScale:case Stage::BindPublishIndex:case Stage::ReadDescriptorCategory:return 6; case Stage::ReadDynamicPatch:case Stage::WriteEditContext:return 18; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadRestoreScreen:screen_=uint8_t(read(Read::PreviousScreen,0));next();break;
case Stage::WriteRestoreScreen:write(Write::Screen,screen_);next();break;
case Stage::ReadRestorePage:page_=uint8_t(read(Read::PreviousPage,0));next();break;
case Stage::WriteRestorePage:write(Write::Page,page_);next();break;
case Stage::WriteSelectArmed:write(Write::Armed,uint8_t(event_>>8));next();break;
case Stage::SelectArmed:stage_=uint8_t(event_>>8)?Stage::ReadSelectOther:Stage::ReturnSelect;break;
case Stage::ReadSelectOther:other_=uint8_t(read(Read::Other,0));next();break;
case Stage::SelectOther:stage_=other_?Stage::TestSelectOther:Stage::ReadSelectPage;break;
case Stage::SelectOtherName:if(other_==2)finish(Exit::Name);else next();break;
case Stage::JoinSelectOther:stage_=Stage::ReturnSelect;break;
case Stage::ReadSelectPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectPageName:if(page_==2)finish(Exit::Name);else next();break;
case Stage::SelectPageRange:stage_=page_>2?Stage::ReturnSelect:Stage::ReadSelectGroup;break;
case Stage::ReadSelectGroup:group_=uint8_t(read(Read::Group,0));next();break;
case Stage::SelectGroupRange:stage_=group_>4?Stage::ReturnSelect:Stage::ReadSelectRouting;break;
case Stage::ReadSelectRouting:routing_=uint16_t(read(Read::Routing,0));next();break;
case Stage::WriteSelectRouting:write(Write::Routing,routing_);next();break;
case Stage::ReadNoteSelectPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectNoteSelectPage:stage_=page_<2?Stage::ReadNoteSelectArmed:Stage::ReturnNoteSelect;break;
case Stage::ReadNoteSelectArmed:armed_=uint8_t(read(Read::Armed,0));next();break;
case Stage::SelectNoteSelectArmed:stage_=armed_?Stage::TestNoteSelectLow:Stage::ReturnNoteSelect;break;
case Stage::SelectNoteSelectLow:stage_=key_<36?Stage::ReturnNoteSelect:Stage::TestNoteSelectHigh;break;
case Stage::SelectNoteSelectHigh:stage_=key_<=84?Stage::SubtractNoteSelectBase:Stage::ReturnNoteSelect;break;
case Stage::SubtractNoteSelectBase:key_=uint8_t(key_-36);next();break;
case Stage::WriteNoteSelect:write(Write::Key,key_);next();break;
case Stage::SelectPanelCalibration:if(childNonzero_)finish(Exit::Calibration);else next();break;
case Stage::SelectPanelEncoder:stage_=event_>>16?Stage::TestPanelSpecialOne:Stage::ReturnPanelEvent;break;
case Stage::SelectPanelSpecialOne:if(key_==32)finish(Exit::SpecialOne);else next();break;
case Stage::SelectPanelSpecialTwo:stage_=key_==33?Stage::ScalePanelSpecialTwo:Stage::TestPanelSpecialThree;break;
case Stage::SelectPanelSpecialThree:if(key_==34)finish(Exit::SpecialThree);else next();break;
case Stage::SelectPanelSpecialFour:if(key_==35)finish(Exit::SpecialFour);else next();break;
case Stage::ScalePanelSpecialTwo:value_>>=1;next();break;
case Stage::EnterPanelSpecialTwo:finish(Exit::SpecialTwo);break;
case Stage::SquareVelocityValue:curve_=uint16_t(unsigned(value_)*value_);next();break;
case Stage::DoubleVelocityValue:curve_=uint16_t(curve_<<1);next();break;
case Stage::ReadNoteOnPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectNoteOnPage:stage_=page_?Stage::CloseNoteOnPage:Stage::ResolveNoteOn;break;
case Stage::SelectNoteOn:stage_=key_?Stage::BindNoteOnVelocity:Stage::ReadNoteOnFallback;break;
case Stage::ReadNoteOnFallback:key_=uint8_t(read(Read::Fallback,0));next();break;
case Stage::AddNoteOnBase:key_=uint8_t(key_+36);next();break;
case Stage::SelectNoteOff:stage_=key_?Stage::BindNoteOffVelocity:Stage::ReadNoteOffFallback;break;
case Stage::ReadNoteOffFallback:key_=uint8_t(read(Read::Fallback,0));next();break;
case Stage::BindNoteOffNext:nextKey_=key_;next();break;
case Stage::AddNoteOffNext:nextKey_=uint8_t(nextKey_+6);next();break;
case Stage::SelectNoteOffNext:stage_=nextKey_<=48?Stage::WriteNoteOffNext:Stage::ClearNoteOffNext;break;
case Stage::ClearNoteOffNext:nextKey_=0;next();break;
case Stage::WriteNoteOffNext:write(Write::Fallback,nextKey_);next();break;
case Stage::AddNoteOffBase:key_=uint8_t(key_+36);next();break;
case Stage::ReadNoteOffPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectNoteOffPage:if(page_)finish(Exit::CloseNote);else next();break;
case Stage::ClearResolvedNote:key_=0;next();break;
case Stage::ReadResolvedGroup:group_=uint8_t(read(Read::Group,0));next();break;
case Stage::SelectResolvedGroup:stage_=group_<=4?Stage::ReturnResolvedNote:Stage::ReadResolvedKey;break;
case Stage::ReadResolvedKey:storedKey_=uint8_t(read(Read::Key,0));next();break;
case Stage::SelectResolvedKey:stage_=storedKey_>48?Stage::ReturnResolvedNote:Stage::BindResolvedNote;break;
case Stage::BindResolvedNote:key_=storedKey_;next();break;
case Stage::AddResolvedBase:key_=uint8_t(key_+36);next();break;
case Stage::SelectDirectEvent:stage_=value_?Stage::AddDirectIndex:Stage::RestoreDirectEvent;break;
case Stage::AddDirectIndex:key_=uint8_t(key_+38);next();break;
case Stage::ReadPublishEnabled:publish_=uint8_t(read(Read::Publish,0));next();break;
case Stage::SelectPublishEnabled:stage_=publish_?Stage::BindPublishIndex:Stage::ReturnPublish;break;
case Stage::ReadAutomaticIndex:key_=uint8_t(read(Read::Automatic,0));next();break;
case Stage::BindAutomaticFlag:automatic_=true;next();break;
case Stage::JoinAutomaticEdit:stage_=Stage::SaveEditIndex;break;
case Stage::BindManualFlag:automatic_=false;next();break;
case Stage::SaveEditIndex:originalKey_=key_;next();break;
case Stage::SelectDynamicIndex:stage_=key_==31?Stage::ReadDynamicPatch:Stage::BindDescriptorIndex;break;
case Stage::ReadDynamicPatch:patch_=read(Read::Patch,0);next();break;
case Stage::ReadDynamicVariant:variant_=uint8_t(read(Read::Byte,patch_+226));next();break;
case Stage::AddDynamicIndex:key_=uint8_t(key_+variant_);next();break;
case Stage::ScaleDescriptorIndex:offset_=uint16_t(unsigned(key_)*8);next();break;
case Stage::ReadDescriptorCategory:category_=uint8_t(read(Read::Byte,0xfd66 + offset_));next();break;
case Stage::DoubleDescriptorCategory:category_=uint8_t(category_<<1);next();break;
case Stage::QuadrupleDescriptorCategory:category_=uint8_t(category_<<1);next();break;
case Stage::ReadEditHandler:read(Read::Handler,0x1013e + category_);next();break;
case Stage::WriteEditContext:write(Write::Context,0);next();break;
case Stage::ReadEditActive:edit_=uint8_t(read(Read::Edit,0));next();break;
case Stage::ReturnRestore:stage_=Stage::Complete;break;
case Stage::RefreshSelectRouting:await(Stage::LoadSelectProgram);break;
case Stage::LoadSelectProgram:await(Stage::ResetSelectPlayback);break;
case Stage::ResetSelectPlayback:await(Stage::ReturnSelect);break;
case Stage::ReturnSelect:stage_=Stage::Complete;break;
case Stage::RefreshNoteSelect:await(Stage::LoadNoteSelect);break;
case Stage::LoadNoteSelect:await(Stage::ResetNoteSelect);break;
case Stage::ResetNoteSelect:await(Stage::ReturnNoteSelect);break;
case Stage::ReturnNoteSelect:stage_=Stage::Complete;break;
case Stage::ObservePanelCalibration:await(Stage::SelectPanelCalibration);break;
case Stage::ReadPanelEncoder:await(Stage::TestPanelEncoder);break;
case Stage::ApplyPanelParameter:await(Stage::ReturnPanelEvent);break;
case Stage::ReturnPanelEvent:stage_=Stage::Complete;break;
case Stage::ApplyVelocityScale:await(Stage::RestoreVelocityEvent);break;
case Stage::ReturnVelocity:stage_=Stage::Complete;break;
case Stage::CloseNoteOnPage:await(Stage::ResolveNoteOn);break;
case Stage::ResolveNoteOn:await(Stage::TestNoteOn);break;
case Stage::StartNoteOn:await(Stage::ReturnNoteOn);break;
case Stage::ReturnNoteOn:stage_=Stage::Complete;break;
case Stage::ResolveNoteOff:await(Stage::TestNoteOff);break;
case Stage::StopNoteOff:await(Stage::ResetNoteOffMidi);break;
case Stage::ResetNoteOffMidi:await(Stage::ReadNoteOffPage);break;
case Stage::ReturnNoteOff:stage_=Stage::Complete;break;
case Stage::ReturnResolvedNote:stage_=Stage::Complete;break;
case Stage::ApplyDirectEdit:await(Stage::RestoreDirectEvent);break;
case Stage::ReturnDirect:stage_=Stage::Complete;break;
case Stage::PublishParameter:await(Stage::ReturnPublish);break;
case Stage::ReturnPublish:stage_=Stage::Complete;break;
case Stage::DispatchEditHandler:await(Stage::ClearEditContext);break;
case Stage::ReturnEdit:stage_=Stage::Complete;break;
case Stage::ReturnEditActive:stage_=Stage::Complete;break;
case Stage::ApplyVelocity:await(Stage::ReturnVelocityApply);break;
case Stage::ReturnVelocityApply:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t event_=0,patch_=0;uint16_t routing_=0,curve_=0,offset_=0;uint8_t screen_=0,page_=0,other_=0,group_=0,armed_=0,key_=0,value_=0,nextKey_=0,storedKey_=0,publish_=0,originalKey_=0,variant_=0,category_=0,edit_=0;bool childNonzero_=false,automatic_=false;
};
}
