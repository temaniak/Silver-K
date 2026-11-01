#pragma once
#include "ksNativeMidiQueueService.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeMidiQueue(A&a,const NativeMidiQueueService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI queue projection");const auto&t=s.queueTask();if(s.kind()==NativeMidiQueueService::Kind::Pop)a.values[6]=(a.values[6]&0xffff0000u)|s.result();uint8_t hc=0;if(s.kind()==NativeMidiQueueService::Kind::Push)hc=controlArithmetic::subtract(int16_t(t.consumer()),int16_t(t.producer()),0).flags&0x21;const auto v=s.kind()==NativeMidiQueueService::Kind::Reset?uint32_t(uint16_t(a.values[0])):a.values[0];a.flags=uint8_t((a.flags&0xd0)|hc|(s.kind()==NativeMidiQueueService::Kind::Reset?v&0x8000?8:!v?4:0:v&0x80000000u?8:!v?4:0));}
}
