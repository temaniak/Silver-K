#pragma once
#include <cstdint>

namespace ks {
// Six multiplexed active-low button rows. Each row is observed only after
// settling, and each changed bit is published in the original physical order.
class PanelMatrixTask final {
public:
    enum class Kind { Initialize, Scan };
    enum class Stage {
        ScanInitial, ResetQueue, ReturnInitial,
        SaveScratch, SaveMux, SaveBits, SaveIndex, SaveTarget, SaveEvent,
        DisableGate, WriteDisabledGate, SelectHistory, ClearKey, SelectMux,
        SetRows, WriteMux, SaveDelay, SetDelay, DecrementDelay, MoreDelay,
        RestoreDelay, ReadPrevious, ReadSample, CompareRow, SelectChanges,
        WriteHistory, SetBits, ShiftBits, SelectBit, SetReleased, TestPressed,
        SelectPressed, SetPressed, RetainEvent, PublishEvent, RestoreEvent,
        AdvanceKey, DecrementBits, MoreBits, SkipUnchanged, AdvanceUnchanged,
        AdvanceMux, AdvanceHistory, DecrementRows, MoreRows, RestoreMuxValue,
        WriteRestoredMux, EnableGate, WriteEnabledGate, RestoreSavedEvent,
        RestoreTarget, RestoreIndex, RestoreBits, RestoreMux, RestoreScratch,
        ReturnScan, AwaitChild, Complete
    };
    enum class Read { Previous, Sample };
    enum class Write { Gate, Mux, History };
    explicit PanelMatrixTask(Kind kind):stage_(kind==Kind::Initialize?Stage::ScanInitial:Stage::SaveScratch){}
    Stage stage()const{return stage_;}
    Stage resumeStage()const{return resume_;}
    bool awaitingChild()const{return stage_==Stage::AwaitChild;}
    bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
    bool complete()const{return stage_==Stage::Complete;}
    uint8_t row()const{return row_;}
    uint8_t key()const{return key_;}
    uint8_t pressed()const{return pressed_;}
    uint8_t previous()const{return previous_;}
    uint8_t sample()const{return sample_;}
    unsigned nextClocks(unsigned storeClocks=3)const{
        switch(stage_){
        case Stage::ScanInitial:case Stage::ResetQueue:case Stage::PublishEvent:return 15;
        case Stage::ReturnInitial:case Stage::ReturnScan:return 17;
        case Stage::SaveScratch:case Stage::SaveMux:case Stage::SaveBits:case Stage::SaveIndex:
        case Stage::SaveTarget:case Stage::SaveEvent:case Stage::RestoreSavedEvent:
        case Stage::RestoreTarget:case Stage::RestoreIndex:case Stage::RestoreBits:
        case Stage::RestoreMux:case Stage::RestoreScratch:return 20;
        case Stage::WriteDisabledGate:case Stage::WriteEnabledGate:return 6+storeClocks;
        case Stage::WriteMux:case Stage::WriteRestoredMux:case Stage::WriteHistory:return 3+storeClocks;
        case Stage::SelectHistory:return 9;
        case Stage::SaveDelay:case Stage::RestoreDelay:return 11;
        case Stage::SetDelay:case Stage::ReadPrevious:case Stage::SetBits:return 6;
        case Stage::ReadSample:return 4;
        case Stage::AwaitChild:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){
        const auto cost=nextClocks(storeClocks);if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ScanInitial:await(Stage::ResetQueue);break;
        case Stage::ResetQueue:await(Stage::ReturnInitial);break;
        case Stage::WriteDisabledGate:write(Write::Gate,0);next();break;
        case Stage::WriteEnabledGate:write(Write::Gate,255);next();break;
        case Stage::WriteMux:write(Write::Mux,uint8_t(0x20+row_));next();break;
        case Stage::WriteRestoredMux:write(Write::Mux,0x30);next();break;
        case Stage::SetDelay:delay_=12;next();break;
        case Stage::DecrementDelay:--delay_;next();break;
        case Stage::MoreDelay:stage_=delay_?Stage::DecrementDelay:Stage::RestoreDelay;break;
        case Stage::ReadPrevious:previous_=uint8_t(read(Read::Previous));next();break;
        case Stage::ReadSample:sample_=uint8_t(read(Read::Sample));next();break;
        case Stage::CompareRow:bits_=uint16_t((unsigned(sample_)<<8)|(previous_^sample_));next();break;
        case Stage::SelectChanges:stage_=previous_==sample_?Stage::AdvanceUnchanged:Stage::WriteHistory;break;
        case Stage::WriteHistory:write(Write::History,sample_);next();break;
        case Stage::SetBits:remainingBits_=8;next();break;
        case Stage::ShiftBits:changedBit_=bits_&1;bits_>>=1;next();break;
        case Stage::SelectBit:stage_=changedBit_?Stage::SetReleased:Stage::AdvanceKey;break;
        case Stage::SetReleased:pressed_=0;next();break;
        case Stage::SelectPressed:stage_=bits_&0x80?Stage::RetainEvent:Stage::SetPressed;break;
        case Stage::SetPressed:pressed_=1;next();break;
        case Stage::PublishEvent:await(Stage::RestoreEvent);break;
        case Stage::AdvanceKey:++key_;next();break;
        case Stage::DecrementBits:--remainingBits_;next();break;
        case Stage::MoreBits:stage_=remainingBits_?Stage::ShiftBits:Stage::SkipUnchanged;break;
        case Stage::SkipUnchanged:stage_=Stage::AdvanceMux;break;
        case Stage::AdvanceUnchanged:key_=uint8_t(key_+8);next();break;
        case Stage::AdvanceHistory:++row_;next();break;
        case Stage::MoreRows:stage_=row_<6?Stage::WriteMux:Stage::RestoreMuxValue;break;
        case Stage::ReturnInitial:case Stage::ReturnScan:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
    Stage stage_,resume_=Stage::Complete;
    uint16_t bits_=0;uint8_t row_=0,key_=0,pressed_=0,previous_=0,sample_=0,remainingBits_=0,delay_=0;
    bool changedBit_=false;
};
}
