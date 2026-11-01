#pragma once
#include <cstdint>
namespace kstation {
// K voice-slot address child 72ba. The selected slot is read live at its
// original boundary. 6fde allocation and its parameter/voice children are
// separate work; this task alone does not implement voice allocation.
class VoiceAddressTask final {
public:
 enum class Stage { ReadSlot, BindStride, Multiply, Extend, AddBase, Return, Complete };
 struct Credits {unsigned read=0,returnStack=0;};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 uint8_t slot()const{return slot_;}uint16_t offset()const{return offset_;}
 uint32_t address()const{return 0xd030u+offset_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::ReadSlot:return 9+c.read;case Stage::Multiply:return 15;case Stage::AddBase:return 9;case Stage::Return:return c.returnStack;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadSlot:slot_=uint8_t(read());stage_=Stage::BindStride;break;case Stage::Multiply:offset_=uint16_t(unsigned(slot_)*18);stage_=Stage::Extend;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:break;default:stage_=Stage(unsigned(stage_)+1);break;}return cost;}
private:Stage stage_=Stage::ReadSlot;uint8_t slot_=0;uint16_t offset_=0;
};
// K part-address child 72ce: 16 bytes per part.
class VoicePartAddressTask final {
public:
 enum class Stage { ReadSlot, BindStride, Multiply, Extend, AddBase, Return, Complete };
 struct Credits {unsigned read=0,returnStack=0;};
 VoicePartAddressTask()=default;explicit VoicePartAddressTask(uint8_t part):stage_(Stage::BindStride),slot_(part){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 uint8_t slot()const{return slot_;}uint16_t offset()const{return offset_;}
 uint32_t address()const{return 0xbfaau+offset_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::ReadSlot:return 9+c.read;case Stage::Multiply:return 15;case Stage::AddBase:return 9;case Stage::Return:return c.returnStack;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadSlot:slot_=uint8_t(read());stage_=Stage::BindStride;break;case Stage::Multiply:offset_=uint16_t(unsigned(slot_)*16);stage_=Stage::Extend;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:break;default:stage_=Stage(unsigned(stage_)+1);break;}return cost;}
private:Stage stage_=Stage::ReadSlot;uint8_t slot_=0;uint16_t offset_=0;
};
// Selection prefix of K allocation 6fde. The return boundary is the next
// 71a0 voice-address call: voice activation and DSP children remain separate.
class VoiceSelectionTask final {
public:
 enum class Stage { DispatchPart,ReadMode,ReadHeld,ReadUnit,ReadPart,RetainTag,ClearCandidate,PublishInitial,ReadCount,InitializeAge,CompareMode,SelectMode,TestHeld,SelectHeld,SetHeldAllocation,SkipOwner,SetReuseAllocation,PublishReuse,SetOwnerAllocation,DispatchOwnerVoice,ReadOwnerAge,ReadOwnerPart,ReadOwnerUnit,CompareOwner,SelectOwner,CompareOldest,SelectOldest,RetainOldest,ReadOldestIndex,ReadOwnerIndex,AdvanceOwnerIndex,PublishOwnerIndex,DecrementOwner,RepeatOwner,PublishOwnerCandidate,PublishOwnerAllocation,TestOldestHigh,SelectOldestHigh,SkipOwnerFinish,ClearNewestAge,DispatchNewestVoice,ReadNewestAge,ReadNewestPart,ReadNewestUnit,CompareNewestOwner,SelectNewestOwner,CopyNewestHigh,MaskNewestHigh,SelectNewestActive,CompareNewest,SelectNewest,RetainNewest,ReadNewestIndex,ReadNewestNext,AdvanceNewestIndex,PublishNewestIndex,DecrementNewest,RepeatNewest,PublishNewestCandidate,TestNewestHigh,SelectNewestHigh,SetNewestAllocation,PublishNewestAllocation,SkipNewestFinish,ComparePolyMode,SelectPolyMode,SetPolyAllocation,PublishPolyAllocation,DispatchFreeVoice,ReadFreeAge,TestFreeHigh,SelectFreeHigh,CompareFreeAge,SelectFreeAge,RetainFreeAge,ReadFreeIndex,ReadFreeNext,AdvanceFreeIndex,PublishFreeIndex,DecrementFree,RepeatFree,PublishFreeCandidate,SkipFreeFinish,ReadIncoming,SetNoteAllocation,PublishNoteAllocation,DispatchNoteVoice,ReadNoteAge,TestNoteHigh,SelectNoteHigh,ReadNotePart,ReadNoteUnit,CompareNoteOwner,SelectNoteOwner,ReadNote,CompareNote,SelectNote,SetRetriggerAllocation,PublishRetrigger,SkipRetrigger,CompareNoteAge,SelectNoteAge,RetainNoteAge,ReadNoteIndex,ReadNoteNext,AdvanceNoteIndex,PublishNoteIndex,DecrementNote,RepeatNote,PublishNoteCandidate,TestNoteBestHigh,SelectNoteBestHigh,AwaitPart,AwaitVoice,Complete };
 using S=Stage;
 enum class Read { Mode,Held,Unit,Part,Count,Age,VoicePart,VoiceUnit,Index,Incoming,VoiceNote };
 enum class Write { Index,Allocation };
 enum class Child { None,Part,Voice };
 struct Credits {unsigned read=0,write=0,stack=0;};
 explicit VoiceSelectionTask(uint32_t patch):patch_(patch){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==S::Complete;}
 Child child()const{return stage_==S::AwaitPart?Child::Part:stage_==S::AwaitVoice?Child::Voice:Child::None;}
 bool resumeChild(uint32_t address){if(stage_==S::AwaitPart){partAddress_=address;stage_=S::ReadMode;return true;}if(stage_==S::AwaitVoice){voiceAddress_=address;stage_=resume_;return true;}return false;}
 uint32_t patchAddress()const{return patch_;}uint32_t partAddress()const{return partAddress_;}uint32_t voiceAddress()const{return voiceAddress_;}
 uint8_t mode()const{return mode_;}uint8_t index()const{return index_;}uint8_t candidate()const{return candidate_;}uint8_t remaining()const{return remaining_;}uint8_t allocation()const{return allocation_;}
 unsigned nextClocks(const Credits&c)const;
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){
 case S::DispatchPart:stage_=S::AwaitPart;break;
 case S::ReadMode:mode_=uint8_t(read(Read::Mode));next();break;
 case S::ReadHeld:held_=uint8_t(read(Read::Held));next();break;
 case S::ReadUnit:unit_=uint8_t(read(Read::Unit));next();break;
 case S::ReadPart:part_=uint8_t(read(Read::Part));next();break;
 case S::ClearCandidate:candidate_=0;next();break;
 case S::PublishInitial:write(Write::Index,0);next();break;
 case S::ReadCount:remaining_=uint8_t(read(Read::Count));next();break;
 case S::InitializeAge:best_=0xffffffffu;next();break;
 case S::SelectMode:stage_=mode_>1?S::ComparePolyMode:S::TestHeld;break;
 case S::SelectHeld:stage_=held_?S::SetHeldAllocation:S::SetReuseAllocation;break;
 case S::SetHeldAllocation:allocation_=1;next();break;
 case S::SkipOwner:stage_=S::PublishPolyAllocation;break;
 case S::SetReuseAllocation:allocation_=4;next();break;
 case S::PublishReuse:write(Write::Allocation,4);next();break;
 case S::SetOwnerAllocation:allocation_=1;next();break;
 case S::DispatchOwnerVoice:resume_=S::ReadOwnerAge;stage_=S::AwaitVoice;break;
 case S::ReadOwnerAge:case S::ReadNewestAge:case S::ReadFreeAge:case S::ReadNoteAge:age_=read(Read::Age);next();break;
 case S::ReadOwnerPart:case S::ReadNewestPart:case S::ReadNotePart:slotPart_=uint8_t(read(Read::VoicePart));next();break;
 case S::ReadOwnerUnit:case S::ReadNewestUnit:case S::ReadNoteUnit:slotUnit_=uint8_t(read(Read::VoiceUnit));next();break;
 case S::SelectOwner:stage_=sameOwner()?S::ClearNewestAge:S::CompareOldest;break;
 case S::SelectOldest:stage_=age_>=best_?S::ReadOwnerIndex:S::RetainOldest;break;
 case S::RetainOldest:best_=age_;next();break;
 case S::ReadOldestIndex:case S::ReadNewestIndex:case S::ReadFreeIndex:case S::ReadNoteIndex:candidate_=uint8_t(read(Read::Index));next();break;
 case S::ReadOwnerIndex:case S::ReadNewestNext:case S::ReadFreeNext:case S::ReadNoteNext:index_=uint8_t(read(Read::Index));next();break;
 case S::AdvanceOwnerIndex:case S::AdvanceNewestIndex:case S::AdvanceFreeIndex:case S::AdvanceNoteIndex:++index_;next();break;
 case S::PublishOwnerIndex:case S::PublishNewestIndex:case S::PublishFreeIndex:case S::PublishNoteIndex:write(Write::Index,index_);next();break;
 case S::DecrementOwner:case S::DecrementNewest:case S::DecrementFree:case S::DecrementNote:--remaining_;next();break;
 case S::RepeatOwner:stage_=remaining_?S::DispatchOwnerVoice:S::PublishOwnerCandidate;break;
 case S::PublishOwnerCandidate:case S::PublishNewestCandidate:case S::PublishFreeCandidate:case S::PublishNoteCandidate:write(Write::Index,candidate_);next();break;
 case S::PublishOwnerAllocation:write(Write::Allocation,allocation_);next();break;
 case S::SelectOldestHigh:stage_=(best_>>16)?S::Complete:S::SkipOwnerFinish;break;
 case S::SkipOwnerFinish:stage_=S::Complete;break;
 case S::ClearNewestAge:best_=0;next();break;
 case S::DispatchNewestVoice:resume_=S::ReadNewestAge;stage_=S::AwaitVoice;break;
 case S::SelectNewestOwner:stage_=sameOwner()?S::CopyNewestHigh:S::ReadNewestNext;break;
 case S::SelectNewestActive:stage_=(age_&0xc0000000u)?S::Complete:S::CompareNewest;break;
 case S::SelectNewest:stage_=age_<=best_?S::ReadNewestNext:S::RetainNewest;break;
 case S::RetainNewest:best_=age_;next();break;
 case S::RepeatNewest:stage_=remaining_?S::DispatchNewestVoice:S::PublishNewestCandidate;break;
 case S::SelectNewestHigh:stage_=(best_>>16)?S::SetNewestAllocation:S::PublishNewestAllocation;break;
 case S::SetNewestAllocation:allocation_=2;next();break;
 case S::PublishNewestAllocation:write(Write::Allocation,allocation_);next();break;
 case S::SkipNewestFinish:stage_=S::Complete;break;
 case S::SelectPolyMode:stage_=mode_==2?S::SetPolyAllocation:S::ReadIncoming;break;
 case S::SetPolyAllocation:allocation_=0;next();break;
 case S::PublishPolyAllocation:write(Write::Allocation,allocation_);next();break;
 case S::DispatchFreeVoice:resume_=S::ReadFreeAge;stage_=S::AwaitVoice;break;
 case S::SelectFreeHigh:stage_=(age_>>16)?S::CompareFreeAge:S::Complete;break;
 case S::SelectFreeAge:stage_=age_>=best_?S::ReadFreeNext:S::RetainFreeAge;break;
 case S::RetainFreeAge:best_=age_;next();break;
 case S::RepeatFree:stage_=remaining_?S::DispatchFreeVoice:S::PublishFreeCandidate;break;
 case S::SkipFreeFinish:stage_=S::Complete;break;
 case S::ReadIncoming:incoming_=uint8_t(read(Read::Incoming));next();break;
 case S::SetNoteAllocation:allocation_=0;next();break;
 case S::PublishNoteAllocation:write(Write::Allocation,allocation_);next();break;
 case S::DispatchNoteVoice:resume_=S::ReadNoteAge;stage_=S::AwaitVoice;break;
 case S::SelectNoteHigh:stage_=(age_>>16)?S::ReadNotePart:S::CompareNoteAge;break;
 case S::SelectNoteOwner:stage_=sameOwner()?S::ReadNote:S::CompareNoteAge;break;
 case S::ReadNote:note_=uint8_t(read(Read::VoiceNote));next();break;
 case S::SelectNote:stage_=note_==incoming_?S::SetRetriggerAllocation:S::CompareNoteAge;break;
 case S::SetRetriggerAllocation:allocation_=4;next();break;
 case S::PublishRetrigger:write(Write::Allocation,4);next();break;
 case S::SkipRetrigger:stage_=S::Complete;break;
 case S::SelectNoteAge:stage_=age_>=best_?S::ReadNoteNext:S::RetainNoteAge;break;
 case S::RetainNoteAge:best_=age_;next();break;
 case S::RepeatNote:stage_=remaining_?S::DispatchNoteVoice:S::PublishNoteCandidate;break;
 case S::SelectNoteBestHigh:stage_=S::Complete;break;
 case S::AwaitPart:case S::AwaitVoice:case S::Complete:break;
 default:next();break;
 }return cost;}
