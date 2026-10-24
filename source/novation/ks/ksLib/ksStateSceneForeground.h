#pragma once
#include "ksStateSceneService.h"
#include "ksStateScreenForeground.h"
#include "ksStateFieldService.h"
#include "ksControlArithmetic.h"
namespace ks {class ControllerCpu;
class StateSceneForeground final {
public:
 using CallerAbi=StateScreenForeground::CallerAbi;using Kind=StateSceneCoordinator::Kind;using Component=StateSceneCoordinator::Component;
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const StateFieldService*service()const{return job_?&job_->service:nullptr;}bool at(Kind k)const{return job_&&job_->kind==k;}bool at(Component)const;void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,11>kinds{};
 // Called once at the external completion edge. The active semantic graph
 // never retains this ABI view. Child projectors consume completed outcomes.
 template<class A,class Voice,class Ui>static void project(A&a,const StateSceneCoordinator::Summary&s,Voice&&voice,Ui&&ui){if(!s.count)throw std::logic_error("Missing scene outcome");projectNode(a,s,0,voice,ui);}
private:
 struct Job{Job(Kind,StateSceneCoordinator::Input,uint32_t,int,uint8_t);StateFieldService service;Kind kind;uint32_t caller=0;int depth=0;uint8_t priority=0;};std::optional<Job>job_;
 static uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}
 static void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}static void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}static void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}
 template<class A>static void move8(A&a,uint8_t v){a.flags=uint8_t((a.flags&0xf1)|nz8(v));}
 template<class A,class Voice,class Ui>static void projectNode(A&a,const StateSceneCoordinator::Summary&s,unsigned n,Voice&voice,Ui&ui){using C=StateSceneCoordinator;using K=C::Kind;if(n>=s.count)throw std::logic_error("Invalid scene outcome tree");const auto&o=s.calls[n];const auto&f=o.facts;auto&v=a.values;
  auto child=[&](K k){for(unsigned i=0;i<o.childCount;++i)if(s.calls[o.children[i]].kind==k){projectNode(a,s,o.children[i],voice,ui);return true;}return false;};
  if(o.kind==K::Bank){v[0]=o.address.page();v[3]=o.address.offset();a.flags=uint8_t((a.flags&0xd0)|4);return;}
  if(o.kind==K::Map){const uint16_t product=uint16_t(unsigned(f.selection)*5);word(v[0],uint16_t((product&0xff00)|o.address.mapped()));a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::add(int8_t(product),int8_t(-64),0).flags&0x21)|nz8(o.address.mapped()));return;}
  if(o.kind==K::Load){child(K::Bank);v[0]=o.address.page()+256;v[1]=0x1748e;v[2]=(v[2]&0xff00)|f.lastByte;v[3]=o.address.record();a.flags=uint8_t((a.flags&0xd0)|4);return;}
  if(o.kind==K::Backup||o.kind==K::Copy){v[0]=f.table+64;v[1]=0x1738e;v[2]=(v[2]&0xff00)|f.lastByte;a.flags=uint8_t((a.flags&0xf1)|4);return;}
  if(o.kind==K::Activate){low(v[0],f.bank);move8(a,f.bank);if(f.bank){child(K::Load);low(v[5],1);move8(a,1);child(K::Apply);}return;}
  if(o.kind==K::Apply){v[0]=f.table+48;v[1]=f.patch;v[2]=f.lastEnabled?f.patch+f.lastParameter:f.lastParameter;word(v[3],uint16_t(unsigned(f.lastEnabled?uint8_t(f.lastEnabled-1):0)<<8));v[4]=0x174a5;if(f.parameterUsed)high(v[5],f.lastOld);const auto hc=f.lastEnabled?controlArithmetic::add(int32_t(f.patch),int32_t(f.lastParameter),0).flags&0x21:0;a.flags=uint8_t((a.flags&0xd0)|hc|4);child(K::Notify);return;}
  if(o.kind==K::Restore){low(v[0],f.previous);move8(a,f.previous);if(!f.previous)return;high(v[0],f.selected);move8(a,f.selected);child(K::Load);v[0]=((f.table+48)&0xffff0000u)|(unsigned(f.selected)<<8);v[1]=f.patch;v[2]=f.lastEnabled?f.patch+f.lastParameter:f.lastParameter;word(v[3],uint16_t(unsigned(f.lastEnabled?f.lastOld:0)<<8));v[4]=0x174a5;const auto hc=f.lastEnabled?controlArithmetic::add(int32_t(f.patch),int32_t(f.lastParameter),0).flags&0x21:0;a.flags=uint8_t((a.flags&0xd0)|hc|4);return;}
  if(o.kind==K::Notify){low(v[0],f.selected);move8(a,f.selected);if(f.voice)voice(a,o);return;}
  const bool knob=o.kind==K::Knob;const auto bank=f.bank;
  if(knob){low(v[6],f.knobMagnitude);a.flags=controlArithmetic::subtract(int8_t(f.knobMagnitude),int8_t(4),a.flags).flags;if(f.knobMagnitude<4)return;move8(a,uint8_t(o.input.event>>8));low(v[0],f.lock);move8(a,f.lock);if(f.lock)return;word(v[0],uint16_t(unsigned(uint8_t(o.input.event>>8))*34));low(v[1],f.knobMode);move8(a,f.knobMode);if(f.knobMode){move8(a,bank);if(bank)return;move8(a,0);if(f.ui)ui(a,o);return;}}
  low(v[0],f.oldBank);a.flags=controlArithmetic::subtract(int8_t(bank),int8_t(f.oldBank),a.flags).flags;if(bank==f.oldBank)return;move8(a,bank);
  if(!bank){move8(a,0);child(K::Restore);child(K::Notify);if(f.ui)ui(a,o);return;}
  move8(a,f.oldBank);if(!f.oldBank)child(K::Map);low(v[0],f.mode);move8(a,f.mode);if(!f.mode){child(K::Restore);child(K::Activate);}if(f.ui)ui(a,o);
 }
};}
