#pragma once

#include "ksCommandParameterEdit.h"
#include "ksCommandEditorSelection.h"

namespace novation::controller::command {
enum class KSGlobalFallbackAction { Ignore,Selector,Bank,GlobalSetting,ContextRequired,ReservedPackedBoundary };
struct KSGlobalFallbackControl {
 KSGlobalFallbackAction action=KSGlobalFallbackAction::Ignore;
 uint8_t parameter=0,field=0,value=0;
};
// Performance's mirrored control path invokes the generic parameter codec,
// not the part's expression/pedal/panic dispatcher. Inhibited ordinary patch
// edits have no musical side effect. Packed controls require an explicit,
// versioned editor source independent of the MIDI recipient. preparePacked
// supplies that contract; this decoder alone never infers a storage target.
inline KSGlobalFallbackControl decodeKsGlobalFallbackControl(const KSParameterMapTables& tables,
 uint8_t selector,ControlChange input)noexcept {
 KSGlobalFallbackControl result;result.value=input.value;
 if(input.controller==32) {if(input.value&0x60)result.action=KSGlobalFallbackAction::Bank;return result;}
 if(input.controller==98){result.action=KSGlobalFallbackAction::Selector;return result;}
 const bool publicParameter=input.controller==6;
 if(publicParameter && selector>=128)return result;
 result.parameter=publicParameter?selector:input.controller;
 if(input.controller==122 && input.value!=63)result.value=input.value<63?0:127;
 if(publicParameter && result.parameter>=104 && result.parameter<=107) {
  const auto& descriptor=tables.packed[result.parameter-104];
  const unsigned selected=descriptor.rightShift<8?input.value>>descriptor.rightShift:0;
  if(selected>descriptor.limit)return result;
  if(selected==descriptor.limit){result.action=KSGlobalFallbackAction::ReservedPackedBoundary;return result;}
  if(selected/2>=descriptor.pairCount){result.action=KSGlobalFallbackAction::ContextRequired;return result;}
  result.field=descriptor.pairs[selected/2].field;
  result.action=KSGlobalFallbackAction::ContextRequired;return result;
 }
 result.field=tables.field[uint8_t(result.parameter+(publicParameter?128:0))];
 const auto type=tables.type[result.field];
 if(type>=126)result.action=KSGlobalFallbackAction::GlobalSetting;
 return result;
}
enum class KSGlobalFallbackStatus { Applied,Ignored,ContextRequired,ReservedPackedBoundary,Invalid,Exhausted };
struct KSGlobalFallbackOutcome {
 KSGlobalFallbackStatus status=KSGlobalFallbackStatus::Invalid;
 KSGlobalFallbackControl control{};
 bool globalDirty=false,clock=false,globalPitch=false,velocity=false,midiPolicy=false,lifecycle=false;
 bool auxiliaryNotify=false;
};
// A typed P09 program-loading request. This is a storage selection, never a
// fifth simultaneous musical part or a fabricated active-part dependency.
struct KSGlobalProgramRequest {
 Session session{};uint64_t sourceRevision=0;
 KSStorageSlot storage=KSStorageSlot::Single;
 uint8_t bank=255,program=0;
};

class KSCommandGlobalFallback final {
public:
 struct PackedEdit {
  Session session{};uint64_t revision=0;uint8_t selector=255;
  KSEditorSelection editor{};KSParameterIngress::Plan plan{};
  // Legacy mirrored ingress marks routed storage0, independently of the
  // editor source. This is a stored Single refresh, not a fifth musical part.
  KSStorageSlot refreshStorage=KSStorageSlot::Single;
  const KSCommandEditorSelection* editorOwner=nullptr;
 };
 KSCommandGlobalFallback(Session session,const KSParameterMapTables& tables,uint8_t selector=255,uint8_t bank=255)noexcept
  :session_(session),tables_(tables),selector_(selector),bank_(bank){}
 KSCommandGlobalFallback(const KSCommandGlobalFallback&)=delete;
 KSCommandGlobalFallback& operator=(const KSCommandGlobalFallback&)=delete;
 uint8_t selector()const noexcept{return selector_;}
 uint8_t bank()const noexcept{return bank_;}
 bool restoreColdSelector(uint8_t selector)noexcept {
  if(revision_ || (selector>=128 && selector!=255))return false;
  selector_=selector;
  return true;
 }
 std::optional<PackedEdit> preparePacked(const KSCommandEditorSelection& editor,const KSParameterIngress& ingress,
     ParameterDependency<KSStorageSlot> dependency,const KSVoiceImagePatch& patch,ControlChange input)const noexcept {
  const auto target=editor.projection();
  if(!target || target->session!=session_ || target->layout!=KSLayout::Performance || target->storage!=dependency.part ||
      dependency.session!=session_ || !room(revision_))return std::nullopt;
  const auto decoded=decodeKsGlobalFallbackControl(tables_,selector_,input);
  if(decoded.action!=KSGlobalFallbackAction::ContextRequired)return std::nullopt;
  const auto plan=ingress.prepareWithSelector(dependency,patch,input,selector_);
  if(!plan || plan->action!=KSParameterIngressAction::Patch)return std::nullopt;
  return PackedEdit{session_,revision_,selector_,*target,*plan,KSStorageSlot::Single,&editor};
 }
 bool currentPacked(const PackedEdit& edit,const KSCommandEditorSelection& editor,const KSParameterIngress& ingress,
     ParameterDependency<KSStorageSlot> dependency,const KSVoiceImagePatch& patch)const noexcept {
  return edit.session==session_ && edit.revision==revision_ && edit.selector==selector_ && room(revision_) &&
      edit.editorOwner==&editor && editor.current(edit.editor) && edit.editor.storage==dependency.part && ingress.current(edit.plan,dependency,patch);
 }
 // Caller prevalidates currentPacked and the complete target edit before any
 // publication, then commits the target aggregate and this receipt on one
 // worker without callbacks. A queue Full must retain the prepared receipt.
 bool commitPacked(const PackedEdit& edit)noexcept {
  if(edit.session!=session_ || edit.revision!=revision_ || edit.selector!=selector_ || !room(revision_) ||
      !edit.editorOwner || !edit.editorOwner->current(edit.editor))return false;
  ++revision_;return true;
 }
 uint64_t revision()const noexcept{return revision_;}
 KSGlobalFallbackOutcome applyControl(KSGlobalState& globals,KSSystemSettings& system,ControlChange input)noexcept {
  KSGlobalFallbackOutcome result;
  if(!valid(globals,system) || input.channel>=16 || input.controller>=128 || input.value>=128)return result;
  result.control=decodeKsGlobalFallbackControl(tables_,selector_,input);
  const auto action=result.control.action;
  if(action==KSGlobalFallbackAction::Ignore){result.status=KSGlobalFallbackStatus::Ignored;return result;}
  if(action==KSGlobalFallbackAction::ContextRequired){result.status=KSGlobalFallbackStatus::ContextRequired;return result;}
  if(action==KSGlobalFallbackAction::ReservedPackedBoundary){result.status=KSGlobalFallbackStatus::ReservedPackedBoundary;return result;}
  if(!room(revision_)){result.status=KSGlobalFallbackStatus::Exhausted;return result;}
  KSGlobalEditSet globalEdit;auto nextSystem=system;bool systemWrite=false;
  if(action==KSGlobalFallbackAction::GlobalSetting) {
   const auto field=result.control.field,value=result.control.value;
   auto master=globals.desiredMaster();
   switch(field) {
    case 89:master.vocoderBalance=value;globalEdit.master=master;break;
    case 90:master.vocoderWidth=value;globalEdit.master=master;break;
    case 91:master.sibilanceLevel=value;globalEdit.master=master;break;
    case 124:master.sibilanceType=value;globalEdit.master=master;break;
    case 208:master.performanceEffectStatus=value;globalEdit.master=master;break;
    case 84:nextSystem.tempo=value;systemWrite=true;break;
    case 246:case 247:case 248: {
     const auto* stored=globals.desiredPart(KSStorageSlot::Single);if(!stored)return result;
     auto candidate=*stored;
     if(field==246)candidate.triggerToggle=value;
     if(field==247)candidate.triggerTime=value;
     if(field==248)candidate.triggerAmount=value;
     globalEdit.source=KSStorageSlot::Single;globalEdit.part=candidate;globalEdit.storedPart=true;break;
    }
    default:
     if(field<233 || field>254)return result;
     nextSystem.settings[field-233]=value;systemWrite=true;break;
   }
  }
  if(systemWrite && !room(system.revision)){result.status=KSGlobalFallbackStatus::Exhausted;return result;}
  const auto globalStatus=globals.canApply(globalEdit);
  if(globalStatus==KSGlobalEdit::Invalid)return result;
  if(globalStatus==KSGlobalEdit::Exhausted){result.status=KSGlobalFallbackStatus::Exhausted;return result;}
  // The worker owns this entire semantic event; no callbacks occur between
  // preflight and these noexcept commits.
  const auto applied=globals.apply(globalEdit);
  if(applied==KSGlobalEdit::Invalid || applied==KSGlobalEdit::Exhausted)std::terminate();
  if(systemWrite) {
   result.clock=nextSystem.tempo!=system.tempo || nextSystem.clockSource()!=system.clockSource();
   result.globalPitch=nextSystem.globalFine()!=system.globalFine();
   result.velocity=nextSystem.velocityPower()!=system.velocityPower();
   result.midiPolicy=nextSystem.midiSendMode()!=system.midiSendMode() || nextSystem.channel()!=system.channel();
   result.lifecycle=nextSystem.settings!=system.settings;
   ++nextSystem.revision;system=nextSystem;
  }
  if(action==KSGlobalFallbackAction::Selector)selector_=input.value;
  if(action==KSGlobalFallbackAction::Bank)bank_=input.value;
  ++revision_;result.globalDirty=applied==KSGlobalEdit::Changed;
  result.auxiliaryNotify=action==KSGlobalFallbackAction::GlobalSetting;
  result.status=KSGlobalFallbackStatus::Applied;return result;
 }
 std::optional<KSGlobalProgramRequest> program(Session session,KSLayout layout,ProgramChange input)noexcept {
  if(session!=session_ || layout!=KSLayout::Performance || input.channel>=16 || input.program>=128 || bank_>=128 || !room(revision_))return std::nullopt;
  const auto selected=bank_;bank_=255;++revision_;
  return KSGlobalProgramRequest{session_,revision_,KSStorageSlot::Single,selected,input.program};
 }
private:
 static bool room(uint64_t revision)noexcept{return revision!=std::numeric_limits<uint64_t>::max();}
 bool valid(const KSGlobalState& globals,const KSSystemSettings& system)const noexcept {
  return globals.session()==session_ && system.session==session_ && globals.layout()==KSLayout::Performance;
 }
 Session session_;const KSParameterMapTables& tables_;
 uint64_t revision_=0;uint8_t selector_=255,bank_=255;
};
}
