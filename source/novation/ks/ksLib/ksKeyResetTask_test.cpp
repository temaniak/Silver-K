#include "ksKeyResetTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::KeyResetTask;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{unsigned cases=0;for(auto kind:{T::Kind::Held,T::Kind::Down})for(unsigned part=0;part<256;++part)for(unsigned latency:{0u,3u}){
 T task(kind);std::array<unsigned,334> memory;memory.fill(0x91);std::vector<unsigned> writes;unsigned clocks=0,stages=0;
 while(!task.complete()){
  if(task.awaitingPart()){check(task.resumePart(uint8_t(part)),"Part reply rejected");continue;}
  auto writer=[&](unsigned offset,unsigned data){check(offset<334,"Write beyond part");memory[offset]=data;writes.push_back(offset);};auto timing=[&](unsigned){return latency;};
  const unsigned cost=task.nextClocks(task.publication()?latency:3u);check(cost>0,"Task stalled");
  check(!task.advanceBefore(cost-1,writer,timing),"Early admission");const auto before=writes.size();
  check(task.advanceBefore(cost,writer,timing)==cost,"Cost mismatch");check(writes.size()<=before+1,"Multiple publications per boundary");clocks+=cost;++stages;
 }
 check(task.part()==part&&task.remaining()==0,"Logical reset context differs");const bool held=kind==T::Kind::Held;check(writes.size()==unsigned(held?19:16),"Publication count differs");
 for(unsigned i=0;i<16;++i)check(writes[i]==unsigned((held?0xdf:0xcf)-i),"Descending clear order differs");
 if(held)check(writes[16]==307&&writes[17]==306&&writes[18]==308,"Counter order differs");
 for(unsigned at=0;at<334;++at){const bool cleared=(at>=(held?0xd0u:0xc0u)&&at<(held?0xe0u:0xd0u))||(held&&(at==306||at==307||at==308));check(memory[at]==unsigned(cleared?0:0x91),"Unrelated state changed");}
 check(clocks==unsigned((held?409:364)-(3-latency)*(held?19:16)),"Whole own schedule differs");
 check(stages==unsigned(held?61:58),"Boundary count differs");check(!task.resumePart(0),"Stray child accepted");++cases;
 }
 std::cout<<"PASS: "<<cases<<" reset modes/part bindings/store delays; exact ordered fields, deadline admission and clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
