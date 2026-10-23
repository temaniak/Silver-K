#pragma once
#include <cstdint>
namespace ks {
// Part selection masks, held-part combinations and per-part reset decisions with live routing observations.
class StatePartSelectionTask final {
public:
 enum class Kind { Select, Reconcile, Resolve, Combine, Held, Changed, ResetParts };enum class Stage { TestSelection, SelectSelection, WriteRequestedPart, BindCurrentSelection, WriteCurrentSelection, ResolveSelectedPart, RefreshPartRouting, ReadSelectionMode, SelectSelectionMode, ReadActivePart, WriteEditPart, RefreshEditPart, RefreshScreen, PublishSelection, ReturnSelection, ReadPreferredMask, ReadAvailableMask, MaskPreferred, SelectPreferred, ReadPreviousMask, MaskPrevious, SelectPrevious, BindFirstMask, BindProbeCount, BindProbeMask, TestProbeMask, SelectProbeMask, ShiftProbeMask, DecrementProbeCount, RepeatProbe, WriteResolvedRequest, WriteResolvedCurrent, ReturnResolved, ReadCombinedMask, ReadCombinedAvailable, CombineMasks, WriteCombinedRequest, WriteCombinedCurrent, RefreshCombined, TestHeldEvent, SelectHeldEvent, InvertReleasedMask, ReadHeldMask, MaskHeldRelease, WriteHeldRelease, SelectHeldRelease, ReturnHeld, ReadPressedHeld, SelectPressedHeld, WritePressedHeld, WritePressedRequest, BindPressedCurrent, JoinPressedCurrent, CombineHeld, WriteCombinedHeld, ReadHeldRequest, CombineHeldRequest, WriteHeldRequest, WriteHeldCurrent, ReadRoutingBefore, RefreshHeldRouting, ReadRoutingAfter, ReinitializeChanged, RefreshHeldScreen, ReturnHeldSelection, ReadChangedMode, SelectChangedMode, DifferenceChanged, MaskRemoved, SelectRemoved, BindChangedPart, BindChangedCount, ShiftRemoved, SelectRemovedBit, SaveChangedMasks, SaveChangedPart, WriteChangedPart, ResetChangedVoices, ResetChangedMidi, RestoreChangedPart, RestoreChangedMasks, NextChangedPart, DecrementChangedCount, RepeatChanged, ReturnChanged, BindCommonPart, BindCommonCount, ReadCommonMode, SelectCommonMode, BindSinglePart, BindSingleCount, SaveCommonPart, WriteCommonPart, ResetCommonMidi, ResetCommonVoices, RestoreCommonPart, NextCommonPart, DecrementCommonCount, RepeatCommon, ReturnCommon, AwaitChild, Complete };
 enum class Read { Mode, ActivePart, Preferred, Available, Previous, Held, HeldRequest };enum class Write { Request, Current, EditPart, HeldRequest, HeldCurrent, Held, Part };
 uint8_t mode()const{return mode_;}uint8_t active()const{return active_;}uint8_t selection()const{return selection_;}uint8_t available()const{return available_;}uint8_t combined()const{return combined_;}uint8_t held()const{return held_;}uint8_t before()const{return before_;}uint8_t after()const{return after_;}uint8_t part()const{return part_;}
 explicit StatePartSelectionTask(Kind kind,uint16_t event=0,uint8_t before=0,uint8_t after=0){event_=event;before_=before;after_=after;switch(kind){case Kind::Select:stage_=Stage::TestSelection;break;case Kind::Reconcile:stage_=Stage::ResolveSelectedPart;break;case Kind::Resolve:stage_=Stage::ReadPreferredMask;break;case Kind::Combine:stage_=Stage::ReadCombinedMask;break;case Kind::Held:stage_=Stage::TestHeldEvent;break;case Kind::Changed:stage_=Stage::ReadChangedMode;break;case Kind::ResetParts:stage_=Stage::BindCommonPart;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectSelection:case Stage::RefreshCombined:case Stage::SelectHeldRelease:return 8; case Stage::WriteRequestedPart:case Stage::WriteCurrentSelection:case Stage::WriteEditPart:case Stage::WriteResolvedRequest:case Stage::WriteResolvedCurrent:case Stage::WriteCombinedRequest:case Stage::WriteCombinedCurrent:case Stage::WriteHeldRelease:case Stage::WritePressedHeld:case Stage::WritePressedRequest:case Stage::WriteCombinedHeld:case Stage::WriteHeldRequest:case Stage::WriteHeldCurrent:case Stage::WriteChangedPart:case Stage::WriteCommonPart:return 9+store; case Stage::ResolveSelectedPart:case Stage::ReinitializeChanged:return 15; case Stage::RefreshPartRouting:case Stage::RefreshEditPart:case Stage::RefreshScreen:case Stage::PublishSelection:case Stage::RefreshHeldRouting:case Stage::RefreshHeldScreen:case Stage::ResetChangedVoices:case Stage::ResetChangedMidi:case Stage::ResetCommonMidi:case Stage::ResetCommonVoices:return 20; case Stage::ReadSelectionMode:case Stage::ReadActivePart:case Stage::ReadPreferredMask:case Stage::ReadAvailableMask:case Stage::ReadPreviousMask:case Stage::ReadCombinedMask:case Stage::ReadCombinedAvailable:case Stage::ReadHeldMask:case Stage::ReadPressedHeld:case Stage::ReadHeldRequest:case Stage::ReadRoutingBefore:case Stage::ReadRoutingAfter:case Stage::ReadChangedMode:case Stage::ReadCommonMode:return 12; case Stage::ReturnSelection:case Stage::ReturnResolved:case Stage::ReturnHeld:case Stage::ReturnHeldSelection:case Stage::ReturnChanged:case Stage::ReturnCommon:return 17; case Stage::SaveChangedMasks:case Stage::SaveChangedPart:case Stage::RestoreChangedPart:case Stage::RestoreChangedMasks:case Stage::SaveCommonPart:case Stage::RestoreCommonPart:return 11; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectSelection:stage_=(event_>>8)?Stage::WriteRequestedPart:Stage::Complete;break;
case Stage::WriteRequestedPart:write(Write::Request,uint8_t(event_));next();break;
case Stage::WriteCurrentSelection:write(Write::Current,uint8_t(event_));next();break;
case Stage::ReadSelectionMode:mode_=uint8_t(read(Read::Mode));next();break;
case Stage::SelectSelectionMode:stage_=mode_?Stage::ReadActivePart:Stage::RefreshScreen;break;
case Stage::ReadActivePart:active_=uint8_t(read(Read::ActivePart));next();break;
case Stage::WriteEditPart:write(Write::EditPart,active_);next();break;
case Stage::ReadPreferredMask:selection_=uint8_t(read(Read::Preferred));next();break;
case Stage::ReadAvailableMask:available_=uint8_t(read(Read::Available));next();break;
case Stage::MaskPreferred:selection_&=available_;next();break;
case Stage::SelectPreferred:stage_=selection_?Stage::WriteResolvedRequest:Stage::ReadPreviousMask;break;
case Stage::ReadPreviousMask:selection_=uint8_t(read(Read::Previous));next();break;
case Stage::MaskPrevious:selection_&=available_;next();break;
case Stage::SelectPrevious:stage_=selection_?Stage::WriteResolvedRequest:Stage::BindFirstMask;break;
case Stage::BindFirstMask:selection_=1;next();break;
case Stage::BindProbeCount:remaining_=3;next();break;
case Stage::SelectProbeMask:stage_=available_&selection_?Stage::WriteResolvedRequest:Stage::ShiftProbeMask;break;
case Stage::ShiftProbeMask:selection_=uint8_t(selection_<<1);next();break;
case Stage::DecrementProbeCount:--remaining_;next();break;
case Stage::RepeatProbe:stage_=remaining_?Stage::BindProbeMask:Stage::WriteResolvedRequest;break;
case Stage::WriteResolvedRequest:write(Write::Request,selection_);next();break;
case Stage::WriteResolvedCurrent:write(Write::Current,selection_);next();break;
case Stage::ReadCombinedMask:combined_=uint8_t(read(Read::Previous));next();break;
case Stage::ReadCombinedAvailable:available_=uint8_t(read(Read::Available));next();break;
case Stage::CombineMasks:combined_|=available_;next();break;
case Stage::WriteCombinedRequest:write(Write::HeldRequest,combined_);next();break;
case Stage::WriteCombinedCurrent:write(Write::HeldCurrent,combined_);next();break;
case Stage::RefreshCombined:stage_=Stage::Complete;break;
case Stage::SelectHeldEvent:stage_=event_>>8?Stage::ReadPressedHeld:Stage::InvertReleasedMask;break;
case Stage::InvertReleasedMask:released_=uint8_t(event_)^15;next();break;
case Stage::ReadHeldMask:held_=uint8_t(read(Read::Held));next();break;
case Stage::MaskHeldRelease:held_&=released_;next();break;
case Stage::WriteHeldRelease:write(Write::Held,held_);next();break;
case Stage::SelectHeldRelease:stage_=held_?Stage::ReturnHeld:Stage::ResolveSelectedPart;break;
case Stage::ReadPressedHeld:held_=uint8_t(read(Read::Held));next();break;
case Stage::SelectPressedHeld:stage_=held_?Stage::CombineHeld:Stage::WritePressedHeld;break;
case Stage::WritePressedHeld:write(Write::Held,uint8_t(event_));next();break;
case Stage::WritePressedRequest:write(Write::HeldRequest,uint8_t(event_));next();break;
case Stage::BindPressedCurrent:combined_=uint8_t(event_);next();break;
case Stage::JoinPressedCurrent:stage_=Stage::WriteHeldCurrent;break;
case Stage::CombineHeld:held_|=uint8_t(event_);next();break;
case Stage::WriteCombinedHeld:write(Write::Held,held_);next();break;
case Stage::ReadHeldRequest:combined_=uint8_t(read(Read::HeldRequest));next();break;
case Stage::CombineHeldRequest:combined_|=uint8_t(event_);next();break;
case Stage::WriteHeldRequest:write(Write::HeldRequest,combined_);next();break;
case Stage::WriteHeldCurrent:write(Write::HeldCurrent,combined_);next();break;
case Stage::ReadRoutingBefore:before_=uint8_t(read(Read::Available));next();break;
case Stage::ReadRoutingAfter:after_=uint8_t(read(Read::Available));next();break;
case Stage::ReadChangedMode:mode_=uint8_t(read(Read::Mode));next();break;
case Stage::SelectChangedMode:stage_=mode_?Stage::DifferenceChanged:Stage::ReturnChanged;break;
case Stage::DifferenceChanged:changed_=after_^before_;next();break;
case Stage::MaskRemoved:changed_&=before_;next();break;
case Stage::SelectRemoved:stage_=changed_?Stage::BindChangedPart:Stage::ReturnChanged;break;
case Stage::BindChangedPart:part_=1;next();break;
case Stage::BindChangedCount:remaining_=4;next();break;
case Stage::ShiftRemoved:selected_=changed_&1;changed_>>=1;next();break;
case Stage::SelectRemovedBit:stage_=selected_?Stage::SaveChangedMasks:Stage::NextChangedPart;break;
case Stage::WriteChangedPart:write(Write::Part,part_);next();break;
case Stage::NextChangedPart:++part_;next();break;
case Stage::DecrementChangedCount:--remaining_;next();break;
case Stage::RepeatChanged:stage_=remaining_?Stage::ShiftRemoved:Stage::ReturnChanged;break;
case Stage::BindCommonPart:part_=0;next();break;
case Stage::BindCommonCount:remaining_=1;next();break;
case Stage::ReadCommonMode:mode_=uint8_t(read(Read::Mode));next();break;
case Stage::SelectCommonMode:stage_=mode_?Stage::SaveCommonPart:Stage::BindSinglePart;break;
case Stage::BindSinglePart:part_=1;next();break;
case Stage::BindSingleCount:remaining_=4;next();break;
case Stage::WriteCommonPart:write(Write::Part,part_);next();break;
case Stage::NextCommonPart:++part_;next();break;
case Stage::DecrementCommonCount:--remaining_;next();break;
case Stage::RepeatCommon:stage_=remaining_?Stage::SaveCommonPart:Stage::ReturnCommon;break;
case Stage::ResolveSelectedPart:await(Stage::RefreshPartRouting);break;
case Stage::RefreshPartRouting:await(Stage::ReadSelectionMode);break;
case Stage::RefreshEditPart:await(Stage::RefreshScreen);break;
case Stage::RefreshScreen:await(Stage::PublishSelection);break;
case Stage::PublishSelection:await(Stage::ReturnSelection);break;
case Stage::ReturnSelection:stage_=Stage::Complete;break;
case Stage::ReturnResolved:stage_=Stage::Complete;break;
case Stage::ReturnHeld:stage_=Stage::Complete;break;
case Stage::RefreshHeldRouting:await(Stage::ReadRoutingAfter);break;
case Stage::ReinitializeChanged:await(Stage::RefreshHeldScreen);break;
case Stage::RefreshHeldScreen:await(Stage::ReturnHeldSelection);break;
case Stage::ReturnHeldSelection:stage_=Stage::Complete;break;
case Stage::ResetChangedVoices:await(Stage::ResetChangedMidi);break;
case Stage::ResetChangedMidi:await(Stage::RestoreChangedPart);break;
case Stage::ReturnChanged:stage_=Stage::Complete;break;
case Stage::ResetCommonMidi:await(Stage::ResetCommonVoices);break;
case Stage::ResetCommonVoices:await(Stage::RestoreCommonPart);break;
case Stage::ReturnCommon:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint16_t event_=0;uint8_t mode_=0,active_=0,selection_=0,available_=0,combined_=0,held_=0,released_=0,before_=0,after_=0,changed_=0,part_=0,remaining_=0;bool selected_=false;
};
}
