#pragma once
#include <array>
#include <cstdint>

namespace kstation {
// Bounded reset leaves for the admitted K image. These jobs do not own the
// complete boot graph. The board owns addresses, bus credits, IRQ arbitration
// and the timer/serial tick after each accepted phase.
class StartupTask final {
public:
    enum class Kind { Pins, WaitStates, ColdClear, MainScratchClear };
    enum class Stage { BindTarget, BindCount, PrepareValue, Publish,
                       AdvanceTarget, Decrement, Repeat, BindRuntimeStack,
                       Return, Complete };
    enum class Field { P6Data, P6Direction, P8Data, P8Direction, P9Data,
                       P9Direction, PAData, PADirection, PBData, PBDirection,
                       P4Direction, AccessState, WaitControl, InternalClear,
                       MainScratch };
    struct Publication { Field field; unsigned index, width; uint8_t value; };
    explicit StartupTask(Kind kind) : kind_(kind), stage_(fixed()?Stage::PrepareValue:Stage::BindTarget) {}
    Kind kind() const { return kind_; }
    Stage stage() const { return stage_; }
    unsigned index() const { return index_; }
    unsigned remaining() const { return remaining_; }
    bool complete() const { return stage_==Stage::Complete; }
    Publication publication() const {
        if(!fixed())return {kind_==Kind::ColdClear?Field::InternalClear:Field::MainScratch,index_,kind_==Kind::ColdClear?2u:1u,0};
        if(kind_==Kind::WaitStates)return {index_?Field::WaitControl:Field::AccessState,0,1,uint8_t(index_?1:0xfb)};
        static constexpr std::array<Field,11> fields{{Field::P6Data,Field::P6Direction,Field::P8Data,Field::P8Direction,Field::P9Data,Field::P9Direction,Field::PAData,Field::PADirection,Field::PBData,Field::PBDirection,Field::P4Direction}};
        static constexpr std::array<uint8_t,11> values{{0,7,1,0x1f,0,0x3a,0x64,0xfc,1,0xff,0xff}};
        if(index_>=fields.size())return {Field::P6Data,0,0,0};
        return {fields[index_],0,1,values[index_]};
    }
    // writeClocks is the board's prospective bus cost, without making IO;
    // returnClocks includes the external return convention when needed.
    unsigned nextClocks(unsigned writeClocks=0,unsigned returnClocks=7) const {
        switch(stage_){
        case Stage::BindTarget:case Stage::BindRuntimeStack:return 9;
        case Stage::BindCount:return kind_==Kind::ColdClear?9:6;
        case Stage::Publish:return 3+writeClocks;
        case Stage::Return:return returnClocks;
        case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Writer> unsigned advanceBefore(unsigned available,unsigned writeClocks,unsigned returnClocks,Writer&& write) {
        const auto cost=nextClocks(writeClocks,returnClocks);
        if(!cost||available<cost)return 0;
        switch(stage_){
        case Stage::BindTarget:index_=0;stage_=Stage::BindCount;break;
        case Stage::BindCount:remaining_=kind_==Kind::ColdClear?256:22;stage_=kind_==Kind::ColdClear?Stage::Publish:Stage::PrepareValue;break;
        case Stage::PrepareValue:stage_=Stage::Publish;break;
        case Stage::Publish:
            write(publication());
            if(fixed()){++index_;stage_=index_==(kind_==Kind::Pins?11u:2u)?Stage::Return:Stage::PrepareValue;}
            else stage_=Stage::AdvanceTarget;
            break;
        case Stage::AdvanceTarget:++index_;stage_=Stage::Decrement;break;
        case Stage::Decrement:--remaining_;stage_=Stage::Repeat;break;
        case Stage::Repeat:stage_=remaining_?Stage::Publish:kind_==Kind::ColdClear?Stage::BindRuntimeStack:Stage::Complete;break;
        case Stage::BindRuntimeStack:case Stage::Return:stage_=Stage::Complete;break;
        case Stage::Complete:break;
        }
        return cost;
    }
private:
    bool fixed() const { return kind_==Kind::Pins||kind_==Kind::WaitStates; }
    Kind kind_;Stage stage_;unsigned index_=0,remaining_=0;
};
}
