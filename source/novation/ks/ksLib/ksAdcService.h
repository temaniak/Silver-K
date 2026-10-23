#pragma once
#include "ksPanelAdcIrqTask.h"
#include "ksPanelAdcSamplesTask.h"
#include "ksPanelAdcScaleTask.h"
#include "ksPanelAdcScanTask.h"
#include "ksPanelInputTask.h"
#include <optional>

namespace ks {
// ADC completion owns the entire sample/threshold/scale/event/mux graph.
// Each phase exposes only timed byte resources and explicit child results.
class AdcService final {
public:
    enum class Component { Interrupt, Samples, OrdinaryScale, SpecialScale, ControlPush, Mux };
    bool complete()const{return interrupt_.complete();}
    uint32_t components()const{return components_;}
    Component component()const{return active_;}
    unsigned nextClocks()const{
        switch(active_){
        case Component::Interrupt:return interrupt_.nextClocks();
        case Component::Samples:return samples_->nextClocks();
        case Component::OrdinaryScale:case Component::SpecialScale:return scale_->nextClocks();
        case Component::ControlPush:return push_->nextClocks(wordClocks(0x1540a+push_->eventIndex()));
        case Component::Mux:return mux_->nextClocks();
        }
        return 0;
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        const auto readWord=[&](uint32_t address,unsigned end){
            const auto high=uint8_t(read(address,end-wordClocks(address)+byteClocks(address)));
            return uint16_t((unsigned(high)<<8)|uint8_t(read(address+1,end)));
        };
        const auto writeWord=[&](uint32_t address,uint16_t value,unsigned end){
            write(address,uint8_t(value>>8),end-wordClocks(address)+byteClocks(address));
            write(address+1,uint8_t(value),end);
        };
        switch(active_){
        case Component::Interrupt:{
            using T=PanelAdcIrqTask;using S=T::Stage;
            const auto stage=interrupt_.stage();const auto address=0x1560a+interrupt_.offset();
            interrupt_.advanceBefore(cost,[&](T::Read field)->uint16_t{
                switch(field){
                case T::Read::Remaining:return read(0x156e3,cost);
                case T::Read::Status:return read(0xffff98,cost);
                case T::Read::Group:case T::Read::ScaleGroup:case T::Read::NextGroup:return read(0x156e2,cost);
                case T::Read::Current:return readWord(address+72,cost);
                case T::Read::History:case T::Read::Preview:return readWord(address,cost);
                case T::Read::Armed:return read(address+144,cost);
                case T::Read::Last:return read(0x156e4,cost);
                }return 0;
            },[&](T::Write field,uint16_t value){
                if(field==T::Write::History)writeWord(address,value,cost);
                else write(field==T::Write::Remaining?0x156e3:field==T::Write::Status?0xffff98:field==T::Write::Armed?address+144:0x156e2,uint8_t(value),cost);
            });
            if(interrupt_.awaitingChild()){
                if(stage==S::SampleSettle||stage==S::SampleScan){samples_.emplace();select(Component::Samples);}
                else if(stage==S::SelectMux){mux_.emplace(PanelAdcScanTask::Kind::Mux);select(Component::Mux);}
                else {
                    const bool special=stage==S::SelectSpecial;
                    scale_.emplace(special?PanelAdcScaleTask::Kind::Special:PanelAdcScaleTask::Kind::Ordinary,
                        stage==S::ScalePreview?interrupt_.preview():interrupt_.accepted(),interrupt_.channel());
                    select(special?Component::SpecialScale:Component::OrdinaryScale);
                }
            }
            break;
        }
        case Component::Samples:{
            using T=PanelAdcSamplesTask;
            const auto address=0x15652+samples_->offset()+2*samples_->channel();
            samples_->advanceBefore(cost,[&](T::Read field)->uint16_t{
                if(field==T::Read::Group)return read(0x156e2,cost);
                if(field==T::Read::Previous)return readWord(address,cost);
                return read(0xffff90+2*samples_->channel()+(field==T::Read::Low),cost-2);
            },[&](uint16_t value){writeWord(address,value,cost);});
            if(samples_->complete()){samples_.reset();interrupt_.resumeChild();select(Component::Interrupt);}
            break;
        }
        case Component::OrdinaryScale:case Component::SpecialScale:{
            using T=PanelAdcScaleTask;
            // The curve table cursor advances past group 255 independently of
            // the wrapping byte channel identifier returned in the event.
            const auto position=(interrupt_.offset()-uint8_t(interrupt_.group()<<1))/2;
            scale_->advanceBefore(cost,[&](T::Read){return read(0xb93c+unsigned(interrupt_.group())+position,cost);},
                [&](T::Write,uint8_t value){write(0x156e4,value,cost);});
            if(scale_->complete()){
                push_.emplace(PanelInputTask::Kind::ControlPush,uint16_t((unsigned(scale_->scaled())<<8)|scale_->channel()));
                select(Component::ControlPush);
            }
            break;
        }
        case Component::ControlPush:{
            using T=PanelInputTask;
            push_->advanceBefore(cost,[&](T::Read field)->uint16_t{
                switch(field){
                case T::Read::Mapping:return read(0xb918+push_->key(),cost);
                case T::Read::Producer:return readWord(0x15406,cost);
                case T::Read::Consumer:return readWord(0x15408,cost);
                case T::Read::Event:return readWord(0x1540a+push_->eventIndex(),cost);
                }return 0;
            },[&](T::Write field,uint16_t value){
                writeWord(field==T::Write::Producer?0x15406:field==T::Write::Consumer?0x15408:0x1540a+push_->eventIndex(),value,cost);
            },wordClocks(0x1540a+push_->eventIndex()));
            if(push_->complete()){push_.reset();scale_.reset();interrupt_.resumeChild();select(Component::Interrupt);}
            break;
        }
        case Component::Mux:{
            using T=PanelAdcScanTask;
            mux_->advanceBefore(cost,[&](T::Read){return read(0x156e2,cost);},[&](T::Write field,uint8_t value){
                write(field==T::Write::Mux?0xffff6e:0x156e3,value,cost);
            });
            if(mux_->complete()){mux_.reset();interrupt_.resumeChild();select(Component::Interrupt);}
            break;
        }
        }
        return cost;
    }
private:
    static unsigned byteClocks(uint32_t address){return address>=0xfffd10?1u:3u;}
    static unsigned wordClocks(uint32_t address){return address<0x400000?3u+3u*(address&1):6u;}
    void select(Component component){active_=component;components_|=1u<<unsigned(component);}
    PanelAdcIrqTask interrupt_;
    std::optional<PanelAdcSamplesTask> samples_;
    std::optional<PanelAdcScaleTask> scale_;
    std::optional<PanelInputTask> push_;
    std::optional<PanelAdcScanTask> mux_;
    Component active_=Component::Interrupt;
    uint32_t components_=1;
};
}
