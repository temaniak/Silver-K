#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Exactly sixteen name bytes, observed/stored one byte at a time, followed by
// the scratch terminator. Six entry forms choose current/multi/selected storage.
class StatePatchNameTask final {
public:enum class Entry {ReadPatch,ReadMulti,ReadProvided,WritePatch,WriteMulti,WriteProvided,WriteSelected,ReadSelected,Empty};enum class Stage {ReadPatchBase,AddPatchName,JumpRead,SetReadMulti,AddReadMulti,SetReadScratch,CaptureScratch,SetReadLength,ReadName,WriteScratch,NextScratch,DecrementRead,AdmitRead,ClearReadTerm,WriteReadTerm,ReturnRead,ReadWriteBase,AddWritePatch,JumpWrite,SetWriteMulti,AddWriteMulti,SetWriteScratch,SetWriteLength,ReadScratch,WriteName,NextName,DecrementWrite,AdmitWrite,ClearWriteTerm,WriteWriteTerm,ReturnWrite,ReadSelectedWrite,JumpSelectedWrite,ReadSelectedRead,JumpSelectedRead,ReturnEmpty,Complete};enum class Read {CurrentBase,SelectedBase,Name,Scratch};enum class Write {Name,Scratch};
 explicit StatePatchNameTask(Entry e):phase_(e==Entry::ReadPatch?Stage::ReadPatchBase:e==Entry::ReadMulti?Stage::SetReadMulti:e==Entry::ReadProvided?Stage::SetReadScratch:e==Entry::WritePatch?Stage::ReadWriteBase:e==Entry::WriteMulti?Stage::SetWriteMulti:e==Entry::WriteProvided?Stage::SetWriteScratch:e==Entry::WriteSelected?Stage::ReadSelectedWrite:e==Entry::ReadSelected?Stage::ReadSelectedRead:Stage::ReturnEmpty){}
 Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}unsigned byteOffset()const{return offset_;}uint8_t byte()const{return byte_;}
 unsigned nextClocks(unsigned storeClocks=3)const{constexpr std::array<unsigned,36> costs{{18,9,3,9,9,9,3,6,8,6,3,3,3,3,6,17,18,9,3,9,9,9,6,8,6,3,3,3,3,6,17,18,3,18,3,17}};if(unsigned(phase_)>=costs.size())return 0;const auto c=costs[unsigned(phase_)];return phase_==Stage::WriteScratch||phase_==Stage::WriteName||phase_==Stage::WriteReadTerm||phase_==Stage::WriteWriteTerm?c-3+storeClocks:c;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){unsigned used=0;while(const auto cost=nextClocks(storeClocks)){if(cost>available-used)break;switch(phase_){
 case Stage::ReadPatchBase:case Stage::ReadWriteBase:(void)read(Read::CurrentBase);next();break;case Stage::ReadSelectedWrite:case Stage::ReadSelectedRead:(void)read(Read::SelectedBase);next();break;
 case Stage::JumpRead:case Stage::JumpSelectedRead:phase_=Stage::SetReadScratch;break;case Stage::JumpWrite:case Stage::JumpSelectedWrite:phase_=Stage::SetWriteScratch;break;
 case Stage::ReadName:byte_=uint8_t(read(Read::Name));next();break;case Stage::WriteScratch:write(Write::Scratch,byte_);next();break;case Stage::NextScratch:++offset_;next();break;case Stage::AdmitRead:phase_=offset_<16?Stage::ReadName:Stage::ClearReadTerm;break;
 case Stage::ReadScratch:byte_=uint8_t(read(Read::Scratch));next();break;case Stage::WriteName:write(Write::Name,byte_);next();break;case Stage::NextName:++offset_;next();break;case Stage::AdmitWrite:phase_=offset_<16?Stage::ReadScratch:Stage::ClearWriteTerm;break;
 case Stage::ClearReadTerm:case Stage::ClearWriteTerm:byte_=0;next();break;case Stage::WriteReadTerm:case Stage::WriteWriteTerm:write(Write::Scratch,0);next();break;
 case Stage::ReturnRead:case Stage::ReturnWrite:case Stage::ReturnEmpty:phase_=Stage::Complete;break;default:next();break;
 }used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_;unsigned offset_=0;uint8_t byte_=0;
};}
