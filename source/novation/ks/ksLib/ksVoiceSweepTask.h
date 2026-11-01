#pragma once
#include <cstdint>
namespace ks {
// Native all-voice release and part-retune loops, with explicit timed child jobs.
// Part and voice selectors are handles, not emulated machine addresses.
class VoiceSweepTask final {
public:
 enum class Kind {ReleaseAll,RetunePart};
 enum class Stage {ZeroIndex,PublishInitial,ReadPart,DispatchControl,AwaitControl,ReadCount,ReadMatch,InitCount,SubtractCount,AdmitCount,RetainRemaining,RetainMatch,DispatchVoice,AwaitVoice,
 ReadAge,AdmitAge,ReadOwner,CompareOwner,AdmitOwner,MaskAge,MarkAge,CaptureAge,LowAge,WriteAge,ReadNote,ReadTranspose,BiasTranspose,ApplyTranspose,WriteNote,
 ReadPatch,DispatchSelect,AwaitSelect,ReadPitch,AdmitPitch,DispatchVoiceAgain,AwaitVoiceAgain,ReadTrackedPitch,BiasPitch,ClearPitchLow,CapturePitch,ClearWireLow,
 CutValue,CutOffset,DispatchCut,AwaitCut,DispatchGate,AwaitGate,ReadIndex,TickIndex,PublishIndex,RestoreMatch,RestoreRemaining,TickRemaining,AdmitRemaining,Finish,Complete};
 enum class Read {Part,Count,Match,Age,Owner,Note,Transpose,Patch,Pitch,TrackedPitch,Index};
 enum class Write {Index,Age,Note};
 enum class Child {None,Control,Voice,Select,VoiceAgain,Cut,Gate};
 explicit VoiceSweepTask(Kind k):kind_(k){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool retune()const{return kind_==Kind::RetunePart;}
 uint8_t index()const{return index_;}uint8_t remaining()const{return remaining_;}uint8_t match()const{return match_;}uint8_t owner()const{return owner_;}uint8_t note()const{return note_;}uint8_t transpose()const{return transpose_;}uint8_t pitch()const{return pitch_;}
 uint8_t slot()const{return slot_;}uint16_t age()const{return age_;}uint32_t patch()const{return patch_;}uint32_t releasedAge()const{return (uint32_t(age_)<<16)|0xffff;}
 uint32_t wireWord()const{return retune()?uint32_t(pitch_)<<16:0x100000;}
 Child child()const{switch(stage_){case Stage::AwaitControl:return Child::Control;case Stage::AwaitVoice:return Child::Voice;case Stage::AwaitSelect:return Child::Select;case Stage::AwaitVoiceAgain:return Child::VoiceAgain;case Stage::AwaitCut:return Child::Cut;case Stage::AwaitGate:return Child::Gate;default:return Child::None;}}
 bool resumeChild(uint8_t slot=0){switch(stage_){case Stage::AwaitControl:stage_=Stage::ReadCount;return true;case Stage::AwaitVoice:slot_=slot;stage_=Stage::ReadAge;return true;case Stage::AwaitSelect:stage_=retune()?Stage::ReadPitch:Stage::CutValue;return true;case Stage::AwaitVoiceAgain:slot_=slot;stage_=Stage::ReadTrackedPitch;return true;case Stage::AwaitCut:stage_=retune()?Stage::ReadIndex:Stage::DispatchGate;return true;case Stage::AwaitGate:stage_=Stage::ReadIndex;return true;default:return false;}}
 template<class StoreClock>unsigned nextClocks(StoreClock&& store)const{switch(stage_){case Stage::PublishInitial:case Stage::PublishIndex:return 9+store(Write::Index);case Stage::ReadPart:case Stage::ReadCount:case Stage::ReadMatch:case Stage::ReadTranspose:case Stage::ReadIndex:return 12;
 case Stage::DispatchControl:case Stage::DispatchVoice:case Stage::DispatchSelect:case Stage::DispatchVoiceAgain:case Stage::DispatchCut:case Stage::DispatchGate:return 20;
 case Stage::RetainRemaining:case Stage::RetainMatch:case Stage::RestoreRemaining:case Stage::RestoreMatch:return 11;
 case Stage::ReadAge:return 15;case Stage::ReadOwner:case Stage::ReadNote:case Stage::ReadPitch:case Stage::ReadTrackedPitch:return 15;
 case Stage::LowAge:case Stage::CutValue:case Stage::CutOffset:return 6;case Stage::WriteAge:return 15+store(Write::Age);case Stage::WriteNote:return 12+store(Write::Note);case Stage::ReadPatch:return 21;
 case Stage::AdmitRemaining:return retune()?8:3;case Stage::Finish:return 17;
 case Stage::AwaitControl:case Stage::AwaitVoice:case Stage::AwaitSelect:case Stage::AwaitVoiceAgain:case Stage::AwaitCut:case Stage::AwaitGate:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class StoreClock>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,StoreClock&& store){unsigned used=0;while(auto cost=nextClocks(store)){if(cost>available-used)break;
 switch(stage_){case Stage::ZeroIndex:index_=0;stage_=Stage::PublishInitial;break;case Stage::PublishInitial:write(Write::Index,index_);stage_=retune()?Stage::ReadPart:Stage::InitCount;break;
 case Stage::ReadPart:match_=uint8_t(read(Read::Part));stage_=Stage::DispatchControl;break;case Stage::DispatchControl:stage_=Stage::AwaitControl;break;case Stage::ReadCount:remaining_=uint8_t(read(Read::Count));stage_=Stage::ReadMatch;break;case Stage::ReadMatch:match_=uint8_t(read(Read::Match));stage_=Stage::RetainRemaining;break;
 case Stage::InitCount:remaining_=14;stage_=Stage::SubtractCount;break;case Stage::SubtractCount:remaining_=uint8_t(remaining_-index_);stage_=Stage::AdmitCount;break;case Stage::AdmitCount:stage_=remaining_?Stage::RetainRemaining:Stage::Finish;break;
 case Stage::RetainRemaining:stage_=retune()?Stage::RetainMatch:Stage::DispatchVoice;break;case Stage::RetainMatch:stage_=Stage::DispatchVoice;break;case Stage::DispatchVoice:stage_=Stage::AwaitVoice;break;
 case Stage::ReadAge:age_=uint16_t(read(Read::Age));stage_=Stage::AdmitAge;break;case Stage::AdmitAge:stage_=age_?(retune()?Stage::ReadOwner:Stage::MaskAge):Stage::ReadIndex;break;
 case Stage::ReadOwner:owner_=uint8_t(read(Read::Owner));stage_=Stage::CompareOwner;break;case Stage::CompareOwner:stage_=Stage::AdmitOwner;break;case Stage::AdmitOwner:stage_=owner_==match_?Stage::ReadNote:Stage::ReadIndex;break;
 case Stage::MaskAge:age_&=0x3fff;stage_=Stage::MarkAge;break;case Stage::MarkAge:age_|=1;stage_=Stage::CaptureAge;break;case Stage::CaptureAge:stage_=Stage::LowAge;break;case Stage::LowAge:stage_=Stage::WriteAge;break;case Stage::WriteAge:write(Write::Age,releasedAge());stage_=Stage::ReadPatch;break;
 case Stage::ReadNote:note_=uint8_t(read(Read::Note));stage_=Stage::ReadTranspose;break;case Stage::ReadTranspose:transpose_=uint8_t(read(Read::Transpose));stage_=Stage::BiasTranspose;break;case Stage::BiasTranspose:transpose_=uint8_t(transpose_-64);stage_=Stage::ApplyTranspose;break;case Stage::ApplyTranspose:note_=uint8_t(note_+transpose_);stage_=Stage::WriteNote;break;case Stage::WriteNote:write(Write::Note,note_);stage_=Stage::ReadPatch;break;
 case Stage::ReadPatch:patch_=uint32_t(read(Read::Patch));stage_=Stage::DispatchSelect;break;case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;case Stage::ReadPitch:pitch_=uint8_t(read(Read::Pitch));stage_=Stage::AdmitPitch;break;case Stage::AdmitPitch:stage_=pitch_?Stage::BiasPitch:Stage::DispatchVoiceAgain;break;case Stage::DispatchVoiceAgain:stage_=Stage::AwaitVoiceAgain;break;case Stage::ReadTrackedPitch:pitch_=uint8_t(read(Read::TrackedPitch));stage_=Stage::BiasPitch;break;
 case Stage::BiasPitch:pitch_=uint8_t(pitch_-60);stage_=Stage::ClearPitchLow;break;case Stage::ClearPitchLow:stage_=Stage::CapturePitch;break;case Stage::CapturePitch:stage_=Stage::ClearWireLow;break;case Stage::ClearWireLow:stage_=Stage::CutOffset;break;
 case Stage::CutValue:stage_=Stage::CutOffset;break;case Stage::CutOffset:stage_=Stage::DispatchCut;break;case Stage::DispatchCut:stage_=Stage::AwaitCut;break;case Stage::DispatchGate:stage_=Stage::AwaitGate;break;
 case Stage::ReadIndex:index_=uint8_t(read(Read::Index));stage_=Stage::TickIndex;break;case Stage::TickIndex:++index_;stage_=Stage::PublishIndex;break;case Stage::PublishIndex:write(Write::Index,index_);stage_=retune()?Stage::RestoreMatch:Stage::RestoreRemaining;break;
 case Stage::RestoreMatch:stage_=Stage::RestoreRemaining;break;case Stage::RestoreRemaining:stage_=Stage::TickRemaining;break;case Stage::TickRemaining:--remaining_;stage_=Stage::AdmitRemaining;break;case Stage::AdmitRemaining:stage_=remaining_?Stage::RetainRemaining:Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}
 used+=cost;}return used;}
private:Kind kind_;Stage stage_=Stage::ZeroIndex;uint8_t index_=0,remaining_=0,match_=0,owner_=0,note_=0,transpose_=0,pitch_=0,slot_=0;uint16_t age_=0;uint32_t patch_=0;
};
}
