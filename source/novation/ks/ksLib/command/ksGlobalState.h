#pragma once

#include "ksCommandGlobals.h"
#include "ksCommandFeedback.h"
#include "ksRefreshPacket.h"
#include "ksCommandEditorSelection.h"
#include <bitset>

namespace novation::controller::command {
enum class KSGlobalGroup:uint8_t { Input,Drive,Eq,Chorus,Reverb,Route,Delay,Pan,Trigger,Master,Count };
#define KS_GLOBAL_FIELDS(X) \
 X(CenteredOne,centeredControls[0],Input) X(CenteredTwo,centeredControls[1],Input) X(CenteredThree,centeredControls[2],Input) \
 X(ExternalFlags,externalFlags,Input) X(DistortionLevel,distortionLevel,Drive) X(DistortionWheel,distortionWheel,Drive) \
 X(DistortionCompensation,distortionCompensation,Drive) X(EqTime,eq.time,Eq) X(EqSync,eq.sync,Eq) X(EqDepth,eqDepth,Eq) \
 X(EqFrequency,eqFrequency,Eq) X(EqShape,eqShape,Eq) X(ChorusTime,chorus.time,Chorus) X(ChorusSync,chorus.sync,Chorus) \
 X(ChorusMode,chorusMode,Chorus) X(ChorusDepth,chorusDepth,Chorus) X(ChorusCentre,chorusCentre,Chorus) \
 X(ChorusFeedback,chorusFeedback,Chorus) X(ChorusSend,chorusSend,Chorus) X(ChorusWheel,chorusWheel,Chorus) \
 X(ReverbSend,reverbSend,Reverb) X(ReverbWheel,reverbWheel,Reverb) X(ReverbDecay,reverbDecay,Reverb) X(ReverbType,reverbType,Reverb) \
 X(PartLevel,partLevel,Route) X(PartOutput,partOutput,Route) X(DelayTime,delay.time,Delay) X(DelaySync,delay.sync,Delay) \
 X(DelayRatio,delayRatio,Delay) X(DelaySend,delaySend,Delay) X(DelayWheel,delayWheel,Delay) X(DelayFeedback,delayFeedback,Delay) \
 X(DelayStereoWidth,delayStereoWidth,Delay) X(PanTime,pan.time,Pan) X(PanSync,pan.sync,Pan) X(PanDepth,panDepth,Pan) \
 X(PanPosition,panPosition,Pan) X(TriggerToggle,triggerToggle,Trigger) X(TriggerTime,triggerTime,Trigger) X(TriggerAmount,triggerAmount,Trigger)
enum class KSGlobalField:uint8_t {
#define KS_GLOBAL_ENUM(name,member,group) name,
 KS_GLOBAL_FIELDS(KS_GLOBAL_ENUM)
#undef KS_GLOBAL_ENUM
 Count
};
enum class KSGlobalMasterField:uint8_t { VocoderBalance,VocoderWidth,SibilanceLevel,SibilanceType,PerformanceEffectStatus };
enum class KSGlobalEdit:uint8_t { Changed,Unchanged,Invalid,Exhausted };
struct KSGlobalEditSet {
 KSStorageSlot source=KSStorageSlot::Single;
 std::optional<KSGlobalPart> part;
 std::optional<KSGlobalMaster> master;
 std::optional<uint8_t> auxiliary;
 bool storedPart=false; // Performance's stored Single trigger fields only.
 const KSCommandEditorSelection* editorOwner=nullptr;
 std::optional<KSEditorSelection> storedEditor; // Explicit full stored-source capability.
 std::optional<uint8_t> storedAuxiliary; // Serialized225 of this storage source.
};

// Typed desired global/FX state. It owns no voice patch, firmware RAM or
// execution phases. The four physical feedback mailboxes have an explicit
// source mapping, including Single's inactive storage slots 1..3.
class KSGlobalState final {
public:
 static constexpr size_t GroupCount=size_t(KSGlobalGroup::Count);
 using Groups=std::bitset<GroupCount>;
 struct Snapshot {
  Session session;KSLayout layout;KSStorageSlot source;
  KSGlobalPart part;KSGlobalMaster master;KSGlobalTables tables;
  uint8_t auxiliary;
  uint64_t programGeneration,partRevision,masterRevision,tablesRevision,layoutRevision,auxRevision,effectRevision,ownerRevision;
  std::array<uint64_t,GroupCount> groupRevisions;
  Groups groups;
  bool includeAuxiliary;
  std::optional<KSPart> mailbox;
  uint64_t requestRevision;
  bool storedSingle=false;
  uint64_t storedRequestRevision=0;
 };
 struct SourceRevision {
  uint64_t program=0,part=0,master=0,tables=0,layout=0,auxiliary=0,storedAuxiliary=0;
  friend constexpr bool operator==(SourceRevision a,SourceRevision b)noexcept {
   return a.program==b.program && a.part==b.part && a.master==b.master && a.tables==b.tables &&
    a.layout==b.layout && a.auxiliary==b.auxiliary && a.storedAuxiliary==b.storedAuxiliary;
  }
 };

