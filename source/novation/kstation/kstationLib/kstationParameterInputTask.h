#pragma once
#include <cstdint>
namespace kstation {
enum class ParameterInputResult {Ordinary,Transformed,Ignored};
struct ParameterInputValue{ParameterInputResult kind;uint32_t offset;uint8_t value;};
// Live packed-field conversion for K's four multiplexed MIDI controllers.
// Descriptor bytes are data: cursor reads remain ordered and are not cached.
class PackedControllerTask final {
public:
    enum class Stage{CompareLower,SelectLower,CompareUpper,SelectUpper,ClearOrdinary,ReturnOrdinary,
        ClearIndex,CopyController,NormalizeController,ScaleFirst,ScaleSecond,ReadDescriptor,
        ClearValue,CopyValue,ReadShift,SelectShift,ShiftValue,DecrementShift,RepeatShift,ReadLimit,
        CompareLimit,SelectLimit,BoundaryResult,ReturnBoundary,ReadMask,ApplyMask,InvertMask,ReadTag,
        RoundSelector,SelectTuple,ReadLeftShift,SelectLeftShift,LeftShiftValue,RotateMask,
        DecrementLeftShift,RepeatLeftShift,ClearOffset,ReadOffset,ReadPacked,Preserve,Merge,
        ClearResult,SetResult,ReturnTransformed,Complete};
    enum class Read{Descriptor,Shift,Limit,Mask,Tag,LeftShift,Offset,Packed};
    struct Credits{unsigned descriptor=21,metadata=8,destination=6,packed=15,ret=7;};
    constexpr PackedControllerTask(uint8_t controller=0,uint8_t value=0,uint32_t destinationHint=0)
        :controller_(controller),value_(value),destination_(destinationHint&0xffffff00u){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    ParameterInputResult result()const{return result_;}uint32_t cursor()const{return cursor_;}
    ParameterInputValue output()const{return {result_,destination_,value_};}
    uint8_t offset()const{return offset_;}uint8_t controller()const{return controller_;}
    uint32_t descriptorIndex()const{return uint8_t(controller_-104u)*4u;}
    unsigned nextClocks(const Credits&c)const{switch(stage_){
        case Stage::ReadDescriptor:return c.descriptor;
        case Stage::ReadShift:case Stage::ReadLimit:case Stage::ReadMask:case Stage::ReadTag:case Stage::ReadLeftShift:return c.metadata;
        case Stage::ReadOffset:return c.destination;case Stage::ReadPacked:return c.packed;
        case Stage::ReturnOrdinary:case Stage::ReturnBoundary:case Stage::ReturnTransformed:return c.ret;
        case Stage::Complete:return 0;default:return 3;}}
    template<class R>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){
        case Stage::SelectLower:stage_=controller_<104?Stage::ClearOrdinary:Stage::CompareUpper;break;
        case Stage::SelectUpper:stage_=controller_<=107?Stage::ClearIndex:Stage::ClearOrdinary;break;
        case Stage::ClearOrdinary:result_=ParameterInputResult::Ordinary;next();break;
        case Stage::ReturnOrdinary:case Stage::ReturnBoundary:case Stage::ReturnTransformed:stage_=Stage::Complete;break;
        case Stage::ReadDescriptor:cursor_=read(Read::Descriptor);next();break;
        case Stage::CopyValue:normalized_=value_;next();break;
        case Stage::ReadShift:shift_=uint8_t(read(Read::Shift));++cursor_;next();break;
        case Stage::SelectShift:stage_=shift_?Stage::ShiftValue:Stage::ReadLimit;break;
        case Stage::ShiftValue:normalized_>>=1;next();break;
        case Stage::DecrementShift:--shift_;next();break;
        case Stage::RepeatShift:stage_=shift_?Stage::ShiftValue:Stage::ReadLimit;break;
        case Stage::ReadLimit:limit_=uint8_t(read(Read::Limit));++cursor_;next();break;
        case Stage::SelectLimit:stage_=limit_>normalized_?Stage::ReadMask:Stage::BoundaryResult;break;
        case Stage::BoundaryResult:result_=normalized_==limit_?ParameterInputResult::Transformed:ParameterInputResult::Ignored;destination_|=limit_;next();break;
        case Stage::ReadMask:mask_=uint8_t(read(Read::Mask));++cursor_;next();break;
        case Stage::ApplyMask:value_&=mask_;next();break;
        case Stage::InvertMask:mask_=uint8_t(~mask_);next();break;
        case Stage::ReadTag:(void)read(Read::Tag);++cursor_;next();break;
        case Stage::RoundSelector:normalized_&=0xfe;next();break;
        case Stage::SelectTuple:cursor_+=normalized_;next();break;
        case Stage::ReadLeftShift:left_=uint8_t(read(Read::LeftShift));++cursor_;next();break;
        case Stage::SelectLeftShift:stage_=left_?Stage::LeftShiftValue:Stage::ClearOffset;break;
        case Stage::LeftShiftValue:value_=uint8_t(value_<<1);next();break;
        case Stage::RotateMask:mask_=uint8_t((mask_<<1)|(mask_>>7));next();break;
        case Stage::DecrementLeftShift:--left_;next();break;
        case Stage::RepeatLeftShift:stage_=left_?Stage::LeftShiftValue:Stage::ClearOffset;break;
        case Stage::ClearOffset:destination_=0;next();break;
        case Stage::ReadOffset:offset_=uint8_t(read(Read::Offset));destination_=offset_;next();break;
        case Stage::ReadPacked:packed_=uint8_t(read(Read::Packed));next();break;
        case Stage::Preserve:packed_&=mask_;next();break;
        case Stage::Merge:value_|=packed_;next();break;
        case Stage::SetResult:result_=ParameterInputResult::Transformed;next();break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_=Stage::CompareLower;ParameterInputResult result_=ParameterInputResult::Ordinary;
    uint8_t controller_,value_,normalized_=0,shift_=0,limit_=0,mask_=0,left_=0,offset_=0,packed_=0;uint32_t cursor_=0,destination_=0;
};
class ParameterInputTask final {
public:
    enum class Entry{Controller,BiasedController,Indexed};
    enum class Stage{Convert,SelectOrdinary,SelectTransformed,ReturnIgnored,CompareRange,SelectRange,
        ReturnRange,Bias,ClearIndex,CopyIndex,ClearOffset,ReadMapping,Complete};
    struct Credits{unsigned call=5,ret=7,mapping=15;};
    constexpr ParameterInputTask(uint8_t controller=0,Entry entry=Entry::Controller,uint8_t value=0)
        :controller_(controller),stage_(entry==Entry::Controller?Stage::Convert:entry==Entry::BiasedController?Stage::Bias:Stage::ClearIndex),value_(value){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    bool awaitingConversion()const{return awaiting_;}bool updates()const{return update_;}uint8_t index()const{return controller_;}
    ParameterInputValue output()const{return {update_?ParameterInputResult::Transformed:result_,offset_,value_};}
    bool resumeConversion(ParameterInputValue result){if(!awaiting_)return false;result_=result.kind;offset_=result.offset;value_=result.value;awaiting_=false;return true;}
    bool resumeConversion(ParameterInputResult result){return resumeConversion(ParameterInputValue{result,offset_,value_});}
    unsigned nextClocks(const Credits&c)const{if(awaiting_)return 0;switch(stage_){case Stage::Convert:return c.call;
        case Stage::ReturnIgnored:case Stage::ReturnRange:return c.ret;case Stage::ReadMapping:return c.mapping;case Stage::Complete:return 0;default:return 3;}}
    template<class R>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read){const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){case Stage::Convert:awaiting_=true;stage_=Stage::SelectOrdinary;break;
        case Stage::SelectOrdinary:stage_=result_==ParameterInputResult::Ordinary?Stage::CompareRange:Stage::SelectTransformed;break;
        case Stage::SelectTransformed:if(result_==ParameterInputResult::Ignored)stage_=Stage::ReturnIgnored;else{update_=true;stage_=Stage::Complete;}break;
        case Stage::ReturnIgnored:case Stage::ReturnRange:stage_=Stage::Complete;break;
        case Stage::SelectRange:stage_=controller_<=50?Stage::Bias:Stage::ReturnRange;break;
        case Stage::Bias:controller_=uint8_t(controller_+128);next();break;
        case Stage::ReadMapping:offset_=uint8_t(read(controller_));update_=true;stage_=Stage::Complete;break;default:next();break;}return cost;
    }
private:void next(){stage_=Stage(unsigned(stage_)+1);}uint8_t controller_;Stage stage_;ParameterInputResult result_=ParameterInputResult::Ordinary;uint32_t offset_=0;uint8_t value_;bool awaiting_=false,update_=false;
};
}