private:
 bool sameOwner()const{return part_==slotPart_&&unit_==slotUnit_;}
 void next(){stage_=S(unsigned(stage_)+1);}
 S stage_=S::DispatchPart,resume_=S::Complete;uint32_t patch_=0,partAddress_=0,voiceAddress_=0,age_=0,best_=0;
 uint8_t mode_=0,held_=0,unit_=0,part_=0,slotPart_=0,slotUnit_=0,remaining_=0,index_=0,candidate_=0,allocation_=0,incoming_=0,note_=0;
};

inline unsigned VoiceSelectionTask::nextClocks(const Credits&c)const{switch(stage_){
case S::DispatchPart:case S::DispatchOwnerVoice:case S::DispatchNewestVoice:case S::DispatchFreeVoice:case S::DispatchNoteVoice:return c.stack;
case S::ReadMode:case S::ReadHeld:case S::ReadUnit:case S::ReadOwnerPart:case S::ReadOwnerUnit:case S::ReadNewestPart:case S::ReadNewestUnit:case S::ReadNotePart:case S::ReadNoteUnit:case S::ReadNote:return 12+c.read;
case S::ReadPart:case S::ReadCount:case S::ReadOldestIndex:case S::ReadOwnerIndex:case S::ReadNewestIndex:case S::ReadNewestNext:case S::ReadFreeIndex:case S::ReadFreeNext:case S::ReadIncoming:case S::ReadNoteIndex:case S::ReadNoteNext:return 9+c.read;
case S::RetainTag:case S::ClearCandidate:case S::CompareMode:case S::TestHeld:case S::SelectHeld:case S::SetHeldAllocation:case S::SetReuseAllocation:case S::SetOwnerAllocation:case S::CompareOwner:case S::SelectOwner:case S::CompareOldest:case S::SelectOldest:case S::RetainOldest:case S::AdvanceOwnerIndex:case S::DecrementOwner:case S::RepeatOwner:case S::TestOldestHigh:case S::ClearNewestAge:case S::CompareNewestOwner:case S::SelectNewestOwner:case S::CopyNewestHigh:case S::MaskNewestHigh:case S::CompareNewest:case S::SelectNewest:case S::RetainNewest:case S::AdvanceNewestIndex:case S::DecrementNewest:case S::RepeatNewest:case S::TestNewestHigh:case S::SelectNewestHigh:case S::SetNewestAllocation:case S::ComparePolyMode:case S::SelectPolyMode:case S::SetPolyAllocation:case S::TestFreeHigh:case S::CompareFreeAge:case S::SelectFreeAge:case S::RetainFreeAge:case S::AdvanceFreeIndex:case S::DecrementFree:case S::RepeatFree:case S::SetNoteAllocation:case S::TestNoteHigh:case S::SelectNoteHigh:case S::CompareNoteOwner:case S::SelectNoteOwner:case S::CompareNote:case S::SelectNote:case S::SetRetriggerAllocation:case S::SkipRetrigger:case S::CompareNoteAge:case S::SelectNoteAge:case S::RetainNoteAge:case S::AdvanceNoteIndex:case S::DecrementNote:case S::RepeatNote:case S::TestNoteBestHigh:case S::SelectNoteBestHigh:return 3;
case S::PublishInitial:case S::PublishReuse:case S::PublishOwnerIndex:case S::PublishOwnerCandidate:case S::PublishOwnerAllocation:case S::PublishNewestIndex:case S::PublishNewestCandidate:case S::PublishNewestAllocation:case S::PublishPolyAllocation:case S::PublishFreeIndex:case S::PublishFreeCandidate:case S::PublishNoteAllocation:case S::PublishRetrigger:case S::PublishNoteIndex:case S::PublishNoteCandidate:return 9+c.write;
case S::InitializeAge:return 9;
case S::SelectMode:case S::SkipOwner:case S::SelectOldestHigh:case S::SkipOwnerFinish:case S::SelectNewestActive:case S::SkipNewestFinish:case S::SelectFreeHigh:case S::SkipFreeFinish:return 8;
case S::ReadOwnerAge:case S::ReadNewestAge:case S::ReadFreeAge:case S::ReadNoteAge:return 15+c.read;
case S::AwaitPart:case S::AwaitVoice:case S::Complete:return 0;
}return 0;}
// K 72e2 copies sixteen bytes in original read-then-write order. Source and
// destination may overlap; no source snapshot or hidden allocation is used.
class VoicePartCopyTask final {
public:
 enum class Stage { NormalizePart,SetCount,Multiply,SelectSource,SelectDestination,ReadByte,WriteByte,AdvanceDestination,DecrementCount,Repeat,Return,Complete };
 struct Credits {unsigned read=0,write=0,returnStack=0;};
 explicit VoicePartCopyTask(uint8_t part):part_(part){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 uint32_t source()const{return 0xbfaau+16u*part_+position_;}uint32_t destination()const{return 0xbfbau+position_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::SetCount:return 6;case Stage::Multiply:return 23;case Stage::SelectSource:case Stage::SelectDestination:return 9;case Stage::ReadByte:return 5+c.read;case Stage::WriteByte:return 3+c.write;case Stage::Return:return c.returnStack;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadByte:value_=uint8_t(read());stage_=Stage::WriteByte;break;case Stage::WriteByte:write(value_);stage_=Stage::AdvanceDestination;break;case Stage::AdvanceDestination:++position_;stage_=Stage::DecrementCount;break;case Stage::Repeat:stage_=position_<16?Stage::ReadByte:Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:break;default:stage_=Stage(unsigned(stage_)+1);break;}return cost;}
private:Stage stage_=Stage::NormalizePart;uint8_t part_=0,value_=0;unsigned position_=0;
};
// Velocity curve child 7260. The exponent loop and optional live table read
// remain individual phases and support every 8-bit input, including wrap.
class VoiceVelocityTask final {
public:
 enum class Stage{CopyInput,DecrementExponent,SelectExponent,CopyPower,MultiplyPower,ShiftPower,RepeatExponent,CopyInverse,XorInverse,TestCurve,SelectOriginal,ReturnOriginal,DecrementCurve1,SelectCurve1,CopyInverseOutput,ReturnInverse,DecrementCurve2,SelectCurve2,CopySquare,MultiplySquare,ShiftSquare,ReturnSquare,DecrementCurve3,SelectCurve3,MultiplyCross,ShiftCross,ReturnCross,DecrementCurve4,NormalizeCurveWord,NormalizeCurveLong,LocateCurve,ReadThreshold,TestParity,SelectParity,CompareEven,SelectEven,ReturnEven,CompareOdd,SelectOdd,ReturnOdd,SetOff,ReturnOff,Complete};
 using S=Stage;struct Credits{unsigned read=0,returnStack=0;};
 VoiceVelocityTask(uint8_t velocity,uint8_t exponent,uint8_t curve):input_(velocity),power_(velocity),exponent_(exponent),curve_(curve){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==S::Complete;}uint8_t result()const{return power_;}uint32_t tableAddress()const{return 0xc06cu+curve_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case S::MultiplyPower:case S::MultiplySquare:case S::MultiplyCross:return 15;case S::LocateCurve:return 9;case S::ReadThreshold:return 3+c.read;case S::ReturnOriginal:case S::ReturnInverse:case S::ReturnSquare:case S::ReturnCross:case S::ReturnEven:case S::ReturnOdd:case S::ReturnOff:return c.returnStack;case S::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){
 case S::DecrementExponent:--exponent_;next();break;case S::SelectExponent:stage_=exponent_?S::CopyPower:S::CopyInverse;break;
 case S::CopyPower:low_=power_;next();break;case S::MultiplyPower:product_=uint16_t(unsigned(input_)*low_);next();break;
 case S::ShiftPower:product_=uint16_t(product_<<1);power_=uint8_t(product_>>8);next();break;case S::RepeatExponent:stage_=S::DecrementExponent;break;
 case S::CopyInverse:low_=power_;next();break;case S::XorInverse:low_^=0x7f;next();break;case S::SelectOriginal:stage_=curve_?S::DecrementCurve1:S::ReturnOriginal;break;
 case S::DecrementCurve1:case S::DecrementCurve2:case S::DecrementCurve3:case S::DecrementCurve4:--curve_;next();break;
 case S::SelectCurve1:stage_=curve_?S::DecrementCurve2:S::CopyInverseOutput;break;case S::CopyInverseOutput:power_=low_;next();break;
 case S::SelectCurve2:stage_=curve_?S::DecrementCurve3:S::CopySquare;break;case S::CopySquare:low_=power_;next();break;
 case S::MultiplySquare:product_=uint16_t(unsigned(power_)*low_);next();break;case S::ShiftSquare:product_=uint16_t(product_<<1);power_=uint8_t(product_>>8);next();break;
 case S::SelectCurve3:stage_=curve_?S::DecrementCurve4:S::MultiplyCross;break;case S::MultiplyCross:product_=uint16_t(unsigned(power_)*low_);next();break;case S::ShiftCross:product_=uint16_t(product_<<1);power_=uint8_t(product_>>8);next();break;
 case S::ReadThreshold:threshold_=uint8_t(read());next();break;case S::SelectParity:stage_=(threshold_&1)?S::CompareOdd:S::CompareEven;break;
 case S::SelectEven:stage_=input_>threshold_?S::SetOff:S::ReturnEven;break;case S::SelectOdd:stage_=input_<threshold_?S::SetOff:S::ReturnOdd;break;case S::SetOff:power_=0x80;next();break;
 case S::ReturnOriginal:case S::ReturnInverse:case S::ReturnSquare:case S::ReturnCross:case S::ReturnEven:case S::ReturnOdd:case S::ReturnOff:stage_=S::Complete;break;case S::Complete:break;default:next();break;}return cost;}
