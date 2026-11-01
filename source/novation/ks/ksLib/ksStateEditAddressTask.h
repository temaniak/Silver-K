#pragma once
#include <cstdint>

namespace ks {
// Resolve panel-edit descriptors into live program/global fields. The second
// base observation is intentionally late: child/IRQ updates can change it.
class StateEditAddressTask final {
public:
    enum class Kind { Indexed, Direct, Global };
    enum class Stage {
        ZeroSelector, ReadSelector, CompareSentinel, SelectDirect, ReadIndexBase,
        LocateSelector, ReadIndex, MaskIndex, NormalizeIndex, LocateMapping,
        ReadMappedOffset, MaskOffset, NormalizeOffset, ReadTargetBase, LocateTarget,
        ReturnIndexed, ZeroDirect, ReadDirectOffset, ReadDirectBase, LocateDirect,
        ReturnDirect, ZeroGlobal, ReadGlobalOffset, LocateGlobal, ReturnGlobal, Complete
    };
    enum class Read { Selector, IndexBase, Index, MappedOffset, TargetBase, DirectOffset, DirectBase, GlobalOffset };
    StateEditAddressTask(Kind kind,uint32_t descriptor):stage_(kind==Kind::Indexed?Stage::ZeroSelector:kind==Kind::Direct?Stage::ZeroDirect:Stage::ZeroGlobal),descriptor_(descriptor){}
    Stage stage()const{return stage_;}
    bool complete()const{return stage_==Stage::Complete;}
    uint32_t descriptor()const{return descriptor_;}
    uint32_t base()const{return base_;}
    uint32_t selectorAddress()const{return base_+selector_;}
    uint32_t mappingAddress()const{return descriptor_+index_+3;}
    uint32_t target()const{return base_+offset_;}
    uint8_t selector()const{return selector_;}
    uint8_t index()const{return index_;}
    uint8_t offset()const{return offset_;}
    unsigned nextClocks()const{
        switch(stage_){
        case Stage::ReadSelector:case Stage::ReadMappedOffset:case Stage::ReadDirectOffset:case Stage::ReadGlobalOffset:return 15;
        case Stage::ReadIndexBase:case Stage::ReadTargetBase:case Stage::ReadDirectBase:return 18;
        case Stage::ReadIndex:return 6;case Stage::LocateGlobal:return 9;
        case Stage::ReturnIndexed:case Stage::ReturnDirect:case Stage::ReturnGlobal:return 17;
        case Stage::Complete:return 0;default:return 3;
        }
    }
    template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadSelector:selector_=uint8_t(read(Read::Selector));next();break;
        case Stage::SelectDirect:stage_=selector_==255?Stage::ZeroDirect:Stage::ReadIndexBase;break;
        case Stage::ReadIndexBase:base_=uint32_t(read(Read::IndexBase));next();break;
        case Stage::ReadIndex:index_=uint8_t(read(Read::Index));next();break;
        case Stage::ReadMappedOffset:offset_=uint8_t(read(Read::MappedOffset));next();break;
        case Stage::ReadTargetBase:base_=uint32_t(read(Read::TargetBase));next();break;
        case Stage::ReadDirectOffset:offset_=uint8_t(read(Read::DirectOffset));next();break;
        case Stage::ReadDirectBase:base_=uint32_t(read(Read::DirectBase));next();break;
        case Stage::ReadGlobalOffset:offset_=uint8_t(read(Read::GlobalOffset));base_=0x168fc;next();break;
        case Stage::ReturnIndexed:case Stage::ReturnDirect:case Stage::ReturnGlobal:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_;uint32_t descriptor_,base_=0;uint8_t selector_=0,index_=0,offset_=0;
};
}
