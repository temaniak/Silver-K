#pragma once
#include "ksMidiFileTransportTask.h"
#include <array>
#include <stdexcept>
#include <type_traits>
namespace ks {
// File-player policy; all named children are owned by the enclosing native
// work graph. Stream retention is a logical value, independent of CPU state.
class MidiFileTransportCoordinator final {
public:
using Request=MidiFileTransportTask::Child;using Stage=MidiFileTransportTask::Stage;
struct Input{uint16_t presentation=0;uint8_t activation=0;uint32_t expressionSource=0;};
struct ChildInput{uint32_t stream=0,tempo=0,expressionSource=0;uint16_t presentation=0;uint8_t activation=0;};
struct ChildResult{uint32_t stream=0,outcome=~0u;uint16_t presentation=0;bool presentationKnown=false;uint8_t key=0,control=0;bool due=false,available=false;bool activationKnown=false;uint8_t activation=0;uint32_t expressionSource=0,expressionSourceMask=0;};
struct Effect{bool read=false,write=false;};
struct PhaseObservation{Stage stage=Stage::Complete;uint32_t address=0,value=0;unsigned width=0;bool read=false,write=false;};
struct Result{uint32_t stream=0,expressionSource=0;uint16_t presentation=0;uint8_t activation=0,mode=0;uint64_t completedChildren=0,events=0,backgroundCycles=0;};
MidiFileTransportCoordinator():MidiFileTransportCoordinator(Input{}){}explicit MidiFileTransportCoordinator(Input in):input_(in){result_.presentation=in.presentation;result_.activation=in.activation;result_.expressionSource=in.expressionSource;}
const PhaseObservation&phaseObservation()const{return observation_;}const Input&input()const{return input_;}const Result&result()const{return result_;}const Effect&effect()const{return effect_;}bool complete()const{return task_.complete();}bool critical()const{return false;}unsigned priority()const{return 0;}Request request()const{return task_.child();}const ChildInput&childInput()const{return child_;}Stage pendingStage()const{return pending_;}const MidiFileTransportTask&policy()const{return task_;}
template<class T>const T*task()const{if constexpr(std::is_same_v<T,MidiFileTransportTask>)return &task_;else return nullptr;}
bool resume(ChildResult r){const auto q=request();if(q==Request::None)return false;if(r.presentationKnown)result_.presentation=r.presentation;if(r.activationKnown)result_.activation=r.activation;result_.expressionSource=(result_.expressionSource&~r.expressionSourceMask)|(r.expressionSource&r.expressionSourceMask);++result_.completedChildren;bool ok=false;
switch(q){case Request::Delay:case Request::Event:ok=task_.resumeSource(r.stream);if(q==Request::Event)++result_.events;break;case Request::Due:ok=task_.resumeDue(r.due);break;case Request::PollKey:ok=task_.resumeKey(r.key);break;case Request::PollControl:ok=task_.resumeControl(r.available,r.control);break;default:ok=task_.resumeChild();break;}
if(pending_==Stage::Clock)++result_.backgroundCycles;result_.stream=task_.stream();return ok;}
unsigned nextClocks(int previous=-1,int previousRead=-1)const{return task_.nextClocks(timing(task_.stage(),previous,-1));}
template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1,int previousRead=-1){const auto cost=nextClocks(previous,previousRead);if(!cost||cost>budget)return 0;effect_={};const auto stage=task_.stage();observation_={};observation_.stage=stage;const auto index=task_.index();
const auto transferRead=[&](uint32_t raw,unsigned width,unsigned post)->uint32_t{const auto a=raw&0xffffff;const auto total=accessCost(a,width,-1);unsigned elapsed=cost-post-total;uint32_t v=0;int p=-1;for(unsigned n=0;n<width;++n){elapsed+=byteCost(a+n,p);p=int(a+n);v=(v<<8)|uint8_t(read((a+n)&0xffffff,elapsed));}observation_.address=a;observation_.value=v;observation_.width=width;observation_.read=true;return v;};
const auto transferWrite=[&](uint32_t a,unsigned width,uint32_t v){observation_.address=a&0xffffff;observation_.value=v;observation_.width=width;observation_.write=true;const auto total=accessCost(a,width,previous);unsigned elapsed=cost-total;int p=previous;for(unsigned n=0;n<width;++n){elapsed+=byteCost(a+n,p);p=int(a+n);write((a+n)&0xffffff,uint8_t(v>>(8*(width-1-n))),elapsed);}};
task_.advanceBefore(cost,[&](Stage q)->uint32_t{switch(q){case Stage::SaveRead:return transferRead(0x167aa+index,1,2);case Stage::RestoreRead:return transferRead(0x1732a+index,1,2);case Stage::ReadDivision:return transferRead(task_.stream()+12,2,0);case Stage::ReadPlaying:return transferRead(0x17328,1,0);case Stage::ReadMode:return result_.mode=uint8_t(transferRead(0x167ba,1,0));default:throw std::logic_error("Unknown file-player read");}},[&](Stage q,uint32_t v){switch(q){case Stage::PlaybackFlag:case Stage::PublishStopped:transferWrite(0x1678c,1,v);break;case Stage::SavePublish:transferWrite(0x1732a+index,1,v);break;case Stage::RestorePublish:transferWrite(0x167aa+index,1,v);break;case Stage::PublishProgram:transferWrite(0x167ab,1,v);break;case Stage::PublishMulti:transferWrite(0x167ba,1,v);break;case Stage::PublishTime:transferWrite(0xffee82,4,v);break;case Stage::PublishDeadline:transferWrite(0x17322,4,v);break;case Stage::PublishDivision:transferWrite(0x17326,2,v);break;case Stage::PublishPlaying:transferWrite(0x17328,1,v);break;default:throw std::logic_error("Unknown file-player publication");}},[&](Stage q){return timing(q,previous,-1);});
if(stage==Stage::RetainStream)effect_.write=true;if(stage==Stage::RestoreStream||stage==Stage::ExitRestoreStream||stage==Stage::Finish)effect_.read=true;
if(stage==Stage::OutputMask)result_.presentation=uint16_t((result_.presentation&0xff00)|255);
if(task_.awaitingChild()){effect_.write=true;pending_=stage;child_={task_.stream(),task_.tempo(),result_.expressionSource,result_.presentation,result_.activation};}
result_.stream=task_.stream();return cost;}
private:
static unsigned byteCost(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
static unsigned accessCost(uint32_t a,unsigned n,int p){unsigned c=0;for(unsigned k=0;k<n;++k){c+=byteCost((a+k)&0xffffff,p);p=int((a+k)&0xffffff);}return c;}
unsigned timing(Stage s,int previous,int previousRead)const{const auto n=task_.index();switch(s){case Stage::SaveRead:return accessCost(0x167aa+n,1,previousRead);case Stage::RestoreRead:return accessCost(0x1732a+n,1,previousRead);case Stage::ReadDivision:return accessCost(task_.stream()+12,2,previousRead);case Stage::ReadPlaying:return accessCost(0x17328,1,previousRead);case Stage::ReadMode:return accessCost(0x167ba,1,previousRead);case Stage::PlaybackFlag:case Stage::PublishStopped:return accessCost(0x1678c,1,previous);case Stage::SavePublish:return accessCost(0x1732a+n,1,previous);case Stage::RestorePublish:return accessCost(0x167aa+n,1,previous);case Stage::PublishProgram:return accessCost(0x167ab,1,previous);case Stage::PublishMulti:return accessCost(0x167ba,1,previous);case Stage::PublishTime:return 12;case Stage::PublishDeadline:return accessCost(0x17322,4,previous);case Stage::PublishDivision:return accessCost(0x17326,2,previous);case Stage::PublishPlaying:return accessCost(0x17328,1,previous);default:return 0;}}
Input input_;Result result_{};PhaseObservation observation_{};Effect effect_{};ChildInput child_{};Stage pending_=Stage::Complete;MidiFileTransportTask task_;
};}
