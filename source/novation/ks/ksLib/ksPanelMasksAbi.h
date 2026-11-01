#pragma once
#include "ksPanelMasksService.h"
#include "ksControlArithmetic.h"
namespace ks {
// Only the external completion edge folds these observed mask outcomes into
// the still-existing caller ABI. It is never used to drive a child or branch.
template<class A>void projectPanelMasks(A&a,const PanelMasksService&s){
 if(!s.complete())throw std::logic_error("Incomplete panel mask projection");
 const auto&t=s.maskTask();const auto&v=s.summary();
 if(!v.ran){a.values[0]&=0xffffff00u;a.flags=uint8_t((a.flags&0xf1)|4);return;}
 a.values[0]=0x00ffec00u|t.previous();
 a.values[1]=(a.values[1]&0xffff0000u)|(v.changed?0x34u:t.decay());
 a.values[2]=(a.values[2]&0xffff0000u)|(uint32_t(v.changed?t.other():t.mask())<<8)|(v.changed?t.output():v.activation);
 const auto compared=controlArithmetic::subtract(int8_t(t.mask()),int8_t(t.previous()),a.flags).flags;
 a.flags=v.changed?uint8_t((compared&0xf1)|8):compared;
}
}
