#include "ksParameterUploadForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}}
void ParameterUploadForeground::project(CallerAbi&a,const ParameterUploadService::Summary&s){
 if(!s.glide)throw std::logic_error("Incomplete parameter upload result");const auto&g=*s.glide;auto&v=a.values;
 auto order=s.coefficient.order;uint32_t upper=s.coefficient.observed?s.coefficient.value>>16:v[0]>>16;
 if(s.productDiscardOrder>order){upper=0;order=s.productDiscardOrder;}if(s.retry.observed&&s.retry.order>order)upper=s.retry.result.remaining;
 v[0]=(upper<<16)|0xffffu;
 const bool weighted=s.distanceObserved&&g.distance();const uint8_t tail=g.time()&&!weighted?uint8_t(g.coefficient()):0;
 v[1]=(uint32_t(g.previous())<<24)|tail;
 uint16_t lane=s.lfo[1].delaySelector?uint16_t(0x159f4u+s.lfo[1].index):uint16_t((unsigned(uint8_t((s.lfo[1].delay^127u)-115))<<8)|((s.lfo[1].delay^127u)>115?0:115));
 lane=uint16_t((unsigned(g.mode())<<8)|uint8_t(s.allocationObserved?g.allocation():lane));
 if(g.time())lane=weighted?uint16_t(unsigned(g.distance())<<8):uint16_t((unsigned(g.adjustment())<<8)|(g.inverted()>118?0:118));
 v[2]=(s.presetObserved?0x10000u:v[2]&0xffff0000u)|lane;
 if(s.voiceObserved)v[3]=0x17560u+20u*s.voice;
 v[5]=0x600000;v[6]=s.input.patch;a.flags=uint8_t((a.flags&0xd0)|8);
}
bool ParameterUploadForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17530||!h.interruptMode2||h.indent<=0||h.getPC()!=0x7de4)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;if(h.regs[5].er!=0x400000&&h.regs[5].er!=0x600000)return false;
  ParameterUploadService::Input in{uint32_t(h.regs[6].er),h.regs[5].er==0x400000?DspPort::A:DspPort::B,uint16_t(h.regs[4].r)};if(!ParameterUploadService::dataSpan(in.patch,256))return false;job_.emplace(in,sp,h.indent,h.exr);++started;
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned dt){h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned dt){h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("Parameter upload phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;++phases;components|=s.components();groups|=s.summary().groups;
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s.summary());for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
