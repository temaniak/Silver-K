#pragma once
#include "ksPanelPedalOutputService.h"
namespace ks {
// Completion-only caller projection. The graph retains packet/queue outcomes,
// never a machine register bank or an intermediate condition-code image.
template<class A>void projectPanelPedalOutput(A&a,const PanelPedalOutputService&s){
 if(!s.complete())throw std::logic_error("Pedal projection requires complete graph");
 const auto&v=s.summary();if(!v.bytes)throw std::logic_error("Pedal serial outcome absent");const auto&last=v.output[v.bytes-1];
 a.values[6]=(uint32_t(a.values[6])&0xffff0000u)|v.event;
 const uint16_t next=uint16_t((last.producer+1)&255);const int32_t restored=int32_t(a.values[0]);
 a.flags=uint8_t((a.flags&0xd0)|(restored<0?8:0)|(!restored?4:0)|(last.consumer<next?1:0)|((last.consumer&0xfff)<(next&0xfff)?32:0));
}
}
