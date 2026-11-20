#pragma once

#include "ksMusicalState.h"

namespace novation::controller::command {
struct KSEditorSelection {
 Session session{};
 KSLayout layout=KSLayout::Single;
 KSStorageSlot storage=KSStorageSlot::Single;
 uint64_t generation=0;
 friend bool operator==(const KSEditorSelection& a,const KSEditorSelection& b)noexcept {
  return a.session==b.session && a.layout==b.layout && a.storage==b.storage && a.generation==b.generation;
 }
};
// Persistent editor source, independent of the routed MIDI recipient. A
// Performance may retain Single storage0 as an editor source; this creates no
// fifth musical part and grants no active-voice/model dependency for slot0.
class KSCommandEditorSelection final {
public:
 explicit KSCommandEditorSelection(Session session)noexcept:session_(session){}
 KSCommandEditorSelection(const KSCommandEditorSelection&)=delete;
 KSCommandEditorSelection& operator=(const KSCommandEditorSelection&)=delete;
 bool canSelect(KSLayout layout,KSStorageSlot storage)const noexcept {
  return session_.owner && session_.generation && generation_!=std::numeric_limits<uint64_t>::max() &&
   (layout==KSLayout::Single || layout==KSLayout::Performance) && size_t(storage)<KSMusicalState::StorageSlotCount &&
   (layout!=KSLayout::Single || storage==KSStorageSlot::Single);
 }
 bool select(KSLayout layout,KSStorageSlot storage)noexcept {
  if(!canSelect(layout,storage))return false;
  layout_=layout;storage_=storage;initialized_=true;++generation_;return true;
 }
 std::optional<KSEditorSelection> projection()const noexcept {
  return initialized_?std::optional{KSEditorSelection{session_,layout_,storage_,generation_}}:std::nullopt;
 }
 bool current(const KSEditorSelection& source)const noexcept {
  const auto now=projection();return now && *now==source;
 }
 // Semantic result of an explicit mode/part-selection reconciliation. With
 // no selected Performance bit the original policy chooses storage4.
 static KSStorageSlot reconciled(KSLayout layout,uint8_t selectedParts)noexcept {
  if(layout==KSLayout::Single)return KSStorageSlot::Single;
  for(size_t i=0;i<3;++i)if(selectedParts&(1u<<i))return KSStorageSlot(i+1);
  return KSStorageSlot(4);
 }
private:
 Session session_{};uint64_t generation_=0;
 KSLayout layout_=KSLayout::Single;KSStorageSlot storage_=KSStorageSlot::Single;bool initialized_=false;
};
}
