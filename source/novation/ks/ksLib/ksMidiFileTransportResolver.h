#pragma once
#include "ksScheduledTimerCoordinator.h"
#include "ksMidiFileEventBindings.h"
#include <array>
namespace ks {
// The resource value has already been read at its original guest boundary.
// This resolver authenticates a finite semantic binding against the verified
// immutable OS metadata image. It performs no resource callback or CPU read.
class MidiFileTransportResolver final {
public:
 struct Metadata final {std::array<uint8_t,96>buttons{};std::array<uint32_t,18>menu{};void assign(const uint8_t*verified){if(!verified)throw std::logic_error("Missing verified controller metadata");for(unsigned n=0;n<buttons.size();++n)buttons[n]=verified[0xfcc4+n];for(unsigned key=0;key<menu.size();++key){uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|verified[0xfd1e +4*key+n];menu[key]=value;}}};
 explicit MidiFileTransportResolver(const Metadata&verifiedMetadata):metadata_(verifiedMetadata){}
 bool buttonDescriptor(unsigned key,unsigned field,unsigned value)const{return key<32&&field<3&&value==metadata_.buttons[3*key+field];}
 StateCentralActionsCoordinator::ButtonAction menuAction(unsigned key,uint32_t raw)const{if(key>=18)throw std::logic_error("Unsupported central menu key");const auto expected=metadata_.menu[key];if(raw!=expected)throw std::logic_error("Unsupported central menu action binding");return StateCentralActionsCoordinator::ButtonAction(key);}
 StateFieldCoordinator::Handler field(uint8_t code,uint32_t raw)const{constexpr uint32_t roots[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(code>=9||raw!=roots[code])throw std::logic_error("Unsupported central field binding");return StateFieldCoordinator::Handler(code);}
 bool schema(uint8_t code,uint32_t raw)const{switch(code){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}}
 bool operator()(uint8_t code,uint32_t raw)const{return schema(code,raw);}
 bool packing(uint32_t raw)const{constexpr uint32_t roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(auto root:roots)if((raw&0xffffff)==root)return true;return false;}
 StatePackFieldsTask::Kind operator()(uint32_t raw)const{constexpr uint32_t roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return StatePackFieldsTask::Kind(n);throw std::logic_error("Unsupported central packing binding");}
 ScheduledTimerTask::ActionHandle boundAction(StateCentralActionsCoordinator::ActionKind kind)const{return {ScheduledTimerCoordinator::binding(ScheduledTimerCoordinator::Action(kind))};}
 MidiFileEventCoordinator::Kind fileMeta(uint8_t index,uint8_t type,uint32_t raw)const{return MidiFileEventBindings{}.fileMeta(index,type,raw);}
private:const Metadata&metadata_;
};
}
