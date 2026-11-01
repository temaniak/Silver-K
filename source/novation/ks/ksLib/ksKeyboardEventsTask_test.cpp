#include "ksKeyboardEventsTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::KeyboardEventsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Keyboard events property differs");}
uint64_t cases=0,intervals=0;
unsigned reference(unsigned input,unsigned flags){unsigned delay=uint16_t(input-((flags&1)?2:3));if(delay&32768)delay=0;unsigned period=flags&1?30:120;uint32_t divisor=delay*delay+period,top=period<<15;while(divisor>65535){divisor>>=1;top>>=1;}const unsigned q=uint16_t(top/divisor);int bumped=int(int8_t(q>>8))+1;return bumped<0?127:uint8_t(bumped);}
template<class Reader,class Writer>void run(T& t,Reader&& read,Writer&& write){unsigned access=0,steps=0;auto r=[&](T::Read f){++access;return read(f);};auto w=[&](T::Write f,unsigned v){++access;write(f,v);};while(!t.complete()){const auto before=t.stage();const auto cost=t.nextClocks(),n=access;check(cost&&++steps<200);check(!t.advanceBefore(cost-1,r,w)&&t.stage()==before&&n==access);check(t.advanceBefore(cost,r,w)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(reference(t.delay(),read(T::Read::Flags))));}++cases;}
int main(){try{
 for(unsigned input=0;input<65536;++input)for(unsigned flags:{0u,1u,2u,3u,255u}){T t(T::Kind::Velocity,0x30000,uint16_t(input));auto read=[&](T::Read){return flags;};auto write=[](T::Write,unsigned){check(false);};run(t,read,write);check(t.velocity()==reference(input,flags));}
 for(unsigned producer=0;producer<512;++producer)for(unsigned consumer=0;consumer<512;++consumer)for(auto kind:{T::Kind::Pop,T::Kind::Append,T::Kind::Reset}){
  T t(kind,0x30000,0x7f42);unsigned p=producer,c=consumer,event=0;auto read=[&](T::Read f){return f==T::Read::Producer?p:f==T::Read::Consumer?c:0xa55au;};auto write=[&](T::Write f,unsigned v){if(f==T::Write::Producer)p=v;else if(f==T::Write::Consumer)c=v;else if(f==T::Write::Event)event=v;else check(false);};run(t,read,write);
  if(kind==T::Kind::Pop)check(t.message()==(producer==consumer?0u:0xa55au)&&c==(producer==consumer?consumer:((consumer+2)&510)));if(kind==T::Kind::Append)check(p==((producer+2)&510)&&event==0x7f42);if(kind==T::Kind::Reset)check(!p&&!c);
 }
 std::cout<<"PASS: "<<cases<<" keyboard event policies / "<<intervals<<" intervals; delay words/velocity modes, all ring pairs, wrap and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
