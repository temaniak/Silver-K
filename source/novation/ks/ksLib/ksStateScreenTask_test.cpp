#include "ksStateScreenTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
using T=ks::StateScreenTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("State screen property differs");}
uint64_t cases=0,intervals=0;
template<class Read,class Write,class Child>void run(T& t,Read&& read,Write&& write,Child&& child){unsigned steps=0,access=0;
 auto reader=[&](T::Read f,uint32_t a){++access;return read(f,a);};auto writer=[&](T::Write f,uint32_t a,uint32_t v){++access;write(f,a,v);};
 while(!t.complete()){const auto stage=t.stage();const auto cost=t.nextClocks();const auto before=access;check(cost&&++steps<200);check(!t.advanceBefore(cost-1,reader,writer)&&t.stage()==stage&&access==before);check(t.advanceBefore(cost,reader,writer)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(child(stage)));}++cases;
}
int main(){try{
 auto nochild=[](S){return T::Reply{0,0,0,false};};
 for(unsigned index=0;index<256;++index)for(unsigned mode:{0u,1u,255u})for(unsigned hardware:{0u,1u})for(unsigned sentinel:{0u,255u})for(unsigned variant:{0u,6u,255u})for(unsigned current:{0u,2u,3u,127u,128u,255u}){
  uint8_t count=sentinel?3:4,page=current;uint32_t chosen=0;std::vector<std::tuple<T::Write,uint32_t,uint32_t>> writes;
  auto read=[&](T::Read f,uint32_t a)->uint32_t{switch(f){case T::Read::Mode:return mode;case T::Read::Hardware:return hardware;case T::Read::PrimaryPointer:chosen=a==48?0x10517:0x30000 + a*32;return chosen;case T::Read::SecondaryPointer:chosen=0x90000 + a*32;return chosen;case T::Read::DescriptorByte:if(a==0x10569)chosen=a;return a==chosen?(chosen>=0x90000?count:sentinel):page;case T::Read::PatchPointer:return 0x40000;case T::Read::Variant:return variant;default:return 0;}};
  auto write=[&](T::Write f,uint32_t a,uint32_t v){writes.emplace_back(f,a,v);if(a==chosen)count=v;else if(a==chosen+1)page=v;else check(false);};
  T t(T::Kind::Descriptor,uint8_t(index));run(t,read,write,nochild);
  uint8_t adjusted=index;if(index==12&&!mode)++adjusted;const auto offset=uint8_t(adjusted*4);uint32_t pointer=offset==48?0x10517:0x30000 + offset*32;if(hardware&&pointer==0x10517)pointer=0x10569;
  unsigned expectedCount=sentinel,expectedPage=current;if(sentinel==255){pointer=0x90000 + uint8_t(variant*4)*32;expectedCount=variant==6?3+!!mode:3;if(variant==6&&expectedPage>=expectedCount)expectedPage=expectedCount-1;}
  check(t.pointer()==pointer+1&&t.count()==expectedCount&&t.page()==expectedPage);
 }
 for(unsigned count=0;count<256;++count)for(unsigned page=0;page<256;++page)for(auto kind:{T::Kind::Set,T::Kind::Next,T::Kind::Previous}){
  const unsigned reset=(count+page)&1;unsigned published=999,ui=999;auto read=[&](T::Read f,uint32_t){return f==T::Read::ResetFlag?reset:7u;};
  auto write=[&](T::Write f,uint32_t a,uint32_t v){if(f==T::Write::DescriptorByte){check(a==0x30001);published=v;}if(f==T::Write::UiPage)ui=v;};
  auto child=[&](S stage){return T::Reply{0x30001,uint8_t(count),uint8_t(page),false};};T t(kind,7);run(t,read,write,child);
  unsigned expected=page;if(kind==T::Kind::Set&&reset)expected=0;if(kind==T::Kind::Next){expected=uint8_t(page+1);if(expected>=count)expected=uint8_t(count-1);}if(kind==T::Kind::Previous)expected=page==0||page>=128?0:page-1;
  check(published==expected&&ui==expected);
 }
 for(unsigned hardware:{0u,1u})for(unsigned state=0;state<65536;++state)for(bool allowed:{false,true}){
  auto read=[&](T::Read f,uint32_t){return f==T::Read::Hardware?hardware:f==T::Read::Screen?state>>8:state&255;};auto write=[](T::Write,uint32_t,uint32_t){check(false);};auto child=[&](S){return T::Reply{0,0,0,allowed};};T t(T::Kind::Gate);run(t,read,write,child);check(t.accepted()==(!hardware&&state==0x902&&allowed));
 }
 for(unsigned page=0;page<256;++page){uint32_t record=0;auto read=[](T::Read f,uint32_t a){return f==T::Read::Screen?7u:123u;};auto write=[&](T::Write f,uint32_t,uint32_t v){if(f==T::Write::RecordPointer)record=v;};auto child=[&](S){return T::Reply{0x30001,4,uint8_t(page),false};};T t(T::Kind::Render);run(t,read,write,child);check(record==0x30002 + 20*page&&t.parameter()==123);}
 for(unsigned character=0;character<256;++character){unsigned chars=0,prints=0;auto read=[&](T::Read f,uint32_t a){if(f==T::Read::RecordPointer)return 0x30000u;check(a>=0x30000&&a<=0x30013);if(a<0x30010){++chars;return character;}return a==0x30010?42u:19u;};auto write=[](T::Write,uint32_t,uint32_t){check(false);};auto child=[&](S s){if(s==S::DisplayCharacter)++prints;return T::Reply{0,0,0,false};};T t(T::Kind::Label);run(t,read,write,child);check(chars==16&&prints==(character==255?0u:16u)&&t.parameter()==42&&t.format()==19);}
 std::cout<<"PASS: "<<cases<<" screen policies / "<<intervals<<" intervals; all page/count/status bytes, descriptor remapping and variant clamp, previous signed-underflow quirk, gate, label omission and live publications/deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
