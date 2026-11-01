#include "ksVoiceTableInitTask.h"
#include <array>
#include <stdexcept>
#include <iostream>
namespace{void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}}
int main(){try{using T=ks::VoiceTableInitTask;using F=T::Field;unsigned cases=0;
for(unsigned seed=0;seed<256;++seed)for(unsigned delay:{0u,1u,3u})for(unsigned budget:{0u,1u,2u,3u,7u,15u,31u,63u}){
 T task;unsigned used=0,published=0,reads=0;auto raw=[&](unsigned index){return uint8_t(seed+index*17);};
 auto read=[&](unsigned index){check(index==reads++,"Mapping read order differs");return raw(index);};auto timing=[&](F,unsigned){return delay;};
 auto write=[&](F f,unsigned index,unsigned data){check(index==published/8,"Voice publication order differs");const F order[]={F::Note,F::Transposed,F::Distance,F::Counter,F::Released,F::Mapping,F::Lower,F::Upper};check(f==order[published%8],"Voice field order differs");const auto mapping=raw(index);unsigned wanted=0;switch(f){case F::Note:case F::Transposed:wanted=60;break;case F::Distance:wanted=64;break;case F::Mapping:wanted=mapping;break;case F::Lower:wanted=uint8_t((mapping>=128?15:mapping<=7?39:55)-mapping);break;case F::Upper:wanted=uint8_t((mapping<128?239:mapping>=248?215:199)-mapping);break;default:break;}check(data==wanted,"Voice default/mapping/bounds differs");++published;};
 while(!task.complete()){auto before=task.stage();const auto count=published,observed=reads;auto cost=task.nextClocks(task.publication()?delay:3);check(cost>0,"Voice initialization stalled");check(!task.advanceBefore(cost-1,read,write,timing)&&task.stage()==before&&published==count&&reads==observed,"Under-admitted initialization had side effects");auto spent=task.advanceBefore(budget,read,write,timing);if(!spent)spent=task.advanceBefore(cost,read,write,timing);used+=spent;}
 unsigned expected=41;for(unsigned index=0;index<14;++index){const auto v=raw(index);expected+=161+(v>=128?6:9+(v>7?3:0))+(v<128?6:9+(v<248?3:0))+7*delay;}
 check(used==expected,"Independent initialization total clocks differ");check(reads==14&&published==112,"Voice initialization read/write counts differ");check(!task.advanceBefore(1000,read,write,timing),"Completed initialization advanced");++cases;
}
std::cout<<"PASS: "<<cases<<" native14-voice default/bounds schedules, every mapping byte/slot, signed split/wrap, ordered112publications/14reads, independent clocks and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