private:void next(){stage_=S(unsigned(stage_)+1);}S stage_=S::CopyInput;uint8_t input_=0,power_=0,exponent_=0,curve_=0,low_=0,threshold_=0;uint16_t product_=0;
};
// Allocation continuation 71a0: explicit children own address, curve, copy
// and DSP work. Every field is read at its original execution boundary.
class VoiceActivationTask final {
public:
 enum class Stage{DispatchVoice,DispatchPart,WritePatch,ReadNote,WriteNote,ReadTranspose,AddTranspose,WriteTransposed,ClearHeld,WriteHeld,ReadUnit,WriteUnit,ReadVelocity,ReadCurve,ReadExponent,AdvanceExponent,DispatchVelocity,WriteVelocity,SetAge,WriteAge,ReadPart,WritePart,DispatchCopy,DispatchSelectDsp,BindCommand,DispatchWriteDsp,BiasCommand,ReadActualNote,SubtractCommand,SelectNegative,NegateCommand,WriteDelta,ClearFlag,WriteFlag,Return,AwaitChild,Complete};
 using S=Stage;enum class Child{None,Voice,Part,Velocity,Copy,SelectDsp,WriteDsp};
 enum class Read{Note,Transpose,Unit,Velocity,Curve,Exponent,Part,ActualNote};enum class Write{Patch,Note,Transposed,Held,Unit,Velocity,Age,Part,Delta,Flag};
 struct Credits{unsigned read=0,write=0,stack=0;};explicit VoiceActivationTask(uint32_t patch):patch_(patch){}
 S stage()const{return stage_;}bool complete()const{return stage_==S::Complete;}Child child()const{return stage_==S::AwaitChild?child_:Child::None;}
 bool resumeChild(uint32_t value=0){if(stage_!=S::AwaitChild)return false;switch(child_){case Child::Voice:voice_=value;break;case Child::Part:partAddress_=value;break;case Child::Velocity:velocity_=uint8_t(value);break;case Child::WriteDsp:command_=uint16_t(value);break;default:break;}stage_=resume_;child_=Child::None;return true;}
 uint32_t patch()const{return patch_;}uint32_t voice()const{return voice_;}uint32_t partAddress()const{return partAddress_;}
 uint8_t velocity()const{return velocity_;}uint8_t exponent()const{return exponent_;}uint8_t curve()const{return curve_;}uint8_t part()const{return part_;}
 unsigned nextClocks(const Credits&c)const;
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){
 case S::DispatchVoice:await(Child::Voice,S::DispatchPart);break;case S::DispatchPart:await(Child::Part,S::WritePatch);break;
 case S::WritePatch:write(Write::Patch,patch_);next();break;
 case S::ReadNote:note_=uint8_t(read(Read::Note));next();break;case S::WriteNote:write(Write::Note,note_);next();break;
 case S::ReadTranspose:transpose_=uint8_t(read(Read::Transpose));next();break;case S::AddTranspose:transposed_=uint8_t(note_+transpose_);next();break;case S::WriteTransposed:write(Write::Transposed,transposed_);next();break;
 case S::WriteHeld:write(Write::Held,0);next();break;case S::ReadUnit:unit_=uint8_t(read(Read::Unit));next();break;case S::WriteUnit:write(Write::Unit,unit_);next();break;
 case S::ReadVelocity:velocity_=uint8_t(read(Read::Velocity));next();break;case S::ReadCurve:curve_=uint8_t(read(Read::Curve));next();break;case S::ReadExponent:exponent_=uint8_t(read(Read::Exponent));next();break;case S::AdvanceExponent:exponent_=uint8_t(exponent_+2);next();break;
 case S::DispatchVelocity:await(Child::Velocity,S::WriteVelocity);break;case S::WriteVelocity:write(Write::Velocity,velocity_);next();break;
 case S::WriteAge:write(Write::Age,0x8000ffffu);next();break;case S::ReadPart:part_=uint8_t(read(Read::Part));next();break;case S::WritePart:write(Write::Part,part_);next();break;
 case S::DispatchCopy:await(Child::Copy,S::DispatchSelectDsp);break;case S::DispatchSelectDsp:await(Child::SelectDsp,S::BindCommand);break;case S::BindCommand:command_=0x805f;next();break;case S::DispatchWriteDsp:await(Child::WriteDsp,S::BiasCommand);break;
 case S::BiasCommand:bias_=uint8_t((command_>>8)+0x3c);next();break;case S::ReadActualNote:actualNote_=uint8_t(read(Read::ActualNote));next();break;case S::SubtractCommand:negative_=actualNote_<bias_;delta_=uint8_t(actualNote_-bias_);next();break;case S::SelectNegative:stage_=negative_?S::NegateCommand:S::WriteDelta;break;case S::NegateCommand:delta_=uint8_t(0-delta_);next();break;case S::WriteDelta:write(Write::Delta,delta_);next();break;case S::WriteFlag:write(Write::Flag,0);next();break;
 case S::Return:stage_=S::Complete;break;case S::AwaitChild:case S::Complete:break;default:next();break;}return cost;}
