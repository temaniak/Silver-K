#pragma once
#include <cstdint>
namespace ks {
// StateEditLabel semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateEditLabelTask final {
public:
 enum class Kind { Label };enum class Stage { BindLabelPosition, ShowLabelPosition, BindLabelIndex, BindLabelStride, ScaleLabelIndex, NormalizeLabelIndex, LocateLabel, BindLabelBuffer, BindLabelLength, ReadLabelCharacter, WriteLabelCharacter, NextLabelDestination, TickLabelLength, RepeatLabel, BindLabelTerminator, WriteLabelTerminator, BindLabelReference, ReadLabelKind, ReadLabelPatch, TestModulationLabel, SelectModulationLabel, ReadModulationSource, SelectModulationSource, BindLfoWord, TickModulationSource, SelectLfoWord, BindEnvelopeWord, WriteModulationWord, JoinModulationWord, TestOscillatorLabel, SelectOscillatorLabel, ReadOscillatorSource, AddOscillatorDigit, WriteOscillatorDigit, JoinOscillatorDigit, TestLfoLabel, SelectLfoLabel, ReadLfoSource, AddLfoDigit, WriteLfoDigit, TestSourceLabel, SelectSourceLabel, ReadSourceKind, BindSourceStride, ScaleSourceKind, NormalizeSourceKind, LocateSourceName, BindSourceBuffer, BindSourceLength, ReadSourceCharacter, WriteSourceCharacter, NextSourceDestination, TickSourceLength, RepeatSource, ReadEnvelopeKind, ReadEnvelopePatch, BindEnvelopeLetter, SubtractEnvelopeLetter, SelectEnvelopeNegative, TestEnvelopeKind, SelectEnvelopeKind, DoubleEnvelopeKind, ReadEnvelopeSource, AddEnvelopeSource, BindEnvelopeStride, ScaleEnvelopeKind, NormalizeEnvelopeKind, LocateEnvelopeName, BindEnvelopeBuffer, BindEnvelopeLength, ReadEnvelopeCharacter, WriteEnvelopeCharacter, NextEnvelopeDestination, TickEnvelopeLength, RepeatEnvelope, PrintLabel, ReturnLabel, AwaitChild, Complete };
 enum class Read { Patch, Byte };enum class Write { Byte, Word, Long };uint32_t patch()const{return patch_;}uint32_t character()const{return character_;}uint32_t kind()const{return kind_;}uint32_t variant()const{return variant_;}
 explicit StateEditLabelTask(Kind kind,uint8_t index=0){index_=index;switch(kind){case Kind::Label:stage_=Stage::BindLabelPosition;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ShowLabelPosition:case Stage::PrintLabel:return 20; case Stage::ScaleLabelIndex:case Stage::ScaleSourceKind:case Stage::ScaleEnvelopeKind:return 15; case Stage::LocateLabel:case Stage::BindLabelBuffer:case Stage::BindLabelReference:case Stage::BindLfoWord:case Stage::BindEnvelopeWord:case Stage::LocateSourceName:case Stage::BindSourceBuffer:case Stage::LocateEnvelopeName:case Stage::BindEnvelopeBuffer:return 9; case Stage::BindLabelLength:case Stage::BindLabelTerminator:case Stage::BindSourceLength:case Stage::BindEnvelopeLength:return 6; case Stage::ReadLabelCharacter:case Stage::ReadSourceCharacter:case Stage::ReadEnvelopeCharacter:return 5+store; case Stage::WriteLabelCharacter:case Stage::WriteLabelTerminator:case Stage::WriteSourceCharacter:case Stage::WriteEnvelopeCharacter:return 3+store; case Stage::ReadLabelKind:case Stage::WriteOscillatorDigit:case Stage::WriteLfoDigit:case Stage::ReadEnvelopeKind:return 6+store; case Stage::ReadLabelPatch:case Stage::ReadModulationSource:case Stage::ReadOscillatorSource:case Stage::ReadLfoSource:case Stage::ReadSourceKind:case Stage::ReadEnvelopePatch:case Stage::ReadEnvelopeSource:return 12+store; case Stage::WriteModulationWord:return 9+store; case Stage::ReturnLabel:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::BindLabelIndex:source_=0xfede + uint32_t(index_)*16;next();break;
case Stage::BindLabelBuffer:destination_=0x167dc;next();break;
case Stage::BindLabelLength:remaining_=16;next();break;
case Stage::ReadLabelCharacter:character_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::WriteLabelCharacter:write(Write::Byte,destination_,character_);next();break;
case Stage::NextLabelDestination:++destination_;next();break;
case Stage::TickLabelLength:--remaining_;next();break;
case Stage::RepeatLabel:stage_=remaining_?Stage::ReadLabelCharacter:Stage::BindLabelTerminator;break;
case Stage::WriteLabelTerminator:write(Write::Word,destination_,0x2000);next();break;
case Stage::ReadLabelKind:kind_=uint8_t(read(Read::Byte,0x167e0));next();break;
case Stage::ReadLabelPatch:patch_=read(Read::Patch,0);next();break;
case Stage::SelectModulationLabel:stage_=kind_==80?Stage::ReadModulationSource:Stage::TestOscillatorLabel;break;
case Stage::ReadModulationSource:variant_=uint8_t(read(Read::Byte,patch_+224));next();break;
case Stage::SelectModulationSource:stage_=variant_?Stage::BindLfoWord:Stage::ReadOscillatorSource;break;
case Stage::BindLfoWord:word_=0x4d6c666f;next();break;
case Stage::TickModulationSource:--variant_;next();break;
case Stage::SelectLfoWord:stage_=variant_?Stage::BindEnvelopeWord:Stage::WriteModulationWord;break;
case Stage::BindEnvelopeWord:word_=0x4d656e76;next();break;
case Stage::WriteModulationWord:write(Write::Long,0x167e4,word_);next();break;
case Stage::SelectOscillatorLabel:stage_=kind_==79?Stage::ReadOscillatorSource:Stage::TestLfoLabel;break;
case Stage::ReadOscillatorSource:variant_=uint8_t(read(Read::Byte,patch_+222));next();break;
case Stage::AddOscillatorDigit:variant_=uint8_t(variant_+49);next();break;
case Stage::WriteOscillatorDigit:write(Write::Byte,0x167e0,variant_);next();break;
case Stage::SelectLfoLabel:stage_=kind_==76?Stage::ReadLfoSource:Stage::TestSourceLabel;break;
case Stage::ReadLfoSource:variant_=uint8_t(read(Read::Byte,patch_+225));next();break;
case Stage::AddLfoDigit:variant_=uint8_t(variant_+49);next();break;
case Stage::WriteLfoDigit:write(Write::Byte,0x167e0,variant_);next();break;
case Stage::SelectSourceLabel:stage_=kind_==83?Stage::ReadSourceKind:Stage::ReadEnvelopeKind;break;
case Stage::ReadSourceKind:variant_=uint8_t(read(Read::Byte,patch_+223));next();break;
case Stage::LocateSourceName:source_=0x10162 + uint32_t(variant_)*6;next();break;
case Stage::BindSourceBuffer:destination_=0x167dc;next();break;
case Stage::BindSourceLength:remaining_=6;next();break;
case Stage::ReadSourceCharacter:character_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::WriteSourceCharacter:write(Write::Byte,destination_,character_);next();break;
case Stage::NextSourceDestination:++destination_;next();break;
case Stage::TickSourceLength:--remaining_;next();break;
case Stage::RepeatSource:stage_=remaining_?Stage::ReadSourceCharacter:Stage::ReadEnvelopeKind;break;
case Stage::ReadEnvelopeKind:kind_=uint8_t(read(Read::Byte,0x167e2));next();break;
case Stage::ReadEnvelopePatch:patch_=read(Read::Patch,0);next();break;
case Stage::SubtractEnvelopeLetter:kind_=uint8_t(kind_-65);next();break;
case Stage::SelectEnvelopeNegative:stage_=kind_&128?Stage::PrintLabel:Stage::TestEnvelopeKind;break;
case Stage::SelectEnvelopeKind:stage_=kind_>3?Stage::PrintLabel:Stage::DoubleEnvelopeKind;break;
case Stage::DoubleEnvelopeKind:kind_=uint8_t(kind_<<1);next();break;
case Stage::ReadEnvelopeSource:variant_=uint8_t(read(Read::Byte,patch_+139));next();break;
case Stage::AddEnvelopeSource:kind_=uint8_t(kind_+variant_);next();break;
case Stage::LocateEnvelopeName:source_=0x10174 + uint32_t(kind_)*6;next();break;
case Stage::BindEnvelopeBuffer:destination_=0x167e2;next();break;
case Stage::BindEnvelopeLength:remaining_=6;next();break;
case Stage::ReadEnvelopeCharacter:character_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::WriteEnvelopeCharacter:write(Write::Byte,destination_,character_);next();break;
case Stage::NextEnvelopeDestination:++destination_;next();break;
case Stage::TickEnvelopeLength:--remaining_;next();break;
case Stage::RepeatEnvelope:stage_=remaining_?Stage::ReadEnvelopeCharacter:Stage::PrintLabel;break;
case Stage::JoinModulationWord:stage_=Stage::ReadOscillatorSource;break;
case Stage::JoinOscillatorDigit:stage_=Stage::ReadEnvelopeKind;break;
case Stage::ShowLabelPosition:await(Stage::BindLabelIndex);break;
case Stage::PrintLabel:await(Stage::ReturnLabel);break;
case Stage::ReturnLabel:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t source_=0,destination_=0,patch_=0,word_=0;uint8_t index_=0,remaining_=0,character_=0,kind_=0,variant_=0;
};
}
