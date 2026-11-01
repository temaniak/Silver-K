#pragma once
#include "kstationVoiceUpdateTask.h"
namespace kstation {
class ControllerVoiceDspWordTask final {
 using T=VoiceDspWriteTask;
public:ControllerVoiceDspWordTask(uint16_t command,uint32_t word):task_(command),word_(word&0xffffff){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaitingDsp();}bool resumeDsp(){return task_.resumeDsp();}uint16_t address()const{return task_.command();}uint32_t word()const{return word_;}bool stackWrites()const{return task_.stage()==T::Stage::DispatchWord;}unsigned nextClocks()const{T::Credits c{3,task_.stage()==T::Stage::Return?7u:10u};return task_.nextClocks(c);}template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;T::Credits c{3,task_.stage()==T::Stage::Return?7u:10u};return task_.advanceBefore(cost,c,[&](bool second){return uint16_t(read(second?0xd026:0xd024,2,cost));});}private:T task_;uint32_t word_;
};
}
