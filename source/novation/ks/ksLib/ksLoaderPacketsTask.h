#pragma once
#include <cstdint>
namespace ks {
// Firmware-update LoaderPackets policy. Logical data and deferred effects; CPU/stack/flags projection remains in the temporary compatibility bridge.
class LoaderPacketsTask final {
public:
 enum class Kind { Header, Footer, Block };enum class Stage { FindHeader, ReadHeaderTag, CompareHeaderTag, RejectHeaderTag, ReadHeaderReserved, ReadHeaderModel, StoreHeaderModel, ReadHeaderDevice, StoreHeaderDevice, ReadHeaderUnused, ReadHeaderCount, StoreHeaderCount, ReadHeaderSize, StoreHeaderSize, AcceptHeader, ReturnHeader, FindFooter, ReadFooterTag, CompareFooterTag, RejectFooterTag, ReadFooterReserved, ReadFooterModel, ReadSavedFooterModel, CompareFooterModel, RejectFooterModel, ReadFooterDevice, ReadSavedFooterDevice, CompareFooterDevice, RejectFooterDevice, ReadFooterUnused, ReadFooterChecksum, AcceptFooter, ReturnFooter, FindBlock, ReadBlockTag, CompareBlockTag, RejectBlockTag, ReadBlockReserved, ReadBlockModel, ReadSavedBlockModel, CompareBlockModel, RejectBlockModel, ReadBlockDevice, ReadSavedBlockDevice, CompareBlockDevice, RejectBlockDevice, ReadBlockUnused, ReadBlockIndex, CompareBlockIndex, RejectBlockIndex, ReadBlockGroups, DecodeBlock, ReadBlockChecksum, AcceptBlock, ReturnBlock, AwaitChild, Complete };
 enum class Read { Model, Device };enum class Write { Model, Device, Count, Size };uint8_t expected()const{return expected_;}
 explicit LoaderPacketsTask(Kind kind,uint16_t block=0){block_=block;switch(kind){case Kind::Header:stage_=Stage::FindHeader;break;case Kind::Footer:stage_=Stage::FindFooter;break;case Kind::Block:stage_=Stage::FindBlock;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t byte,uint32_t payload,uint32_t sum,uint32_t destination){if(!awaitingChild())return false;byte_=byte;payload_=payload;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::FindHeader:case Stage::ReadHeaderTag:case Stage::ReadHeaderReserved:case Stage::ReadHeaderModel:case Stage::ReadHeaderDevice:case Stage::ReadHeaderUnused:case Stage::ReadHeaderCount:case Stage::ReadHeaderSize:case Stage::FindFooter:case Stage::ReadFooterTag:case Stage::ReadFooterReserved:case Stage::ReadFooterModel:case Stage::ReadFooterDevice:case Stage::ReadFooterUnused:case Stage::ReadFooterChecksum:case Stage::ReadBlockTag:case Stage::ReadBlockReserved:case Stage::ReadBlockModel:case Stage::ReadBlockDevice:return 20; case Stage::StoreHeaderModel:case Stage::StoreHeaderDevice:return 9+store; case Stage::StoreHeaderCount:case Stage::FindBlock:case Stage::ReadBlockUnused:case Stage::ReadBlockIndex:case Stage::ReadBlockGroups:case Stage::DecodeBlock:case Stage::ReadBlockChecksum:return 15; case Stage::StoreHeaderSize:return 24; case Stage::ReturnHeader:case Stage::ReturnFooter:case Stage::ReturnBlock:return 17; case Stage::ReadSavedFooterModel:case Stage::ReadSavedFooterDevice:case Stage::ReadSavedBlockModel:case Stage::ReadSavedBlockDevice:return 12; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::RejectHeaderTag:stage_=byte_!=0x7f?Stage::ReturnHeader:Stage::ReadHeaderReserved;break;
case Stage::RejectFooterTag:stage_=byte_!=0x7d?Stage::ReturnFooter:Stage::ReadFooterReserved;break;
case Stage::RejectFooterModel:stage_=byte_!=expected_?Stage::ReturnFooter:Stage::ReadFooterDevice;break;
case Stage::RejectFooterDevice:stage_=byte_!=expected_?Stage::ReturnFooter:Stage::ReadFooterUnused;break;
case Stage::RejectBlockTag:stage_=byte_!=0x7e?Stage::ReturnBlock:Stage::ReadBlockReserved;break;
case Stage::RejectBlockModel:stage_=byte_!=expected_?Stage::ReturnBlock:Stage::ReadBlockDevice;break;
case Stage::RejectBlockDevice:stage_=byte_!=expected_?Stage::ReturnBlock:Stage::ReadBlockUnused;break;
case Stage::RejectBlockIndex:stage_=uint16_t(payload_)!=block_?Stage::ReturnBlock:Stage::ReadBlockGroups;break;
case Stage::StoreHeaderModel:write(Write::Model,byte_);next();break;
case Stage::StoreHeaderDevice:write(Write::Device,byte_);next();break;
case Stage::StoreHeaderCount:write(Write::Count,uint16_t(payload_));next();break;
case Stage::StoreHeaderSize:write(Write::Size,payload_);next();break;
case Stage::ReadSavedFooterModel:expected_=uint8_t(read(Read::Model));next();break;
case Stage::ReadSavedFooterDevice:expected_=uint8_t(read(Read::Device));next();break;
case Stage::ReadSavedBlockModel:expected_=uint8_t(read(Read::Model));next();break;
case Stage::ReadSavedBlockDevice:expected_=uint8_t(read(Read::Device));next();break;
case Stage::FindHeader:await(Stage::ReadHeaderTag);break;
case Stage::ReadHeaderTag:await(Stage::CompareHeaderTag);break;
case Stage::ReadHeaderReserved:await(Stage::ReadHeaderModel);break;
case Stage::ReadHeaderModel:await(Stage::StoreHeaderModel);break;
case Stage::ReadHeaderDevice:await(Stage::StoreHeaderDevice);break;
case Stage::ReadHeaderUnused:await(Stage::ReadHeaderCount);break;
case Stage::ReadHeaderCount:await(Stage::StoreHeaderCount);break;
case Stage::ReadHeaderSize:await(Stage::StoreHeaderSize);break;
case Stage::ReturnHeader:stage_=Stage::Complete;break;
case Stage::FindFooter:await(Stage::ReadFooterTag);break;
case Stage::ReadFooterTag:await(Stage::CompareFooterTag);break;
case Stage::ReadFooterReserved:await(Stage::ReadFooterModel);break;
case Stage::ReadFooterModel:await(Stage::ReadSavedFooterModel);break;
case Stage::ReadFooterDevice:await(Stage::ReadSavedFooterDevice);break;
case Stage::ReadFooterUnused:await(Stage::ReadFooterChecksum);break;
case Stage::ReadFooterChecksum:await(Stage::AcceptFooter);break;
case Stage::ReturnFooter:stage_=Stage::Complete;break;
case Stage::FindBlock:await(Stage::ReadBlockTag);break;
case Stage::ReadBlockTag:await(Stage::CompareBlockTag);break;
case Stage::ReadBlockReserved:await(Stage::ReadBlockModel);break;
case Stage::ReadBlockModel:await(Stage::ReadSavedBlockModel);break;
case Stage::ReadBlockDevice:await(Stage::ReadSavedBlockDevice);break;
case Stage::ReadBlockUnused:await(Stage::ReadBlockIndex);break;
case Stage::ReadBlockIndex:await(Stage::CompareBlockIndex);break;
case Stage::ReadBlockGroups:await(Stage::DecodeBlock);break;
case Stage::DecodeBlock:await(Stage::ReadBlockChecksum);break;
case Stage::ReadBlockChecksum:await(Stage::AcceptBlock);break;
case Stage::ReturnBlock:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t payload_=0;uint16_t block_=0;uint8_t byte_=0,expected_=0;
};
}
