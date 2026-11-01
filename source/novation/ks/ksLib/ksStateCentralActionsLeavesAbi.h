#pragma once
#include "ksStateCentralActionsService.h"
#include "ksStateCentralActionsAbi.h"
#include "ksStateFieldForeground.h"
#include "ksStateEditActionForeground.h"
#include "ksStateProgramBrowserForeground.h"
#include "ksStateStoreMenuForeground.h"
#include "ksStatePlaybackModeForeground.h"
#include "ksStatePartNavigationForeground.h"
#include "ksStateSceneToggleForeground.h"
#include "ksStateProgramLoadForeground.h"
#include "ksStateSaveForeground.h"
#include "ksControllerDspAbi.h"
#include "ksMidiPacketAbi.h"
#include "ksStateSignedNumberAbi.h"
namespace ks {namespace centralActionsLeavesBoundary {
using S=StateCentralActionsService;using A=StateFieldForeground::CallerAbi;using Q=S::Request;using namespace centralActionsBoundary;
inline uint8_t lcdArithmetic(const StateDisplayService::LcdResult&r,uint8_t fallback){using P=PanelTextTask::Action;return !r.observed?fallback&0x21:r.action==P::Write?controlArithmetic::subtract(int8_t(r.reply),int8_t(20),0).flags&0x21:r.action==P::Command?controlArithmetic::subtract(int8_t(r.input),int8_t(251),0).flags&0x21:0;}
inline void events(A&a,const StateEventsService&s){using K=StateEventsService::Kind;const auto curve=[&]{const auto x=s.selector();return uint8_t(x==34?s.value()<128:x==35?s.scaledValue()&1:x==32?((unsigned(s.squareInput())*s.squareInput())>>8)&1:x<32);};if(s.kind()==K::ButtonPop)word(a,6,s.event());else a.values[6]=s.available()?0x10000u|s.event():0;a.flags=uint8_t((a.flags&0xd0)|nz32(a.values[0])|(s.kind()==K::ControlPop&&s.available()?curve():0));}
inline void screen(A&a,const StateScreenService&s){const auto&o=s.summary();using K=StateScreenService::Kind;if(s.kind()!=K::Gate){if(s.kind()==K::Set)a.values[2]=0x16800u|o.setReset;StateScreenForeground::projectRender(a,o);}const auto&g=o.gate;const auto entry=a.flags;low(a,0,uint8_t(g.accepted));uint8_t hc=entry&0x21;if(g.hardware)low(a,1,g.hardware);else{word(a,1,uint16_t((unsigned(g.screen)<<8)|g.page));hc=controlArithmetic::subtract(int16_t(uint16_t(a.values[1])),int16_t(0x902),0).flags&0x21;if(g.editObserved)low(a,1,g.edit);}a.flags=uint8_t((entry&0xd0)|hc|nz8(uint8_t(a.values[0])));if(o.reset){a.values[0]=0x816000;a.values[1]=0x159ec;a.values[2]=(a.values[2]&0xff00u)|o.resetLast;a.flags=uint8_t((a.flags&0xf1)|4);}}
inline void settings(A&a,const PanelSettingsService&s){const auto entry=a.flags;uint8_t flags=entry&0xd0;if(s.kind()==PanelSettingsService::Kind::Select){low(a,0,s.mode());if(s.noticePrinted())a.values[6]&=65535;if(s.showedMode())flags|=uint8_t(lcdArithmetic(s.lcdResult(),entry)|nz16(uint16_t(a.values[6])));else flags=controlArithmetic::subtract(int8_t(s.mode()),int8_t(5),entry).flags;}else{if(s.inputAvailable()){a.values[6]=0x10000u|s.pollEvent();if(s.noticePrinted())a.values[6]&=65535;low(a,0,s.mode());}else a.values[6]=0;if(s.coefficientsObserved()){const auto span=uint8_t(s.maximum()-s.minimum()),center=uint8_t(s.minimum()+(span>>1));const unsigned q=span?s.coefficientBias()/span:0,rem=s.coefficientBias()-q*span;word(a,0,uint16_t((unsigned(s.minimum())<<8)|span));a.values[1]=s.coefficientHistory();a.values[2]=s.coefficientHistory()-256;word(a,3,uint16_t((rem<<8)|uint8_t(q)));(void)center;}if(s.eventDisplayed())low(a,0,s.value());word(a,5,uint16_t(0x400|s.encoderGain()));word(a,6,s.event());const auto delta=s.encoderDelta()?s.encoderGain():uint8_t(0);if(delta){const int sum=int(s.pollDuty())+int(int8_t(delta));word(a,0,uint16_t((sum<0?0:unsigned(sum)&0xff00u)|s.value()));word(a,1,uint16_t(int16_t(int8_t(delta))));flags|=uint8_t(lcdArithmetic(s.lcdResult(),0)|nz16(uint16_t(a.values[0])));}else flags|=4;}a.flags=flags;}
inline void panel(A&a,const S&s){const auto&p=s.panel();a.values[0]=p.current();a.values[1]=p.pointer();a.values[2]=(p.lastResolvedBase()&0xffff0000u)|p.bounds();a.values[3]=(p.lastBitSelectorReference()&0xffff0000u)|p.bounds();word(a,4,0x8046);a.values[5]=0x60010e;word(a,6,uint16_t((p.panelTask().value()<<8)|21));if(!s.lastIndicator())throw std::logic_error("Missing central panel indicator completion");const auto&t=*s.lastIndicator();const auto input=uint8_t(p.panelTask().value());const auto mode=t.mode();uint8_t hc=0;if(mode==3)hc=(input>>4)&1;else if(mode!=1&&mode!=2){const auto limit=t.limit();hc=controlArithmetic::subtract(int8_t(input),int8_t(limit),0).flags&0x21;const auto amount=input>limit?limit:input;if(int8_t(amount)>0)hc=uint8_t((hc&32)|(amount<=8?((unsigned(s.lastIndicatorSet())>>(8-amount))&1):0));}a.flags=uint8_t((a.flags&0xd0)|hc|nz32(a.values[0]));}
template<class Project>void outcome(A&a,const S&s,const S::Outcome&o,Project&external){if(!o.complete)throw std::logic_error("Central UI child projection before complete");switch(o.family){
 case S::Family::External:external(o.external,a);break;
 case S::Family::Events:events(a,s.events(o.slot));break;
 case S::Family::Screen:screen(a,s.screen(o.slot));break;
 case S::Family::Probe:move8(a,1,s.probe(o.slot).edit());break;
 case S::Family::Settings:settings(a,s.settings());break;
 case S::Family::Controls:move8(a,6,s.controls().switches());break;
 case S::Family::Dsp:projectControllerDsp(a,s.dsp());break;
 case S::Family::Midi:projectMidiPacket(a,s.midi());break;
 case S::Family::Field:{const auto&f=s.field(o.slot);if(o.request==Q::UiRefresh)StateFieldForeground::projectUi(a,f);else if(o.request==Q::Notify)StateFieldForeground::projectScene(a,f);else if(o.request==Q::NameInsertDigit||o.request==Q::NameToggle||o.request==Q::NameInsertSpace||o.request==Q::NameClearSuffix||o.request==Q::SceneDecrement||o.request==Q::SceneIncrement)StateFieldForeground::projectSceneEdit(a,f);else StateFieldForeground::project(a,f,f.input(),f.handlerBinding(),f.recipeBinding());break;}
 case S::Family::Heavy:std::visit([&](const auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,StateEditActionService>)StateEditActionForeground::project(a,t,o.recipe);else if constexpr(std::is_same_v<T,StateProgramBrowserService>)StateProgramBrowserForeground::project(a,t);else if constexpr(std::is_same_v<T,StateStoreMenuService>)StateStoreMenuForeground::project(a,t,o.recipe);else if constexpr(std::is_same_v<T,StatePlaybackModeService>)StatePlaybackModeForeground::project(a,t,o.recipe);else if constexpr(std::is_same_v<T,StatePartNavigationService>)StatePartNavigationForeground::project(a,t);else if constexpr(std::is_same_v<T,StateSceneToggleService>)StateSceneToggleForeground::project(a,t);else throw std::logic_error("Missing central heavy action outcome");},s.heavy());break;
 case S::Family::Lower:StatePlaybackModeForeground::projectLower(a,s.lower(o.slot));break;
 case S::Family::Reset:playbackModeProjection::reset(a,s.reset(o.slot));break;
 case S::Family::Program:StateProgramLoadForeground::project(a,s.program());break;
 case S::Family::Apply:StateFieldForeground::projectApply(a,s.apply(),o.recipe);break;
 case S::Family::Panel:panel(a,s);break;
 case S::Family::Save:{StateSaveForeground::CallerAbi out;out.values=a.values;out.flags=a.flags;StateSaveForeground::project(out,s.save(),s.saveInput(),o.recipe);a.values=out.values;a.flags=out.flags;break;}
 case S::Family::Repeat:a.values[6]=s.repeat().tick;value16(a,uint16_t(s.repeat().tick));break;
 case S::Family::Display:projectStateSignedNumber(a,s.display());break;
 }}
}
template<class CallerAbi,class ExternalProject>void projectStateCentralActions(CallerAbi&out,const StateCentralActionsService&s,ExternalProject&&external){if(!s.complete())throw std::logic_error("Central UI projection before complete");centralActionsLeavesBoundary::A a;a.values=out.values;a.flags=out.flags;auto&&p=external;projectStateCentralActions(a,s.coordinator(),[&](StateCentralActionsCoordinator::OutcomeHandle h,auto&abi){if(!h.value||h.value>s.outcomeCount())throw std::logic_error("Central UI completed outcome identity");centralActionsLeavesBoundary::outcome(abi,s,s.outcome(h.value-1),p);});out.values=a.values;out.flags=a.flags;}
}
