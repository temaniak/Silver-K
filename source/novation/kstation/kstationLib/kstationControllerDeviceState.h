#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace kstation {
class ControllerDeviceState;
// Board-specific mapping and peripherals supply their own decoding. The bus
// receives the physical 24-bit address after the device clock is advanced.
class ControllerResourceBus {
public:
 virtual ~ControllerResourceBus()=default;
 virtual bool mapped(uint32_t address)const=0;
 virtual uint8_t read(uint32_t address)=0;
 virtual void write(uint32_t address,uint8_t value)=0;
 void attachState(ControllerDeviceState* state){state_=state;}
protected:
 ControllerDeviceState* state_=nullptr;
};

// Board memory, bus timing and interrupt requests, without CPU registers,
// program counters, architectural frames or firmware instruction dispatch.
// Create the owning instance outside realtime processing.
class ControllerDeviceState final {
public:
 static constexpr size_t MemorySize=1u<<24;
 static constexpr unsigned ImmutableCapacity=8192;
 ControllerDeviceState()=default;
 ControllerDeviceState(const ControllerDeviceState&)=delete;
 ControllerDeviceState&operator=(const ControllerDeviceState&)=delete;
 uint8_t memory[MemorySize]{};
 uint64_t cycles=0,pendingIrqs=0,pendingIrqsHigh=0;
 bool interruptMode2=false;
 uint8_t interruptPriority[128]{};
 int lastRead=-1,lastWrite=-1;
 void attachBus(ControllerResourceBus* bus){bus_=bus;if(bus_)bus_->attachState(this);}
 bool protectImmutable(uint32_t begin,uint32_t end){if(begin>=end||end>MemorySize||protectedCount_==protected_.size())return false;protected_[protectedCount_++]={begin,end,false};return true;}
 bool protectImmutableVerified(uint32_t begin,uint32_t end,const uint8_t*expected){if(!expected||begin>=end||end>MemorySize||protectedCount_==protected_.size())return false;for(uint32_t a=begin;a<end;++a)if(readResource(a)!=expected[a-begin])return false;protected_[protectedCount_++]={begin,end,true};return true;}
 bool isImmutable(uint32_t address)const{auto a=address&0xffffff;for(unsigned i=0;i<protectedCount_;++i)if(a>=protected_[i].begin&&a<protected_[i].end)return true;return false;}
 bool hasImmutableCapacity(unsigned additional)const{return additional<=protected_.size()-protectedCount_;}
 bool isRangeVerifiedImmutable(uint32_t begin,uint32_t end)const{if(begin>=end||end>MemorySize)return false;auto cursor=begin;while(cursor<end){auto furthest=cursor;for(unsigned i=0;i<protectedCount_;++i)if(protected_[i].verified&&protected_[i].begin<=cursor&&protected_[i].end>furthest)furthest=protected_[i].end;if(furthest==cursor)return false;cursor=furthest;}return true;}
 bool isVerifiedImmutable(uint32_t address)const{auto a=address&0xffffff;for(unsigned i=0;i<protectedCount_;++i)if(protected_[i].verified&&a>=protected_[i].begin&&a<protected_[i].end)return true;return false;}
 bool hasImmutableBindings()const{return protectedCount_!=0;}
 unsigned immutableBindingCount()const{return protectedCount_;}
 // A bound runtime must be recreated together with its board state.
 void resetForBoot(){if(protectedCount_)throw std::logic_error("Cannot reset bound K controller state");std::memset(memory,0,sizeof(memory));cycles=pendingIrqs=pendingIrqsHigh=0;interruptMode2=false;std::memset(interruptPriority,0,sizeof(interruptPriority));lastRead=lastWrite=-1;protectedCount_=0;}
 void loadMemory(const uint8_t* data,size_t size,size_t address){if(address>MemorySize||size>MemorySize-address)throw std::out_of_range("K controller image range");for(unsigned i=0;i<protectedCount_;++i)if(size&&address<protected_[i].end&&protected_[i].begin<address+size)throw std::logic_error("K immutable controller image");std::memcpy(memory+address,data,size);}
 void clockInternal(unsigned clocks){cycles+=clocks;}
 void clockMemory(uint32_t address,int& previous){const auto a=address&0xffffff;if(a<0x400000){if((previous&1)||a!=uint32_t(previous|1))cycles+=3;previous=int(a);}else if(a>=0xfffd10&&a<0xffffa0){if((previous&1)||a!=uint32_t(previous|1))++cycles;previous=int(a);}else{cycles+=a>=0xffffa0?1:3;previous=-1;}}
 // Untimed byte transactions support semantic phase schedulers that have
 // already computed the exact bus timestamp and history.
 uint8_t readResource(uint32_t address){auto a=address&0xffffff;return bus_&&bus_->mapped(a)?bus_->read(a):memory[a];}
 void writeResource(uint32_t address,uint8_t value){auto a=address&0xffffff;if(isImmutable(a))throw std::logic_error("K immutable controller resource");if(bus_&&bus_->mapped(a))bus_->write(a,value);else memory[a]=value;}
 uint8_t read8(uint32_t address){auto a=address&0xffffff;clockMemory(a,lastRead);return readResource(a);}
 uint16_t read16(uint32_t a){auto hi=read8(a);return uint16_t((unsigned(hi)<<8)|read8(a+1));}
 uint32_t read32(uint32_t a){auto hi=read16(a);return (uint32_t(hi)<<16)|read16(a+2);}
 void write8(uint32_t address,uint8_t value){auto a=address&0xffffff;if(isImmutable(a))throw std::logic_error("K immutable controller resource");clockMemory(a,lastWrite);writeResource(a,value);}
 void write16(uint32_t a,uint16_t v){for(unsigned i=0;i<2;++i)if(isImmutable(a+i))throw std::logic_error("K immutable controller resource");write8(a,uint8_t(v>>8));write8(a+1,uint8_t(v));}
 void write32(uint32_t a,uint32_t v){for(unsigned i=0;i<4;++i)if(isImmutable(a+i))throw std::logic_error("K immutable controller resource");write16(a,uint16_t(v>>16));write16(a+2,uint16_t(v));}
 void requestInterrupt(int vector){if(vector<0||vector>=128)return;if(interruptMode2){if(vector<64)pendingIrqs|=uint64_t(1)<<vector;else pendingIrqsHigh|=uint64_t(1)<<(vector-64);return;}if(vector>=64)return;if(vector>=12&&vector<18&&!(read8(0xfffff5)&(1u<<(vector-12))))return;pendingIrqs|=uint64_t(1)<<vector;}
private:
 struct ImmutableRange{uint32_t begin=0,end=0;bool verified=false;};
 std::array<ImmutableRange,ImmutableCapacity>protected_{};unsigned protectedCount_=0;
 ControllerResourceBus* bus_=nullptr;
};
}
