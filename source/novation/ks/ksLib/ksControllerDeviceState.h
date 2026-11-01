#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ks {
class ControllerDeviceState;
// Physical board resources have no architectural CPU state or continuation.
class ControllerResourceBus {
public:
    virtual ~ControllerResourceBus()=default;
    virtual uint8_t read(uint32_t)=0;
    virtual void write(uint32_t,uint8_t)=0;
    void setDeviceState(ControllerDeviceState* p){resources=p;}
protected:
    ControllerDeviceState* resources=nullptr;
};
// The actual memory, bus clock/history, mapping and pending interrupt state.
// Compatibility CPU adapters may borrow this owner during the migration;
// native services use it without creating a CPU frame.
class ControllerDeviceState {
public:
    ControllerDeviceState()=default;
    ControllerDeviceState(const ControllerDeviceState&)=delete;
    ControllerDeviceState&operator=(const ControllerDeviceState&)=delete;
    uint8_t memory[1<<24]{};
    uint64_t cycles=0,pending_irqs=0,pending_irqs_high=0;
    bool interruptMode2=false;
    uint8_t interruptPriority[128]{};
    int lastread=-1,lastwrite=-1;
    const uint8_t* nativeLowBase=nullptr;
    unsigned nativeLowSize=0;
    bool nativeLowWritable=false;
    void attachResourceBus(ControllerResourceBus* device){bus_=device;if(bus_)bus_->setDeviceState(this);}
    // Untimed transactions: native phase owners supply the exact timestamp.
    uint8_t readResource(uint32_t a){a&=0xffffff;return bus_?bus_->read(a):memory[a];}
    void writeResource(uint32_t a,uint8_t v){a&=0xffffff;if(bus_)bus_->write(a,v);else memory[a]=v;}
    void loadmem(const uint8_t* data,uint32_t size,uint32_t address){
        if(address>sizeof(memory) || size>sizeof(memory)-address)throw std::out_of_range("KS image range");
        std::memcpy(memory+address,data,size);
    }
    void clockI(unsigned n){cycles+=n;}
    void clockMem(int a,int& previous){
        if(a<0x400000){if((previous&1)||a!=(previous|1))cycles+=3;previous=a;}
        else if(a>=0xfffd10 && a<0xffffa0){if((previous&1)||a!=(previous|1))++cycles;previous=a;}
        else if(a>=0xffffa0){++cycles;previous=-1;}
        else {cycles+=3;previous=-1;}
    }
    void interrupt(int vector){
        if(vector<0 || vector>=128)return;
        if(interruptMode2){if(vector<64)pending_irqs|=1ull<<vector;else pending_irqs_high|=1ull<<(vector-64);return;}
        if(vector>=64)return;
        if(vector>=12 && vector<18 && !(read8(0xfffff5)&(1u<<(vector-12))))return;
        pending_irqs|=1ull<<vector;
    }
    int8_t read8(int address){
        if(!nativeLowBase){const auto a=unsigned(address)&0xffffff;clockMem(a,lastread);return bus_?int8_t(bus_->read(a)):int8_t(memory[a]);}
        const auto a=unsigned(address)&0xffffff;clockMem(a,lastread);
        if(a<0x200000)return a<nativeLowSize?int8_t(nativeLowBase[a]):int8_t(-1);
        if((a>=0xa00000 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00))return int8_t(memory[a]);
        return bus_?int8_t(bus_->read(a)):int8_t(memory[a]);
    }
    uint16_t read16(int a){const auto high=uint8_t(read8(a));return uint16_t((unsigned(high)<<8)|uint8_t(read8(a+1)));}
    uint32_t read32(int a){const auto high=read16(a);return (uint32_t(high)<<16)|read16(a+2);}
    void write8(int8_t value,int address){
        if(!nativeLowBase){const auto a=unsigned(address)&0xffffff;clockMem(a,lastwrite);if(bus_)bus_->write(a,uint8_t(value));else memory[a]=uint8_t(value);return;}
        const auto a=unsigned(address)&0xffffff;clockMem(a,lastwrite);
        if(a>=0xb8e8 && a<0x200000 && nativeLowWritable){memory[0xa00000+a]=uint8_t(value);return;}
        if((a>=0xa0b8e8 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00)){memory[a]=uint8_t(value);return;}
        if(bus_)bus_->write(a,uint8_t(value));else memory[a]=uint8_t(value);
    }
    void write16(int16_t v,int a){write8(int8_t(v>>8),a);write8(int8_t(v),a+1);}
    void write32(int32_t v,int a){write16(int16_t(v>>16),a);write16(int16_t(v),a+2);}
protected:
    ControllerResourceBus* bus_=nullptr;
};
}
