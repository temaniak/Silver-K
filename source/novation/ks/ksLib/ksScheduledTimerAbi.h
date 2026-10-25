#pragma once
#include "ksScheduledTimerService.h"
#include "ksStateCentralActionsLeavesAbi.h"
#include "ksStateStatusAbi.h"
namespace ks {
template<class A>void projectScheduledTimer(A&out,const ScheduledTimerService&s){if(!s.complete())throw std::logic_error("Timer projection before complete");using S=ScheduledTimerService;using K=S::Kind;using namespace centralActionsBoundary;centralActionsLeavesBoundary::A a;a.values=out.values;a.flags=out.flags;const auto saved5=uint16_t(a.values[5]);const auto saved6=a.values[6];const auto&c=s.coordinator().summary();
 if(s.kind()==K::Cancel){word(a,0,0);a.flags=uint8_t((a.flags&0xd0)|4);}
 else if(s.kind()==K::Arm){a.values[6]=c.counter;value16(a,uint16_t(c.counter));}
 else {if(c.interval){word(a,5,c.interval);a.values[6]=c.counter;a.values[6]=(a.values[6]&65535u)|(uint32_t(c.started)<<16);const auto difference=controlArithmetic::subtract(int16_t(c.counter),int16_t(c.started),a.flags);a.flags=difference.flags;word(a,6,uint16_t(difference.value));a.flags=controlArithmetic::subtract(int16_t(c.elapsed),int16_t(c.interval),a.flags).flags;
 if(c.fired){a.flags=controlArithmetic::subtract(int16_t(0),int16_t(0),a.flags).flags;a.values[6]=c.actionWord;value32(a,c.actionWord);std::visit([&](const auto&child){using T=std::decay_t<decltype(child)>;if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Missing completed timer callback");else if constexpr(std::is_same_v<T,StateFieldService>)StateFieldForeground::project(a,child,child.input(),child.handlerBinding(),child.recipeBinding());else if constexpr(std::is_same_v<T,S::Notice>){move8(a,0,0);if(!child.ui||!child.ui->complete())throw std::logic_error("Notice clear missing UI outcome");StateFieldForeground::projectUi(a,*child.ui);}else if constexpr(std::is_same_v<T,StateCentralActionsService>)projectStateCentralActions(a,child,[](auto,auto&){throw std::logic_error("External timer callback projection");});else if constexpr(std::is_same_v<T,StateProgramBrowserService>)StateProgramBrowserForeground::project(a,child);else if constexpr(std::is_same_v<T,StateStatusService>)projectStateStatus(a,child);},s.child());}
 a.values[6]=saved6;value32(a,saved6);}word(a,5,saved5);value16(a,saved5);}
 out.values=a.values;out.flags=a.flags;}
}
