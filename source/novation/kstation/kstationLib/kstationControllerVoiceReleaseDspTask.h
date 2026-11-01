#pragma once
#include "kstationVoiceAllocationTask.h"
#include "kstationVoiceUpdateTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceReleaseDspTask final {
 using D=VoiceDspSelectTask;using G=VoiceGateTask;using W=VoiceDspWriteTask;
public:
 enum class Kind:uint8_t{Select,Gate};explicit ControllerVoiceReleaseDspTask(Kind kind):kind_(kind){if(kind==Kind::Select)select_.emplace();else gate_.emplace();}bool complete()const{return kind_==Kind::Select?select_->complete():gate_->complete();}bool awaitingDsp()const{return select_?select_->awaitingDsp():write_&&write_->awaitingDsp();}bool resumeDsp(){return awaitingDsp()&&(select_?select_->resumeDsp():write_->resumeDsp());}bool isWrite()const{return kind_==Kind::Gate;}uint16_t address()const{return write_?write_->command():0;}
 bool stackWrites()const{return select_?select_->stage()==D::Stage::DispatchInvalidate:write_?write_->stage()==W::Stage::DispatchWord:gate_->stage()==G::Stage::DispatchWrite;}
 unsigned nextClocks()const{if(select_){D::Credits c{3,3,select_->stage()==D::Stage::Return?7u:10u};return select_->nextClocks(c);}if(write_){W::Credits c{3,write_->stage()==W::Stage::Return?7u:10u};return write_->nextClocks(c);}G::Credits c{gate_->stage()==G::Stage::Return?7u:10u};return gate_->nextClocks(c);}
 template<class R,class Wr>unsigned advanceBefore(unsigned budget,R&&read,Wr&&write){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(select_){D::Credits c{3,3,select_->stage()==D::Stage::Return?7u:10u};return select_->advanceBefore(cost,c,[&](){return read(0xd023,1,cost);},[&](D::Write f,uint16_t v){write(f==D::Write::First?0xd024:0xd026,v,2,cost);});}if(write_){W::Credits c{3,write_->stage()==W::Stage::Return?7u:10u};auto used=write_->advanceBefore(cost,c,[&](bool second){return uint16_t(read(second?0xd026:0xd024,2,cost));});if(write_->complete()){write_.reset();gate_->resumeWrite();}return used;}G::Credits c{gate_->stage()==G::Stage::Return?7u:10u};auto used=gate_->advanceBefore(cost,c);if(gate_->awaitingWrite())write_.emplace(0x8052);return used;}
private:Kind kind_;std::optional<D>select_{};std::optional<G>gate_{};std::optional<W>write_{};
};
}
