#pragma once
#include "ksDiagnosticTransferTask.h"
#include "ksDiagnosticCodecService.h"
#include "ksPanelMonitorService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Diagnostic dump owns its complete formatter, packing and UART children.
// Each block reuses the same bounded frame and preserves the live stream.
class DiagnosticTransferService final {
public:
using Kind=DiagnosticTransferTask::Kind;enum class Component{Transfer,Codec,Serial,Lcd,Monitor};
struct Input{uint32_t source=0,checksum=0;uint16_t block=0;};struct Effect{bool read=false,write=false;};
struct Result{uint32_t source=0,sum=0,progress=0;uint16_t block=0,total=0;DiagnosticCodecService::Result encoded{};};
explicit DiagnosticTransferService(Kind k):DiagnosticTransferService(k,Input{}){}DiagnosticTransferService(Kind k,Input i):kind_(k),input_(i),source_(i.source){push(k,i.block,i.checksum);}
Kind kind()const{return kind_;}const Input&input()const{return input_;}const Result&result()const{return result_;}const Effect&effect()const{return effect_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return !depth_;}bool critical()const{return false;}
template<class T>const T*task()const{if constexpr(std::is_same_v<T,DiagnosticTransferService>)return this;else if constexpr(std::is_same_v<T,DiagnosticTransferTask>)return depth_&&component_==Component::Transfer?&frames_[depth_-1]->task:nullptr;else if constexpr(std::is_same_v<T,DiagnosticCodecService>)return component_==Component::Codec?&*codec_:nullptr;else if constexpr(std::is_same_v<T,DiagnosticUartService>)return component_==Component::Serial?&*serial_:nullptr;else if constexpr(std::is_same_v<T,PanelSettingsLcdService>)return component_==Component::Lcd?&*lcd_:nullptr;else if constexpr(std::is_same_v<T,PanelMonitorService>)return component_==Component::Monitor?&*monitor_:nullptr;else{if(component_==Component::Codec)return codec_->template task<T>();if(component_==Component::Serial)return serial_->template task<T>();if(component_==Component::Monitor)return monitor_->template task<T>();return nullptr;}}
unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Codec:return codec_->nextClocks(previous);case Component::Serial:return serial_->nextClocks(previous);case Component::Lcd:return lcd_->nextClocks(previous);case Component::Monitor:return monitor_->nextClocks(previous);case Component::Transfer:return frames_[depth_-1]->task.nextClocks(byteClock(frames_[depth_-1]->task.stage()==DiagnosticTransferTask::Stage::StoreDumpModel?0x156e6:0x156e7,previous));}return 0;}
template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
if(component_!=Component::Transfer){bool done=false;if(component_==Component::Codec){codec_->advanceBefore(cost,read,write,previous);effect_={codec_->effect().read,codec_->effect().write};done=codec_->complete();if(done&&codec_->kind()==DiagnosticCodecTask::Kind::Block){result_.encoded=codec_->result();source_=result_.encoded.source;}}
else if(component_==Component::Serial){serial_->advanceBefore(cost,read,write,previous);effect_={serial_->effect().read,serial_->effect().write};done=serial_->complete();}
else if(component_==Component::Lcd){lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};done=lcd_->complete();}
else{monitor_->advanceBefore(cost,read,write,previous);effect_={monitor_->effect().read,monitor_->effect().write};done=monitor_->complete();if(done&&monitor_->kind()==PanelMonitorService::Kind::Percent)result_.progress=monitor_->summary().percentResult;}
if(done){frames_[depth_-1]->task.resumeChild(result_.encoded.sum);select(Component::Transfer);}return cost;}
using T=DiagnosticTransferTask;using S=T::Stage;auto&t=frames_[depth_-1]->task;const auto stage=t.stage();const auto byte=[&](uint32_t a,unsigned back=0){return uint8_t(read(a&0xffffff,cost-back));};const auto word=[&](uint32_t a){return uint16_t(uint16_t(byte(a,3))<<8|byte(a+1));};const auto wide=[&](uint32_t a){return (uint32_t(byte(a,9))<<24)|(uint32_t(byte(a+1,6))<<16)|(uint32_t(byte(a+2,3))<<8)|byte(a+3);};
t.advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Model:return byte(0x8041b8,2);case T::Read::Device:return byte(0x8041b9,2);case T::Read::SavedModel:return byte(0x156e6);case T::Read::SavedDevice:return byte(0x156e7);case T::Read::Blocks:return word(0x804188);case T::Read::Size:return wide(0x804184);}return 0;},[&](T::Write q,uint8_t v){write(q==T::Write::Model?0x156e6:0x156e7,v,cost);},byteClock(stage==S::StoreDumpModel?0x156e6:0x156e7,previous));
if(stage==S::BindDumpData)source_=0x804000;
effect_.write=t.awaitingChild();effect_.read=stage==S::ReturnDump||stage==S::ReturnHeader||stage==S::ReturnFooter||stage==S::ReturnBlock;
if(t.awaitingChild()){
if(stage==S::ShowDumpNotice){lcd_.emplace(PanelSettingsLcdService::Kind::Text,PanelSettingsLcdService::Input{0,0xbaab,0xbaab});select(Component::Lcd);}
else if(stage==S::ShowDumpName){PanelMonitorService::Input i;i.source=0x8041b8;i.presentation=uint16_t(i.source);monitor_.emplace(PanelMonitorService::Kind::WithPatch,i);select(Component::Monitor);}
else if(stage==S::ShowDumpProgress){PanelMonitorService::Input i;i.numerator=t.block();i.divisor=t.total();monitor_.emplace(PanelMonitorService::Kind::Percent,i);select(Component::Monitor);}
else if(stage==S::SendDumpHeader)push(Kind::Header,0,0);
else if(stage==S::SendDumpBlock)push(Kind::Block,t.block(),t.sum());
else if(stage==S::SendDumpFooter)push(Kind::Footer,0,t.sum());
else if(stage==S::BeginHeader||stage==S::BeginFooter||stage==S::BeginBlock)startCodec(DiagnosticCodecTask::Kind::Prefix,0);
else if(stage==S::SendHeaderZero||stage==S::SendFooterZero||stage==S::SendBlockZero)startCodec(DiagnosticCodecTask::Kind::Word,0);
else if(stage==S::SendHeaderBlocks)startCodec(DiagnosticCodecTask::Kind::Word,t.total());
else if(stage==S::SendHeaderSize)startCodec(DiagnosticCodecTask::Kind::Long,t.size());
else if(stage==S::SendFooterChecksum)startCodec(DiagnosticCodecTask::Kind::Long,t.sum());
else if(stage==S::SendBlockChecksum)startCodec(DiagnosticCodecTask::Kind::Long,result_.encoded.sum);
else if(stage==S::SendBlockIndex)startCodec(DiagnosticCodecTask::Kind::Word,t.block());
else if(stage==S::SendBlockSize)startCodec(DiagnosticCodecTask::Kind::Word,32);
else if(stage==S::SendHeaderReservedOne||stage==S::SendHeaderReservedTwo||stage==S::SendFooterReservedOne||stage==S::SendFooterReservedTwo||stage==S::SendBlockReserved)startCodec(DiagnosticCodecTask::Kind::Long,0);
else if(stage==S::EncodeBlockData){DiagnosticCodecService::Input i;i.source=source_;i.sum=0;i.weight=0;i.carry=0;codec_.emplace(DiagnosticCodecTask::Kind::Block,i);select(Component::Codec);}
else {uint8_t value=0;switch(stage){case S::SendHeaderTag:value=0x7f;break;case S::SendFooterTag:value=0x7d;break;case S::SendBlockTag:value=0x7e;break;case S::SendHeaderModel:case S::SendFooterModel:case S::SendBlockModel:value=t.model();break;case S::SendHeaderDevice:case S::SendFooterDevice:case S::SendBlockDevice:value=t.device();break;case S::SendHeaderEnd:case S::SendFooterEnd:case S::SendBlockEnd:value=0xf7;break;default:break;}serial_.emplace(value);select(Component::Serial);}}
else if(t.complete()){const auto sum=frames_[depth_-1]->kind==Kind::Block?result_.encoded.sum:t.sum();const auto block=t.block(),total=t.total();--depth_;if(depth_){frames_[depth_-1]->task.resumeChild(sum);select(Component::Transfer);}else{result_.source=source_;result_.sum=sum;result_.block=block;result_.total=total;}}
return cost;}
private:struct Frame{Kind kind;DiagnosticTransferTask task;Frame(Kind k,uint16_t block,uint32_t sum):kind(k),task(k,block,sum){}};
static unsigned byteClock(uint32_t a,int p){return !(p&1)&&a==(unsigned(p)|1u)?0:3;}void select(Component c){component_=c;components_|=1u<<unsigned(c);}void push(Kind k,uint16_t block,uint32_t sum){if(depth_==frames_.size())throw std::logic_error("Diagnostic transfer depth exceeded");frames_[depth_++].emplace(k,block,sum);select(Component::Transfer);}void startCodec(DiagnosticCodecTask::Kind k,uint32_t payload){DiagnosticCodecService::Input i;i.payload=payload;codec_.emplace(k,i);select(Component::Codec);}
Kind kind_;Input input_;Result result_{};uint32_t source_=0,components_=1;unsigned depth_=0;Component component_=Component::Transfer;Effect effect_{};std::array<std::optional<Frame>,2>frames_{};std::optional<DiagnosticCodecService>codec_;std::optional<DiagnosticUartService>serial_;std::optional<PanelSettingsLcdService>lcd_;std::optional<PanelMonitorService>monitor_;
};}
