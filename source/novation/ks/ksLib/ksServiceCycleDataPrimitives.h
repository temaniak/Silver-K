#pragma once
#include "ksStateChannelService.h"
#include "ksStatePanelService.h"
#include "ksStateRefreshService.h"
#include "ksVoiceFeedbackService.h"
#include "ksPartFeedbackService.h"
#include "ksPanelMasksService.h"
#include "ksDiagnosticTransferService.h"
namespace ks { namespace serviceCycleData {
// Only the three resource operands consumed by later native operations cross
// service boundaries. No flags, instruction address or processor bank exists
// in this composition. Every assignment comes from completed domain facts.
template<class C>void dspExpression(C&c,const ControllerDspService*s){if(s&&s->summary().selectedPort)c.expressionSource=s->summary().port==DspPort::A?0x400000u:0x600000u;}
template<class C>C global(const GlobalUpdateService&s,C c){if(!s.complete())throw std::logic_error("Incomplete global domain outcome");const auto&r=s.summary();if(!r.final||!r.time[2]||!r.pair)throw std::logic_error("Missing global domain facts");const auto source=r.effect?r.effect->second():(r.time[2]->mode()?0x159f4u+r.time[2]->index():r.pair->pair());c.activation=uint8_t(source>>8);c.presentation=uint16_t(r.patch);if(const auto*d=s.lastDspService())c.expressionSource=d->port()==DspPort::A?0x400000u:0x600000u;return c;}
template<class C>C deferred(const StateDeferredPartsService&s,C c){if(!s.complete())throw std::logic_error("Incomplete deferred domain outcome");if(s.summary().refreshed){if(!s.voice()||!s.voice()->global())throw std::logic_error("Missing deferred voice outcome");const auto activation=c.activation;c=global(*s.voice()->global(),c);c.activation=activation;}return c;}
template<class C>C clock(const ClockSyncService&s,C c){if(!s.complete())throw std::logic_error("Incomplete clock domain outcome");const auto&v=s.summary();if(v.divider&1)return c;c.activation=uint8_t(v.previousPeriod>>8);for(auto kind:{ClockSyncService::Component::Coefficient,ClockSyncService::Component::PeriodAddress,ClockSyncService::Component::PeriodValue})dspExpression(c,s.transaction(kind));return c;}
template<class C>C refresh(const StateRefreshService&s,C c){if(!s.complete()||!s.packet()||!s.deferred()||!s.clock())throw std::logic_error("Incomplete refresh domain outcome");dspExpression(c,s.packet());if(s.summary().panelRefreshed){if(!s.panel())throw std::logic_error("Missing refresh panel facts");c.expressionSource=0x60010e;c.activation=uint8_t(s.panel()->bounds()>>8);c.presentation=uint16_t((s.panel()->panelTask().value()<<8)|21u);}c=deferred(*s.deferred(),c);return clock(*s.clock(),c);}
template<class C>C voiceFeedback(const VoiceFeedbackService&s,C c){if(!s.complete())throw std::logic_error("Incomplete voice feedback domain outcome");const auto&v=s.summary();dspExpression(c,s.selection());dspExpression(c,s.statusRead());dspExpression(c,s.ageRead());if(v.activated)c.presentation=65535;else if(v.countdownRead)c.presentation=v.nextCountdown;return c;}
template<class C>C partFeedback(const PartFeedbackService&s,C c){if(!s.complete())throw std::logic_error("Incomplete part feedback domain outcome");for(unsigned n=0;n<4;++n){dspExpression(c,s.status(n));if(s.summary().refreshed&(1u<<n)){dspExpression(c,s.clear(n));if(!s.global(n))throw std::logic_error("Missing feedback global outcome");c=global(*s.global(n),c);}}return c;}
template<class C>C masks(const PanelMasksService&s,C c){if(!s.complete())throw std::logic_error("Incomplete panel mask domain outcome");if(s.summary().ran)c.activation=s.summary().changed?s.maskTask().other():s.maskTask().mask();return c;}
template<class C>C channel(const StateChannelService&s,C c){if(!s.complete())throw std::logic_error("Incomplete channel domain outcome");return c;}
template<class C>C panel(const StatePanelService&s,C c){if(!s.complete())throw std::logic_error("Incomplete keyboard domain outcome");c.presentation=s.event();return c;}
template<class C>C diagnostic(const DiagnosticTransferService&s,C c){if(!s.complete())throw std::logic_error("Incomplete diagnostic domain outcome");using K=DiagnosticTransferService::Kind;if(s.kind()==K::Dump||s.kind()==K::Block){c.activation=uint8_t(s.result().encoded.weight>>8);c.presentation=uint16_t(s.result().source);if(s.kind()==K::Dump)c.expressionSource=s.result().sum;}return c;}
}}
