#pragma once
#include <cstdint>
namespace kstation {
class ActiveParameterGroupTask final {
public:
    enum class Stage{BindField,NormalizeWord,NormalizeLong,Locate,Read,Return,Complete};
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint8_t value()const{return value_;}
    template<class R>void advance(R&&read){if(stage_==Stage::Read)value_=uint8_t(read());if(!complete())stage_=Stage(unsigned(stage_)+1);}
private:Stage stage_=Stage::BindField;uint8_t value_=0;
};
// Publish a packed program value and coordinate decoded, voice and panel jobs.
// Child completion carries native data; no architectural flags drive policy.
class ParameterUpdateTask final {
public:
    enum class Stage{PublishProgram,PublishMirror,RetainOffset,Unpack,UpdateVoices,RestoreOffset,
        ReadEditor,SelectEditor,ReadEditorOffset,CompareOffset,SelectMatch,SkipDisplay,
        ReadActiveGroup,SelectGroup,Display,Panel,Return,Complete};
    enum class Child{None,Unpack,Voices,ActiveGroup,Display,Panel};
    enum class Read{Editor,EditorOffset};enum class Write{Program,Mirror};
    struct Credits{unsigned write=15,read=12,retain=6,restore=6,call=10,ret=7;};
    constexpr ParameterUpdateTask(uint32_t offset=0,uint8_t value=0):offset_(offset),value_(value){}
    Stage stage()const{return stage_;}Child child()const{return child_;}
    bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return child_!=Child::None;}
    uint32_t offset()const{return offset_;}uint8_t value()const{return value_;}
    bool resumeChild(Child child,uint8_t value=0){if(child==Child::None||child!=child_)return false;if(child==Child::ActiveGroup)group_=value;child_=Child::None;return true;}
    unsigned nextClocks(const Credits&c)const{if(awaitingChild())return 0;switch(stage_){
        case Stage::PublishProgram:case Stage::PublishMirror:return c.write;
        case Stage::RetainOffset:return c.retain;case Stage::RestoreOffset:return c.restore;
        case Stage::Unpack:case Stage::UpdateVoices:case Stage::ReadActiveGroup:case Stage::Display:case Stage::Panel:return c.call;
        case Stage::ReadEditor:case Stage::ReadEditorOffset:return c.read;
        case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
    template<class R,class W>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){
        case Stage::PublishProgram:write(Write::Program,value_);next();break;case Stage::PublishMirror:write(Write::Mirror,value_);next();break;
        case Stage::Unpack:child_=Child::Unpack;next();break;case Stage::UpdateVoices:child_=Child::Voices;next();break;
        case Stage::ReadEditor:editor_=uint8_t(read(Read::Editor));next();break;
        case Stage::SelectEditor:stage_=editor_?Stage::ReadEditorOffset:Stage::ReadActiveGroup;break;
        case Stage::ReadEditorOffset:editorOffset_=uint8_t(read(Read::EditorOffset));next();break;
        case Stage::SelectMatch:stage_=editorOffset_==uint8_t(offset_)?Stage::Display:Stage::SkipDisplay;break;
        case Stage::SkipDisplay:stage_=Stage::Panel;break;
        case Stage::ReadActiveGroup:child_=Child::ActiveGroup;stage_=Stage::SelectGroup;break;
        case Stage::SelectGroup:stage_=group_?Stage::Display:Stage::Panel;break;
        case Stage::Display:child_=Child::Display;stage_=Stage::Panel;break;
        case Stage::Panel:child_=Child::Panel;stage_=Stage::Return;break;
        case Stage::Return:stage_=Stage::Complete;break;default:next();break;
        }return cost;
    }
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::PublishProgram;Child child_=Child::None;uint32_t offset_;uint8_t value_,editor_=0,editorOffset_=0,group_=0;
};
}
