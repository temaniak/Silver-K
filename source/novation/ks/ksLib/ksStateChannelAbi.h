#pragma once
#include "ksStateChannelService.h"
namespace ks {
template<class A>void projectStateChannel(A&a,const StateChannelService&s){
 if(!s.complete())throw std::logic_error("Incomplete channel projection");const auto&t=s.channelTask();uint8_t value;
 if(s.kind()==StateChannelService::Kind::ModeStatus){value=(!t.mode()||t.multiStatus())?0x91:0;a.values[0]=(a.values[0]&0xffffff00u)|value;}
 else {a.values[0]=0x174ae;a.values[1]=(a.values[1]&0xffffff00u)|t.channel();value=t.channel();}
 a.flags=uint8_t((a.flags&0xf1)|(value&128?8:0)|(!value?4:0));
}
}
