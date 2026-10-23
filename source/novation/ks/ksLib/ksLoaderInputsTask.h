#pragma once
#include <cstdint>
namespace ks {
// Firmware-update LoaderInputs policy. Logical data and deferred effects; CPU/stack/flags projection remains in the temporary compatibility bridge.
class LoaderInputsTask final {
public:
 enum class Kind { Sample, Released, Stable, Pressed, Confirm };enum class Stage { DisableInputs, WriteInputDirection, BindFirstBank, WriteFirstBank, SaveFirstWord, BindFirstOuter, SaveFirstOuter, BindFirstInner, TickFirstInner, RepeatFirstInner, RestoreFirstOuter, TickFirstOuter, RepeatFirstOuter, RestoreFirstWord, ReadFirstBank, BindSecondBank, WriteSecondBank, SaveSecondWord, BindSecondOuter, SaveSecondOuter, BindSecondInner, TickSecondInner, RepeatSecondInner, RestoreSecondOuter, TickSecondOuter, RepeatSecondOuter, RestoreSecondWord, ReadSecondBank, RetainFirstPair, BindThirdBank, WriteThirdBank, SaveThirdWord, BindThirdOuter, SaveThirdOuter, BindThirdInner, TickThirdInner, RepeatThirdInner, RestoreThirdOuter, TickThirdOuter, RepeatThirdOuter, RestoreThirdWord, ReadThirdBank, BindFourthBank, WriteFourthBank, SaveFourthWord, BindFourthOuter, SaveFourthOuter, BindFourthInner, TickFourthInner, RepeatFourthInner, RestoreFourthOuter, TickFourthOuter, RepeatFourthOuter, RestoreFourthWord, ReadFourthBank, BindIdleBank, WriteIdleBank, EnableInputs, WriteIdleDirection, ReturnInputs, ReadReleased, CompareReleased, RepeatReleased, SaveReleaseWord, BindReleaseOuter, SaveReleaseOuter, BindReleaseInner, TickReleaseInner, RepeatReleaseInner, RestoreReleaseOuter, TickReleaseOuter, RepeatReleaseOuter, RestoreReleaseWord, ReadReleasedAgain, CompareReleasedAgain, RepeatReleasedAgain, ReturnReleased, ReadStable, RetainStable, SaveStableWord, BindStableOuter, SaveStableOuter, BindStableInner, TickStableInner, RepeatStableInner, RestoreStableOuter, TickStableOuter, RepeatStableOuter, RestoreStableWord, ReadStableAgain, CompareStable, RepeatStable, ReturnStable, ReadPressed, ComparePressed, RepeatPressed, ReturnPressed, WaitRelease, WaitPress, CompareConfirmation, ReturnConfirmation, AwaitChild, Complete };
 enum class Read { Bank };enum class Write { Direction, Mux };uint32_t sample()const{return sample_;}
 explicit LoaderInputsTask(Kind kind){switch(kind){case Kind::Sample:stage_=Stage::DisableInputs;break;case Kind::Released:stage_=Stage::ReadReleased;break;case Kind::Stable:stage_=Stage::ReadStable;break;case Kind::Pressed:stage_=Stage::ReadPressed;break;case Kind::Confirm:stage_=Stage::WaitRelease;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t sample){if(!awaitingChild())return false;sample_=sample;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::WriteInputDirection:case Stage::WriteIdleDirection:return 6+store; case Stage::WriteFirstBank:case Stage::WriteSecondBank:case Stage::WriteThirdBank:case Stage::WriteFourthBank:case Stage::WriteIdleBank:return 3+store; case Stage::SaveFirstWord:case Stage::SaveFirstOuter:case Stage::RestoreFirstOuter:case Stage::RestoreFirstWord:case Stage::SaveSecondWord:case Stage::SaveSecondOuter:case Stage::RestoreSecondOuter:case Stage::RestoreSecondWord:case Stage::SaveThirdWord:case Stage::SaveThirdOuter:case Stage::RestoreThirdOuter:case Stage::RestoreThirdWord:case Stage::SaveFourthWord:case Stage::SaveFourthOuter:case Stage::RestoreFourthOuter:case Stage::RestoreFourthWord:case Stage::SaveReleaseWord:case Stage::SaveReleaseOuter:case Stage::RestoreReleaseOuter:case Stage::RestoreReleaseWord:case Stage::SaveStableWord:case Stage::SaveStableOuter:case Stage::RestoreStableOuter:case Stage::RestoreStableWord:return 11; case Stage::BindFirstOuter:case Stage::BindFirstInner:case Stage::BindSecondOuter:case Stage::BindSecondInner:case Stage::BindThirdOuter:case Stage::BindThirdInner:case Stage::BindFourthOuter:case Stage::BindFourthInner:case Stage::BindReleaseOuter:case Stage::BindReleaseInner:case Stage::BindStableOuter:case Stage::BindStableInner:return 6; case Stage::ReadFirstBank:case Stage::ReadSecondBank:case Stage::ReadThirdBank:case Stage::ReadFourthBank:return 4; case Stage::ReturnInputs:case Stage::ReturnReleased:case Stage::ReturnStable:case Stage::ReturnPressed:case Stage::ReturnConfirmation:return 17; case Stage::ReadReleased:case Stage::ReadReleasedAgain:case Stage::ReadStable:case Stage::ReadStableAgain:return 20; case Stage::CompareReleased:case Stage::CompareReleasedAgain:case Stage::ComparePressed:case Stage::CompareConfirmation:return 9; case Stage::ReadPressed:case Stage::WaitRelease:case Stage::WaitPress:return 15; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::BindFirstOuter:outer_=1;next();break;
case Stage::BindFirstInner:inner_=2663;next();break;
case Stage::TickFirstInner:--inner_;next();break;
case Stage::RepeatFirstInner:stage_=inner_?Stage::TickFirstInner:Stage::RestoreFirstOuter;break;
case Stage::TickFirstOuter:--outer_;next();break;
case Stage::RepeatFirstOuter:stage_=outer_?Stage::SaveFirstOuter:Stage::RestoreFirstWord;break;
case Stage::BindSecondOuter:outer_=1;next();break;
case Stage::BindSecondInner:inner_=2663;next();break;
case Stage::TickSecondInner:--inner_;next();break;
case Stage::RepeatSecondInner:stage_=inner_?Stage::TickSecondInner:Stage::RestoreSecondOuter;break;
case Stage::TickSecondOuter:--outer_;next();break;
case Stage::RepeatSecondOuter:stage_=outer_?Stage::SaveSecondOuter:Stage::RestoreSecondWord;break;
case Stage::BindThirdOuter:outer_=1;next();break;
case Stage::BindThirdInner:inner_=2663;next();break;
case Stage::TickThirdInner:--inner_;next();break;
case Stage::RepeatThirdInner:stage_=inner_?Stage::TickThirdInner:Stage::RestoreThirdOuter;break;
case Stage::TickThirdOuter:--outer_;next();break;
case Stage::RepeatThirdOuter:stage_=outer_?Stage::SaveThirdOuter:Stage::RestoreThirdWord;break;
case Stage::BindFourthOuter:outer_=1;next();break;
case Stage::BindFourthInner:inner_=2663;next();break;
case Stage::TickFourthInner:--inner_;next();break;
case Stage::RepeatFourthInner:stage_=inner_?Stage::TickFourthInner:Stage::RestoreFourthOuter;break;
case Stage::TickFourthOuter:--outer_;next();break;
case Stage::RepeatFourthOuter:stage_=outer_?Stage::SaveFourthOuter:Stage::RestoreFourthWord;break;
case Stage::BindReleaseOuter:outer_=10;next();break;
case Stage::BindReleaseInner:inner_=2663;next();break;
case Stage::TickReleaseInner:--inner_;next();break;
case Stage::RepeatReleaseInner:stage_=inner_?Stage::TickReleaseInner:Stage::RestoreReleaseOuter;break;
case Stage::TickReleaseOuter:--outer_;next();break;
case Stage::RepeatReleaseOuter:stage_=outer_?Stage::SaveReleaseOuter:Stage::RestoreReleaseWord;break;
case Stage::BindStableOuter:outer_=10;next();break;
case Stage::BindStableInner:inner_=2663;next();break;
case Stage::TickStableInner:--inner_;next();break;
case Stage::RepeatStableInner:stage_=inner_?Stage::TickStableInner:Stage::RestoreStableOuter;break;
case Stage::TickStableOuter:--outer_;next();break;
case Stage::RepeatStableOuter:stage_=outer_?Stage::SaveStableOuter:Stage::RestoreStableWord;break;
case Stage::ReadFirstBank:sample_=uint32_t(uint8_t(read(Read::Bank)))<<8;next();break;
case Stage::ReadSecondBank:sample_|=uint8_t(read(Read::Bank));next();break;
case Stage::RetainFirstPair:sample_|=sample_<<16;next();break;
case Stage::ReadThirdBank:sample_=(sample_&0xffff00ffu)|(uint32_t(uint8_t(read(Read::Bank)))<<8);next();break;
case Stage::ReadFourthBank:sample_=(sample_&0xffffff00u)|uint8_t(read(Read::Bank));next();break;
case Stage::RepeatReleased:stage_=sample_!=0xffffffffu?Stage::ReadReleased:Stage::SaveReleaseWord;break;
case Stage::RepeatReleasedAgain:stage_=sample_!=0xffffffffu?Stage::ReadReleased:Stage::ReturnReleased;break;
case Stage::RetainStable:previous_=sample_;next();break;
case Stage::RepeatStable:stage_=sample_!=previous_?Stage::ReadStable:Stage::ReturnStable;break;
case Stage::RepeatPressed:stage_=sample_==0xffffffffu?Stage::ReadPressed:Stage::ReturnPressed;break;
case Stage::WriteInputDirection:write(Write::Direction,0);next();break;
case Stage::WriteFirstBank:write(Write::Mux,34);next();break;
case Stage::WriteSecondBank:write(Write::Mux,35);next();break;
case Stage::WriteThirdBank:write(Write::Mux,36);next();break;
case Stage::WriteFourthBank:write(Write::Mux,37);next();break;
case Stage::WriteIdleBank:write(Write::Mux,48);next();break;
case Stage::WriteIdleDirection:write(Write::Direction,255);next();break;
case Stage::ReturnInputs:stage_=Stage::Complete;break;
case Stage::ReadReleased:await(Stage::CompareReleased);break;
case Stage::ReadReleasedAgain:await(Stage::CompareReleasedAgain);break;
case Stage::ReturnReleased:stage_=Stage::Complete;break;
case Stage::ReadStable:await(Stage::RetainStable);break;
case Stage::ReadStableAgain:await(Stage::CompareStable);break;
case Stage::ReturnStable:stage_=Stage::Complete;break;
case Stage::ReadPressed:await(Stage::ComparePressed);break;
case Stage::ReturnPressed:stage_=Stage::Complete;break;
case Stage::WaitRelease:await(Stage::WaitPress);break;
case Stage::WaitPress:await(Stage::CompareConfirmation);break;
case Stage::ReturnConfirmation:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t sample_=0,previous_=0;uint16_t outer_=0,inner_=0;
};
}
