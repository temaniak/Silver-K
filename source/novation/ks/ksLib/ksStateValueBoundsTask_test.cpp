#include "ksStateValueBoundsTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::StateValueBoundsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StateValueBounds policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
for(unsigned type=0;type<256;++type)for(unsigned value=0;value<256;++value)for(bool absolute:{false,true}){T t(T::Kind::Resolve,type,value,absolute,0x30000);unsigned reads=0;auto r=[&](T::Read,uint32_t){++reads;return 3u;};auto w=[](int,int){};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,r,w)&&s==t.stage()&&n==reads);check(t.advanceBefore(cost,r,w)==cost);++intervals;}unsigned expected=value,limits=0x7f00;if(type==1){limits=0x4c34;if(!absolute){uint16_t scaled=uint16_t(int(int8_t(uint8_t(value-61)))*49);expected=uint8_t((scaled>>8)+64);}}if(type==2){limits=0x720e;if(!absolute){uint16_t scaled=uint16_t(int(int8_t(uint8_t(value-63)))*202);expected=uint8_t((scaled>>8)+64);}}if(type==10||type==21||type==22){limits=type==10?0x1300:type==21?0x1700:0x1f00;if(!absolute)expected=(value*(type==10?40:type==21?48:64))>>8;}check(t.value()==expected&&t.bounds()==limits&&t.pointer()==0x30000+(type==3?3:type==19?312:0));++cases;}

std::cout<<"PASS: "<<cases<<" StateValueBounds policies / "<<intervals<<" intervals; logical routes/math/deferred effects and insufficient budgets\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
