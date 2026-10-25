#pragma once
#include "ksNativeParameterEditService.h"
#include "ksNativeParameterSupportAbi.h"
#include "ksStatePlaybackModeProjection.h"
#include "ksStateFieldForeground.h"
#include "ksControlArithmetic.h"
namespace ks {
template<class A>void projectNativeParameterEdit(A&a,const NativeParameterEditService&s){if(!s.complete())throw std::logic_error("Incomplete native parameter edit projection");const auto&r=s.summary();const auto&t=s.editTask();const auto low=[](uint32_t&v,uint8_t n){v=(v&0xffffff00u)|n;};const auto word=[](uint32_t&v,uint16_t n){v=(v&0xffff0000u)|n;};const auto nz8=[](uint8_t n){return uint8_t((n&128?8:0)|(!n?4:0));};const auto move=[&](uint8_t n){a.flags=uint8_t((a.flags&0xf1)|nz8(n));};
 if(r.entry==NativeParameterEditService::Entry::Public){a.flags=controlArithmetic::subtract(int8_t(r.input.parameter),int8_t(127),a.flags).flags;if(r.input.parameter>127)return;projectNativeParameterSupport(a,*r.map);if(r.map->summary().map->outcome()==StateFieldMapTask::Outcome::AboveLimit)return;}
 const bool packed=r.map&&r.map->summary().map->outcome()!=StateFieldMapTask::Outcome::Unmapped;
 if(!packed){a.values[0]=r.type;a.values[1]=t.field();if(r.entry==NativeParameterEditService::Entry::Public)low(a.values[6],t.parameter());a.flags=controlArithmetic::subtract(int8_t(r.type),int8_t(126),uint8_t(a.flags&0xd0)).flags;if(r.type<126){low(a.values[0],r.inhibit);move(r.inhibit);if(r.inhibit)return;}else move(t.value());}
 if(r.bind){const auto retainedField=a.values[1];const auto retainedRequest=uint16_t(a.values[6]);projectNativeParameterSupport(a,*r.bind);a.values[1]=retainedField;word(a.values[6],retainedRequest);}
 if(r.expand){a.values[0]=r.live+t.field();a.flags=uint8_t((a.flags&0xd0)|nz8(t.value()));const auto retainedField=uint16_t(a.values[1]);projectNativeParameterSupport(a,*r.expand);playbackModeProjection::lower(a,*r.routing);projectNativeParameterSupport(a,*r.dirty);word(a.values[1],retainedField);}
 low(a.values[0],r.notificationMode);move(r.notificationMode);if(r.notificationMode){low(a.values[0],r.notificationField);move(r.notificationField);a.flags=controlArithmetic::subtract(int8_t(a.values[1]),int8_t(r.notificationField),a.flags).flags;}else{low(a.values[1],r.probe->edit());move(r.probe->edit());}
 if(r.displayed){StateFieldForeground::CallerAbi b;b.values=a.values;b.flags=a.flags;StateFieldForeground::projectUi(b,*s.displayOutcome());a.values=b.values;a.flags=b.flags;}
 low(a.values[0],1);move(1);
}
}
