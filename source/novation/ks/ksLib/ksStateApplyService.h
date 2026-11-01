#pragma once
#include "ksStateManualEditTask.h"
#include "ksStatePostEditTask.h"
#include "ksStateStoreService.h"
#include "ksStateStorageTask.h"
#include "ksStateStoredValueTask.h"
#include "ksStatePresetAddressTask.h"
#include "ksStateProgramTask.h"
#include "ksMidiOutputTask.h"
#include "ksStateChannelTask.h"
#include "ksPartControlTasks.h"
#include "ksSerialForegroundService.h"
#include "ksPartSignalTask.h"
#include "ksDspRoutingTask.h"
#include "ksDspAddressTask.h"
#include "ksDspWordTask.h"
#include "ksDspFaultTask.h"
#include <optional>
namespace ks {
// Complete live edit publication and retained-value lookup. Every child,
// including packed fields, UART waits and DSP timeout notification, is owned
// here. Inputs, observations and musical outputs are ordinary native values.
class StateApplyService final {
public:
 enum class Kind {Apply,Post,StoredValue,ClassifyStored,CurrentPreset,OverlayPreset,ProgramIndex};
 enum class Component {Apply,Post,Packing,Midi,Channel,Part,Serial,Dirty,Signal,DspRouting,DspAddress,DspWord,DspFault,Stored,Classify,Preset,Program};
 struct Input{uint32_t reference=0;uint16_t request=0;};
 explicit StateApplyService(Kind kind):StateApplyService(kind,Input{}){}
 StateApplyService(Kind kind,Input input):kind_(kind),reference_(input.reference),request_(input.request){
  switch(kind){case Kind::Apply:manual_.emplace(reference_,uint8_t(request_>>8));select(Component::Apply);break;
  case Kind::Post:post_.emplace(reference_);select(Component::Post);break;
  case Kind::StoredValue:stored_.emplace(StateStoredValueTask::Kind::Read,reference_);select(Component::Stored);break;
  case Kind::ClassifyStored:classify_.emplace(StateStoredValueTask::Kind::Classify,reference_);select(Component::Classify);break;
  case Kind::CurrentPreset:startPreset(false);break;case Kind::OverlayPreset:startPreset(true);break;
  case Kind::ProgramIndex:startProgram(reference_);break;}
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}
 bool critical()const{return serial_&&serial_->critical();}uint32_t reference()const{return reference_;}
 uint32_t field()const{return post_?post_->field():manual_?manual_->field():reference_;}uint32_t liveBase()const{return live_;}uint32_t postBase()const{return post_?post_->base():0;}
 uint8_t type()const{return type_;}uint8_t cached()const{return cached_;}uint8_t pickup()const{return markerPrepared_?marker_:pickup_;}bool pickupObserved()const{return pickupObserved_;}
 bool packed()const{return packed_;}uint32_t packingBase()const{return packingBase_;}
 bool signalled()const{return signalled_;}uint8_t signalMask()const{return signalMask_;}uint16_t dspCached()const{return dspCached_;}bool dspRetried()const{return dspRetried_;}uint16_t dspRemaining()const{return dspRemaining_;}
 bool enabledObserved()const{return enabledObserved_;}uint8_t postEnabled()const{return postEnabled_;}
 uint32_t storedPointer()const{return stored_?stored_->pointer():reference_;}uint8_t storedValue()const{return stored_?uint8_t(stored_->value()):0;}
 bool presetAvailable()const{return presetAvailable_;}uint32_t presetAddress()const{return presetAddress_;}uint8_t presetBank()const{return presetBank_;}uint8_t presetSlot()const{return presetSlot_;}
 uint16_t programIndex()const{return program_?program_->index():programIndex_;}uint8_t programBank()const{return programBank_;}uint8_t programSlot()const{return programSlot_;}
 bool special()const{const auto f=uint8_t(reference_);return f>=212||f==126||(f>=84&&f<=124);}
 const StateManualEditTask*manualTask()const{return component_==Component::Apply&&manual_?&*manual_:nullptr;}
 const StatePostEditTask*postTask()const{return component_==Component::Post&&post_?&*post_:nullptr;}
 const StateStoreService*packingTask()const{return component_==Component::Packing&&packing_?&*packing_:nullptr;}
 const MidiOutputTask*midiTask()const{return component_==Component::Midi&&midi_?&*midi_:nullptr;}
 const StateChannelTask*channelTask()const{return component_==Component::Channel&&channel_?&*channel_:nullptr;}
 const PartLookupTask*partTask()const{return component_==Component::Part&&part_?&*part_:nullptr;}
 const SerialForegroundService*serialTask()const{return component_==Component::Serial&&serial_?&*serial_:nullptr;}
 const StateStorageTask*dirtyTask()const{return component_==Component::Dirty&&dirty_?&*dirty_:nullptr;}
 const PartSignalTask*signalTask()const{return component_==Component::Signal&&signal_?&*signal_:nullptr;}
 const DspRoutingTask*routingTask()const{return component_==Component::DspRouting&&routing_?&*routing_:nullptr;}
 const DspAddressTask*addressTask()const{return component_==Component::DspAddress&&address_?&*address_:nullptr;}
 const DspWordTask*wordTask()const{return component_==Component::DspWord&&word_?&*word_:nullptr;}
 const DspFaultTask*faultTask()const{return component_==Component::DspFault&&fault_?&*fault_:nullptr;}
 const StateStoredValueTask*storedTask()const{return component_==Component::Stored&&stored_?&*stored_:nullptr;}
 const StateStoredValueTask*classifyTask()const{return component_==Component::Classify&&classify_?&*classify_:nullptr;}
 const StatePresetAddressTask*presetTask()const{return component_==Component::Preset&&preset_?&*preset_:nullptr;}
 const StateProgramTask*programTask()const{return component_==Component::Program&&program_?&*program_:nullptr;}
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;
  switch(component_){case Component::Apply:return manual_->nextClocks(byteClocks(manualTarget(),previousWrite));case Component::Post:return post_->nextClocks();
  case Component::Packing:return packing_->nextClocks(previousWrite);case Component::Midi:return midi_->nextClocks();case Component::Channel:return channel_->nextClocks();
  case Component::Part:return part_->stage()==PartLookupTask::Stage::Complete?17:part_->nextClocks();case Component::Serial:return serial_->nextClocks();case Component::Dirty:return dirty_->nextClocks();
  case Component::Signal:return signal_->nextClocks();case Component::DspRouting:return routing_->nextClocks();
  case Component::DspAddress:return address_->complete()?17:address_->nextBoundaryClocks();case Component::DspWord:return word_->complete()?17:word_->nextBoundaryClocks();
  case Component::DspFault:return fault_->complete()?17:fault_->nextBoundaryClocks();
  case Component::Stored:return stored_->nextClocks(stored_->stage()==StateStoredValueTask::Stage::ReadMultiBase?6:3);
  case Component::Classify:return classify_->nextClocks();case Component::Preset:return preset_->nextClocks(byteClocks(0x167ad,previousWrite));case Component::Program:return program_->nextClocks();}return 0;
 }
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto longValue=[&](uint32_t a){uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|uint8_t(read((a+n)&0xffffff,cost-3*((3-n)/2)));return value;};
  const auto dataBase=[&](uint32_t at,bool optional=false){const auto value=longValue(at);if((value||!optional)&&!StateStoreService::dataSpan(value,256))throw std::logic_error("Unsupported evolving edit data base");return value;};
  const auto wordValue=[&](uint32_t a){const auto high=uint8_t(read(a,cost));return uint16_t((unsigned(high)<<8)|uint8_t(read(a+1,cost)));};
  switch(component_){
  case Component::Apply:{using T=StateManualEditTask;using S=T::Stage;const auto stage=manual_->stage();
   manual_->advanceBefore(cost,[&](T::Read f)->uint32_t{switch(f){case T::Read::Override:return dataBase(0x167a6,true);case T::Read::Current:return dataBase(0x167bc);case T::Read::LiveBase:return live_=dataBase(0x167c0);case T::Read::WriteOverride:return writeBase_=dataBase(0x167a6,true);case T::Read::WriteCurrent:return writeBase_=dataBase(0x167bc);default:break;}
    const auto a=f==T::Read::Type?71586+manual_->field():f==T::Read::Cached?live_+manual_->field():f==T::Read::Input?reference_:f==T::Read::Pickup?0x169f9:f==T::Read::StorePolicy||f==T::Read::MidiPolicy?0x169ed:f==T::Read::MidiControl?71330+manual_->field():0x16798;
    if(f==T::Read::Type||f==T::Read::MidiControl){const auto begin=f==T::Read::Type?71586u:71330u;const auto at=a&0xffffff;if(at<begin||at-begin>=256)throw std::logic_error("Unsupported evolving edit table index");}else if(f==T::Read::Cached||f==T::Read::Input){if(!StateStoreService::dataSpan(a,1))throw std::logic_error("Unsupported evolving edit value observation");}
    const auto value=uint8_t(read(a&0xffffff,cost));if(f==T::Read::Type)type_=value;if(f==T::Read::Cached)cached_=value;if(f==T::Read::Pickup){pickup_=value;pickupObserved_=true;}return value;
   },[&](T::Write f,uint8_t value){const auto at=manualTarget();if((f==T::Write::Input||f==T::Write::Marker||f==T::Write::Cached||f==T::Write::Preset)&&!StateStoreService::dataSpan(at,1))throw std::logic_error("Unsupported evolving edit value publication");write(at&0xffffff,value,cost);},byteClocks(manualTarget(),previousWrite));
   if(stage==S::SetUpMarker){marker_=254;markerPrepared_=true;}if(stage==S::SetDownMarker){marker_=253;markerPrepared_=true;}if(stage==S::WriteMarker)markerWritten_=true;if(stage==S::CaptureMarker)cached_=marker_;
   if(manual_->awaitingChild())switch(manual_->child()){
    case T::Child::PackedFields:packing_.emplace(StateStoreService::Kind::PackOne,StateStoreService::Input{0,type_});packed_=true;select(Component::Packing);break;
    case T::Child::MidiHigh:case T::Child::Midi:midi_.emplace(MidiOutputTask::Kind::Control,uint16_t((unsigned(manual_->midiValue())<<8)|(manual_->child()==T::Child::MidiHigh?0x62:manual_->midiControl())),0,type_);select(Component::Midi);break;
    case T::Child::Dirty:dirty_.emplace(StateStorageTask::Kind::Dirty);select(Component::Dirty);break;}
   else if(manual_->awaitingPost()){manual_->retirePost();post_.emplace(reference_);select(Component::Post);}break;
  }
  case Component::Post:{using T=StatePostEditTask;const auto stage=post_->stage();post_->advanceBefore(cost,[&](T::Read f){if(f==T::Read::Enabled){enabledObserved_=true;return uint32_t(postEnabled_=uint8_t(read((post_->base()+103)&0xffffff,cost)));}return dataBase(f==T::Read::Override?0x167a6:0x167bc,f==T::Read::Override);});
   if(post_->awaitingChild()){signalMask_=post_->child()==T::Child::Rate?1:2;signal_.emplace(signalMask_==1?PartSignalTask::Kind::Start:PartSignalTask::Kind::Continue);signalled_=true;select(Component::Signal);}else if(post_->complete())complete_=true;(void)stage;break;
  }
  case Component::Packing:{packing_->advanceBefore(cost,read,write,resolve,previousWrite);if(packing_->complete()){packingBase_=packing_->base();const auto value=packing_->value();const auto field=packing_->field();packing_.reset();manual_->resumeChild(value,field);select(Component::Apply);}break;}
  case Component::Midi:{using T=MidiOutputTask;const auto stage=midi_->stage();midi_->advanceBefore(cost,[&](T::Read){return read(0x169b1,cost);},[](T::Write,uint8_t){});
   if(midi_->awaitingChild()){if(stage==T::Stage::SelectRawControlChannel){channel_.emplace(StateChannelTask::Kind::SelectedChannel);select(Component::Channel);}else startSerial(midi_->low(),false);}
   else if(midi_->complete())startSerial(midi_->low(),true);break;
  }
  case Component::Channel:{using T=StateChannelTask;channel_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::SelectedPart?0x16798:0x174ae + 20*channel_->part()+11,cost);},[](T::Write,uint8_t){});
   if(channel_->awaitingControls()){part_.emplace(PartLookupTask::controlsFor(channel_->part()));select(Component::Part);}else if(channel_->complete()){const auto event=uint16_t((unsigned(channel_->part())<<8)|channel_->channel());channel_.reset();midi_->resumeChild(event,0,false);select(Component::Midi);}break;
  }
  case Component::Part:if(part_->stage()==PartLookupTask::Stage::Complete){part_.reset();channel_->resumeControls();select(Component::Channel);}else part_->advanceBefore(cost,[](PartLookupTask::Layout){return uint8_t(0);});break;
  case Component::Serial:serial_->advanceBefore(cost,read,write);if(serial_->complete()){serial_.reset();if(serialTail_){midi_.reset();manual_->resumeChild();select(Component::Apply);}else {midi_->resumeChild(midi_->event(),0,false);select(Component::Midi);}}break;
  case Component::Dirty:{using T=StateStorageTask;dirty_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::Dirty?0x1679a:0x17526,cost);},[&](T::Write,uint32_t value){write(0x1679a,uint8_t(value),cost);});if(dirty_->complete()){dirty_.reset();manual_->resumeChild();select(Component::Apply);}break;}
  case Component::Signal:{using T=PartSignalTask;signal_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::Part?0x17526:0x167ba,cost);});if(signal_->awaiting()){wire_=signal_->wire();routing_.emplace(DspRoutingTask::Kind::WriteA,signal_->address());select(Component::DspRouting);}else if(signal_->stage()==T::Stage::Complete){signal_.reset();post_->resumeChild();select(Component::Post);}break;}
  case Component::DspRouting:{using T=DspRoutingTask;routing_->advanceBefore(cost,[&](T::Read){return dspCached_=wordValue(0x159f2);},[](T::Write,uint16_t){});if(routing_->complete()){address_.emplace(0x8040);select(Component::DspAddress);}break;}
  case Component::DspAddress:if(address_->complete()){address_.reset();word_.emplace(wire_,true,0x8040);select(Component::DspWord);}else {using T=DspAddressTask;const auto stage=address_->stage();if(stage==T::Stage::ResetRetries){dspRetried_=true;dspRemaining_=0;}if(stage==T::Stage::DecrementRetries)dspRemaining_=uint16_t(address_->retries()-1);address_->advanceBefore(cost,[&](unsigned offset){return read(0x400000 + offset,cost);},[&](unsigned offset,uint8_t value){write(0x400000 + offset,value,cost);});if(address_->stage()==T::Stage::AwaitFault){faultFromWord_=false;fault_.emplace();select(Component::DspFault);}}break;
  case Component::DspWord:if(word_->complete()){word_.reset();signal_->resumeChild();select(Component::Signal);}else {using T=DspWordTask;const auto stage=word_->stage();if(stage==T::Stage::ResetRetries){dspRetried_=true;dspRemaining_=0;}if(stage==T::Stage::DecrementRetries)dspRemaining_=uint16_t(word_->retries()-1);word_->advanceBefore(cost,[&]{return read(0x400004,cost);},[&](unsigned offset,uint8_t value){write(0x400000 + offset,value,cost);},[&](uint16_t value){write(0x159f2,uint8_t(value>>8),cost);write(0x159f3,uint8_t(value),cost);});if(word_->stage()==T::Stage::AwaitFault){faultFromWord_=true;fault_.emplace();select(Component::DspFault);}}break;
  case Component::DspFault:if(fault_->complete()){fault_.reset();if(faultFromWord_){word_->resumeFault();select(Component::DspWord);}else {address_->resumeFault();select(Component::DspAddress);}}else {using T=DspFaultTask;fault_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::Latch?0xffec22:0xffff62,f==T::Read::Control?cost-1:cost);},[&](T::Write f,uint8_t value){write(f==T::Write::Latch?0xffec22:f==T::Write::Data?0xffff61:0xffff62,value,cost);});}break;
  case Component::Stored:{using T=StateStoredValueTask;using S=T::Stage;const auto stage=stored_->stage();stored_->advanceBefore(cost,[&](T::Read f,uint32_t ref){return f==T::Read::Multi?dataBase(0x167c0):uint32_t(read(ref&0xffffff,cost));},[](int,int){},stage==S::ReadMultiBase?6:3);
   if(stored_->awaitingChild()){if(stage==S::ClassifyPatchField){classify_.emplace(T::Kind::Classify,reference_);select(Component::Classify);}else startPreset(stage==S::ResolveAlternatePatch);}else if(stored_->complete())complete_=true;break;
  }
  case Component::Classify:classify_->advanceBefore(cost,[](StateStoredValueTask::Read,uint32_t){return uint32_t(0);},[](int,int){});if(classify_->complete()){classify_.reset();if(stored_){stored_->resumeChild(0,special());select(Component::Stored);}else complete_=true;}break;
  case Component::Preset:{using T=StatePresetAddressTask;preset_->advanceBefore(cost,[&](T::Read f){const auto value=uint8_t(read(f==T::Read::Bank?0x167ac:0x167ad,cost));if(f==T::Read::Bank)presetBank_=value;else presetSlot_=value;return value;},[&](T::Write,uint8_t value){write(0x167ad,value,cost);},byteClocks(0x167ad,previousWrite));
   if(preset_->awaitingIndex())startProgram(0x167ac);else if(preset_->complete()){presetAddress_=preset_->address();presetAvailable_=preset_->available();if(stored_){stored_->resumeChild(presetAvailable_?presetAddress_:0x16700,!presetAvailable_);preset_.reset();select(Component::Stored);}else complete_=true;}break;
  }
  case Component::Program:{using T=StateProgramTask;const auto stage=program_->stage();program_->advanceBefore(cost,[&](T::Read f){const auto value=uint8_t(read((programSource_+(f==T::Read::Bank?0:1))&0xffffff,f==T::Read::Bank?cost-2:cost));if(f==T::Read::Bank)programBank_=value;else programSlot_=value;return value;},[](T::Write,uint8_t){});
   if(program_->complete()){programIndex_=program_->index();if(preset_){preset_->resumeIndex(programIndex_);program_.reset();select(Component::Preset);}else complete_=true;}(void)stage;break;
  }
  }
  return cost;
 }
 bool clearsWriteHistory()const;
 bool clearsReadHistory()const;
