#pragma once
#include <cstdint>
#include <array>

namespace kstation {
class PatchTransposeTask final {
public:
    enum class Stage{BindScale,Multiply,ReadPrevious,PublishPrevious,PublishCurrent,Return,Complete};
    struct Credits{unsigned read=12,write=12,ret=7;};
    explicit constexpr PatchTransposeTask(int8_t semitones=0):semitones_(semitones){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::Multiply:return 18;
        case Stage::ReadPrevious:return c.read;case Stage::PublishPrevious:case Stage::PublishCurrent:return c.write;
        case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
    template<class R,class W>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){case Stage::Multiply:value_=uint8_t(int(semitones_)*12);break;
        case Stage::ReadPrevious:previous_=uint8_t(read());break;case Stage::PublishPrevious:write(true,previous_);break;
        case Stage::PublishCurrent:write(false,value_);break;default:break;}stage_=Stage(unsigned(stage_)+1);return cost;
    }
private:Stage stage_=Stage::BindScale;int8_t semitones_;uint8_t value_=0,previous_=0;
};
// Field layout of K's packed 128-byte program. The sequence is the publication
// order of the decoded mirrors, rather than an instruction/opcode program.
class PatchUnpackTask final {
public:
    struct Field{uint8_t packed,decoded,mask,shift,bias;bool direct;};
    inline static constexpr Field Fields[]{
        {0x58,0x8e,3,0,0,false},{0x58,0x8f,1,2,0,false},{0x58,0xa6,1,1,0,false},{0x58,0xa7,1,1,0,false},{0x58,0x90,3,1,0,false},
        {0x50,0x9d,1,0,0,false},{0x50,0x9e,1,1,0,false},{0x50,0x9f,1,1,0,false},{0x50,0x8d,15,1,0,true},
        {0x4f,0xa0,1,0,0,false},{0x4f,0xa1,1,1,0,false},{0x4f,0xa2,1,1,0,false},{0x4f,0xa3,1,1,0,false},{0x4f,0xa4,1,1,0,false},{0x4f,0xa5,1,1,0,false},
        {5,0x83,3,0,0,false},{5,0x84,3,2,0,false},{5,0x85,3,2,0,false},{5,0x86,1,2,0,true},
        {6,0x87,3,0,63,false},{6,0x88,3,2,63,false},{6,0x89,3,2,63,false},{6,0x8a,1,2,0,true},
        {0,0x81,7,0,0,false},{0,0x80,3,3,0,false},{0,0x82,3,2,0,false},
        {0x4e,0x9b,1,0,0,false},{0x4e,0x9c,1,1,0,false},{0x4e,0x8b,3,1,0,false},{0x4e,0x8c,3,2,0,false},
        {0x79,0x91,7,0,0,false},{0x79,0x92,3,3,0,false},
        {0x7a,0x93,3,0,0,false},{0x7a,0x94,3,2,0,false},{0x7a,0x95,3,2,0,false},
        {0x7b,0x96,7,0,0,false},{0x7b,0x98,1,3,0,false},{0x7b,0x97,1,1,0,false},{0x7b,0x99,1,1,0,false},{0x7b,0x9a,1,1,0,true},
        {0x7f,0xa8,3,0,0,false},{0x7f,0xa9,3,2,0,false},{0x7f,0xaa,3,2,0,false},{0x7f,0xab,1,2,0,true},
        {0x7e,0xac,7,0,0,false}
    };
    enum class Stage{ReadPacked,Shift,Copy,Mask,Bias,Publish,SignLeft,SignRight,
        PublishSigned,RetainUi,BindUi,DispatchUi,AwaitUi,RestoreUi,Return,Complete};
    struct Credits{unsigned read=15,write=15,retain=6,restore=6,call=10,ret=7;};
    Stage stage()const{return stage_;}unsigned index()const{return index_;}
    const Field&field()const{return Fields[index_];}unsigned shiftIndex()const{return shift_;}
    unsigned signIndex()const{return sign_;}bool complete()const{return stage_==Stage::Complete;}
    bool awaitingUi()const{return stage_==Stage::AwaitUi;}
    bool resumeUi(){if(!awaitingUi())return false;stage_=Stage::RestoreUi;return true;}
    uint8_t packed()const{return packed_;}uint8_t value()const{return value_;}
    unsigned nextClocks(const Credits&c)const{switch(stage_){
        case Stage::ReadPacked:return c.read;case Stage::Publish:case Stage::PublishSigned:return c.write;
        case Stage::RetainUi:return c.retain;case Stage::RestoreUi:return c.restore;
        case Stage::DispatchUi:return c.call;case Stage::Return:return c.ret;
        case Stage::AwaitUi:case Stage::Complete:return 0;default:return 3;}}
    template<class R,class W>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){
        case Stage::ReadPacked:packed_=uint8_t(read(field().packed));beginField();break;
        case Stage::Shift:packed_>>=1;if(++shift_==field().shift)beginValue();break;
        case Stage::Copy:value_=packed_;stage_=Stage::Mask;break;
        case Stage::Mask:if(field().direct){packed_&=field().mask;value_=packed_;}else value_&=field().mask;stage_=field().bias?Stage::Bias:Stage::Publish;break;
        case Stage::Bias:value_=uint8_t(value_+field().bias);stage_=Stage::Publish;break;
        case Stage::Publish:{write(field().decoded,value_);const auto old=field().packed;
            if(index_+1==std::size(Fields)){stage_=Stage::SignLeft;break;}
            ++index_;if(field().packed!=old)stage_=Stage::ReadPacked;else beginField();break;}
        case Stage::SignLeft:packed_=uint8_t(packed_<<1);sign_=0;stage_=Stage::SignRight;break;
        case Stage::SignRight:packed_=uint8_t((packed_>>1)|(packed_&0x80));if(++sign_==4)stage_=Stage::PublishSigned;break;
        case Stage::PublishSigned:write(0xad,packed_);stage_=Stage::RetainUi;break;
        case Stage::RetainUi:stage_=Stage::BindUi;break;case Stage::BindUi:stage_=Stage::DispatchUi;break;
        case Stage::DispatchUi:stage_=Stage::AwaitUi;break;case Stage::RestoreUi:stage_=Stage::Return;break;
        case Stage::Return:stage_=Stage::Complete;break;default:break;
        }return cost;
    }
private:
    void beginField(){shift_=0;if(field().shift)stage_=Stage::Shift;else beginValue();}
    void beginValue(){if(field().direct){value_=packed_;stage_=Stage::Mask;}else stage_=Stage::Copy;}
    Stage stage_=Stage::ReadPacked;unsigned index_=0,shift_=0,sign_=0;uint8_t packed_=0,value_=0;
};
}
