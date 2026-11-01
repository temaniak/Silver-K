#pragma once
#include "ksStateSceneEditService.h"
#include "ksStateScreenForeground.h"
#include "ksStateFieldService.h"
#include "ksControlArithmetic.h"
namespace ks {class ControllerCpu;
class StateSceneEditForeground final {
public:
 using CallerAbi=StateScreenForeground::CallerAbi;using Kind=StateSceneEditCoordinator::Kind;using Component=StateSceneEditCoordinator::Component;
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const StateFieldService*service()const{return job_?&job_->service:nullptr;}bool at(Kind k)const{return job_&&job_->kind==k;}bool at(Component)const;void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,20>kinds{};
 // One combinational projection after completion; semantic child observations
 // are supplied by the owning graph, not stored as an active register bank.
 template<class A,class Scene,class Ui,class Character>static void project(A&a,const StateSceneEditCoordinator::Summary&s,Scene&&scene,Ui&&ui,Character&&character){if(!s.count)throw std::logic_error("Missing scene edit outcome");projectNode(a,s,0,scene,ui,character);}
private:
 struct Job{Job(Kind,StateSceneEditCoordinator::Input,uint32_t,int,uint8_t);StateFieldService service;Kind kind;uint32_t caller=0;int depth=0;uint8_t priority=0;};std::optional<Job>job_;
 static uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}static void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}static void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}static void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}template<class A>static void move8(A&a,uint8_t v){a.flags=uint8_t((a.flags&0xf1)|nz8(v));}
 template<class A,class Scene,class Ui,class Character>static void projectNode(A&a,const StateSceneEditCoordinator::Summary&s,unsigned n,Scene&scene,Ui&ui,Character&character){using C=StateSceneEditCoordinator;using K=C::Kind;using Q=C::Request;if(n>=s.count)throw std::logic_error("Invalid scene edit tree");const auto&o=s.calls[n];const auto&f=o.facts;auto&v=a.values;
 auto child=[&](K k){for(unsigned i=0;i<o.childCount;++i)if(s.calls[o.children[i]].kind==k){projectNode(a,s,o.children[i],scene,ui,character);return true;}return false;};auto find=[&](K k)->const C::Outcome*{for(unsigned i=0;i<o.childCount;++i)if(s.calls[o.children[i]].kind==k)return &s.calls[o.children[i]];return nullptr;};
 if(o.kind==K::Record){const auto product=unsigned(uint8_t(f.originalRow-1))*8;v[0]=o.address.record();v[1]=f.base;word(v[2],uint16_t(product));a.flags=controlArithmetic::add(int32_t(f.base),int32_t(product+16),a.flags).flags;return;}
 if(o.kind==K::Field){child(K::Record);v[0]=o.address.record();v[1]=uint8_t(f.originalField*2);v[2]=o.address.limit();a.flags&=0xd0;return;}
 if(o.kind==K::Clamp){const auto sum=uint8_t(f.originalBank+uint8_t(o.input.event));high(v[0],o.address.bank());if(!sum||sum>=128)a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::add(int8_t(f.originalBank),int8_t(o.input.event),0).flags&0x21));else{const auto compared=controlArithmetic::subtract(int8_t(sum),int8_t(16),a.flags).flags;a.flags=sum<=16?compared:uint8_t((compared&0xf1)|nz8(16));}return;}
 if(o.kind==K::ReadName){v[0]=f.target;low(v[1],f.character);a.flags=uint8_t((a.flags&0xd0)|nz8(f.character));return;}
 if(o.kind==K::DecodeName){const auto raw=f.decodeInput;low(v[1],f.index);uint8_t hc=0;if(raw>=65&&raw<=90){high(v[1],1);hc=controlArithmetic::add(int8_t(raw),int8_t(-64),0).flags&0x21;}else if(raw>=97&&raw<=122){high(v[1],0);hc=controlArithmetic::add(int8_t(raw),int8_t(-96),0).flags&0x21;}else if(raw!=32)hc=controlArithmetic::add(int8_t(raw),int8_t(-6),0).flags&0x21;a.flags=uint8_t((a.flags&0xd0)|hc|nz8(f.index));return;}
 if(o.kind==K::EncodeName){const auto index=f.index;low(v[1],f.character);if(!index)move8(a,32);else if(index>=27)a.flags=controlArithmetic::add(int8_t(index),int8_t(6),a.flags).flags;else{high(v[1],f.caseValue);const auto upper=controlArithmetic::add(int8_t(index),int8_t(64),a.flags).flags;if(f.caseValue)a.flags=uint8_t((upper&0xf1)|nz8(f.caseValue));else a.flags=controlArithmetic::add(int8_t(uint8_t(index+64)),int8_t(32),a.flags).flags;}return;}
 if(o.kind==K::Category){const auto sum=uint8_t(f.originalCategory+uint8_t(o.input.event));const auto normalized=sum&128?uint8_t(0):sum;low(v[0],f.category);a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::subtract(int8_t(normalized),int8_t(24),0).flags&0x21)|nz8(f.category));if(f.ui)ui(a,o);return;}
 if(o.kind==K::Begin){low(v[0],0);move8(a,0);child(K::ReadName);child(K::DecodeName);return;}
 if(o.kind==K::Toggle){child(K::ReadName);child(K::DecodeName);low(v[2],uint8_t(f.caseValue^1));move8(a,uint8_t(f.caseValue^1));child(K::CommitName);return;}
 if(o.kind==K::ClearSuffix){const auto count=uint8_t(16-f.cursor);v[0]=0x1733c + f.cursor+(count?unsigned(count):65536u);word(v[1],0);low(v[2],32);a.flags=uint8_t((a.flags&0xd0)|4);if(f.ui)ui(a,o);return;}
 if(o.kind==K::MoveName){const auto cursor=uint8_t(f.cursor+uint8_t(o.input.event))&15;low(v[0],cursor);low(v[6],uint8_t(0xc1 + cursor));a.flags=controlArithmetic::add(int8_t(0xc1),int8_t(cursor),a.flags).flags;if(f.display)character(a,o);const auto*r=find(K::ReadName);if(r){child(K::ReadName);a.flags=controlArithmetic::subtract(int8_t(r->facts.character),int8_t(32),a.flags).flags;child(K::DecodeName);}return;}
 if(o.kind==K::InsertSpace||o.kind==K::InsertDigit||o.kind==K::InsertRaw){v[0]=0x1733c + f.cursor;low(v[6],1);a.flags=uint8_t(a.flags&0xd0);child(K::MoveName);if(f.ui)ui(a,o);return;}
 if(o.kind==K::AdjustName||o.kind==K::CommitName){if(o.kind==K::AdjustName){const auto*r=find(K::ReadName);if(!r)throw std::logic_error("Missing edit name read");child(K::ReadName);a.flags=controlArithmetic::subtract(int8_t(r->facts.character),int8_t(32),a.flags).flags;if(r->facts.character==32){low(v[1],f.previousIndex);move8(a,f.previousIndex);}else child(K::DecodeName);const auto adjusted=controlArithmetic::add(int8_t(v[1]),int8_t(o.input.event),a.flags);low(v[1],uint8_t(adjusted.value));a.flags=adjusted.flags;if(uint8_t(v[1])>=128){low(v[1],0);move8(a,0);}const auto compared=controlArithmetic::subtract(int8_t(v[1]),int8_t(58),a.flags).flags;low(v[1],f.index);a.flags=uint8_t((compared&0xf1)|nz8(f.index));}child(K::EncodeName);move8(a,f.character);if(f.ui)ui(a,o);return;}
 low(v[0],f.mode);move8(a,f.mode);
 if(o.kind!=K::Adjust&&f.mode){a.flags=controlArithmetic::subtract(int8_t(f.mode),int8_t(2),a.flags).flags;if(f.mode==2)child(o.kind==K::Increment?K::ClearSuffix:o.kind==K::Decrement?K::InsertSpace:K::MoveName);return;}
 if(o.kind==K::Increment){const auto next=uint8_t(f.originalRow+1);const auto out=next<5?next:uint8_t(4);low(v[0],out);a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::subtract(int8_t(next),int8_t(5),0).flags&0x21)|nz8(out));if(f.ui)ui(a,o);return;}
 if(o.kind==K::Decrement){const auto out=f.originalRow>0&&f.originalRow<128?uint8_t(f.originalRow-1):uint8_t(0);low(v[0],out);move8(a,out);if(f.ui)ui(a,o);return;}
 if(o.kind==K::Sub){const auto sum=uint8_t(f.originalField+uint8_t(o.input.event));const auto raw=sum>=128?uint8_t(3):sum;const auto out=raw<=3?raw:uint8_t(0);low(v[0],out);a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::subtract(int8_t(raw),int8_t(3),0).flags&0x21)|nz8(out));if(f.scene)scene(a,o,Q::SceneMap);if(f.ui)ui(a,o);return;}
 if(f.mode){a.flags=controlArithmetic::subtract(int8_t(f.mode),int8_t(2),a.flags).flags;if(f.mode==2){child(K::AdjustName);return;}child(K::Clamp);v[0]=f.preview+16;v[1]=0x1734c;v[2]=(v[2]&0xff00)|f.lastByte;a.flags=uint8_t((a.flags&0xd0)|4);if(f.ui)ui(a,o);return;}
 low(v[0],f.originalRow);move8(a,f.originalRow);if(!f.originalRow){child(K::Clamp);if(f.scene)scene(a,o,Q::SceneSelectBank);return;}
 child(K::Field);low(v[1],f.value);word(v[1],f.value);word(v[6],uint16_t(int16_t(int8_t(o.input.event))));const auto total=int16_t(f.value)+int8_t(o.input.event);const auto add=controlArithmetic::add(int16_t(f.value),int16_t(int8_t(o.input.event)),a.flags);word(v[1],uint16_t(add.value));a.flags=add.flags;
 if(total<0){low(v[1],0);move8(a,0);}else{word(v[3],f.maximum);const auto compared=controlArithmetic::subtract(int16_t(total),int16_t(f.maximum),a.flags).flags;a.flags=compared;if(total>f.maximum){low(v[1],f.maximum);move8(a,f.maximum);}}move8(a,uint8_t(v[1]));low(v[5],0);move8(a,0);if(f.scene)scene(a,o,Q::SceneApply);if(f.ui)ui(a,o);
 }
};}
