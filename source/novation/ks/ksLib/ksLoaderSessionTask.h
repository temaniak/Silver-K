#pragma once
#include <cstdint>
namespace ks {
// Firmware-update LoaderSession policy. Logical data and deferred effects; CPU/stack/flags projection remains in the temporary compatibility bridge.
class LoaderSessionTask final {
public:
 enum class Kind { Update };enum class Stage { ReadEntryInputs, CompareEntryInputs, SelectUpdate, ReturnIgnored, BindUpdateNotice, ShowUpdateNotice, ConfirmUpdate, SelectConfirmation, ReturnCancelled, BindReceiveNotice, ShowReceiveNotice, ConfigureSerial, BindReceiveMode, WriteReceiveMode, LowerInterruptMask, SaveSettleWord, BindSettleOuter, SaveSettleOuter, BindSettleInner, TickSettleInner, RepeatSettleInner, RestoreSettleOuter, TickSettleOuter, RepeatSettleOuter, RestoreSettleWord, ResetReceiveQueue, ClearReceiveStatus, StoreReceiveStatus, ReceiveHeader, RetryHeader, BindImageNotice, ShowImageNotice, BindImageIdentity, ShowImageIdentity, ClearBlockIndex, ReadBlockCount, BindReceiveDestination, ReceiveBlock, CheckBlockStatus, CompareBlockChecksum, AcceptBlockChecksum, ShowBlockError, StopBlockError, NextBlockIndex, ReadProgressTotal, BindProgressIndex, ShowBlockProgress, TickBlockCount, RepeatBlocks, ReceiveFooter, CheckFooterStatus, MaskInterrupts, BindChecksumSource, ReadImageWords, DoubleImageWords, ClearImageSum, ClearSampleHigh, ReadImageWord, AddImageWord, TickImageWords, RepeatImageWords, CompareImageChecksum, AcceptImageChecksum, ShowChecksumError, StopChecksumError, StoreImageChecksum, AdvanceChecksumDestination, BindGuardCount, StoreGuardWord, AdvanceGuardDestination, TickGuardCount, RepeatGuard, BindSuccessStatus, StoreSuccessStatus, ReturnUpdate, AwaitChild, Complete };
 enum class Read { BlockCount, ImageWords, ImageWord };enum class Write { ReceiveMode, Status, Checksum, Guard };uint16_t remaining()const{return remaining_;}uint16_t total()const{return total_;}uint16_t sample()const{return sample_;}uint32_t cursor()const{return cursor_;}uint32_t words()const{return words_;}
 explicit LoaderSessionTask(Kind kind){switch(kind){case Kind::Update:stage_=Stage::ReadEntryInputs;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t reply,uint32_t sum,uint32_t cursor,bool accepted){if(!awaitingChild())return false;reply_=reply;ok_=accepted;if(resume_==Stage::CheckBlockStatus){sum_=sum;cursor_=cursor;}stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3,unsigned word=6)const{switch(stage_){case Stage::ReadEntryInputs:case Stage::ConfirmUpdate:case Stage::ReadBlockCount:case Stage::ReadProgressTotal:return 15; case Stage::CompareEntryInputs:case Stage::BindUpdateNotice:case Stage::BindReceiveNotice:case Stage::BindImageNotice:case Stage::BindImageIdentity:case Stage::BindReceiveDestination:case Stage::BindChecksumSource:case Stage::BindGuardCount:return 9; case Stage::ReturnIgnored:case Stage::ReturnCancelled:case Stage::ReturnUpdate:return 17; case Stage::ShowUpdateNotice:case Stage::ShowReceiveNotice:case Stage::ConfigureSerial:case Stage::ResetReceiveQueue:case Stage::ReceiveHeader:case Stage::ShowImageNotice:case Stage::ShowImageIdentity:case Stage::ReceiveBlock:case Stage::ShowBlockError:case Stage::ShowBlockProgress:case Stage::ReceiveFooter:case Stage::ShowChecksumError:return 20; case Stage::WriteReceiveMode:return 3+store; case Stage::LowerInterruptMask:case Stage::BindSettleOuter:case Stage::BindSettleInner:case Stage::MaskInterrupts:return 6; case Stage::SaveSettleWord:case Stage::SaveSettleOuter:case Stage::RestoreSettleOuter:case Stage::RestoreSettleWord:return 11; case Stage::StoreReceiveStatus:case Stage::StoreSuccessStatus:return 9+store; case Stage::ReadImageWords:return 24; case Stage::ReadImageWord:return 5+word; case Stage::StoreImageChecksum:return 6+store; case Stage::StoreGuardWord:return 3+word; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3,unsigned word=6){const auto cost=nextClocks(store,word);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectUpdate:stage_=reply_==0xffbfbfbfu?Stage::BindUpdateNotice:Stage::ReturnIgnored;break;
case Stage::SelectConfirmation:stage_=ok_?Stage::BindReceiveNotice:Stage::ReturnCancelled;break;
case Stage::WriteReceiveMode:write(Write::ReceiveMode,0,0x50);next();break;
case Stage::BindSettleOuter:outer_=10;next();break;
case Stage::BindSettleInner:inner_=2663;next();break;
case Stage::TickSettleInner:--inner_;next();break;
case Stage::RepeatSettleInner:stage_=inner_?Stage::TickSettleInner:Stage::RestoreSettleOuter;break;
case Stage::TickSettleOuter:--outer_;next();break;
case Stage::RepeatSettleOuter:stage_=outer_?Stage::SaveSettleOuter:Stage::RestoreSettleWord;break;
case Stage::StoreReceiveStatus:write(Write::Status,0,0);next();break;
case Stage::RetryHeader:stage_=ok_?Stage::BindImageNotice:Stage::ReceiveHeader;break;
case Stage::ReadBlockCount:remaining_=uint16_t(read(Read::BlockCount,0));next();break;
case Stage::CheckBlockStatus:stage_=ok_?Stage::CompareBlockChecksum:Stage::ShowBlockError;break;
case Stage::AcceptBlockChecksum:stage_=sum_==reply_?Stage::NextBlockIndex:Stage::ShowBlockError;break;
case Stage::StopBlockError:stage_=Stage::StopBlockError;break;
case Stage::ReadProgressTotal:total_=uint16_t(read(Read::BlockCount,0));next();break;
case Stage::TickBlockCount:--remaining_;next();break;
case Stage::RepeatBlocks:stage_=remaining_?Stage::ReceiveBlock:Stage::ReceiveFooter;break;
case Stage::CheckFooterStatus:stage_=ok_?Stage::MaskInterrupts:Stage::ShowChecksumError;break;
case Stage::BindChecksumSource:cursor_=0xa04000;next();break;
case Stage::ReadImageWords:words_=uint32_t(read(Read::ImageWords,0));next();break;
case Stage::DoubleImageWords:words_>>=1;next();break;
case Stage::ClearImageSum:sum_=0;next();break;
case Stage::ReadImageWord:sample_=uint16_t(read(Read::ImageWord,cursor_));cursor_+=2;next();break;
case Stage::AddImageWord:sum_+=sample_;next();break;
case Stage::TickImageWords:--words_;next();break;
case Stage::RepeatImageWords:stage_=words_?Stage::ReadImageWord:Stage::CompareImageChecksum;break;
case Stage::AcceptImageChecksum:stage_=sum_==reply_?Stage::StoreImageChecksum:Stage::ShowChecksumError;break;
case Stage::StopChecksumError:stage_=Stage::StopChecksumError;break;
case Stage::StoreImageChecksum:write(Write::Checksum,cursor_,reply_);next();break;
case Stage::AdvanceChecksumDestination:cursor_+=4;next();break;
case Stage::BindGuardCount:guards_=128;next();break;
case Stage::StoreGuardWord:write(Write::Guard,cursor_,0);next();break;
case Stage::AdvanceGuardDestination:cursor_+=2;next();break;
case Stage::TickGuardCount:--guards_;next();break;
case Stage::RepeatGuard:stage_=guards_?Stage::StoreGuardWord:Stage::BindSuccessStatus;break;
case Stage::StoreSuccessStatus:write(Write::Status,0,0x55);next();break;
case Stage::ReadEntryInputs:await(Stage::CompareEntryInputs);break;
case Stage::ReturnIgnored:stage_=Stage::Complete;break;
case Stage::ShowUpdateNotice:await(Stage::ConfirmUpdate);break;
case Stage::ConfirmUpdate:await(Stage::SelectConfirmation);break;
case Stage::ReturnCancelled:stage_=Stage::Complete;break;
case Stage::ShowReceiveNotice:await(Stage::ConfigureSerial);break;
case Stage::ConfigureSerial:await(Stage::BindReceiveMode);break;
case Stage::ResetReceiveQueue:await(Stage::ClearReceiveStatus);break;
case Stage::ReceiveHeader:await(Stage::RetryHeader);break;
case Stage::ShowImageNotice:await(Stage::BindImageIdentity);break;
case Stage::ShowImageIdentity:await(Stage::ClearBlockIndex);break;
case Stage::ReceiveBlock:await(Stage::CheckBlockStatus);break;
case Stage::ShowBlockError:await(Stage::StopBlockError);break;
case Stage::ShowBlockProgress:await(Stage::TickBlockCount);break;
case Stage::ReceiveFooter:await(Stage::CheckFooterStatus);break;
case Stage::ShowChecksumError:await(Stage::StopChecksumError);break;
case Stage::ReturnUpdate:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t reply_=0,sum_=0,cursor_=0,words_=0;uint16_t remaining_=0,total_=0,sample_=0,guards_=0,outer_=0,inner_=0;bool ok_=false;
};
}
