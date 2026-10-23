#pragma once
#include <cstdint>

namespace ks {
// Mode status and channel mirrors share musical bytes, not CPU state. The
// selected part is latched once; its channel is observed after the lookup.
class StateChannelTask final {
public:
    enum class Kind { ModeStatus, GlobalMirror, SelectedChannel };
    enum class Stage {
        ReadMode, SelectMode, ReadMultiStatus, SelectMultiStatus, PrepareMessage,
        WriteMessage, ReturnMode, SelectGlobals, ReadGlobalChannel,
        WriteGlobalChannel, SelectMirror, WriteMirror, ReturnGlobal,
        AdmitContext, ReadSelectedPart, BindSelectedPart, LookupControls,
        ReadSelectedChannel, ReleaseContext, ReturnSelected, AwaitControls, Complete
    };
    enum class Read { Mode, MultiStatus, GlobalChannel, SelectedPart, SelectedChannel };
    enum class Write { Message, GlobalChannel, Mirror };
    explicit StateChannelTask(Kind kind):kind_(kind),stage_(kind==Kind::ModeStatus?Stage::ReadMode:kind==Kind::GlobalMirror?Stage::SelectGlobals:Stage::AdmitContext){}
    Kind kind() const { return kind_; }
    Stage stage() const { return stage_; }
    bool complete() const { return stage_==Stage::Complete; }
    bool awaitingControls() const { return stage_==Stage::AwaitControls; }
    bool resumeControls(){if(!awaitingControls())return false;stage_=Stage::ReadSelectedChannel;return true;}
    uint8_t mode()const{return mode_;}
    uint8_t multiStatus()const{return multiStatus_;}
    uint8_t channel()const{return channel_;}
    uint8_t part()const{return part_;}
    unsigned nextClocks(unsigned writeClocks=3)const{
        switch(stage_){
        case Stage::ReadMode:case Stage::ReadMultiStatus:case Stage::ReadSelectedPart:return 12;
        case Stage::WriteMessage:return 9+writeClocks;
        case Stage::SelectGlobals:case Stage::SelectMirror:return 9;
        case Stage::ReadGlobalChannel:case Stage::ReadSelectedChannel:return 15;
        case Stage::WriteGlobalChannel:case Stage::WriteMirror:return 12+writeClocks;
        case Stage::AdmitContext:case Stage::ReleaseContext:case Stage::LookupControls:return 20;
        case Stage::ReturnMode:case Stage::ReturnGlobal:case Stage::ReturnSelected:return 17;
        case Stage::AwaitControls:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned writeClocks=3){
        const auto cost=nextClocks(writeClocks);if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));next();break;
        case Stage::SelectMode:stage_=mode_?Stage::ReadMultiStatus:Stage::PrepareMessage;break;
        case Stage::ReadMultiStatus:multiStatus_=uint8_t(read(Read::MultiStatus));next();break;
        case Stage::SelectMultiStatus:stage_=multiStatus_?Stage::PrepareMessage:Stage::ReturnMode;break;
        case Stage::WriteMessage:write(Write::Message,0x91);next();break;
        case Stage::ReadGlobalChannel:channel_=uint8_t(read(Read::GlobalChannel));next();break;
        case Stage::WriteGlobalChannel:write(Write::GlobalChannel,channel_);next();break;
        case Stage::WriteMirror:write(Write::Mirror,channel_);next();break;
        case Stage::ReadSelectedPart:part_=uint8_t(read(Read::SelectedPart));next();break;
        case Stage::LookupControls:stage_=Stage::AwaitControls;break;
        case Stage::ReadSelectedChannel:channel_=uint8_t(read(Read::SelectedChannel));next();break;
        case Stage::ReturnMode:case Stage::ReturnGlobal:case Stage::ReturnSelected:stage_=Stage::Complete;break;
        default:next();break;
        }
        return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Kind kind_;Stage stage_;uint8_t mode_=0,multiStatus_=0,part_=0,channel_=0;
};
}
