#pragma once
#include "kstationSerialTxService.h"
#include <cstdint>
namespace kstation {
// Semantic SCI transmit child with board resource addresses and per-phase
// offsets. Context-save/return timing is supplied by the board scheduler.
class ControllerSerialTxTask final {
public:
 using Credits=SerialTxService::Credits;using Stage=SerialTxService::Stage;
 Stage stage()const{return task_.stage();}bool complete()const{return task_.complete();}unsigned nextClocks(const Credits&c)const{return task_.nextClocks(c);}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=task_.nextClocks(c);if(!cost||cost>available)return 0;return task_.advanceBefore(cost,c,[&](SerialTxService::Read r)->uint32_t{switch(r){case SerialTxService::Read::Realtime:return read(0xfffe73,cost);case SerialTxService::Read::Consumer:{auto high=read(0xc494,cost);auto low=read(0xc495,cost);return (uint16_t(high)<<8)|uint8_t(low);}case SerialTxService::Read::Producer:{auto high=read(0xc492,cost);auto low=read(0xc493,cost);return (uint16_t(high)<<8)|uint8_t(low);}case SerialTxService::Read::Data:return read(task_.dataReference(),cost);case SerialTxService::Read::Status:return read(0xffffb4,cost);}return 0;},[&](SerialTxService::Write w,uint16_t v){switch(w){case SerialTxService::Write::Realtime:write(0xfffe73,uint8_t(v),cost);break;case SerialTxService::Write::Consumer:write(0xc494,uint8_t(v>>8),cost);write(0xc495,uint8_t(v),cost);break;case SerialTxService::Write::Transmit:write(0xffffb3,uint8_t(v),cost);break;case SerialTxService::Write::Control:write(0xffffb2,uint8_t(v),cost);break;case SerialTxService::Write::Status:write(0xffffb4,uint8_t(v),cost);break;}});}
private:SerialTxService task_{};
};
}
