#include "ksPanelDecayTask.h"
#include <array>
#include <stdexcept>
#include <iostream>
namespace{void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}}
int main(){try{using T=ks::PanelDecayTask;using R=T::Read;using W=T::Write;unsigned cases=0;
auto verify=[&](unsigned seed,unsigned pendingValue,unsigned hold,unsigned budget){
 std::array<std::array<uint8_t,4>,2> cells{};std::array<uint8_t,2> pending{};unsigned expected=306,expectedWrites=2+(hold?1:0),reads=0,writes=0,latches=0;std::array<uint8_t,2> changed{};
 for(unsigned bank=0;bank<2;++bank){pending[bank]=uint8_t(pendingValue+73*bank);for(unsigned index=0;index<4;++index){auto v=cells[bank][index]=uint8_t(seed+17*index+7*bank);if(v){++expectedWrites;expected+=12+(v==1?3:0);}if(v==1)changed[bank]=1;}}
 if(hold)expected+=18;if(hold==1)expected+=45;
 const auto originalCells=cells;const auto originalPending=pending;T task;unsigned used=0;
 auto read=[&](R field,unsigned bank,unsigned index){++reads;return field==R::Cell?originalCells[bank][index]:field==R::Pending?originalPending[bank]:uint8_t(hold);};
 auto write=[&](W field,unsigned bank,unsigned index,uint8_t value){++writes;if(field==W::Cell){check(originalCells[bank][index]&&value==(originalCells[bank][index]>>1),"Decay value/order differs");cells[bank][index]=value;}else if(field==W::Pending){check(value==uint8_t(originalPending[bank]|changed[bank]),"Pending expiry merge differs");pending[bank]=value;}else check(hold&&value==uint8_t(hold-1),"Holdoff publication differs");};
 while(!task.complete()){if(task.awaiting()){check(hold==1&&T::latchRequest()==12,"Unexpected latch request");++latches;check(task.resumeChild(),"Latch did not resume");continue;}auto before=task.stage();auto cost=task.nextClocks();auto observed=reads,published=writes;check(cost>0,"Decay stalled");check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before&&reads==observed&&writes==published,"Under-admitted decay caused side effects");auto spent=task.advanceBefore(budget,read,write);if(!spent)spent=task.advanceBefore(cost,read,write);used+=spent;}
 check(used==expected&&reads==11&&writes==expectedWrites&&latches==(hold==1),"Independent decay schedule/count differs");for(unsigned bank=0;bank<2;++bank)for(unsigned index=0;index<4;++index)check(cells[bank][index]==(originalCells[bank][index]>>1),"Decayed final cell differs");check(!task.advanceBefore(1000,read,write),"Completed decay advanced");++cases;
};
for(unsigned seed=0;seed<256;++seed)for(unsigned value=0;value<256;++value)for(unsigned budget:{0u,7u})verify(seed,value,value,budget);
for(unsigned seed=0;seed<256;++seed)for(unsigned hold=0;hold<256;++hold)for(unsigned budget:{3u,63u})verify(seed,seed,hold,budget);
for(unsigned seed:{0u,1u,2u,3u,127u,128u,255u})for(unsigned value=0;value<256;++value)for(unsigned hold:{0u,1u,2u,255u})for(unsigned budget:{0u,1u,2u,3u,7u,15u,31u,63u})verify(seed,value,hold,budget);
std::cout<<"PASS: "<<cases<<" native panel decay schedules, all cells/pending/holdoff bytes, independent expiry/latch decisions/clocks, deadline refusal\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
