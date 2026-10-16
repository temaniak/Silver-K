#pragma once
#include <array>
#include <cstdint>
namespace ks {
class StatePresetAddressTask final {
public:enum class Kind {Current,Overlay};enum class Stage {SetCurrentSource,ReadCurrentIndex,SetCurrentStride,ScaleCurrent,AddCurrentBase,ReturnCurrent,RetainProgram,SetOverlaySource,ReadBank,CompareBank,AdmitBank,RestoreUnavailableProgram,MarkUnavailable,ReturnUnavailable,SetAlias,ReadProgram,CompareAlias,AdmitAlias,SetAliasProgram,ReadAliasIndex,RestoreAliasProgram,SetAliasStride,ScaleAlias,AddAliasBase,RestoreProgram,MarkAvailable,ReturnAlias,AwaitIndex,Complete};enum class Read {Bank,Program};enum class Write {Alias,Restore};
 explicit StatePresetAddressTask(Kind k):phase_(k==Kind::Current?Stage::SetCurrentSource:Stage::RetainProgram){}
 Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingIndex()const{return phase_==Stage::AwaitIndex;}bool resumeIndex(uint16_t index){if(!awaitingIndex())return false;index_=index;phase_=resume_;return true;}bool available()const{return available_;}uint32_t address()const{return address_;}
 unsigned nextClocks(unsigned storeClocks=3)const{constexpr std::array<unsigned,27> costs{{9,20,6,23,9,17,11,9,6,3,3,11,3,17,3,12,3,3,12,20,12,6,23,9,11,3,17}};return unsigned(phase_)>=costs.size()?0:phase_==Stage::SetAliasProgram||phase_==Stage::RestoreAliasProgram?costs[unsigned(phase_)]-3+storeClocks:costs[unsigned(phase_)];}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){unsigned used=0;while(const auto cost=nextClocks(storeClocks)){if(cost>available-used)break;switch(phase_){case Stage::ReadCurrentIndex:resume_=Stage::SetCurrentStride;phase_=Stage::AwaitIndex;break;case Stage::ReadAliasIndex:resume_=Stage::RestoreAliasProgram;phase_=Stage::AwaitIndex;break;
 case Stage::ScaleCurrent:case Stage::ScaleAlias:address_=uint32_t(index_)*256;next();break;case Stage::AddCurrentBase:case Stage::AddAliasBase:address_+=0x820000;available_=true;next();break;
 case Stage::ReadBank:bank_=uint8_t(read(Read::Bank));next();break;case Stage::AdmitBank:phase_=bank_<=4?Stage::RestoreUnavailableProgram:Stage::SetAlias;break;case Stage::ReadProgram:program_=uint8_t(read(Read::Program));next();break;case Stage::AdmitAlias:phase_=program_==49?Stage::RestoreUnavailableProgram:Stage::SetAliasProgram;break;
 case Stage::SetAliasProgram:write(Write::Alias,49);next();break;case Stage::RestoreAliasProgram:write(Write::Restore,program_);next();break;case Stage::MarkUnavailable:available_=false;next();break;
 case Stage::ReturnCurrent:case Stage::ReturnUnavailable:case Stage::ReturnAlias:phase_=Stage::Complete;break;default:next();break;
 }used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_,resume_=Stage::Complete;uint8_t bank_=0,program_=0;uint16_t index_=0;uint32_t address_=0;bool available_=false;
};}
