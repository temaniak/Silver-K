#include "ksKeyboardScanTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::KeyboardScanTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Keyboard scan property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned current=0;current<65536;++current)for(unsigned previous:{0u,65535u,21845u,43690u}){T t(T::Kind::Transitions,0x30000,0x31000,uint16_t(current),uint16_t(previous));unsigned dispatch=0,writes=0,reads=0;auto read=[&](T::Read,unsigned)->uint16_t{++reads;return 0;};auto write=[&](T::Write f,unsigned,uint16_t v){++writes;check(f==T::Write::Page&&v==current);};
  while(!t.complete()){auto stage=t.stage();auto cost=t.nextClocks();const auto n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&stage==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){const unsigned i=dispatch++,expected=(((current>>(2*i))&1)<<3)|(((current>>(2*i+1))&1)<<2)|(((previous>>(2*i))&1)<<1)|((previous>>(2*i+1))&1);check(t.transition()==expected);check(t.resumeChild());}}
  check(dispatch==8&&writes==1&&reads==0&&t.record()==0x31020);++cases;}
 for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){T t(T::Kind::Scan);unsigned writes=0,mux=0,children=0;auto read=[&](T::Read f,unsigned)->uint16_t{if(f==T::Read::Tick)return 65535;if(f==T::Read::Page)return 0;if(f==T::Read::Upper||f==T::Read::Lower)return a;return b;};auto write=[&](T::Write f,unsigned,uint16_t v){++writes;if(f==T::Write::Tick)check(v==0);else{check(f==T::Write::Mux&&v==(1u<<((++mux)&7)));}};while(!t.complete()){const auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){++children;check(t.current()==(((a*257)&0xfce0)|((((b>>2)<<8)|b)&0x31f)));check(t.resumeChild());}}check(writes==9&&mux==8);++cases;}
 std::cout<<"PASS: "<<cases<<" keyboard scan policies / "<<intervals<<" intervals; contact bit order, eight pages/mux sequence, tick wrap, live reads and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
