#pragma once

#include "ksParameterIngress.h"
#include "ksGlobalState.h"
#include "ksCommandRouting.h"
#include <exception>

namespace novation::controller::command {
// Remaining musical policy fields, distinct from voice DSP parameters and FX.
// Raw packed bytes are serialized identities; expanded fields are projections.
struct KSParameterMetadata {
 uint64_t revision=0;
 uint8_t keyPolicy=0,lfoPolicy=0,noteAssignment=0,mixerReserved=0,fxReserved=0;
 uint8_t expressionController=1,serialized178=0,channel=0,lowKey=0,highKey=127,transpose=64,detune=64,velocityCurve=0;
 // Identified serialized payload fields. The bounded ordinary-voice corpus
 // shows no independent DSP effect for71/174/175; no formula is invented.
 uint8_t serialized71=0,serialized174=0,serialized175=0,uiLabel139=0,category176=0;
 uint8_t arpDivision=3,arpGate=64,arpMode=0,arpRhythm=0,arpVelocity=0;
 static KSParameterMetadata fromPatch(const KSVoiceImagePatch& patch)noexcept {
  return {0,patch[88],patch[122],patch[126],uint8_t(patch[127]&0xbc),uint8_t(patch[121]&0xe0),patch[177],patch[178],
   patch[181],patch[182],patch[183],patch[184],patch[185],patch[186],patch[71],patch[174],patch[175],patch[139],patch[176],
   patch[85],patch[86],patch[87],patch[149],patch[167]};
 }
 KSPartRoute route()const noexcept{return {uint8_t(channel<16?channel:KSPartRoute::NoChannel),lowKey,highKey};}
};
// Native ownership for the authenticated serialized system-setting range.
// Audible getters are named; lifecycle/UI-only bytes remain identified data,
// never firmware RAM or an executable handler table.
struct KSSystemSettings {
 Session session{};uint64_t revision=0;uint8_t tempo=0;
 std::array<uint8_t,22> settings{}; // serialized fields233..254;233 is opaque global storage.
 uint8_t opaque233()const noexcept{return settings[0];}
 uint8_t globalFine()const noexcept{return settings[244-233];}
 uint8_t velocityPower()const noexcept{return settings[245-233];}
 uint8_t midiSendMode()const noexcept{return settings[241-233];}
 uint8_t channel()const noexcept{return settings[242-233];}
 uint8_t clockSource()const noexcept{return settings[243-233];}
 uint8_t globalSync()const noexcept{return settings[249-233];}
 static KSSystemSettings fromPatch(Session session,const KSVoiceImagePatch& patch)noexcept {
  KSSystemSettings result;result.session=session;result.tempo=patch[84];
  std::copy_n(patch.begin()+233,22,result.settings.begin());return result;
 }
};
struct KSParameterPolicyChanges {
 KSParameterMetadata metadata;
 bool routing=false,notePolicy=false,expression=false,globalPitch=false,velocity=false,midiPolicy=false,clock=false,lifecycle=false;
};
inline void projectKsGlobals(KSVoiceImagePatch& patch,const KSGlobalPart& p,const KSGlobalMaster& master,uint8_t auxiliary)noexcept {
 patch[81]=p.centeredControls[0];patch[82]=p.centeredControls[1];patch[83]=p.centeredControls[2];patch[123]=p.externalFlags;
 patch[97]=p.distortionLevel;patch[98]=p.distortionWheel;patch[99]=p.distortionCompensation;
 patch[94]=p.eq.time;patch[95]=p.eq.sync;patch[96]=p.eqDepth;patch[93]=p.eqFrequency;patch[92]=p.eqShape;
 patch[112]=p.chorus.time;patch[113]=p.chorus.sync;patch[218]=p.chorusMode;patch[115]=p.chorusDepth;
 patch[116]=p.chorusCentre;patch[114]=p.chorusFeedback;patch[110]=p.chorusSend;patch[111]=p.chorusWheel;
 patch[107]=p.reverbSend;patch[108]=p.reverbWheel;patch[109]=p.reverbDecay;patch[217]=p.reverbType;
 patch[179]=p.partLevel;patch[180]=p.partOutput;patch[102]=p.delay.time;patch[103]=p.delay.sync;patch[106]=p.delayRatio;
 patch[100]=p.delaySend;patch[101]=p.delayWheel;patch[104]=p.delayFeedback;patch[105]=p.delayStereoWidth;
 patch[118]=p.pan.time;patch[119]=p.pan.sync;patch[120]=p.panDepth;patch[117]=p.panPosition;
 patch[246]=p.triggerToggle;patch[247]=p.triggerTime;patch[248]=p.triggerAmount;
 patch[89]=master.vocoderBalance;patch[90]=master.vocoderWidth;patch[91]=master.sibilanceLevel;
 patch[124]=master.sibilanceType;patch[208]=master.performanceEffectStatus;patch[225]=auxiliary;
}
inline std::optional<KSVoiceImagePatch> projectKsOwnedPatch(const KSVoiceParameterBank& bank,const KSGlobalState& globals,
 const KSMusicalState& model,KSStorageSlot part,const KSParameterMetadata& metadata,const KSSystemSettings& system,uint64_t rate,
 bool storedContext=false)noexcept {
 const auto dependency=storedContext?model.storedDependency(part,rate):model.dependency(part,rate);
 const auto musical=model.part(part);const auto* global=globals.desiredPart(part);
 const auto storedAuxiliary=globals.storedAuxiliary(part);
 if(!dependency || !musical || !global || !storedAuxiliary || model.session()!=globals.session() || model.session()!=system.session ||
    metadata.lowKey>=128 || metadata.highKey>=128 || global->reverbType>7 || global->chorusMode>3 || *storedAuxiliary>1)return std::nullopt;
 const auto observed=bank.observe(*dependency,musical->parameters);if(!observed)return std::nullopt;
 auto patch=observed->patch;projectKsGlobals(patch,*global,globals.desiredMaster(),*storedAuxiliary);
 patch[88]=metadata.keyPolicy;patch[122]=metadata.lfoPolicy;patch[126]=metadata.noteAssignment;
 patch[121]=uint8_t(metadata.fxReserved | (global->reverbType&7) | ((global->chorusMode&3)<<3));
 patch[127]=uint8_t(metadata.mixerReserved | (patch[222]&3) | ((*storedAuxiliary&1)<<6));
 patch[177]=metadata.expressionController;patch[178]=metadata.serialized178;patch[181]=metadata.channel;
 patch[182]=metadata.lowKey;patch[183]=metadata.highKey;patch[184]=metadata.transpose;patch[185]=metadata.detune;patch[186]=metadata.velocityCurve;
 patch[71]=metadata.serialized71;patch[174]=metadata.serialized174;patch[175]=metadata.serialized175;
 patch[139]=metadata.uiLabel139;patch[176]=metadata.category176;
 patch[85]=metadata.arpDivision;patch[86]=metadata.arpGate;patch[87]=metadata.arpMode;
 patch[149]=metadata.arpRhythm;patch[167]=metadata.arpVelocity;
 patch[84]=system.tempo;std::copy(system.settings.begin(),system.settings.end(),patch.begin()+233);
 // Trigger controls are FX ownership even though their serialized field IDs
 // share the high system-settings range.
 patch[246]=global->triggerToggle;patch[247]=global->triggerTime;patch[248]=global->triggerAmount;
 expandKsParameterFields(patch);return patch;
}
enum class KSParameterEditStatus { Prepared,Applied,Stale,Invalid,Unsupported,Busy,Exhausted };

class KSCommandParameterEdit final {
public:
 KSParameterEditStatus prepare(KSParameterIngress& ingress,KSVoiceParameterBank& bank,KSGlobalState& globals,KSMusicalState& model,
  KSParameterMetadata& metadata,KSSystemSettings& system,const KSParameterIngress::Plan& plan,uint64_t rate,
  const KSCommandEditorSelection* editorOwner=nullptr,std::optional<KSEditorSelection> editorSource=std::nullopt)noexcept {
  if(prepared_)return KSParameterEditStatus::Invalid;
  if(model.voiceOperationBusy())return KSParameterEditStatus::Busy;
  const bool storedContext=editorOwner || editorSource;
  if(storedContext && (!editorOwner || !editorSource || !editorOwner->current(*editorSource) ||
    editorSource->session!=model.session() || editorSource->layout!=model.layout() || editorSource->storage!=plan.dependency.part))
    return KSParameterEditStatus::Stale;
  const auto canonical=projectKsOwnedPatch(bank,globals,model,plan.dependency.part,metadata,system,rate,storedContext);
  const auto dependency=storedContext?model.storedDependency(plan.dependency.part,rate):model.dependency(plan.dependency.part,rate);
  if(!canonical || !dependency || !ingress.current(plan,*dependency,*canonical))return KSParameterEditStatus::Stale;
  const auto modelPart=model.part(plan.dependency.part);
  const auto bankSource=modelPart?bank.observe(*dependency,modelPart->parameters):std::nullopt;
  if(!bankSource)return KSParameterEditStatus::Stale;
  if(plan.action==KSParameterIngressAction::ReservedControl || plan.action==KSParameterIngressAction::ReservedPackedBoundary)
   return KSParameterEditStatus::Unsupported;
  auto after=*canonical;
  if(plan.action==KSParameterIngressAction::Patch) {
   if(!plan.editCount || plan.editCount>plan.edits.size())return KSParameterEditStatus::Invalid;
   std::bitset<256> seen;
   for(size_t i=0;i<plan.editCount;++i) {
    const auto edit=plan.edits[i];if(edit.field>=256 || seen.test(edit.field))return KSParameterEditStatus::Invalid;
    auto probe=metadata;
    if(!KSVoiceParameterBank::owns(edit.field) && edit.field!=46 && edit.field!=150 && !globalField(edit.field) &&
       !masterField(edit.field) && edit.field!=84 && !(edit.field>=233&&edit.field<=254) &&
       !(edit.field>=209&&edit.field<=227) && !metadataField(probe,edit.field,edit.value))
     return KSParameterEditStatus::Unsupported;
    seen.set(edit.field);after[edit.field]=edit.value;
   }
   if(after!=plan.after)return KSParameterEditStatus::Invalid;
   auto expanded=after;expandKsParameterFields(expanded);if(expanded!=after)return KSParameterEditStatus::Invalid;
  } else if(plan.action==KSParameterIngressAction::GlobalSetting) {
   if(plan.editCount || plan.before!=plan.after)return KSParameterEditStatus::Invalid;
   if(plan.field!=84 && !masterField(plan.field) && !(plan.field>=233&&plan.field<=254))return KSParameterEditStatus::Unsupported;
   after[plan.field]=plan.value;
  } else if(plan.before!=plan.after || plan.editCount)return KSParameterEditStatus::Invalid;
  const bool musical=plan.action==KSParameterIngressAction::Patch || plan.action==KSParameterIngressAction::GlobalSetting;
  if(musical && (!(storedContext?model.canTouchStoredParameters(plan.dependency.part):model.canTouchParameters(plan.dependency.part)) || metadata.revision==std::numeric_limits<uint64_t>::max() ||
     system.revision==std::numeric_limits<uint64_t>::max()))return KSParameterEditStatus::Exhausted;
  KSParameterMetadata nextMetadata=metadata;KSSystemSettings nextSystem=system;
  std::array<KSVoiceParameterEdit,256> voiceEdits{};size_t count=0;
  for(size_t field=0;field<256;++field)if(after[field]!=(*canonical)[field]) {
   if(KSVoiceParameterBank::owns(uint16_t(field)))voiceEdits[count++]={uint16_t(field),after[field]};
   else if(field==46 || field==150 || globalField(uint16_t(field)) || masterField(uint16_t(field))){}
   else if(field>=209 && field<=227){} // validated canonical derived projection
   else if(field==84)nextSystem.tempo=after[field];
   else if(field>=233 && field<=254)nextSystem.settings[field-233]=after[field];
   else if(!metadataField(nextMetadata,uint16_t(field),after[field]))return KSParameterEditStatus::Unsupported;
  }
  if(after[46]>=128 || after[150]>=128 || nextMetadata.lowKey>=128 || nextMetadata.highKey>=128)return KSParameterEditStatus::Invalid;
  const auto* sustain=model.sustain(plan.dependency.part);
  if(nextMetadata.keyPolicy!=metadata.keyPolicy && (!storedContext || sustain) &&
     (!model.sustain(plan.dependency.part) || model.sustain(plan.dependency.part)->revision==std::numeric_limits<uint64_t>::max()))
   return KSParameterEditStatus::Exhausted;
  const auto globalPart=KSGlobalPart::fromPatch(after);const auto master=KSGlobalMaster::fromPatch(after,globals.observedEffectStatus());
  KSGlobalEditSet globalEdit;globalEdit.source=plan.dependency.part;globalEdit.part=globalPart;globalEdit.master=master;globalEdit.storedAuxiliary=after[225];
  globalEdit.editorOwner=editorOwner;globalEdit.storedEditor=editorSource;
  const auto globalStatus=globals.canApply(globalEdit);
  if(globalStatus==KSGlobalEdit::Invalid)return KSParameterEditStatus::Invalid;
  if(globalStatus==KSGlobalEdit::Exhausted)return KSParameterEditStatus::Exhausted;
  std::optional<KSVoiceParameterBank::Edit> prepared;
  if(count){prepared=bank.prepare(*dependency,voiceEdits.data(),count);if(!prepared)return KSParameterEditStatus::Invalid;}
  ingress_=&ingress;bank_=&bank;globals_=&globals;model_=&model;metadata_=&metadata;system_=&system;plan_=plan;before_=*canonical;after_=after;
  dependency_=*dependency;metadataRevision_=metadata.revision;systemRevision_=system.revision;bankEdit_=prepared;globalEdit_=globalEdit;
  bankRevision_=bankSource->revision;
  nextMetadata_=nextMetadata;nextSystem_=nextSystem;musical_=musical;rate_=rate;prepared_=true;
  editorOwner_=editorOwner;editorSource_=editorSource;
  globalRevision_=*globals.sourceRevision(plan.dependency.part);
  changes_.metadata=nextMetadata;
  changes_.routing=metadata.channel!=nextMetadata.channel || metadata.lowKey!=nextMetadata.lowKey || metadata.highKey!=nextMetadata.highKey;
  changes_.notePolicy=metadata.keyPolicy!=nextMetadata.keyPolicy || metadata.noteAssignment!=nextMetadata.noteAssignment ||
    metadata.transpose!=nextMetadata.transpose || metadata.detune!=nextMetadata.detune ||
    (*canonical)[0]!=after[0] || (*canonical)[127]!=after[127];
  changes_.expression=metadata.expressionController!=nextMetadata.expressionController;
  changes_.globalPitch=system.globalFine()!=nextSystem.globalFine();
  changes_.velocity=metadata.velocityCurve!=nextMetadata.velocityCurve || system.velocityPower()!=nextSystem.velocityPower();
  changes_.midiPolicy=system.midiSendMode()!=nextSystem.midiSendMode() || system.channel()!=nextSystem.channel();
  changes_.clock=system.tempo!=nextSystem.tempo || system.clockSource()!=nextSystem.clockSource();
  changes_.lifecycle=nextSystem.settings!=system.settings || metadata.uiLabel139!=nextMetadata.uiLabel139 || metadata.category176!=nextMetadata.category176;
  return KSParameterEditStatus::Prepared;
 }
 KSParameterEditStatus commit(NativeFrame at,bool deferPatch=true)noexcept {
  if(!prepared_ || committed_)return KSParameterEditStatus::Stale;
  if(model_->voiceOperationBusy())return KSParameterEditStatus::Busy;
  const bool storedContext=editorOwner_!=nullptr;
  if(storedContext && (!editorSource_ || !editorOwner_->current(*editorSource_) || editorSource_->session!=model_->session() ||
    editorSource_->layout!=model_->layout() || editorSource_->storage!=dependency_.part))return KSParameterEditStatus::Stale;
  const auto canonical=projectKsOwnedPatch(*bank_,*globals_,*model_,dependency_.part,*metadata_,*system_,rate_,storedContext);
  const auto dependency=storedContext?model_->storedDependency(dependency_.part,rate_):model_->dependency(dependency_.part,rate_);
  if(!canonical || *canonical!=before_ || !dependency || !(*dependency==dependency_) || metadata_->revision!=metadataRevision_ ||
     system_->revision!=systemRevision_ || !ingress_->current(plan_,*dependency,*canonical))return KSParameterEditStatus::Stale;
  const auto modelPart=model_->part(dependency_.part);
  const auto bankSource=modelPart?bank_->observe(*dependency,modelPart->parameters):std::nullopt;
  if(!bankSource || bankSource->revision!=bankRevision_)return KSParameterEditStatus::Stale;
  const auto globalRevision=globals_->sourceRevision(dependency_.part);
  if(!globalRevision || !(*globalRevision==globalRevision_))return KSParameterEditStatus::Stale;
  if(!model_->canPublish(at) || (musical_&&!(storedContext?model_->canTouchStoredParameters(dependency_.part):
     model_->canTouchParameters(dependency_.part))))return KSParameterEditStatus::Exhausted;
  const auto* sustain=model_->sustain(dependency_.part);
  if(musical_ && nextMetadata_.keyPolicy!=metadata_->keyPolicy && (!storedContext || sustain) &&
     (!model_->sustain(dependency_.part) || model_->sustain(dependency_.part)->revision==std::numeric_limits<uint64_t>::max()))
   return KSParameterEditStatus::Exhausted;
  const auto globalStatus=globals_->canApply(globalEdit_);
  if(globalStatus==KSGlobalEdit::Invalid)return KSParameterEditStatus::Invalid;
  if(globalStatus==KSGlobalEdit::Exhausted)return KSParameterEditStatus::Exhausted;
  if(bankEdit_) {
   std::array<KSVoiceParameterEdit,256> edits{};size_t count=0;
   for(size_t f=0;f<256;++f)if(KSVoiceParameterBank::owns(uint16_t(f)) && before_[f]!=after_[f])edits[count++]={uint16_t(f),after_[f]};
   const auto current=bank_->prepare(*dependency,edits.data(),count);
   if(!current || current->revision!=bankEdit_->revision)return KSParameterEditStatus::Stale;
  }
  // One worker, no callbacks: all following mutations were prevalidated.
  if(musical_) {
   // The authenticated type>=126 path stores a setting and skips the
   // firmware's MarkDirty operation. Preserve the source barrier without
   // inventing an immediate full voice/global transfer for that event.
   if(storedContext?!model_->editStoredParameters(dependency_.part,{after_[46],after_[150]},false):
      !model_->editParameters(dependency_.part,{after_[46],after_[150]},deferPatch && plan_.action==KSParameterIngressAction::Patch))std::terminate();
   const auto afterDependency=*(storedContext?model_->storedDependency(dependency_.part,rate_):model_->dependency(dependency_.part,rate_));
   if(bankEdit_?!bank_->commit(*bankEdit_,*dependency,afterDependency):!bank_->rebase(*dependency,afterDependency))std::terminate();
   const auto applied=globals_->apply(globalEdit_);if(applied==KSGlobalEdit::Invalid || applied==KSGlobalEdit::Exhausted)std::terminate();
   if(nextMetadata_.keyPolicy!=metadata_->keyPolicy && sustain &&
      !model_->setSustainBypass(dependency_.part,bool(nextMetadata_.keyPolicy&4)))std::terminate();
   nextMetadata_.revision=metadataRevision_+1;*metadata_=nextMetadata_;
   nextSystem_.revision=systemRevision_+1;*system_=nextSystem_;
  }
  if(!ingress_->commit(plan_,*dependency,before_))std::terminate();
  if(musical_ && !model_->publish(at))std::terminate();
  committed_=true;return KSParameterEditStatus::Applied;
 }
 const KSParameterPolicyChanges& policy()const noexcept{return changes_;}
 const KSVoiceImagePatch& after()const noexcept{return after_;}
 void reset()noexcept{prepared_=committed_=false;bankEdit_.reset();editorOwner_=nullptr;editorSource_.reset();}
private:
 static bool globalField(uint16_t f)noexcept {
  return (f>=81&&f<=83) || (f>=92&&f<=120) || f==123 || f==179 || f==180 || f==217 || f==218 || (f>=246&&f<=248);
 }
 static bool masterField(uint16_t f)noexcept{return f==89 || f==90 || f==91 || f==124 || f==208;}
 static bool metadataField(KSParameterMetadata& target,uint16_t field,uint8_t value)noexcept {
  switch(field){case 88:target.keyPolicy=value;break;case 121:target.fxReserved=value&0xe0;break;
   case 85:if(value>15)return false;target.arpDivision=value;break;
   case 86:target.arpGate=value;break;
   case 87:if(value>7)return false;target.arpMode=value;break;
   case 149:if(value>32)return false;target.arpRhythm=value;break;
   case 167:target.arpVelocity=value;break;
   case 71:target.serialized71=value;break;case 174:target.serialized174=value;break;case 175:target.serialized175=value;break;
   case 139:target.uiLabel139=value;break;case 176:target.category176=value;break;
   case 122:target.lfoPolicy=value;break;case 126:target.noteAssignment=value;break;case 127:target.mixerReserved=value&0xbc;break;
   case 177:target.expressionController=value;break;case 178:target.serialized178=value;break;case 181:target.channel=value;break;
   case 182:target.lowKey=value;break;case 183:target.highKey=value;break;case 184:target.transpose=value;break;
   case 185:target.detune=value;break;case 186:target.velocityCurve=value;break;default:return false;}return true;
 }
 KSParameterIngress* ingress_=nullptr;KSVoiceParameterBank* bank_=nullptr;KSGlobalState* globals_=nullptr;KSMusicalState* model_=nullptr;
 KSParameterMetadata* metadata_=nullptr;KSSystemSettings* system_=nullptr;
 KSParameterIngress::Plan plan_{};KSVoiceImagePatch before_{},after_{};ParameterDependency<KSStorageSlot> dependency_{};
 uint64_t rate_=0,metadataRevision_=0,systemRevision_=0,bankRevision_=0;std::optional<KSVoiceParameterBank::Edit> bankEdit_;
 KSGlobalState::SourceRevision globalRevision_{};
 const KSCommandEditorSelection* editorOwner_=nullptr;std::optional<KSEditorSelection> editorSource_;
 KSGlobalEditSet globalEdit_;KSParameterMetadata nextMetadata_;KSSystemSettings nextSystem_;KSParameterPolicyChanges changes_;
 bool musical_=false,prepared_=false,committed_=false;
};
}
