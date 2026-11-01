#pragma once
#include "ksStateProgramLoadService.h"
#include "ksStateSaveAbi.h"
#include "ksControlArithmetic.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateProgramLoadForeground final {
public:
 using CallerAbi=StateSaveCallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}const StateProgramLoadService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateProgramLoadService::Kind k)const{return service()&&service()->kind()==k;}bool at(StateProgramLoadService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,3>kinds{};
 template<class A>static void projectRelease(A&a,const StateReloadVoiceService&s){if(!s.complete()||s.kind()!=StateReloadVoiceService::Kind::Voices||!s.voiceTask())throw std::logic_error("Incomplete voice release outcome");auto&v=a.values;const auto*walk=s.voiceTask();const auto remainingPrefix=v[0]&0xff00u;v[0]=(v[0]&0xffff0000u)|remainingPrefix;v[1]=(v[1]&0xffffff00u)|walk->part();v[2]=(v[2]&0xffff0000u)|1u;v[3]=0x17560+20*s.lastSlot();if(s.releasedAny()){v[0]=(s.dspRetried()?uint32_t(s.dspRemaining())<<16:0u)|remainingPrefix;v[1]&=0xffffu;v[2]=(unsigned(s.lastReleaseAge())<<16)|1u;v[4]=(v[4]&0xffff0000u)|uint16_t(s.lastAddress()+1);v[5]=s.dspPort();v[6]=s.lastPatch();}a.flags=uint8_t((a.flags&0xd0)|(v[0]&0x80000000u?8:0)|(!v[0]?4:0));}
 // Completion-only ABI composition. No ABI value is retained by the service.
 template<class A>static void project(A&a,const StateProgramLoadService&s){using K=StateProgramLoadService::Kind;if(!s.complete())throw std::logic_error("Program projection before completion");auto&v=a.values;const auto&f=s.summary();const auto entry=a.flags;auto low=[](uint32_t&x,uint8_t y){x=(x&0xffffff00u)|y;};auto word=[](uint32_t&x,uint16_t y){x=(x&0xffff0000u)|y;};auto nz8=[](uint8_t x){return uint8_t((x&128?8:0)|(!x?4:0));};auto nz16=[](uint16_t x){return uint8_t((x&0x8000?8:0)|(!x?4:0));};
 if(f.loaded){const auto*d=s.data();const auto*p=d->panel();if(!d||!p||!s.voices())throw std::logic_error("Missing single-load outcome");v[0]=p->current();v[1]=p->pointer();v[2]=(p->lastResolvedBase()&0xffff0000u)|p->bounds();v[3]=(p->lastBitSelectorReference()&0xffff0000u)|p->bounds();v[4]=(d->summary().expandedTarget&0xffff0000u)|0x8046u;v[5]=0x60010e;word(v[6],uint16_t((unsigned(d->summary().menu)<<8)|21u));projectRelease(a,*s.voices());low(v[6],0);v[0]=f.context;
 }
 if(f.reloaded){const auto*reload=s.reload();const auto*p=reload->panel();v[0]=p->current();v[1]=p->pointer();v[2]=(p->lastResolvedBase()&0xffff0000u)|p->bounds();v[3]=(p->lastBitSelectorReference()&0xffff0000u)|p->bounds();v[4]=(f.reloadExpandedTarget&0xffff0000u)|0x8046u;v[5]=0x60010e;if(f.reloadVoicePatchObserved)v[6]=f.lastReloadVoicePatch;word(v[6],uint16_t((unsigned(f.menu)<<8)|21u));}
 if(s.kind()==K::LoadSingle){a.flags=uint8_t((entry&0xd0)|4);return;}
 if(s.kind()==K::CommitSingle){if(f.mirrored)low(v[1],f.globalChannel);v[0]=0x10000u|f.remembered;word(v[6],f.presentation);const auto compared=controlArithmetic::subtract(int8_t(f.remembered>>8),int8_t(4),entry).flags;a.flags=f.rememberedWritten?uint8_t((compared&0xf1)|nz16(f.remembered)):compared;}
 else{v[0]=f.reloaded?(v[0]&0xffffff00u)|f.selection:0x16800u|f.selection;word(v[6],f.presentation);const auto*serial=s.serial();if(!serial||!serial->complete())throw std::logic_error("Missing completed program packet");const auto consumer=serial->consumer(),next=serial->nextProducer();const auto hc=uint8_t((consumer<next?1:0)|((consumer&0xfff)<(next&0xfff)?32:0));a.flags=uint8_t((entry&0xd0)|hc|nz8(f.selection));}
 }
private:struct Job{Job(StateProgramLoadService::Kind k,StateProgramLoadService::Input in,uint32_t c,int d,uint8_t p):service(k,in),caller(c),depth(d),priority(p){}StateProgramLoadService service;uint32_t caller;int depth;uint8_t priority;};std::optional<Job>job_;
};}
