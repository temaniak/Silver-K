#include "ksInternalMidiQueueTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::InternalMidiQueueTask;
void check(bool b){if(!b)throw std::runtime_error("Internal MIDI queue property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(auto kind:{T::Kind::Reset,T::Kind::Pop,T::Kind::Push})for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer){
  T task(kind,uint8_t(producer^consumer));unsigned reads=0,writes=0;unsigned p=producer,c=consumer,overflow=99;bool wroteByte=false;const unsigned source=(producer*17+consumer*23)&255;
  auto read=[&](T::Read field){++reads;if(field==T::Read::Consumer)return c;if(field==T::Read::Producer)return p;check(kind==T::Kind::Pop&&task.index()==consumer&&c==((consumer+1)&255));return source;};
  auto write=[&](T::Write field,unsigned value){++writes;switch(field){case T::Write::Producer:p=value;break;case T::Write::Consumer:c=value;break;case T::Write::Overflow:overflow=value;break;case T::Write::Byte:check(kind==T::Kind::Push&&task.index()==producer&&p==producer&&value==(producer^consumer));wroteByte=true;break;}};
  while(!task.complete()){const auto cost=task.nextClocks();const auto stage=task.stage(),before=stage;const auto count=reads+writes;check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;}
  if(kind==T::Kind::Reset)check(!reads&&writes==3&&!p&&!c&&!overflow);
  else if(kind==T::Kind::Pop)check(task.available()==(producer!=consumer)&&p==producer&&c==(producer==consumer?consumer:(consumer+1)&255)&&writes==(producer==consumer?0u:1u)&&(!task.available()||task.value()==source));
  else{const bool full=consumer==((producer+1)&255);check(wroteByte&&c==consumer&&task.full()==full&&p==(full?producer:(producer+1)&255)&&overflow==(full?2u:99u)&&writes==2);}
  ++cases;
 }
 std::cout<<"PASS: "<<cases<<" queue policies / "<<intervals<<" intervals; every index pair, full/empty/wrap, ordered accesses and admission deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