private:void next(){stage_=S(unsigned(stage_)+1);}void await(Child c,S resume){child_=c;resume_=resume;stage_=S::AwaitChild;}
 S stage_=S::DispatchVoice,resume_=S::Complete;Child child_=Child::None;uint32_t patch_=0,voice_=0,partAddress_=0;uint16_t command_=0;uint8_t note_=0,transpose_=0,transposed_=0,unit_=0,velocity_=0,curve_=0,exponent_=0,part_=0,bias_=0,actualNote_=0,delta_=0;bool negative_=false;
};

inline unsigned VoiceActivationTask::nextClocks(const Credits&c)const{switch(stage_){
case S::DispatchVoice:case S::DispatchPart:case S::DispatchVelocity:case S::DispatchCopy:case S::DispatchSelectDsp:case S::DispatchWriteDsp:case S::Return:return c.stack;
case S::WritePatch:case S::WriteAge:return 15+c.write;
case S::ReadNote:case S::ReadVelocity:case S::ReadPart:return 9+c.read;
case S::WriteNote:case S::WriteTransposed:case S::WriteHeld:case S::WriteUnit:case S::WriteVelocity:case S::WritePart:case S::WriteDelta:case S::WriteFlag:return 12+c.write;
case S::ReadTranspose:case S::ReadUnit:case S::ReadCurve:case S::ReadExponent:case S::ReadActualNote:return 12+c.read;
case S::AddTranspose:case S::ClearHeld:case S::AdvanceExponent:case S::BiasCommand:case S::SubtractCommand:case S::SelectNegative:case S::NegateCommand:case S::ClearFlag:return 3;
case S::SetAge:return 9;
case S::BindCommand:return 6;
case S::AwaitChild:case S::Complete:return 0;
}return 0;}
class VoiceDspSelectTask final {
public:
 enum class Stage{ReadSlot,NormalizeSlot,SetFirst,MultiplyFirst,WriteFirst,SetSecond,MultiplySecond,WriteSecond,DispatchInvalidate,AwaitDsp,Return,Complete};
 enum class Write{First,Second};struct Credits{unsigned read=0,write=0,stack=0;};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDsp()const{return stage_==Stage::AwaitDsp;}bool resumeDsp(){if(!awaitingDsp())return false;stage_=Stage::Return;return true;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::ReadSlot:return 9+c.read;case Stage::WriteFirst:case Stage::WriteSecond:return 9+c.write;case Stage::SetFirst:case Stage::SetSecond:return 6;case Stage::MultiplyFirst:case Stage::MultiplySecond:return 23;case Stage::DispatchInvalidate:case Stage::Return:return c.stack;case Stage::AwaitDsp:case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned available,const Credits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadSlot:slot_=uint8_t(read());next();break;case Stage::MultiplyFirst:first_=uint16_t(81u*slot_);next();break;case Stage::WriteFirst:write(Write::First,first_);next();break;case Stage::MultiplySecond:second_=uint16_t(30u*slot_);next();break;case Stage::WriteSecond:write(Write::Second,second_);next();break;case Stage::DispatchInvalidate:stage_=Stage::AwaitDsp;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitDsp:case Stage::Complete:break;default:next();break;}return cost;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::ReadSlot;uint8_t slot_=0;uint16_t first_=0,second_=0;
};
class VoiceDspReadTask final {
public:
 enum class Stage{ReadBase,AddBase,DispatchRead,AwaitDsp,Return,Complete};struct Credits{unsigned read=0,stack=0;};
 explicit VoiceDspReadTask(uint16_t command):command_(command){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDsp()const{return stage_==Stage::AwaitDsp;}bool resumeDsp(){if(!awaitingDsp())return false;stage_=Stage::Return;return true;}uint16_t command()const{return command_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::ReadBase:return 9+c.read;case Stage::DispatchRead:case Stage::Return:return c.stack;case Stage::AwaitDsp:case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned available,const Credits&c,R&&read){auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadBase:base_=uint16_t(read());stage_=Stage::AddBase;break;case Stage::AddBase:command_=uint16_t(command_+base_);stage_=Stage::DispatchRead;break;case Stage::DispatchRead:stage_=Stage::AwaitDsp;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::AwaitDsp:case Stage::Complete:break;}return cost;}
private:Stage stage_=Stage::ReadBase;uint16_t command_=0,base_=0;
};

}
