#pragma once
#include "ksStateFactoryService.h"
#include "ksStateSaveForeground.h"
namespace ks {
// Completion-only compatibility edge. No ABI value is retained by the graph.
inline void projectStateFactory(StateSaveCallerAbi&a,const StateFactoryService&s){
 if(!s.complete())throw std::logic_error("Factory ABI projection requires completed work");
 if(s.kind()!=StateFactoryService::Kind::GlobalLoad){a.values[5]=s.kind()==StateFactoryService::Kind::Provided?s.input().source:0x167fc;a.values[6]=0x81ff00;StateSaveForeground::project(a,*s.save(),StateFactoryService::saveInput(a.values[5]));}
 a.values[0]=0x81ff15;a.values[1]=0x169fb;a.values[2]=(a.values[2]&0xff00u)|s.summary().globalsLast;a.flags=uint8_t((a.flags&0xf1)|4);
 if(s.kind()==StateFactoryService::Kind::Factory){a.values[6]=0x14edf;StateSaveForeground::project(a,*s.notice(),StateFactoryService::noticeInput());a.values[0]&=0xffffff00u;a.flags=uint8_t((a.flags&0xf1)|4);}
}
}