private:
 static unsigned byteClocks(uint32_t a,int previous){a&=0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startSerial(uint8_t value,bool tail){serialTail_=tail;serial_.emplace(SerialForegroundService::Kind::Transmit,value);select(Component::Serial);}
 void startPreset(bool overlay){preset_.emplace(overlay?StatePresetAddressTask::Kind::Overlay:StatePresetAddressTask::Kind::Current);select(Component::Preset);}
 void startProgram(uint32_t source){programSource_=source;program_.emplace(StateProgramTask::Kind::ReadIndex);select(Component::Program);}
 uint32_t manualTarget()const{using S=StateManualEditTask::Stage;switch(manual_->stage()){case S::WriteInput:return reference_;case S::WriteMarker:case S::WriteCached:return live_+manual_->field();case S::WritePreset:return writeBase_+manual_->field();case S::WritePart:return 0x17526;default:return 0x1679c;}}
 Kind kind_;Component component_=Component::Apply;uint32_t components_=0;bool complete_=false;
 uint32_t reference_=0,live_=0,writeBase_=0,packingBase_=0,wire_=0,programSource_=0,presetAddress_=0;uint16_t request_=0,dspCached_=0,dspRemaining_=0,programIndex_=0;
 uint8_t type_=0,cached_=0,pickup_=0,marker_=0,signalMask_=0,postEnabled_=0,presetBank_=0,presetSlot_=0,programBank_=0,programSlot_=0;
 bool packed_=false,pickupObserved_=false,markerWritten_=false,markerPrepared_=false,serialTail_=false,signalled_=false,dspRetried_=false,faultFromWord_=false,enabledObserved_=false,presetAvailable_=false;
 std::optional<StateManualEditTask>manual_;std::optional<StatePostEditTask>post_;std::optional<StateStoreService>packing_;std::optional<MidiOutputTask>midi_;std::optional<StateChannelTask>channel_;std::optional<PartLookupTask>part_;std::optional<SerialForegroundService>serial_;std::optional<StateStorageTask>dirty_;std::optional<PartSignalTask>signal_;std::optional<DspRoutingTask>routing_;std::optional<DspAddressTask>address_;std::optional<DspWordTask>word_;std::optional<DspFaultTask>fault_;std::optional<StateStoredValueTask>stored_,classify_;std::optional<StatePresetAddressTask>preset_;std::optional<StateProgramTask>program_;
};

