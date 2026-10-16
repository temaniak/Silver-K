#pragma once
#include <array>
#include <cstdint>
namespace ks {class StateProgramChangeTask final {public:enum class Kind {ReceivedMulti,CommitSingle,CommitMulti,LoadSingle};enum class Stage {MaskReceivedBank,CompareReceivedBank,AdmitReceivedBank,CompareReceivedProgram,AdmitReceivedProgram,WriteReceivedProgram,ReadPreviousMode,SetMultiMode,WriteMultiMode,ComparePreviousMode,AdmitPreviousMode,ClearSinglePanel,ResetMultiPanel,LoadReceivedMulti,RefreshReceivedDisplay,ReturnReceived,SetSingleGlobals,ReadSingleLimit,CompareSingleLimit,AdmitSingleLimit,LoadCurrentSingle,RefreshSingleContext,SetSingleMessage,ReadSingleBank,ReadOverlay,AdmitOverlay,MarkOverlay,ClearOverlay,WriteOverlay,SendSingleBank,ReadSingleProgram,SendSingleProgram,ReadSingleWord,CompareSingleBank,AdmitSingleBank,WriteRememberedSingle,ReturnSingle,SetMultiGlobals,ReadMultiLimit,CompareMultiLimit,AdmitMultiLimit,LoadCurrentMulti,ReadSelection,RetainSelection,ClearSelection,WriteClearedSelection,SetMultiMessage,ReadMultiBank,MarkMulti,SendMultiBank,ReadMultiProgram,SendMultiProgram,RestoreSelection,WriteRestoredSelection,ReturnMulti,GetSingleAddress,LoadSingleAddress,BindSingleVoices,RefreshSingleVoices,PrepareSingleEngine,ClearReady,WritePatchReady,WritePanelReady,ReturnLoad,AwaitChild,Complete};enum class Read {PreviousMode,SingleLimit,SingleBank,Overlay,SingleProgram,SingleWord,MultiLimit,Selection,MultiBank,MultiProgram};enum class Write {ReceivedProgram,MultiMode,Overlay,RememberedSingle,Selection,PatchReady,PanelReady};enum class Child {ClearSinglePanel,ResetMultiPanel,LoadReceivedMulti,RefreshReceivedDisplay,LoadCurrentSingle,RefreshSingleContext,SendSingleBank,SendSingleProgram,LoadCurrentMulti,SendMultiBank,SendMultiProgram,GetSingleAddress,LoadSingleAddress,BindSingleVoices,RefreshSingleVoices,PrepareSingleEngine};explicit StateProgramChangeTask(Kind k,uint16_t received=0):phase_(k==Kind::ReceivedMulti?Stage::MaskReceivedBank:k==Kind::CommitSingle?Stage::SetSingleGlobals:k==Kind::CommitMulti?Stage::SetMultiGlobals:Stage::GetSingleAddress),input_(received){}Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingChild()const{return phase_==Stage::AwaitChild;}Child child()const{return child_;}uint8_t bank()const{return bank_;}uint8_t program()const{return program_;}bool resumeChild(){if(!awaitingChild())return false;phase_=resume_;return true;}
 unsigned nextClocks(unsigned storeClocks=3)const{constexpr std::array<unsigned,64> costs{{3,3,3,3,3,12,12,3,12,3,3,20,20,20,20,17,9,15,3,3,20,20,3,12,12,3,3,3,12,20,12,20,12,3,3,12,17,9,15,3,3,20,12,11,3,12,3,12,3,20,12,20,11,12,17,15,15,20,20,20,3,15,12,17}};if(unsigned(phase_)>=costs.size())return 0;switch(phase_){case Stage::WriteReceivedProgram:case Stage::WriteMultiMode:case Stage::WriteOverlay:case Stage::WriteRememberedSingle:case Stage::WriteClearedSelection:case Stage::WriteRestoredSelection:case Stage::WritePatchReady:case Stage::WritePanelReady: return costs[unsigned(phase_)]-3+storeClocks;default:return costs[unsigned(phase_)];}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){unsigned used=0;while(const auto c=nextClocks(storeClocks)){if(c>available-used)break;switch(phase_){case Stage::MaskReceivedBank:input_=(input_&0x3fff);next();break;
case Stage::AdmitReceivedBank:phase_=uint8_t(input_>>8)==1?Stage::CompareReceivedProgram:Stage::ReturnReceived;break;
case Stage::AdmitReceivedProgram:phase_=uint8_t(input_)<100?Stage::WriteReceivedProgram:Stage::ReturnReceived;break;
case Stage::WriteReceivedProgram:write(Write::ReceivedProgram,uint8_t(input_));next();break;
case Stage::ReadPreviousMode:mode_=uint8_t(read(Read::PreviousMode));next();break;
case Stage::WriteMultiMode:write(Write::MultiMode,1);next();break;
case Stage::AdmitPreviousMode:phase_=mode_==1?Stage::LoadReceivedMulti:Stage::ClearSinglePanel;break;
case Stage::ClearSinglePanel:child_=Child::ClearSinglePanel;resume_=Stage::ResetMultiPanel;phase_=Stage::AwaitChild;break;
case Stage::ResetMultiPanel:child_=Child::ResetMultiPanel;resume_=Stage::LoadReceivedMulti;phase_=Stage::AwaitChild;break;
case Stage::LoadReceivedMulti:child_=Child::LoadReceivedMulti;resume_=Stage::RefreshReceivedDisplay;phase_=Stage::AwaitChild;break;
case Stage::RefreshReceivedDisplay:child_=Child::RefreshReceivedDisplay;resume_=Stage::ReturnReceived;phase_=Stage::AwaitChild;break;
case Stage::ReturnReceived:phase_=Stage::Complete;break;
case Stage::ReadSingleLimit:limit_=uint8_t(read(Read::SingleLimit));next();break;
case Stage::AdmitSingleLimit:phase_=limit_<=63?Stage::SetSingleMessage:Stage::LoadCurrentSingle;break;
case Stage::LoadCurrentSingle:child_=Child::LoadCurrentSingle;resume_=Stage::RefreshSingleContext;phase_=Stage::AwaitChild;break;
case Stage::RefreshSingleContext:child_=Child::RefreshSingleContext;resume_=Stage::SetSingleMessage;phase_=Stage::AwaitChild;break;
case Stage::ReadSingleBank:bank_=uint8_t(read(Read::SingleBank));next();break;
case Stage::ReadOverlay:overlay_=uint8_t(read(Read::Overlay));next();break;
case Stage::AdmitOverlay:phase_=overlay_?Stage::MarkOverlay:Stage::SendSingleBank;break;
case Stage::MarkOverlay:bank_|=32;next();break;
case Stage::WriteOverlay:write(Write::Overlay,0);next();break;
case Stage::SendSingleBank:child_=Child::SendSingleBank;resume_=Stage::ReadSingleProgram;phase_=Stage::AwaitChild;break;
case Stage::ReadSingleProgram:program_=uint8_t(read(Read::SingleProgram));next();break;
case Stage::SendSingleProgram:child_=Child::SendSingleProgram;resume_=Stage::ReadSingleWord;phase_=Stage::AwaitChild;break;
case Stage::ReadSingleWord:word_=uint16_t(read(Read::SingleWord));next();break;
case Stage::AdmitSingleBank:phase_=uint8_t(word_>>8)<=4?Stage::ReturnSingle:Stage::WriteRememberedSingle;break;
case Stage::WriteRememberedSingle:write(Write::RememberedSingle,word_);next();break;
case Stage::ReturnSingle:phase_=Stage::Complete;break;
case Stage::ReadMultiLimit:limit_=uint8_t(read(Read::MultiLimit));next();break;
case Stage::AdmitMultiLimit:phase_=limit_<=63?Stage::ReadSelection:Stage::LoadCurrentMulti;break;
case Stage::LoadCurrentMulti:child_=Child::LoadCurrentMulti;resume_=Stage::ReadSelection;phase_=Stage::AwaitChild;break;
case Stage::ReadSelection:selection_=uint8_t(read(Read::Selection));next();break;
case Stage::WriteClearedSelection:write(Write::Selection,0);next();break;
case Stage::ReadMultiBank:bank_=uint8_t(read(Read::MultiBank));next();break;
case Stage::MarkMulti:bank_|=64;next();break;
case Stage::SendMultiBank:child_=Child::SendMultiBank;resume_=Stage::ReadMultiProgram;phase_=Stage::AwaitChild;break;
case Stage::ReadMultiProgram:program_=uint8_t(read(Read::MultiProgram));next();break;
case Stage::SendMultiProgram:child_=Child::SendMultiProgram;resume_=Stage::RestoreSelection;phase_=Stage::AwaitChild;break;
case Stage::WriteRestoredSelection:write(Write::Selection,selection_);next();break;
case Stage::ReturnMulti:phase_=Stage::Complete;break;
case Stage::GetSingleAddress:child_=Child::GetSingleAddress;resume_=Stage::LoadSingleAddress;phase_=Stage::AwaitChild;break;
case Stage::LoadSingleAddress:child_=Child::LoadSingleAddress;resume_=Stage::BindSingleVoices;phase_=Stage::AwaitChild;break;
case Stage::BindSingleVoices:child_=Child::BindSingleVoices;resume_=Stage::RefreshSingleVoices;phase_=Stage::AwaitChild;break;
case Stage::RefreshSingleVoices:child_=Child::RefreshSingleVoices;resume_=Stage::PrepareSingleEngine;phase_=Stage::AwaitChild;break;
case Stage::PrepareSingleEngine:child_=Child::PrepareSingleEngine;resume_=Stage::ClearReady;phase_=Stage::AwaitChild;break;
case Stage::WritePatchReady:write(Write::PatchReady,0);next();break;
case Stage::WritePanelReady:write(Write::PanelReady,0);next();break;
case Stage::ReturnLoad:phase_=Stage::Complete;break; default:next();break;}used+=c;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_,resume_=Stage::Complete;Child child_=Child::ClearSinglePanel;uint16_t input_=0,word_=0;uint8_t mode_=0,limit_=0,bank_=0,program_=0,overlay_=0,selection_=0;};}
