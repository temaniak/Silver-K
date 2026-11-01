#pragma once
#include "kstationPatchUnpackTask.h"
#include <optional>
namespace kstation {
// Unpack both live program and mirror and publish transpose. Wrapper intervals
// are domain-owned and do not retain a guest register or return stack.
class ControllerPatchUnpackTask final {
 using X=PatchUnpackTask;using XS=X::Stage;using Y=PatchTransposeTask;using YS=Y::Stage;
public:bool complete()const{return complete_;}bool stackWrites()const{if(transpose_)return false;if(unpack_){auto s=unpack_->stage();return s==XS::RetainUi||s==XS::DispatchUi;}return phase_==2;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;if(transpose_){Y::Credits c;auto s=transpose_->stage();if(s==YS::PublishPrevious||s==YS::PublishCurrent)c.write=9+bus(s==YS::PublishPrevious?0xcd79:0xcd78,1,previous);return transpose_->nextClocks(c);}if(unpack_){X::Credits c;auto s=unpack_->stage();c.read=12+bus(0xba5c+unpack_->field().packed,1,-1);auto off=s==XS::PublishSigned?0xad:unpack_->field().decoded;c.write=12+bus(destination_+off,1,previous);return unpack_->nextClocks(c);}return phase_==2?5:9;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;if(transpose_){Y::Credits c;auto s=transpose_->stage();if(s==YS::PublishPrevious||s==YS::PublishCurrent)c.write=9+bus(s==YS::PublishPrevious?0xcd79:0xcd78,1,previous);auto used=transpose_->advanceBefore(cost,c,[&](){return read(0xcd78,1,cost);},[&](bool prev,uint8_t v){write(prev?0xcd79:0xcd78,v,1,cost);});if(transpose_->complete()){transpose_.reset();unpack_->resumeUi();}return used;}if(unpack_){X::Credits c;auto s=unpack_->stage();c.read=12+bus(0xba5c+unpack_->field().packed,1,-1);auto off=s==XS::PublishSigned?0xad:unpack_->field().decoded;c.write=12+bus(destination_+off,1,previous);auto used=unpack_->advanceBefore(cost,c,[&](unsigned field){return read(0xba5c+field,1,cost);},[&](unsigned field,uint8_t v){write(destination_+field,v,1,cost);});if(unpack_->awaitingUi())transpose_.emplace(int8_t(unpack_->packed()));if(unpack_->complete()){unpack_.reset();if(destination_==0xba5c)phase_=3;else complete_=true;}return used;}if(phase_==0)phase_=1;else if(phase_==1)phase_=2;else if(phase_==2){destination_=0xba5c;unpack_.emplace();}else{destination_=0xcf36;unpack_.emplace();}return cost;}
private:static unsigned bus(uint32_t a,unsigned n,int previous){unsigned cost=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((previous&1)||x!=uint32_t(previous|1))cost+=x<0x400000?3:1;previous=int(x);}else{cost+=x>=0xffffa0?1:3;previous=-1;}}return cost;}unsigned phase_=0;uint32_t destination_=0;bool complete_=false;std::optional<X>unpack_{};std::optional<Y>transpose_{};
};
}
