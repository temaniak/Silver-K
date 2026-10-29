#pragma once
#include "kstationSerialRxService.h"
#include <cstdint>
namespace kstation {
// Semantic SCI receive child. F8 clock feedback remains an explicit child.
class ControllerSerialRxTask final {
public:
 using Credits=SerialRxService::Credits;using Stage=SerialRxService::Stage;
 Stage stage()const{return task_.stage();}bool complete()const{return task_.complete();}bool awaitingClock()const{return task_.awaitingClock();}bool resumeClock(){return task_.resumeClock();}uint32_t dataReference()const{return task_.dataReference();}unsigned nextClocks(const Credits&c)const{return task_.nextClocks(c);}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=task_.nextClocks(c);if(!cost||cost>available)return 0;return task_.advanceBefore(cost,c,[&](SerialRxService::Read r)->uint32_t{switch(r){case SerialRxService::Read::Status:return read(0xffffb4,cost);case SerialRxService::Read::Receive:return read(0xffffb5,cost);case SerialRxService::Read::Producer:{auto h=read(0xfffd2c,cost);auto l=read(0xfffd2d,cost);return (uint16_t(h)<<8)|uint8_t(l);}case SerialRxService::Read::Consumer:{auto h=read(0xfffd2e,cost);auto l=read(0xfffd2f,cost);return (uint16_t(h)<<8)|uint8_t(l);}}return 0;},[&](SerialRxService::Write w,uint16_t v){switch(w){case SerialRxService::Write::Status:write(0xffffb4,uint8_t(v),cost);break;case SerialRxService::Write::Data:write(task_.dataReference(),uint8_t(v),cost);break;case SerialRxService::Write::Producer:write(0xfffd2c,uint8_t(v>>8),cost);write(0xfffd2d,uint8_t(v),cost);break;case SerialRxService::Write::Error:write(0xfffe30,uint8_t(v),cost);break;}});}
private:SerialRxService task_{};
};
}
