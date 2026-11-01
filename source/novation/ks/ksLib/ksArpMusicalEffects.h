#pragma once
#include "ksNativeArpOrderedService.h"
namespace ks {
// Bounded musical outcomes for a repeated-note transaction. Later jobs can
// preserve earlier voice/sample/tuning bindings; only the latest relevant
// observation of each named source is retained, with its ordering.
struct ArpMusicalEffects final {
 enum class Payload{Preserved,Append,SamplePatch,VoiceSelection,VoiceWord};
 enum class FullTuning{None,Gate,Curve,Sample};
 enum class Notification{None,Patch,Random};
 unsigned order=0,voiceOrder=0,sampleOrder=0,patchOrder=0,upperPatchOrder=0,fullTuningOrder=0,presetOrder=0,laneOrder=0,modeOrder=0;
 uint8_t voice=0,sample=0,gatePart=0,gateCursor=0,curveByte=0,glideMode=0;uint16_t address=0,gateAge=0,tuningLane=0;uint32_t patchReference=0,upperPatch=0,voicePayload=0,curveAddress=0,sampleReference=0;bool addressKnown=false,gateLoop=false,gateReleased=false;Payload payload=Payload::Preserved;FullTuning fullTuning=FullTuning::None;Notification notification=Notification::None;uint8_t randomChoice=0;
 void markVoice(uint8_t v){voice=v;voiceOrder=++order;}
 void observe(const NativeArpOrderedService&s){if(s.random()){notification=Notification::Random;randomChoice=s.random()->summary().index;patchOrder=++order;}if(s.emit())observe(*s.emit());}
 void observe(const NativeArpEmitService&s){
  if(const auto*g=s.append()){payload=Payload::Append;voicePayload=0;const auto&v=g->summary();const auto&o=v.outputs;if(o.voice)markVoice(o.lastVoice->lastSlot());if(o.released){addressKnown=true;address=uint16_t(o.releasedAddress+1);}if(o.patchObserved){upperPatch=o.patch;upperPatchOrder=++order;}
   if(v.gate.held){const auto&t=*g->held()->summary().activityTask;fullTuning=FullTuning::Gate;gatePart=t.part();gateCursor=uint8_t(t.cursor());gateLoop=v.gate.loop;gateReleased=o.released;gateAge=o.releasedAge;fullTuningOrder=++order;}
  }
  const auto*emission=s.emission();if(!emission||!emission->program())return;const auto&p=emission->program()->summary();if(p.temporary){sample=p.selectTask->sampleIndex();sampleOrder=++order;}if(!p.played)return;const auto&v=*p.start;const auto&t=*v.start();if(p.temporary){payload=Payload::SamplePatch;voicePayload=p.selectTask->patchReference();}
  if(v.allocation()){const auto&a=*v.allocation();markVoice(a.summary().slot);addressKnown=true;address=uint16_t((unsigned(a.population()->pitch())<<8)|a.population()->distance());payload=Payload::VoiceSelection;voicePayload=1;fullTuning=FullTuning::Curve;curveAddress=a.population()->curve()>=4?0x1534eu+a.velocity()->curve():0;curveByte=a.summary().mirrorByte;fullTuningOrder=++order;}
  if(!t.suppressed()){
   if(v.parameters()){const auto&r=v.parameters()->summary();if(r.voiceObserved)markVoice(r.voice);if(r.presetObserved)presetOrder=++order;const auto&g=*r.glide;const auto&l=r.lfo[1];const uint16_t initial=l.delaySelector?uint16_t(0x159f4u+l.index):uint16_t((unsigned(uint8_t((l.delay^127u)-115))<<8)|((l.delay^127u)>115?0:115));tuningLane=uint16_t((unsigned(g.mode())<<8)|uint8_t(r.allocationObserved?g.allocation():initial));if(g.time())tuningLane=r.distanceObserved&&g.distance()?uint16_t(unsigned(g.distance())<<8):uint16_t((unsigned(r.distanceObserved?0:g.adjustment())<<8)|(g.inverted()>118?0:118));laneOrder=++order;}
   else if(v.glide()){const auto&g=*v.glide();if(v.summary().voiceObserved)markVoice(v.summary().voice);if(g.time()){tuningLane=v.summary().distanceObserved&&g.distance()?uint16_t(unsigned(g.distance())<<8):uint16_t((unsigned(v.summary().distanceObserved?0:g.adjustment())<<8)|(g.inverted()>118?0:118));laneOrder=++order;}else if(v.summary().allocationObserved){tuningLane=uint16_t((unsigned(g.mode())<<8)|g.allocation());laneOrder=++order;}else{glideMode=g.mode();modeOrder=++order;}}
   if(!v.gate())throw std::logic_error("Missing complete arp voice gate");const auto&g=v.gate()->summary();addressKnown=true;address=g.offsetAdded?g.address:VoiceStartTask::gateOffset();payload=Payload::VoiceWord;voicePayload=t.dspWord();
  }
  notification=Notification::Patch;patchReference=v.summary().patch;patchOrder=++order;
  if(p.temporary){fullTuning=FullTuning::Sample;sampleReference=p.selectTask->originalReference();fullTuningOrder=++order;}
 }
 uint32_t tuningBindingAfter(uint32_t prior)const{if(fullTuning!=FullTuning::None){switch(fullTuning){case FullTuning::Gate:{const auto cursor=0x160d2u+334*gatePart+208+2*gateCursor;prior=gateLoop?(gateReleased?uint32_t(gateAge)<<16:cursor&0xffff0000u)|(cursor&0xff00u):cursor;break;}case FullTuning::Curve:prior=(curveAddress&0xff00u)|curveByte;break;case FullTuning::Sample:prior=sampleReference;break;default:break;}}if(presetOrder>fullTuningOrder)prior=(prior&0xffffu)|0x10000u;if(laneOrder>fullTuningOrder)prior=(prior&0xffff0000u)|tuningLane;if(modeOrder>fullTuningOrder&&modeOrder>laneOrder)prior=(prior&0xffff00ffu)|(unsigned(glideMode)<<8);return prior;}
 uint32_t voiceBindingAfter(uint32_t prior)const{if(voiceOrder)prior=0x17560u+20*voice;if(sampleOrder>voiceOrder)prior=(prior&0xffffff00u)|sample;return prior;}
 uint32_t notificationAfter(uint32_t prior)const{if(notification==Notification::Patch)prior=patchReference;else if(notification==Notification::Random)prior=uint32_t(randomChoice)*0x10001u;if(upperPatchOrder>patchOrder)prior=(upperPatch&0xffff0000u)|(prior&0xffffu);return prior;}
 uint16_t payloadPrefixAfter(uint16_t prior)const{switch(payload){case Payload::Append:return 0;case Payload::SamplePatch:return uint16_t(voicePayload>>16);case Payload::VoiceSelection:return uint16_t(voicePayload);case Payload::VoiceWord:return uint16_t(voicePayload>>8);default:return prior;}}
};
}
