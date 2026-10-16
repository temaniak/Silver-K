#pragma once
#include <array>
#include <cstdint>
namespace ks {
class StateFlashCommitTask final {public:enum class Stage {ClearPending,WritePending,DispatchProgram,RetainSource,RetainTarget,ReadPending,AdmitPending,DispatchIdle,RestoreTarget,RestoreSource,ReturnCommit,AwaitChild,Complete};enum class Child {Program,Idle};
 Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingChild()const{return phase_==Stage::AwaitChild;}Child child()const{return child_;}bool resumeChild(){if(!awaitingChild())return false;phase_=resume_;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,11> costs{{3,15,20,20,20,15,3,20,20,20,17}};return unsigned(phase_)<costs.size()?costs[unsigned(phase_)]:0;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;switch(phase_){case Stage::WritePending:write(uint16_t(0));next();break;case Stage::DispatchProgram:await(Child::Program,Stage::RetainSource);break;case Stage::ReadPending:pending_=uint16_t(read());next();break;case Stage::AdmitPending:phase_=pending_?Stage::DispatchIdle:Stage::RestoreTarget;break;case Stage::DispatchIdle:await(Child::Idle,Stage::RestoreTarget);break;case Stage::ReturnCommit:phase_=Stage::Complete;break;default:next();break;}used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}void await(Child c,Stage s){child_=c;resume_=s;phase_=Stage::AwaitChild;}Stage phase_=Stage::ClearPending,resume_=Stage::Complete;Child child_=Child::Program;uint16_t pending_=0;
};}
