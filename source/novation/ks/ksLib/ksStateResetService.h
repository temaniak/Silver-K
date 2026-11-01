#pragma once
#include "ksStateForegroundTask.h"
#include "ksPanelInputTask.h"
#include "ksKeyboardEventsTask.h"
#include <optional>

namespace ks {
// Complete foreground reset: clear all three event rings in their original
// order. Children return logical completion, with no machine continuation.
class StateResetService final {
public:
    enum class Kind { State, Buttons, Controls, Keyboard };
    enum class Component { State, Buttons, Controls, Keyboard };
    explicit StateResetService(Kind kind=Kind::State):kind_(kind){
        if(kind==Kind::Buttons||kind==Kind::Controls)startRing(kind==Kind::Controls);
        else if(kind==Kind::Keyboard)startKeyboard();
        else select(Component::State);
    }
    const StateForegroundTask*stateTask()const{return !complete_&&active_==Component::State?&state_:nullptr;}
    const PanelInputTask*panelTask()const{return !complete_&&(active_==Component::Buttons||active_==Component::Controls)?&*ring_:nullptr;}
    const KeyboardEventsTask*keyboardTask()const{return !complete_&&active_==Component::Keyboard?&*keyboard_:nullptr;}
    Kind kind()const{return kind_;}
    Component component()const{return active_;}
    uint32_t components()const{return components_;}
    bool complete()const{return complete_;}
    unsigned nextClocks()const{
        if(complete_)return 0;
        if(active_==Component::State)return state_.nextClocks();
        return active_==Component::Keyboard?keyboard_->nextClocks():ring_->nextClocks();
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&&,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        const auto word=[&](uint32_t address,uint16_t value){
            // All ring indices are aligned words in low data RAM: both byte
            // observations share the end of the three-clock bus transfer.
            write(address,uint8_t(value>>8),cost);write(address+1,uint8_t(value),cost);
        };
        if(active_==Component::State){
            using T=StateForegroundTask;const auto stage=state_.stage();
            state_.advanceBefore(cost,[](T::Read){return uint16_t(0);},[](T::Write,uint16_t){});
            if(state_.awaitingChild()){
                if(stage==T::Stage::ResetNotes)startKeyboard();
                else startRing(stage==T::Stage::ResetControls);
            }else if(state_.complete())complete_=true;
        }else if(active_==Component::Keyboard){
            using T=KeyboardEventsTask;
            keyboard_->advanceBefore(cost,[](T::Read){return uint16_t(0);},[&](T::Write field,uint16_t value){
                word(field==T::Write::Producer?0x156e8:0x156ea,value);
            });
            if(keyboard_->complete())finishChild();
        }else{
            using T=PanelInputTask;const uint32_t producer=active_==Component::Controls?0x15406:0x1537c;
            ring_->advanceBefore(cost,[](T::Read){return uint16_t(0);},[&](T::Write field,uint16_t value){
                word(producer+(field==T::Write::Consumer?2:0),value);
            });
            if(ring_->complete())finishChild();
        }
        return cost;
    }
private:
    void select(Component value){active_=value;components_|=1u<<unsigned(value);}
    void startRing(bool controls){ring_.emplace(controls?PanelInputTask::Kind::ControlReset:PanelInputTask::Kind::ButtonReset);select(controls?Component::Controls:Component::Buttons);}
    void startKeyboard(){keyboard_.emplace(KeyboardEventsTask::Kind::Reset);select(Component::Keyboard);}
    void finishChild(){
        if(kind_==Kind::State){ring_.reset();keyboard_.reset();state_.resumeChild(0,false);select(Component::State);}
        else complete_=true;
    }
    Kind kind_;
    StateForegroundTask state_{StateForegroundTask::Kind::Reset};
    std::optional<PanelInputTask> ring_;
    std::optional<KeyboardEventsTask> keyboard_;
    Component active_=Component::State;
    uint32_t components_=0;
    bool complete_=false;
};
}
