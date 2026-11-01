#pragma once
#include "kstationPatchPackTask.h"
#include <optional>
namespace kstation {
class ControllerPatchPackTask final {
 using R=PatchPackRefreshTask;using RS=R::Stage;using S=PatchPackSelectTask;using SS=S::Stage;using P=PatchPackTask;using PS=P::Stage;
public:
 bool unsupportedTarget()const{return unsupported_;}bool complete()const{return refresh_.complete()&&!select_&&!pack_;}
 bool stackWrites()const{if(pack_)return false;if(select_)return select_->stage()==SS::Save||select_->stage()==SS::Call;return refresh_.stage()==RS::Pack;}
 unsigned nextClocks(int previous=-1)const{if(unsupported_)return 0;if(pack_){switch(pack_->stage()){case PS::Read:return 15;case PS::BindDestination:return 9;case PS::Return:return 7;case PS::Complete:return 0;default:return 3;}}if(select_){switch(select_->stage()){case SS::Save:case SS::Restore:return 6;case SS::ReadTarget:return 21;case SS::BindPatch:return 9;case SS::Call:return 5;case SS::Return:return 7;case SS::AwaitChild:case SS::Complete:return 0;default:return 3;}}switch(refresh_.stage()){case RS::Pack:return 5;case RS::Write:return 3+bus(destination_,previous);case RS::Return:return 7;case RS::AwaitChild:case RS::Complete:return 0;default:return 3;}}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&read,Write&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;if(pack_){pack_->advance([&](uint32_t a){return uint8_t(read(a,1,cost));});if(pack_->complete()){value_=pack_->value();destination_=pack_->spec().destination;pack_.reset();select_->resume();}return cost;}if(select_){select_->advance([&](uint32_t a){return read(a,4,cost);});if(select_->awaitingChild()){unsigned i=0;for(;i<PatchPackSpecs.size()&&PatchPackSpecs[i].entry!=select_->target();++i){}if(i==PatchPackSpecs.size()){unsupported_=true;return cost;}pack_.emplace(i,0xba5c);}if(select_->complete()){select_.reset();refresh_.resume();}return cost;}refresh_.advance([&](){write(destination_,value_,1,cost);});if(refresh_.awaitingChild())select_.emplace(refresh_.index());return cost;}
private:static unsigned bus(uint32_t a,int p){a&=0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0))return (p&1)||a!=uint32_t(p|1)?(a<0x400000?3:1):0;return a>=0xffffa0?1:3;}bool unsupported_=false;R refresh_{};std::optional<S>select_{};std::optional<P>pack_{};uint8_t value_=0,destination_=0;
};
}
