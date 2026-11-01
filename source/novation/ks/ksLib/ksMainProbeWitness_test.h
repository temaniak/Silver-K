#pragma once
#include "ksCurrentNativeChildren_test.h"
#include <array>
#include <ostream>
namespace ks {
// Probe-only samples. Counts below are containing-parent witnesses, never
// standalone admissions or inferred counts of unsampled children.
struct MainProbeKindWitness final {
 CurrentNativeParentForTests parent{};uint64_t job=0,before=0,samples=0,sampledJobs=0,returnedJobs=0;bool pending=false;
 uint32_t sampledKinds=0,returnedKinds=0,components=0,returnedComponents=0,pendingKinds=0,pendingComponents=0;
 std::array<uint32_t,16>pendingByKind{},returnedByKind{};std::array<uint64_t,16>returnedJobsByKind{};
 void poll(){if(!pending)return;if(parent.returns()>before){returnedKinds|=pendingKinds;returnedComponents|=pendingComponents;for(unsigned k=0;k<16;++k){returnedByKind[k]|=pendingByKind[k];if(pendingKinds&(1u<<k))++returnedJobsByKind[k];}++returnedJobs;pending=false;}else if(parent.job()!=job)throw std::logic_error("Probe sampled child lost its actual containing parent");}
 void sample(CurrentNativeParentForTests p,unsigned kind,uint32_t mask){poll();if(!p||kind>=16)throw std::logic_error("Probe child witness binding");++samples;if(!pending){parent=p;job=p.job();before=p.returns();pending=true;pendingKinds=pendingComponents=0;pendingByKind.fill(0);++sampledJobs;}else if(parent.completed!=p.completed||job!=p.job())throw std::logic_error("Probe changed child parent before return");sampledKinds|=1u<<kind;components|=mask;pendingKinds|=1u<<kind;pendingComponents|=mask;pendingByKind[kind]|=mask;}
 void write(std::ostream&o,const char*prefix)const{o<<",\""<<prefix<<"samples\":"<<samples<<",\""<<prefix<<"sampled_parent_jobs\":"<<sampledJobs<<",\""<<prefix<<"returned_parent_jobs\":"<<returnedJobs<<",\""<<prefix<<"sampled_kind_mask\":"<<sampledKinds<<",\""<<prefix<<"returned_parent_kind_mask\":"<<returnedKinds<<",\""<<prefix<<"sampled_components\":"<<components<<",\""<<prefix<<"returned_parent_components\":"<<returnedComponents<<",\""<<prefix<<"returned_parent_components_by_kind\":[";for(unsigned k=0;k<16;++k){if(k)o<<',';o<<returnedByKind[k];}o<<']';}
};
struct MainProbeRoles final {
 // Legacy role order: messages/on/off/pedal/allocation/start/release/transport.
 static uint32_t release(const NativeVoiceReleaseService*p){if(!p||p->kind()!=NativeVoiceReleaseService::Kind::Voices)return 0;return(1u<<6)|(p->releasedAny()?1u<<7:0);}
 static uint32_t start(const NativeVoiceStartService*p){if(!p)return 0;uint32_t r=p->summary().starts?1u<<5:0;if(p->allocation())r|=1u<<4;if(p->summary().words)r|=1u<<7;return r;}
 static uint32_t program(const NativeNoteProgramService*p){if(!p)return 0;uint32_t r=0;if(p->summary().start)r|=start(&*p->summary().start);r|=start(p->task<NativeVoiceStartService>());return r;}
 static uint32_t part(const NativePartNoteService*p){if(!p)return 0;const auto&f=p->summary();uint32_t r=1u<<(f.kind==NativePartNoteService::Kind::On?1:2);if(f.program)r|=program(&*f.program);r|=program(p->task<NativeNoteProgramService>());if(f.release)r|=release(&*f.release);r|=release(p->task<NativeVoiceReleaseService>());if(f.state&&f.state->summary().released)r|=1u<<6;if(const auto*s=p->task<NativeNoteStateService>())r|=release(s->task<NativeVoiceReleaseService>());return r;}
 static uint32_t emission(const NativeArpEmissionService*p){if(!p)return 0;return program(p->program())|release(p->release());}
 static uint32_t handler(const NativeMidiHandlerService&p){uint32_t r=1;const auto&f=p.frames();if(const auto*s=std::get_if<NativePartNoteService>(&f))r|=part(s);if(const auto*s=std::get_if<NativeMidiControlService>(&f)){if(s->summary().pedaled||s->task<NativePedalControlService>())r|=1u<<3;r|=release(s->releaseOutcome());r|=release(s->task<NativeVoiceReleaseService>());}if(const auto*s=std::get_if<NativeInternalNoteService>(&f))r|=emission(s->emissionOutcome());return r;}
 static uint32_t route(const NativeMidiRouteService*p){if(!p)return 0;uint32_t r=0;for(unsigned n=0;n<p->summary().count;++n)r|=handler(p->handlerOutcome(n));return r;}
 static uint32_t pump(const NativeMidiPumpService*p){if(!p)return 0;uint32_t r=0;if(const auto*d=p->decodeOutcome()){r|=route(d->routeProgress());if(const auto*h=d->immediateOutcome())r|=handler(*h);}if(const auto*d=p->queuedDecodeOutcome()){r|=route(d->routeProgress());if(const auto*h=d->immediateOutcome())r|=handler(*h);}return r;}
 static uint32_t emit(const NativeArpEmitService*p){if(!p)return 0;uint32_t r=emission(p->emission());if(const auto*g=p->append())if(g->summary().outputs.lastVoice)r|=release(&*g->summary().outputs.lastVoice);return r;}
 static uint32_t arp(const NativeArpOwnedService*p){if(!p)return 0;uint32_t r=0;if(const auto*m=p->arp().main()){r|=release(m->release());if(const auto*c=m->chord())r|=emit(c->lastEmit());if(const auto*s=m->run()){if(const auto*o=s->lastOrdered())r|=emit(o->emit());if(const auto*q=s->lastSearch())r|=emit(q->emit());}if(const auto*g=m->gate())if(g->summary().outputs.lastVoice)r|=release(&*g->summary().outputs.lastVoice);}return r;}
 std::array<MainProbeKindWitness,17>children{};uint64_t samples=0;
 void observe(const RuntimeController&r){for(auto&w:children)w.poll();const auto*m=currentNativeMainForTests(r);if(!m)return;uint32_t mask=pump(m->current<NativeMidiPumpService>())|arp(m->current<NativeArpOwnedService>());if(!mask)return;++samples;const auto parent=currentNativeMainParentForTests(r);for(unsigned k=0;k<8;++k)if(mask&(1u<<k))children[m->childIndex()].sample(parent,k,0);}
 uint32_t returnedMask()const{uint32_t m=0;for(const auto&w:children)m|=w.returnedKinds;return m;}
 uint64_t returnedJobs()const{uint64_t n=0;for(const auto&w:children)n+=w.returnedJobs;return n;}
 void write(std::ostream&o)const{o<<",\"native_main_role_witness_samples\":"<<samples<<",\"native_main_role_witness_returned_parent_jobs\":"<<returnedJobs()<<",\"native_main_role_returned_parent_mask\":"<<returnedMask()<<",\"native_main_role_returned_parent_jobs_by_role\":[";for(unsigned k=0;k<8;++k){if(k)o<<',';uint64_t n=0;for(const auto&w:children)n+=w.returnedJobsByKind[k];o<<n;}o<<']';}
};
// A flattened PartSelection policy task is a different owner from the heavy
// PartNavigation service. Sample real typed current/finished coordinator nodes,
// then publish their sampled stages only after the containing Main child returns.
// These are task/parent witnesses, never legacy admissions or step execution counts.
struct MainProbePartSelectionTaskWitness final {
 MainProbeKindWitness owner{};
 std::array<uint64_t,2> sampledStages{},pendingStages{},returnedStages{};
 uint32_t sampledCompleteKinds=0,pendingCompleteKinds=0,returnedCompleteKinds=0;
 uint32_t sampledFinishedKinds=0,pendingFinishedKinds=0,returnedFinishedKinds=0;
 void poll(){const auto returned=owner.returnedJobs;owner.poll();if(owner.returnedJobs!=returned){for(unsigned i=0;i<2;++i)returnedStages[i]|=pendingStages[i];returnedCompleteKinds|=pendingCompleteKinds;returnedFinishedKinds|=pendingFinishedKinds;}}
 void sample(CurrentNativeParentForTests parent,unsigned kind,const StatePartSelectionTask&task,bool finished){
  poll();if(!owner.pending){pendingStages={};pendingCompleteKinds=pendingFinishedKinds=0;}
  const auto stage=unsigned(task.stage());if(kind>=7||stage>=128)throw std::logic_error("Probe PartSelection typed task binding");
  owner.sample(parent,kind,0);const auto bit=uint64_t(1)<<(stage&63);sampledStages[stage/64]|=bit;pendingStages[stage/64]|=bit;
  if(task.complete()){sampledCompleteKinds|=1u<<kind;pendingCompleteKinds|=1u<<kind;}
  if(finished){if(!task.complete())throw std::logic_error("Probe PartSelection finished node is not complete");sampledFinishedKinds|=1u<<kind;pendingFinishedKinds|=1u<<kind;}
 }
 bool returnedPartButtonTask()const{constexpr auto kinds=(1u<<unsigned(StatePartSelectionTask::Kind::Select))|(1u<<unsigned(StatePartSelectionTask::Kind::Held));return owner.returnedJobs&&(owner.returnedKinds&kinds)&&(returnedStages[0]||returnedStages[1]);}
 void write(std::ostream&o)const{
  const char*base="native_main_part_selection_task_";
  o<<",\""<<base<<"samples\":"<<owner.samples<<",\""<<base<<"sampled_parent_jobs\":"<<owner.sampledJobs<<",\""<<base<<"returned_parent_jobs\":"<<owner.returnedJobs
   <<",\""<<base<<"sampled_kind_mask\":"<<owner.sampledKinds<<",\""<<base<<"returned_parent_kind_mask\":"<<owner.returnedKinds
   <<",\""<<base<<"sampled_stage_mask\":["<<sampledStages[0]<<','<<sampledStages[1]<<']'
   <<",\""<<base<<"returned_parent_sampled_stage_mask\":["<<returnedStages[0]<<','<<returnedStages[1]<<']'
   <<",\""<<base<<"sampled_complete_kind_mask\":"<<sampledCompleteKinds<<",\""<<base<<"returned_parent_sampled_complete_kind_mask\":"<<returnedCompleteKinds
   <<",\""<<base<<"sampled_finished_node_kind_mask\":"<<sampledFinishedKinds<<",\""<<base<<"returned_parent_sampled_finished_node_kind_mask\":"<<returnedFinishedKinds;
 }
};
struct MainProbeUiWitness final {
 MainProbeKindWitness playback,part,browser;MainProbePartSelectionTaskWitness partSelectionTask;
 void observe(const RuntimeController&r){playback.poll();part.poll();browser.poll();partSelectionTask.poll();const auto*m=currentNativeMainForTests(r);if(!m)return;const auto parent=currentNativeMainParentForTests(r);const auto play=[&](const StatePlaybackModeService*s){if(s)playback.sample(parent,unsigned(s->kind()),s->components());};const auto browse=[&](const StateProgramBrowserService*s){if(s)browser.sample(parent,unsigned(s->kind()),s->components());};if(const auto*ui=currentNativeCentralForTests(r)){
 const auto&coordinator=ui->coordinator();const auto*current=coordinator.task<StatePartSelectionTask>();
 for(unsigned n=0;n<coordinator.nodeCount();++n){const auto&node=coordinator.node(n);const auto*task=std::get_if<StatePartSelectionTask>(&node.task);if(!task)continue;
  using K=StateCentralActionsCoordinator::Kind;if(node.kind<K::PartSelect||node.kind>K::PartReset)throw std::logic_error("Probe PartSelection coordinator kind differs from actual task");
  // Finished nodes are immutable outcome evidence, not active selectors.
  if(task==current||node.finished)partSelectionTask.sample(parent,unsigned(node.kind)-unsigned(K::PartSelect),*task,node.finished);
 }
 const auto&heavy=ui->heavy();play(std::get_if<StatePlaybackModeService>(&heavy));if(const auto*s=std::get_if<StatePartNavigationService>(&heavy))part.sample(parent,unsigned(s->kind()),s->components());browse(std::get_if<StateProgramBrowserService>(&heavy));if(const auto*s=std::get_if<StateStoreMenuService>(&heavy))play(s->playback());}if(const auto*t=m->current<StateTickService>())if(t->encoder())browse(t->encoder()->browser());if(const auto*t=m->current<ScheduledTimerService>())if(t->component()==ScheduledTimerService::Component::Browser)browse(t->task<StateProgramBrowserService>());}
 void write(std::ostream&o)const{playback.write(o,"native_main_playback_mode_");part.write(o,"native_main_part_navigation_");browser.write(o,"native_main_program_browser_");partSelectionTask.write(o);}
};
}
