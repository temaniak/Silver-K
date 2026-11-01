#pragma once
#include "ksInternalKeyOutputService.h"
namespace ks {
template<class A>void projectInternalKeyOutput(A&a,const InternalKeyOutputService&s){if(!s.complete())throw std::logic_error("Incomplete internal key output projection");const auto&r=s.summary();using K=InternalKeyOutputService::Kind;const auto base=0x160d2u+334*r.part;
 if(r.kind==K::Set||r.kind==K::Clear){a.values[1]=base+224+r.task->group();a.flags&=0xd0;return;}
 a.values[0]=base;a.values[1]=base+224;a.values[2]&=0xffff0000u;
 if(r.scanned){a.values[3]=(a.values[3]&0xffff0000u)|128;a.values[5]=(a.values[5]&0xffff0000u)|r.noteVelocity;}
 if(r.emitted)a.values[6]=(a.values[6]&0xffff0000u)|r.event;
 a.flags=uint8_t((a.flags&0xd0)|4);
}
}
