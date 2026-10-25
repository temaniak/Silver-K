#pragma once
#include "ksSysExNativeService.h"
#include "ksSysExNativeAbi.h"
#include "ksStateCentralActionsLeavesAbi.h"
#include "ksStateFactoryAbi.h"
#include "ksNativeKeyAbi.h"
namespace ks {namespace sysexLeavesBoundary {
using namespace centralActionsBoundary;
template<class A>void panel(A&a,const StateReloadPanelService&p,const std::optional<PanelControlTask>&indicator,uint8_t indicatorSet,uint32_t workingBase){
 a.values[0]=p.current();a.values[1]=p.pointer();a.values[2]=(p.lastResolvedBase()&0xffff0000u)|p.bounds();a.values[3]=(p.lastBitSelectorReference()&0xffff0000u)|p.bounds();a.values[4]=(workingBase&0xffff0000u)|0x8046;a.values[5]=0x60010e;word(a,6,uint16_t((p.panelTask().value()<<8)|21));if(!indicator)throw std::logic_error("Missing completed SysEx panel indicator");const auto&t=*indicator;const auto input=uint8_t(p.panelTask().value());uint8_t hc=0;if(t.mode()==3)hc=(input>>4)&1;else if(t.mode()!=1&&t.mode()!=2){const auto limit=t.limit();hc=controlArithmetic::subtract(int8_t(input),int8_t(limit),0).flags&0x21;const auto amount=input>limit?limit:input;if(int8_t(amount)>0)hc=uint8_t((hc&32)|(amount<=8?((unsigned(indicatorSet)>>(8-amount))&1):0));}a.flags=uint8_t((a.flags&0xd0)|hc|nz32(a.values[0]));
}
template<class A>void packing(A&a,const StateStoreService&s,uint32_t recipe){auto&v=a.values;if(s.kind()==StateStoreService::Kind::MultiSerialize){v[0]=s.original();v[1]=0x167b8;v[2]=(recipe&0xffff0000u)|5;v[3]=0x10000u|s.lastProgram();v[4]=s.base();a.flags=uint8_t((a.flags&0xd0)|nz32(v[0]));}else{const auto old=uint16_t(v[2]);word(a,0,uint16_t((unsigned(s.contributor())<<8)|s.value()));v[1]=s.base()+s.field();v[2]=(recipe&0xffff0000u)|(old&0xff00u)|7;v[4]=s.base();a.flags&=0xd0;}}
template<class A>void dump(A&a,const StateStoreDumpService&s,uint32_t recipe){using K=SysExDumpTask::Kind;auto&v=a.values;const auto&sum=s.summary();const StateStoreDumpService::Facts*last=&sum.last[0];for(unsigned n=0;n+1<sum.depthMaximum;++n)last=&sum.last[n+1];const auto k=last->kind;
 if(sum.packed)packing(a,*s.preparation(),recipe);
 if(s.lastHeader()){projectSysExOutput(a,*s.lastHeader());low(a,0,k==K::Patch?0:k==K::Multi?2:k==K::Global?4:k==K::Performance?3:1);value8(a,uint8_t(v[0]));}
 if(k==K::Resolve){v[0]=(v[0]&65535u)|0x10000u;v[4]=last->source;}
 if(s.lastSend()){projectSysExOutput(a,*s.lastSend());high(a,0,247);value8(a,247);if(s.lastByte())projectSysExOutput(a,*s.lastByte());}
 if(k==K::Large||k==K::Small){v[4]=last->source;low(a,0,last->program);cmp8(a,last->program,uint8_t(k==K::Large?100:50));}
 else if(k==K::Performance){v[4]=last->source;word(a,0,0x100);a.flags=uint8_t((a.flags&0xf1)|4);}
 if(s.kind()==K::Select){if(!s.lastSend())cmp8(a,uint8_t(s.input().selection),12);if(s.output()&&s.output()->kind()==SysExOutputService::Kind::ResetRx)projectSysExOutput(a,*s.output());}
}
template<class A>void outcome(A&a,const SysExNativeService&s,const SysExNativeService::Outcome&o){using Q=SysExNativeService::Request;using C=SysExNativeService::Component;if(!o.complete)throw std::logic_error("Incomplete SysEx native outcome");switch(o.component){
 case C::Expand:projectSysExPatchExpand(a,s.expand(o.slot));break;
 case C::Single:{const auto&child=*s.single();panel(a,*child.panel(),s.singleIndicator(),s.singleIndicatorSet(),child.summary().expandedTarget);break;}
 case C::Multi:{const auto&child=*s.multi();if(s.reloadVoicePatchObserved())a.values[6]=s.lastReloadVoicePatch();panel(a,*child.panel(),s.multiIndicator(),s.multiIndicatorSet(),child.coordinator().expandedCurrentBase());break;}
 case C::Globals:{StateSaveCallerAbi b{a.values,a.flags};projectStateFactory(b,*s.globals());a.values=b.values;a.flags=b.flags;break;}
 case C::Keys:projectNativeKey(a,*s.keys());break;
 case C::Semitone:projectStateCentralActions(a,*s.semitone(),[](auto,auto&){throw std::logic_error("Unexpected external SysEx semitone outcome");});break;
 case C::Save:{StateSaveService::Input in;in.source=o.input.source;in.program=o.input.record;in.noticeAction.value=0x55d4;in.saveAction.value=0x55e2;StateSaveCallerAbi b{a.values,a.flags};StateSaveForeground::project(b,s.save(o.slot),in,o.recipe);a.values=b.values;a.flags=b.flags;break;}
 case C::Display:{const auto&d=s.display(o.slot).display;word(a,6,d.output());a.flags=uint8_t((a.flags&0xd0)|centralActionsLeavesBoundary::lcdArithmetic(d.lcdResult(),a.flags)|nz16(uint16_t(a.values[0])));break;}
 case C::Dump:dump(a,*s.dump(),o.recipe);break;
 default:throw std::logic_error("Missing SysEx native completion projector");
}}
}
template<class A>void projectSysExNative(A&a,const SysExNativeService&s){if(!s.complete())throw std::logic_error("SysEx projection before complete");projectSysExNativePolicy(a,s.coordinator(),[&](unsigned h,auto&out){if(!h||h>s.outcomeCount())throw std::logic_error("SysEx outcome handle");sysexLeavesBoundary::outcome(out,s,s.outcome(h-1));});}
}
