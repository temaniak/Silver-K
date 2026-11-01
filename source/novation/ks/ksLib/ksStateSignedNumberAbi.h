#pragma once
#include "ksStateDisplayService.h"
#include "ksControlArithmetic.h"
namespace ks {
// Full SignedThree restores its three saved 32-bit values. Its LCD transport
// restores the presentation word; only the external completion flags change.
template<class A>void projectStateSignedNumber(A&a,const StateDisplayService&s){if(!s.complete()||s.kind()!=StateDisplayService::Kind::SignedNumber)throw std::logic_error("Incomplete signed display projection");const auto&r=s.lcdResult();using P=PanelTextTask::Action;const uint8_t hc=!r.observed?a.flags&0x21:r.action==P::Write?controlArithmetic::subtract(int8_t(r.reply),int8_t(20),0).flags&0x21:r.action==P::Command?controlArithmetic::subtract(int8_t(r.input),int8_t(251),0).flags&0x21:0;const auto v=a.values[0];a.flags=uint8_t((a.flags&0xd0)|hc|(v&0x80000000u?8:0)|(!v?4:0));}
}