 KSGlobalState(Session session,KSLayout layout,const KSGlobalTables& tables,KSGlobalMaster master={},uint8_t auxiliary=0)noexcept
  :session_(session),layout_(layout),tables_(tables),master_(master),auxiliary_(auxiliary),observedEffect_(master.previousEffectStatus){}
 KSGlobalState(const KSGlobalState&)=delete;KSGlobalState& operator=(const KSGlobalState&)=delete;
 Session session()const noexcept{return session_;}
 KSLayout layout()const noexcept{return layout_;}
 uint64_t layoutRevision()const noexcept{return layoutRevision_;}
 std::optional<SourceRevision> sourceRevision(KSStorageSlot source)const noexcept {
  const auto index=size_t(source);if(index>=parts_.size() || !parts_[index].loaded)return std::nullopt;
  return SourceRevision{parts_[index].generation,parts_[index].revision,masterRevision_,tablesRevision_,layoutRevision_,auxRevision_,parts_[index].auxiliaryRevision};
 }
 const KSGlobalPart* desiredPart(KSStorageSlot source)const noexcept {
  return size_t(source)<parts_.size()&&parts_[size_t(source)].loaded?&parts_[size_t(source)].part:nullptr;
 }
 KSGlobalMaster desiredMaster()const noexcept{auto master=master_;master.previousEffectStatus=observedEffect_;return master;}
 uint8_t observedEffectStatus()const noexcept{return observedEffect_;}
 uint8_t auxiliary()const noexcept{return auxiliary_;}
 std::optional<uint8_t> storedAuxiliary(KSStorageSlot source)const noexcept {
  const auto index=size_t(source);return index<parts_.size() && parts_[index].loaded?std::optional{parts_[index].auxiliary}:std::nullopt;
 }
 // Sample the explicitly selected editor's per-storage225 at the actual
 // auxiliary packet boundary. This never changes any stored patch identity.
 KSGlobalEdit selectAuxiliary(const KSCommandEditorSelection& editor,const KSEditorSelection& source)noexcept {
  if(!editor.current(source) || source.session!=session_ || source.layout!=layout_)return KSGlobalEdit::Invalid;
  const auto value=storedAuxiliary(source.storage);return value?editAuxiliary(*value):KSGlobalEdit::Invalid;
 }
 bool validAuxiliary()const noexcept{return auxiliary_<=1;}
 uint64_t auxiliaryRevision()const noexcept{return auxRevision_;}
 // Called by the separate B8045 owner only after its real transfer drains.
 // Acknowledge that auxiliary generation, never a part FX application.
 bool acknowledgeAuxiliary(uint64_t revision,uint8_t value)noexcept {
  if(revision!=auxRevision_ || value!=auxiliary_)return false;
  auxDirty_=false;return true;
 }
 static constexpr std::optional<KSStorageSlot> mailboxSource(KSLayout layout,KSPart mailbox)noexcept {
  return size_t(mailbox)<4?std::optional{KSStorageSlot(size_t(mailbox)+(layout==KSLayout::Performance?1:0))}:std::nullopt;
 }
 static constexpr KSGlobalGroup group(KSGlobalField field)noexcept {
  switch(field){
#define KS_GLOBAL_GROUP(name,member,g) case KSGlobalField::name:return KSGlobalGroup::g;
   KS_GLOBAL_FIELDS(KS_GLOBAL_GROUP)
#undef KS_GLOBAL_GROUP
   default:return KSGlobalGroup::Count;
  }
 }
 static constexpr std::optional<uint8_t> value(const KSGlobalPart& part,KSGlobalField field)noexcept {
  switch(field){
#define KS_GLOBAL_READ(name,member,g) case KSGlobalField::name:return part.member;
   KS_GLOBAL_FIELDS(KS_GLOBAL_READ)
#undef KS_GLOBAL_READ
   default:return std::nullopt;
  }
 }
 bool loadPart(KSStorageSlot source,const KSGlobalPart& part)noexcept {
  const auto i=size_t(source);if(!validAuxiliary() || i>=parts_.size() || !valid(part) || !room(parts_[i].generation) || !room(parts_[i].revision))return false;
  auto& slot=parts_[i];slot.part=part;slot.loaded=true;++slot.generation;++slot.revision;slot.dirty.set();slot.groupRevisions.fill(slot.revision);return true;
 }
 bool loadPart(KSStorageSlot source,const KSGlobalPart& part,uint8_t auxiliary)noexcept {
  const auto index=size_t(source);
  if(auxiliary>1 || index>=parts_.size() || !room(parts_[index].auxiliaryRevision) || !loadPart(source,part))return false;
  parts_[index].auxiliary=auxiliary;++parts_[index].auxiliaryRevision;return true;
 }
 KSGlobalEdit edit(KSStorageSlot source,KSGlobalField field,uint8_t next)noexcept {
  const auto i=size_t(source);if(!active(source) || !parts_[i].loaded)return KSGlobalEdit::Invalid;
  const auto old=value(parts_[i].part,field);if(!old)return KSGlobalEdit::Invalid;if(*old==next)return KSGlobalEdit::Unchanged;
  auto candidate=parts_[i].part;
  switch(field){
#define KS_GLOBAL_WRITE(name,member,g) case KSGlobalField::name:candidate.member=next;break;
   KS_GLOBAL_FIELDS(KS_GLOBAL_WRITE)
#undef KS_GLOBAL_WRITE
   default:return KSGlobalEdit::Invalid;
  }
  if(!valid(candidate))return KSGlobalEdit::Invalid;
  auto groups=Groups{}.set(size_t(group(field)));
  // One verified source flag controls both external audio and the trigger tail.
  if(field==KSGlobalField::ExternalFlags)groups.set(size_t(KSGlobalGroup::Trigger));
  return change(source,candidate,groups);
 }
 KSGlobalEdit editMaster(KSGlobalMasterField field,uint8_t next)noexcept {
  auto candidate=master_;uint8_t* target=nullptr;
  switch(field){case KSGlobalMasterField::VocoderBalance:target=&candidate.vocoderBalance;break;
   case KSGlobalMasterField::VocoderWidth:target=&candidate.vocoderWidth;break;
   case KSGlobalMasterField::SibilanceLevel:target=&candidate.sibilanceLevel;break;
   case KSGlobalMasterField::SibilanceType:target=&candidate.sibilanceType;break;
   case KSGlobalMasterField::PerformanceEffectStatus:target=&candidate.performanceEffectStatus;break;default:return KSGlobalEdit::Invalid;}
  if(*target==next)return KSGlobalEdit::Unchanged;
  if(!room(masterRevision_))return KSGlobalEdit::Exhausted;
  for(const auto& p:parts_)if(p.loaded && !room(p.revision))return KSGlobalEdit::Exhausted;
  *target=next;master_=candidate;++masterRevision_;
  for(auto& p:parts_)if(p.loaded){++p.revision;p.dirty.set(size_t(KSGlobalGroup::Master));p.groupRevisions[size_t(KSGlobalGroup::Master)]=p.revision;}
  return KSGlobalEdit::Changed;
 }
 KSGlobalEdit editAuxiliary(uint8_t value)noexcept {
  if(!validAuxiliary() || value>1)return KSGlobalEdit::Invalid;
  if(auxiliary_==value)return KSGlobalEdit::Unchanged;if(!room(auxRevision_))return KSGlobalEdit::Exhausted;
  auxiliary_=value;++auxRevision_;auxDirty_=true;return KSGlobalEdit::Changed;
 }
 // A complete host/model event may edit part FX, common master and auxiliary
 // together. Validate every source/domain/counter before the first mutation.
 KSGlobalEdit canApply(const KSGlobalEditSet& edit)const noexcept {
  if(!validAuxiliary() || (edit.auxiliary && *edit.auxiliary>1))return KSGlobalEdit::Invalid;
  if(!validEditTarget(edit))return KSGlobalEdit::Invalid;
  std::array<bool,5> changed{};
  if(edit.part)for(size_t i=0;i<size_t(KSGlobalField::Count);++i)
   if(value(parts_[size_t(edit.source)].part,KSGlobalField(i))!=value(*edit.part,KSGlobalField(i)))changed[size_t(edit.source)]=true;
  const bool masterChanged=edit.master &&
   (edit.master->vocoderBalance!=master_.vocoderBalance || edit.master->vocoderWidth!=master_.vocoderWidth ||
    edit.master->sibilanceLevel!=master_.sibilanceLevel || edit.master->sibilanceType!=master_.sibilanceType ||
    edit.master->performanceEffectStatus!=master_.performanceEffectStatus);
  const bool auxiliaryChanged=edit.auxiliary && *edit.auxiliary!=auxiliary_;
  const bool storedChanged=edit.storedAuxiliary && *edit.storedAuxiliary!=parts_[size_t(edit.source)].auxiliary;
  if(storedChanged && !room(parts_[size_t(edit.source)].auxiliaryRevision))return KSGlobalEdit::Exhausted;
  if((masterChanged&&!room(masterRevision_)) || (auxiliaryChanged&&!room(auxRevision_)))return KSGlobalEdit::Exhausted;
  bool any=masterChanged||auxiliaryChanged||storedChanged;
  for(size_t i=0;i<parts_.size();++i)if(changed[i] || (masterChanged&&parts_[i].loaded)) {
   any=true;if(!room(parts_[i].revision))return KSGlobalEdit::Exhausted;
  }
  return any?KSGlobalEdit::Changed:KSGlobalEdit::Unchanged;
 }
 KSGlobalEdit apply(const KSGlobalEditSet& edit)noexcept {
  if(!validAuxiliary() || (edit.auxiliary && *edit.auxiliary>1))return KSGlobalEdit::Invalid;
  if(!validEditTarget(edit))return KSGlobalEdit::Invalid;
  std::array<Groups,5> changed{};
  if(edit.part) {
   const auto& before=parts_[size_t(edit.source)].part;
   for(size_t i=0;i<size_t(KSGlobalField::Count);++i) {
    const auto field=KSGlobalField(i);
    if(value(before,field)!=value(*edit.part,field)) {
     changed[size_t(edit.source)].set(size_t(group(field)));
     if(field==KSGlobalField::ExternalFlags)changed[size_t(edit.source)].set(size_t(KSGlobalGroup::Trigger));
    }
   }
  }
  const bool masterChanged=edit.master &&
   (edit.master->vocoderBalance!=master_.vocoderBalance || edit.master->vocoderWidth!=master_.vocoderWidth ||
    edit.master->sibilanceLevel!=master_.sibilanceLevel || edit.master->sibilanceType!=master_.sibilanceType ||
    edit.master->performanceEffectStatus!=master_.performanceEffectStatus);
  const bool auxiliaryChanged=edit.auxiliary && *edit.auxiliary!=auxiliary_;
  const bool storedChanged=edit.storedAuxiliary && *edit.storedAuxiliary!=parts_[size_t(edit.source)].auxiliary;
  if(storedChanged && !room(parts_[size_t(edit.source)].auxiliaryRevision))return KSGlobalEdit::Exhausted;
  if(masterChanged)for(size_t i=0;i<parts_.size();++i)if(parts_[i].loaded)changed[i].set(size_t(KSGlobalGroup::Master));
  if((masterChanged&&!room(masterRevision_)) || (auxiliaryChanged&&!room(auxRevision_)))return KSGlobalEdit::Exhausted;
  bool any=masterChanged||auxiliaryChanged||storedChanged;
  for(size_t i=0;i<parts_.size();++i)if(changed[i].any()) {
   any=true;if(!room(parts_[i].revision))return KSGlobalEdit::Exhausted;
  }
  if(!any)return KSGlobalEdit::Unchanged;
  if(edit.part)parts_[size_t(edit.source)].part=*edit.part;
  if(masterChanged) {
   const auto previous=master_.previousEffectStatus;master_=*edit.master;master_.previousEffectStatus=previous;++masterRevision_;
  }
  if(auxiliaryChanged){auxiliary_=*edit.auxiliary;++auxRevision_;auxDirty_=true;}
  if(storedChanged){auto& source=parts_[size_t(edit.source)];source.auxiliary=*edit.storedAuxiliary;++source.auxiliaryRevision;}
  for(size_t i=0;i<parts_.size();++i)if(changed[i].any()) {
   auto& part=parts_[i];++part.revision;part.dirty|=changed[i];
   for(size_t group=0;group<GroupCount;++group)if(changed[i][group])part.groupRevisions[group]=part.revision;
  }
  return KSGlobalEdit::Changed;
 }
 bool changeLayout(KSLayout layout)noexcept {
  if(layout!=KSLayout::Single && layout!=KSLayout::Performance)return false;
  if(layout==layout_)return true;if(!room(layoutRevision_))return false;
  for(const auto& p:parts_)if(p.loaded&&!room(p.revision))return false;
  layout_=layout;++layoutRevision_;
  for(auto& p:parts_)if(p.loaded){++p.revision;p.dirty.set();p.groupRevisions.fill(p.revision);}return true;
 }
 // The scene owner has read back the mailboxes after sending all FX and
 // routing writes. Retire the same fade requests in the semantic owner.
 bool acknowledgeScene(const std::bitset<5>& refreshed,uint8_t effect,uint8_t mailboxMask=15)noexcept {
  if(busy() || !room(effectRevision_))return false;
  for(size_t i=0;i<requests_.size();++i)if(mailboxMask&(1u<<i))acknowledged_[i]=requests_[i];
  observedEffect_=effect;++effectRevision_;
  for(size_t i=0;i<parts_.size();++i)if(refreshed.test(i)) {
   parts_[i].applied=parts_[i].revision;parts_[i].dirty.reset();
  }
  return true;
 }
 bool observePart(KSPart mailbox,uint32_t word)noexcept {
  if(size_t(mailbox)>=requests_.size() || word>0xffffff)return false;
  if(KSCommandFeedback::partResult(word).action==KSPartFeedbackAction::Continue)return true;
  auto& request=requests_[size_t(mailbox)];return advanceRevision(request);
 }
 bool rebuildPending(KSPart mailbox)const noexcept {
  return size_t(mailbox)<4 && requests_[size_t(mailbox)]!=acknowledged_[size_t(mailbox)];
 }
 bool acknowledgeInactiveSingleMailbox(KSPart mailbox)noexcept {
  if(layout_!=KSLayout::Single || mailbox==KSPart::One || busy() || !rebuildPending(mailbox))return false;
  const auto i=size_t(mailbox);
  acknowledged_[i]=requests_[i];
  return true;
 }
 Groups dirty(KSStorageSlot source)const noexcept{return size_t(source)<parts_.size()?parts_[size_t(source)].dirty:Groups{};}
 bool canRequireRefresh(KSStorageSlot source)const noexcept {
  return active(source) && parts_[size_t(source)].loaded && room(parts_[size_t(source)].revision);
 }
 bool requireRefresh(KSStorageSlot source)noexcept {
  const auto i=size_t(source);if(!canRequireRefresh(source))return false;
  auto& part=parts_[i];++part.revision;part.dirty.set();part.groupRevisions.fill(part.revision);return true;
 }
 bool canRequireStoredSingleRefresh()const noexcept {
  return layout_==KSLayout::Performance && parts_[0].loaded && room(storedRequests_);
 }
 bool requireStoredSingleRefresh()noexcept {
  if(!canRequireStoredSingleRefresh())return false;
  ++storedRequests_;return true;
 }
 bool storedSingleRefreshPending()const noexcept{return storedRequests_!=storedAcknowledged_;}
 uint64_t appliedRevision(KSStorageSlot source)const noexcept{return size_t(source)<parts_.size()?parts_[size_t(source)].applied:0;}
 bool busy()const noexcept{return owner_!=nullptr;}
 // A caller may preflight a whole voice/configuration/global intent before
 // publishing its controller-only configuration or starting voice writes.
 bool canPrepareRefresh(KSStorageSlot source,bool storedSingle=false)const noexcept {
  const auto i=size_t(source);
  return i<parts_.size() && parts_[i].loaded && !busy() && validAuxiliary() && tables_.timingRate &&
   valid(parts_[i].part) && room(effectRevision_) && ownerRevision_<std::numeric_limits<uint64_t>::max()-1 &&
   (storedSingle?source==KSStorageSlot::Single && canRequireStoredSingleRefresh():canRequireRefresh(source));
 }
 std::optional<Snapshot> snapshotStoredSingle()const noexcept {return snapshot(KSStorageSlot::Single,{},true);}
 std::optional<Snapshot> snapshot(KSStorageSlot source,std::optional<KSPart> mailbox={},bool storedSingle=false)const noexcept {
  const auto i=size_t(source);if(!validAuxiliary() || i>=parts_.size() || !parts_[i].loaded || !tables_.timingRate || !valid(parts_[i].part) ||
     (layout_!=KSLayout::Single && layout_!=KSLayout::Performance) ||
     (storedSingle && (layout_!=KSLayout::Performance || source!=KSStorageSlot::Single || mailbox || !storedSingleRefreshPending())) ||
     (mailbox && ((layout_==KSLayout::Single && *mailbox!=KSPart::One) ||
                  mailboxSource(layout_,*mailbox)!=source || !rebuildPending(*mailbox))) ||
     (!mailbox && !storedSingle && (!active(source) || (!parts_[i].dirty.any() && !auxDirty_))) ||
     !room(effectRevision_) || ownerRevision_>=std::numeric_limits<uint64_t>::max()-1)return std::nullopt;
  auto master=master_;master.previousEffectStatus=observedEffect_;
  return Snapshot{session_,layout_,source,parts_[i].part,master,tables_,auxiliary_,parts_[i].generation,parts_[i].revision,
   masterRevision_,tablesRevision_,layoutRevision_,auxRevision_,effectRevision_,ownerRevision_,parts_[i].groupRevisions,
   parts_[i].dirty,auxDirty_&&!mailbox&&!storedSingle,mailbox,mailbox?requests_[size_t(*mailbox)]:0,storedSingle,storedSingle?storedRequests_:0};
 }
 bool current(const Snapshot& s,bool accepted,const void* owner)const noexcept {
  const auto i=size_t(s.source);
  if(!(s.session==session_) || s.layout!=layout_ || i>=parts_.size() || !parts_[i].loaded ||
     (s.storedSingle && (layout_!=KSLayout::Performance || s.source!=KSStorageSlot::Single || s.mailbox)) ||
     s.programGeneration!=parts_[i].generation || s.layoutRevision!=layoutRevision_ || s.tablesRevision!=tablesRevision_ ||
     ownerRevision_!=s.ownerRevision+(accepted?1:0) || (accepted?owner_!=owner:busy()))return false;
  if(!accepted && ownerRevision_>=std::numeric_limits<uint64_t>::max()-1)return false;
  return accepted || (s.partRevision==parts_[i].revision && s.masterRevision==masterRevision_ &&
   s.auxRevision==auxRevision_ && s.effectRevision==effectRevision_ &&
   s.includeAuxiliary==(auxDirty_&&!s.mailbox&&!s.storedSingle) &&
   (!s.storedSingle || s.storedRequestRevision==storedRequests_) &&
   (!s.mailbox || s.requestRevision==requests_[size_t(*s.mailbox)]));
 }
 bool claim(const Snapshot& s,const void* owner)noexcept {
  if(!owner || !current(s,false,owner))return false;owner_=owner;++ownerRevision_;return true;
 }
 bool complete(const Snapshot& s,const void* owner)noexcept {
  if(!current(s,true,owner) || !room(effectRevision_))return false;
  auto& part=parts_[size_t(s.source)];
  for(size_t g=0;g<GroupCount;++g)if(s.groups[g] && part.groupRevisions[g]==s.groupRevisions[g])part.dirty.reset(g);
  part.applied=s.partRevision;
  if(s.includeAuxiliary && s.auxRevision==auxRevision_)auxDirty_=false;
  if(s.mailbox)acknowledged_[size_t(*s.mailbox)]=s.requestRevision;
  if(s.storedSingle)storedAcknowledged_=s.storedRequestRevision;
  observedEffect_=(s.layout==KSLayout::Single || s.storedSingle)?0:s.master.performanceEffectStatus;++effectRevision_;
  owner_=nullptr;++ownerRevision_;return true;
 }
 bool abandon(const void* owner)noexcept {
  if(!owner || owner_!=owner)return false;owner_=nullptr;++ownerRevision_;return true;
 }
private:
 bool validEditTarget(const KSGlobalEditSet& edit)const noexcept {
  if(!edit.part && !edit.storedAuxiliary)return true;
  const auto* before=desiredPart(edit.source);
  if(!before || (edit.part && !valid(*edit.part)) || (edit.storedAuxiliary && *edit.storedAuxiliary>1))return false;
  if(active(edit.source))return true;
  if(edit.editorOwner && edit.storedEditor && layout_==KSLayout::Performance && edit.source==KSStorageSlot::Single &&
    edit.editorOwner->current(*edit.storedEditor) && edit.storedEditor->session==session_ &&
    edit.storedEditor->layout==layout_ && edit.storedEditor->storage==edit.source)return true;
  if(!edit.part || edit.storedAuxiliary || !edit.storedPart || layout_!=KSLayout::Performance || edit.source!=KSStorageSlot::Single)return false;
  for(size_t i=0;i<size_t(KSGlobalField::Count);++i)if(group(KSGlobalField(i))!=KSGlobalGroup::Trigger &&
   value(*before,KSGlobalField(i))!=value(*edit.part,KSGlobalField(i)))return false;
  return true;
 }
 struct Part {KSGlobalPart part{};bool loaded=false;uint8_t auxiliary=0;uint64_t generation=0,revision=0,applied=0,auxiliaryRevision=0;Groups dirty;
              std::array<uint64_t,GroupCount> groupRevisions{};};
 static bool room(uint64_t revision)noexcept{return revision!=std::numeric_limits<uint64_t>::max();}
 bool active(KSStorageSlot source)const noexcept{return layout_==KSLayout::Single?source==KSStorageSlot::Single:size_t(source)>0&&size_t(source)<5;}
 bool valid(const KSGlobalPart& p)const noexcept {
  return p.eq.sync<=tables_.reciprocalTimes.size() && p.chorus.sync<=tables_.reciprocalTimes.size() &&
   p.pan.sync<=tables_.reciprocalTimes.size() && p.delay.sync<=tables_.delayTimes.size() && p.delayRatio<tables_.delayRatios.size();
 }
 KSGlobalEdit change(KSStorageSlot source,const KSGlobalPart& value,Groups groups)noexcept {
  auto& part=parts_[size_t(source)];if(!room(part.revision))return KSGlobalEdit::Exhausted;
  part.part=value;++part.revision;part.dirty|=groups;
  for(size_t g=0;g<GroupCount;++g)if(groups[g])part.groupRevisions[g]=part.revision;
  return KSGlobalEdit::Changed;
 }
 Session session_;KSLayout layout_;KSGlobalTables tables_;KSGlobalMaster master_;uint8_t auxiliary_=0,observedEffect_=0;
 uint64_t masterRevision_=0,tablesRevision_=0,layoutRevision_=0,auxRevision_=0,effectRevision_=0,ownerRevision_=0;
 bool auxDirty_=false;const void* owner_=nullptr;
 std::array<Part,5> parts_{};std::array<uint64_t,4> requests_{},acknowledged_{};
 uint64_t storedRequests_=0,storedAcknowledged_=0;
};
#undef KS_GLOBAL_FIELDS
}
