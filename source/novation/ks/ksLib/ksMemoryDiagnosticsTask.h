#pragma once
#include <cstdint>
namespace ks {
// Diagnostic RAM pattern generation, write/verify passes, mismatch reporting and failure halt. Cursor references are opaque data handles.
class MemoryDiagnosticsTask final {
public:
 enum class Kind { Memory, Patterns, Pair, Pass, ControllerFault };enum class Stage { BindMemoryNotice, ShowMemoryNotice, BindMemoryBase, BindMemoryLength, BindMemoryRounds, TestMemory, TestMemoryFailure, SelectMemoryFailure, BindMemorySuccess, ShowMemorySuccess, ReturnMemorySuccess, BindMemoryFailure, ShowMemoryFailure, BindFailureHigh, ShowFailureHigh, BindFailureLow, ShowFailureLow, HaltFailure, BindPatternStart, BindPatternStride, ClearPatternMask, TestPatternPair, TestFirstFailure, SelectFirstFailure, InvertPatternMask, TestInversePair, TestInverseFailure, SelectInverseFailure, NextPatternStart, NextPatternStride, DecrementPatternRounds, RepeatPattern, ReturnPattern, BindWritePass, SaveWritePattern, WritePattern, RestoreWritePattern, BindVerifyPass, SaveVerifyPattern, VerifyPattern, RestoreVerifyPattern, ReturnPatternPair, BindPassCursor, BindPassCount, BindPassPattern, MixPassPattern, TestPassMode, SelectPassMode, WritePassPattern, JoinPassAdvance, ReadPassPattern, ComparePassPattern, SelectPassMatch, MarkPassFailure, ReturnPassFailure, AdvancePassPattern, AdvancePassCursor, DecrementPassCount, RepeatPass, ReturnPass, HaltController, AwaitChild, Complete };
 enum class Read { Pattern };enum class Write { Pattern };uint16_t actual()const{return actual_;}uint16_t expected()const{return expected_;}uint16_t pattern()const{return pattern_;}uint32_t cursor()const{return cursor_;}uint8_t mode()const{return mode_;}
 explicit MemoryDiagnosticsTask(Kind kind,uint32_t base=0,uint32_t length=0,uint16_t pattern=0,uint16_t stride=0,uint16_t mask=0,uint8_t mode=0,uint8_t rounds=0){base_=base;length_=length;pattern_=pattern;stride_=stride;mask_=mask;mode_=mode;rounds_=rounds;switch(kind){case Kind::Memory:stage_=Stage::BindMemoryNotice;break;case Kind::Patterns:stage_=Stage::BindPatternStart;break;case Kind::Pair:stage_=Stage::BindWritePass;break;case Kind::Pass:stage_=Stage::BindPassCursor;break;case Kind::ControllerFault:stage_=Stage::HaltController;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t flags){if(!awaitingChild())return false;childFlags_=flags;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindMemoryNotice:case Stage::BindMemoryBase:case Stage::BindMemoryLength:case Stage::BindMemorySuccess:case Stage::BindMemoryFailure:return 9; case Stage::ShowMemoryNotice:case Stage::ShowMemorySuccess:case Stage::ShowMemoryFailure:case Stage::ShowFailureHigh:case Stage::ShowFailureLow:case Stage::SaveWritePattern:case Stage::RestoreWritePattern:case Stage::SaveVerifyPattern:case Stage::RestoreVerifyPattern:return 20; case Stage::TestMemory:case Stage::TestPatternPair:case Stage::TestInversePair:case Stage::WritePattern:case Stage::VerifyPattern:return 15; case Stage::ReturnMemorySuccess:case Stage::ReturnPattern:case Stage::ReturnPatternPair:case Stage::ReturnPassFailure:case Stage::ReturnPass:return 17; case Stage::BindPatternStart:case Stage::BindPatternStride:case Stage::NextPatternStart:case Stage::NextPatternStride:return 6; case Stage::WritePassPattern:case Stage::ReadPassPattern:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::HaltController:stage_=Stage::HaltController;break;
case Stage::TestMemoryFailure:failed_=childFlags_&4;next();break;
case Stage::SelectMemoryFailure:stage_=failed_?Stage::BindMemoryFailure:Stage::BindMemorySuccess;break;
case Stage::HaltFailure:stage_=Stage::HaltFailure;break;
case Stage::BindPatternStart:pattern_=0;next();break;
case Stage::BindPatternStride:stride_=1;next();break;
case Stage::ClearPatternMask:mask_=0;next();break;
case Stage::TestFirstFailure:failed_=childFlags_&4;next();break;
case Stage::SelectFirstFailure:stage_=failed_?Stage::ReturnPattern:Stage::InvertPatternMask;break;
case Stage::InvertPatternMask:mask_=uint16_t(~mask_);next();break;
case Stage::TestInverseFailure:failed_=childFlags_&4;next();break;
case Stage::SelectInverseFailure:stage_=failed_?Stage::ReturnPattern:Stage::NextPatternStart;break;
case Stage::NextPatternStart:pattern_=uint16_t(pattern_+0xd05);next();break;
case Stage::NextPatternStride:stride_=uint16_t(stride_+0x4d);next();break;
case Stage::DecrementPatternRounds:--rounds_;next();break;
case Stage::RepeatPattern:stage_=rounds_?Stage::ClearPatternMask:Stage::ReturnPattern;break;
case Stage::BindWritePass:mode_=0;next();break;
case Stage::BindVerifyPass:mode_=1;next();break;
case Stage::BindPassCursor:cursor_=base_;next();break;
case Stage::BindPassCount:remaining_=length_;next();break;
case Stage::BindPassPattern:expected_=pattern_;next();break;
case Stage::MixPassPattern:expected_^=mask_;next();break;
case Stage::SelectPassMode:stage_=mode_&1?Stage::ReadPassPattern:Stage::WritePassPattern;break;
case Stage::WritePassPattern:write(Write::Pattern,cursor_,expected_);next();break;
case Stage::JoinPassAdvance:stage_=Stage::AdvancePassPattern;break;
case Stage::ReadPassPattern:actual_=uint16_t(read(Read::Pattern,cursor_));next();break;
case Stage::SelectPassMatch:stage_=actual_==expected_?Stage::AdvancePassPattern:Stage::MarkPassFailure;break;
case Stage::MarkPassFailure:mode_|=4;next();break;
case Stage::AdvancePassPattern:pattern_=uint16_t(pattern_+stride_);next();break;
case Stage::AdvancePassCursor:cursor_+=2;next();break;
case Stage::DecrementPassCount:--remaining_;next();break;
case Stage::RepeatPass:stage_=remaining_?Stage::BindPassPattern:Stage::ReturnPass;break;
case Stage::ShowMemoryNotice:await(Stage::BindMemoryBase);break;
case Stage::TestMemory:await(Stage::TestMemoryFailure);break;
case Stage::ShowMemorySuccess:await(Stage::ReturnMemorySuccess);break;
case Stage::ReturnMemorySuccess:stage_=Stage::Complete;break;
case Stage::ShowMemoryFailure:await(Stage::BindFailureHigh);break;
case Stage::ShowFailureHigh:await(Stage::BindFailureLow);break;
case Stage::ShowFailureLow:await(Stage::HaltFailure);break;
case Stage::TestPatternPair:await(Stage::TestFirstFailure);break;
case Stage::TestInversePair:await(Stage::TestInverseFailure);break;
case Stage::ReturnPattern:stage_=Stage::Complete;break;
case Stage::WritePattern:await(Stage::RestoreWritePattern);break;
case Stage::VerifyPattern:await(Stage::RestoreVerifyPattern);break;
case Stage::ReturnPatternPair:stage_=Stage::Complete;break;
case Stage::ReturnPassFailure:stage_=Stage::Complete;break;
case Stage::ReturnPass:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t base_=0,length_=0,cursor_=0,remaining_=0;uint16_t pattern_=0,stride_=0,mask_=0,actual_=0,expected_=0;uint8_t mode_=0,rounds_=0,childFlags_=0;bool failed_=false;
};
}
