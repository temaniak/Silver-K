#pragma once
#include <array>
#include <cstdint>
namespace ks {
class StateStoreServicesTask final {public:enum class Kind {Offset,Check,Message};enum class Stage {SetFlagBase,ReadFlag,AdmitProgram,AdvanceSource,AdvanceTarget,RetainSource,RetainTarget,DispatchMessage,RestoreTarget,RestoreSource,ReturnOffset,SetCheckBase,ReadCheckFlag,AdmitCheck,DispatchCheckMessage,ClearResult,ReturnCheck,SetMessage,DispatchDisplay,AwaitMessage,AwaitProgram,AwaitDisplay,Complete};enum class Service {Message,Program,Display};
 explicit StateStoreServicesTask(Kind k):phase_(k==Kind::Offset?Stage::SetFlagBase:k==Kind::Check?Stage::SetCheckBase:Stage::SetMessage){}
 Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingMessage()const{return phase_==Stage::AwaitMessage;}bool awaitingTail()const{return phase_==Stage::AwaitProgram||phase_==Stage::AwaitDisplay;}Service service()const{return phase_==Stage::AwaitProgram?Service::Program:phase_==Stage::AwaitDisplay?Service::Display:Service::Message;}
 Stage resumeStage()const{return resume_;}bool resumeMessage(){if(!awaitingMessage())return false;phase_=resume_;return true;}bool retireTail(){if(!awaitingTail())return false;phase_=Stage::Complete;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,19> costs{{9,15,3,9,9,20,20,15,20,20,17,9,15,3,15,3,17,9,8}};return unsigned(phase_)<costs.size()?costs[unsigned(phase_)]:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;switch(phase_){case Stage::ReadFlag:case Stage::ReadCheckFlag:flag_=uint8_t(read());next();break;case Stage::AdmitProgram:phase_=flag_?Stage::AwaitProgram:Stage::AdvanceSource;break;case Stage::DispatchMessage:resume_=Stage::RestoreTarget;phase_=Stage::AwaitMessage;break;case Stage::AdmitCheck:phase_=flag_?Stage::ReturnCheck:Stage::DispatchCheckMessage;break;case Stage::DispatchCheckMessage:resume_=Stage::ClearResult;phase_=Stage::AwaitMessage;break;case Stage::DispatchDisplay:phase_=Stage::AwaitDisplay;break;case Stage::ReturnOffset:case Stage::ReturnCheck:phase_=Stage::Complete;break;default:next();break;}used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_,resume_=Stage::Complete;uint8_t flag_=0;
};}