inline bool StateApplyService::clearsWriteHistory()const{
 if(const auto*t=manualTask()){using S=StateManualEditTask::Stage;switch(t->stage()){case S::RetainReference:case S::RetainRequest:case S::PackFields:case S::RetainMidiRequest:case S::DispatchMidiHigh:case S::DispatchMidi:case S::MarkDirty:return true;default:break;}}
 if(const auto*t=postTask()){using S=StatePostEditTask::Stage;switch(t->stage()){case S::RetainReference:case S::RetainRequest:case S::RefreshRate:case S::RefreshState:return true;default:break;}}
 if(const auto*t=packingTask()){if(const auto*p=t->packTask()){using S=StatePackRefreshTask::Stage;if(p->stage()==S::RetainGroup||p->stage()==S::DispatchRecipe)return true;}}
 if(const auto*t=midiTask()){using S=MidiOutputTask::Stage;switch(t->stage()){case S::SaveControlEvent:case S::SaveControlContext:case S::SelectRawControlChannel:case S::SendControlStatus:case S::SendControlIndex:return true;default:break;}}
 if(const auto*t=channelTask())return t->stage()==StateChannelTask::Stage::AdmitContext||t->stage()==StateChannelTask::Stage::LookupControls;
 if(const auto*t=serialTask())return t->atTransmit(SerialTransmitTask::Stage::RetainFirst)||t->atTransmit(SerialTransmitTask::Stage::RetainSecond)||t->atTransmit(SerialTransmitTask::Stage::RetainCritical);
 if(const auto*t=signalTask())return t->stage()==PartSignalTask::Stage::Dispatch;
 if(const auto*t=routingTask())return t->stage()==DspRoutingTask::Stage::DispatchWrite;
 if(const auto*t=addressTask())return t->stage()==DspAddressTask::Stage::DispatchFault;
 if(const auto*t=wordTask())return t->stage()==DspWordTask::Stage::DispatchFault;
 if(const auto*t=faultTask()){using S=DspFaultTask::Stage;return t->stage()==S::Admission||t->stage()==S::LatchAdmission||t->stage()==S::DelayAdmission;}
 if(const auto*t=storedTask()){using S=StateStoredValueTask::Stage;return t->stage()==S::ClassifyPatchField||t->stage()==S::ResolveAlternatePatch||t->stage()==S::ResolvePatch;}
 if(const auto*t=presetTask()){using S=StatePresetAddressTask::Stage;return t->stage()==S::RetainProgram||t->stage()==S::ReadCurrentIndex||t->stage()==S::ReadAliasIndex;}
 if(const auto*t=programTask())return t->stage()==StateProgramTask::Stage::RetainBank||t->stage()==StateProgramTask::Stage::RetainExtra;
 return false;
}
inline bool StateApplyService::clearsReadHistory()const{
 if(const auto*t=manualTask()){using S=StateManualEditTask::Stage;return t->stage()==S::RestoreMidiRequest||t->stage()==S::RestoreRequest||t->stage()==S::RestoreReference;}
 if(const auto*t=postTask()){using S=StatePostEditTask::Stage;return t->stage()==S::RestoreRequest||t->stage()==S::RestoreReference||t->stage()==S::ReturnPost;}
 if(const auto*t=packingTask()){if(const auto*p=t->packTask()){using S=StatePackRefreshTask::Stage;return p->stage()==S::RestoreGroup||p->stage()==S::ReturnEntry;}if(const auto*p=t->recipeTask())return p->stage()==StatePackFieldsTask::Stage::ReturnPacked;}
 if(const auto*t=midiTask())return t->stage()==MidiOutputTask::Stage::RestoreControlContext||t->stage()==MidiOutputTask::Stage::RestoreControlEvent;
 if(const auto*t=channelTask())return t->stage()==StateChannelTask::Stage::ReleaseContext||t->stage()==StateChannelTask::Stage::ReturnSelected;
 if(const auto*t=partTask())return t->stage()==PartLookupTask::Stage::Complete;
 if(const auto*t=serialTask())return t->atTransmit(SerialTransmitTask::Stage::RestoreCritical)||t->atTransmit(SerialTransmitTask::Stage::RestoreSecond)||t->atTransmit(SerialTransmitTask::Stage::RestoreFirst)||t->atTransmit(SerialTransmitTask::Stage::Finish);
 if(const auto*t=dirtyTask())return t->stage()==StateStorageTask::Stage::ReturnDirty;
 if(const auto*t=signalTask())return t->stage()==PartSignalTask::Stage::Finish;
 if(const auto*t=addressTask())return t->complete();if(const auto*t=wordTask())return t->complete();
 if(const auto*t=faultTask()){using S=DspFaultTask::Stage;return t->complete()||t->stage()==S::DelayRelease||t->stage()==S::LatchRelease||t->stage()==S::CompletionDelay;}
 if(const auto*t=storedTask())return t->stage()==StateStoredValueTask::Stage::ReturnStoredValue;
 if(const auto*t=classifyTask())return t->stage()==StateStoredValueTask::Stage::ReturnSpecial;
 if(const auto*t=presetTask()){using S=StatePresetAddressTask::Stage;return t->stage()==S::RestoreUnavailableProgram||t->stage()==S::ReturnUnavailable||t->stage()==S::RestoreProgram||t->stage()==S::ReturnAlias||t->stage()==S::ReturnCurrent;}
 if(const auto*t=programTask()){using S=StateProgramTask::Stage;return t->stage()==S::RestoreExtra||t->stage()==S::RestoreBank||t->stage()==S::ReturnIndex;}return false;
}
}
