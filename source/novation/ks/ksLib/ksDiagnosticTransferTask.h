#pragma once
#include <cstdint>
namespace ks {
// Diagnostic dump header/block/footer lifecycle with live identity/size fields, wrapping block count and32-bit accumulated checksum.
class DiagnosticTransferTask final {
public:
 enum class Kind { Dump, Header, Footer, Block };enum class Stage { BindDumpNotice, ShowDumpNotice, BindDumpName, ShowDumpName, BindDumpIdentity, ReadDumpModel, ReadDumpDevice, StoreDumpModel, StoreDumpDevice, SendDumpHeader, ClearDumpChecksum, ClearDumpBlock, BindDumpData, ReadDumpBlocks, SendDumpBlock, AddDumpChecksum, NextDumpBlock, ReadDumpBlockTotal, BindDumpProgress, ShowDumpProgress, DecrementDumpBlocks, RepeatDumpBlock, SendDumpFooter, ReturnDump, BeginHeader, BindHeaderTag, SendHeaderTag, ClearHeaderFlags, SendHeaderFlags, ReadHeaderModel, SendHeaderModel, ReadHeaderDevice, SendHeaderDevice, ClearHeaderZero, SendHeaderZero, ReadHeaderBlocks, SendHeaderBlocks, ReadHeaderSize, SendHeaderSize, ClearHeaderReservedOne, SendHeaderReservedOne, ClearHeaderReservedTwo, SendHeaderReservedTwo, BindHeaderEnd, SendHeaderEnd, ReturnHeader, BeginFooter, BindFooterTag, SendFooterTag, ClearFooterFlags, SendFooterFlags, ReadFooterModel, SendFooterModel, ReadFooterDevice, SendFooterDevice, ClearFooterZero, SendFooterZero, BindFooterChecksum, SendFooterChecksum, ClearFooterReservedOne, SendFooterReservedOne, ClearFooterReservedTwo, SendFooterReservedTwo, BindFooterEnd, SendFooterEnd, ReturnFooter, BeginBlock, BindBlockTag, SendBlockTag, ClearBlockFlags, SendBlockFlags, ReadBlockModel, SendBlockModel, ReadBlockDevice, SendBlockDevice, ClearBlockZero, SendBlockZero, BindBlockIndex, SendBlockIndex, BindBlockSize, SendBlockSize, EncodeBlockData, BindBlockChecksum, SendBlockChecksum, ClearBlockReserved, SendBlockReserved, BindBlockEnd, SendBlockEnd, ReturnBlock, AwaitChild, Complete };
 enum class Read { Model, Device, SavedModel, SavedDevice, Blocks, Size };enum class Write { Model, Device };uint16_t remaining()const{return remaining_;}uint16_t total()const{return total_;}uint16_t block()const{return block_;}uint32_t sum()const{return sum_;}uint32_t size()const{return size_;}uint8_t model()const{return model_;}uint8_t device()const{return device_;}
 explicit DiagnosticTransferTask(Kind kind,uint16_t block=0,uint32_t checksum=0){block_=block;sum_=checksum;switch(kind){case Kind::Dump:stage_=Stage::BindDumpNotice;break;case Kind::Header:stage_=Stage::BeginHeader;break;case Kind::Footer:stage_=Stage::BeginFooter;break;case Kind::Block:stage_=Stage::BeginBlock;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t sum){if(!awaitingChild())return false;childSum_=sum;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindDumpNotice:case Stage::BindDumpName:case Stage::BindDumpIdentity:case Stage::BindDumpData:return 9; case Stage::ShowDumpNotice:case Stage::ShowDumpName:case Stage::SendDumpBlock:case Stage::ShowDumpProgress:case Stage::BeginHeader:case Stage::SendHeaderTag:case Stage::SendHeaderFlags:case Stage::SendHeaderModel:case Stage::SendHeaderDevice:case Stage::SendHeaderZero:case Stage::SendHeaderBlocks:case Stage::SendHeaderSize:case Stage::SendHeaderReservedOne:case Stage::SendHeaderReservedTwo:case Stage::SendHeaderEnd:case Stage::BeginFooter:case Stage::SendFooterTag:case Stage::SendFooterFlags:case Stage::SendFooterModel:case Stage::SendFooterDevice:case Stage::SendFooterZero:case Stage::SendFooterChecksum:case Stage::SendFooterReservedOne:case Stage::SendFooterReservedTwo:case Stage::SendFooterEnd:case Stage::SendBlockTag:case Stage::SendBlockFlags:case Stage::SendBlockModel:case Stage::SendBlockDevice:case Stage::SendBlockEnd:return 20; case Stage::ReadDumpModel:case Stage::ReadDumpDevice:return 8; case Stage::StoreDumpModel:case Stage::StoreDumpDevice:return 9+store; case Stage::SendDumpHeader:case Stage::ReadDumpBlockTotal:case Stage::SendDumpFooter:case Stage::ReadHeaderBlocks:case Stage::BeginBlock:case Stage::SendBlockZero:case Stage::SendBlockIndex:case Stage::SendBlockSize:case Stage::EncodeBlockData:case Stage::SendBlockChecksum:case Stage::SendBlockReserved:return 15; case Stage::ReadDumpBlocks:return 18; case Stage::ReturnDump:case Stage::ReturnHeader:case Stage::ReturnFooter:case Stage::ReturnBlock:return 17; case Stage::ReadHeaderModel:case Stage::ReadHeaderDevice:case Stage::ReadFooterModel:case Stage::ReadFooterDevice:case Stage::ReadBlockModel:case Stage::ReadBlockDevice:return 12; case Stage::ReadHeaderSize:return 24; case Stage::BindBlockSize:return 6; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadDumpModel:model_=uint8_t(read(Read::Model));next();break;
case Stage::ReadDumpDevice:device_=uint8_t(read(Read::Device));next();break;
case Stage::StoreDumpModel:write(Write::Model,model_);next();break;
case Stage::StoreDumpDevice:write(Write::Device,device_);next();break;
case Stage::ClearDumpChecksum:sum_=0;next();break;
case Stage::ClearDumpBlock:block_=0;next();break;
case Stage::ReadDumpBlocks:remaining_=uint16_t(read(Read::Blocks));next();break;
case Stage::AddDumpChecksum:sum_+=childSum_;next();break;
case Stage::NextDumpBlock:++block_;next();break;
case Stage::ReadDumpBlockTotal:total_=uint16_t(read(Read::Blocks));next();break;
case Stage::DecrementDumpBlocks:--remaining_;next();break;
case Stage::RepeatDumpBlock:stage_=remaining_?Stage::SendDumpBlock:Stage::SendDumpFooter;break;
case Stage::ReadHeaderModel:model_=uint8_t(read(Read::SavedModel));next();break;
case Stage::ReadHeaderDevice:device_=uint8_t(read(Read::SavedDevice));next();break;
case Stage::ReadHeaderBlocks:total_=uint16_t(read(Read::Blocks));next();break;
case Stage::ReadHeaderSize:size_=uint32_t(read(Read::Size));next();break;
case Stage::ReadFooterModel:model_=uint8_t(read(Read::SavedModel));next();break;
case Stage::ReadFooterDevice:device_=uint8_t(read(Read::SavedDevice));next();break;
case Stage::ReadBlockModel:model_=uint8_t(read(Read::SavedModel));next();break;
case Stage::ReadBlockDevice:device_=uint8_t(read(Read::SavedDevice));next();break;
case Stage::ShowDumpNotice:await(Stage::BindDumpName);break;
case Stage::ShowDumpName:await(Stage::BindDumpIdentity);break;
case Stage::SendDumpHeader:await(Stage::ClearDumpChecksum);break;
case Stage::SendDumpBlock:await(Stage::AddDumpChecksum);break;
case Stage::ShowDumpProgress:await(Stage::DecrementDumpBlocks);break;
case Stage::SendDumpFooter:await(Stage::ReturnDump);break;
case Stage::ReturnDump:stage_=Stage::Complete;break;
case Stage::BeginHeader:await(Stage::BindHeaderTag);break;
case Stage::SendHeaderTag:await(Stage::ClearHeaderFlags);break;
case Stage::SendHeaderFlags:await(Stage::ReadHeaderModel);break;
case Stage::SendHeaderModel:await(Stage::ReadHeaderDevice);break;
case Stage::SendHeaderDevice:await(Stage::ClearHeaderZero);break;
case Stage::SendHeaderZero:await(Stage::ReadHeaderBlocks);break;
case Stage::SendHeaderBlocks:await(Stage::ReadHeaderSize);break;
case Stage::SendHeaderSize:await(Stage::ClearHeaderReservedOne);break;
case Stage::SendHeaderReservedOne:await(Stage::ClearHeaderReservedTwo);break;
case Stage::SendHeaderReservedTwo:await(Stage::BindHeaderEnd);break;
case Stage::SendHeaderEnd:await(Stage::ReturnHeader);break;
case Stage::ReturnHeader:stage_=Stage::Complete;break;
case Stage::BeginFooter:await(Stage::BindFooterTag);break;
case Stage::SendFooterTag:await(Stage::ClearFooterFlags);break;
case Stage::SendFooterFlags:await(Stage::ReadFooterModel);break;
case Stage::SendFooterModel:await(Stage::ReadFooterDevice);break;
case Stage::SendFooterDevice:await(Stage::ClearFooterZero);break;
case Stage::SendFooterZero:await(Stage::BindFooterChecksum);break;
case Stage::SendFooterChecksum:await(Stage::ClearFooterReservedOne);break;
case Stage::SendFooterReservedOne:await(Stage::ClearFooterReservedTwo);break;
case Stage::SendFooterReservedTwo:await(Stage::BindFooterEnd);break;
case Stage::SendFooterEnd:await(Stage::ReturnFooter);break;
case Stage::ReturnFooter:stage_=Stage::Complete;break;
case Stage::BeginBlock:await(Stage::BindBlockTag);break;
case Stage::SendBlockTag:await(Stage::ClearBlockFlags);break;
case Stage::SendBlockFlags:await(Stage::ReadBlockModel);break;
case Stage::SendBlockModel:await(Stage::ReadBlockDevice);break;
case Stage::SendBlockDevice:await(Stage::ClearBlockZero);break;
case Stage::SendBlockZero:await(Stage::BindBlockIndex);break;
case Stage::SendBlockIndex:await(Stage::BindBlockSize);break;
case Stage::SendBlockSize:await(Stage::EncodeBlockData);break;
case Stage::EncodeBlockData:await(Stage::BindBlockChecksum);break;
case Stage::SendBlockChecksum:await(Stage::ClearBlockReserved);break;
case Stage::SendBlockReserved:await(Stage::BindBlockEnd);break;
case Stage::SendBlockEnd:await(Stage::ReturnBlock);break;
case Stage::ReturnBlock:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t sum_=0,childSum_=0,size_=0;uint16_t block_=0,remaining_=0,total_=0;uint8_t model_=0,device_=0;
};
}
