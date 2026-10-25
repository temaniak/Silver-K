#pragma once
#include "ksControlArithmetic.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace ks {
class ControllerCpu;
class ControllerBus {
public:
    virtual ~ControllerBus()=default;
    virtual uint8_t read(uint32_t)=0;
    virtual void write(uint32_t,uint8_t)=0;
    void setState(ControllerCpu* p){state=p;}
protected:
    ControllerCpu* state=nullptr;
};
class ControllerReference {
public:
    virtual ~ControllerReference()=default;
    virtual void step(ControllerCpu&)=0;
protected:
    static uint8_t mappedRead(ControllerCpu&,uint32_t);
    static void mappedWrite(ControllerCpu&,uint32_t,uint8_t);
};
using ControllerReferenceFactory=std::unique_ptr<ControllerReference>(*)();
// Only test executables link an implementation. Register once, before main.
void setControllerReferenceFactoryForTests(ControllerReferenceFactory);
bool controllerReferenceAvailable();
// Temporary compatibility state, NOT the completed semantic controller. This
// owns no emulator, opcode decoder or 128 MiB generic memory-map table. Its
// remaining register/flags/stack/continuation ABI still has to be eliminated.
class ControllerCpu final {
public:
    ControllerCpu();
    ~ControllerCpu();
    ControllerCpu(const ControllerCpu&)=delete;
    ControllerCpu& operator=(const ControllerCpu&)=delete;
    // Comparison-only adapter. Production has no registered implementation;
    // native execution locks this entry even in a differential-test process.
    void step();
    uint8_t memory[1<<24]{};
    struct Value {
        union {struct {union {struct {int8_t rl,rh;};int16_t r;};int16_t e;};int32_t er=0;};
    } regs[8]{};
    uint32_t pc=0;
    uint8_t ccr=128,exr=0;
    uint64_t cycles=0,pending_irqs=0,pending_irqs_high=0;
    bool interruptMode2=false;
    uint8_t interruptPriority[128]{};
    int indent=0,lastread=-1,lastwrite=-1;
    int getPC()const{return int32_t(pc);}
    static uint32_t makepc(uint32_t a){return a;}
    static int pcoff(uint32_t a){return int32_t(a);}
    uint32_t getSP()const{return uint32_t(regs[7].er);}
    void setSP(uint32_t a){regs[7].er=int32_t(a);}
    void loadmem(const uint8_t* data,uint32_t size,uint32_t address){
        if(address>sizeof(memory) || size>sizeof(memory)-address)throw std::out_of_range("KS image range");
        std::memcpy(memory+address,data,size);
    }
    void memmap(ControllerBus* device,int start,int length=1);
    void boot(){pc=read32(0)&0xffffff;cycles=pending_irqs=pending_irqs_high=0;ccr=128;exr=0;}
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
    uint64_t interpretedSteps=0;
    bool interpreterAllowed=true;
    bool nativeFetchClocksOnly=false;
    // Native data RAM access avoids the H8 device-map dispatch. MMIO, Flash,
    // waves and protected code writes still use the actual Board callbacks.
    const uint8_t* nativeLowBase=nullptr;
    unsigned nativeLowSize=0;
    bool nativeLowWritable=false;
    int8_t read8(int address){
        if(!nativeLowBase){const auto a=unsigned(address)&0xffffff;clockMem(a,lastread);return bus?int8_t(bus->read(a)):int8_t(memory[a]);}
        const auto a=unsigned(address)&0xffffff;clockMem(a,lastread);
        if(a<0x200000)return a<nativeLowSize?int8_t(nativeLowBase[a]):int8_t(-1);
        if((a>=0xa00000 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00))return int8_t(memory[a]);
        return bus?int8_t(bus->read(a)):int8_t(memory[a]);
    }
    uint16_t read16(int a){const auto high=uint8_t(read8(a));return uint16_t((unsigned(high)<<8)|uint8_t(read8(a+1)));}
    uint32_t read32(int a){const auto high=read16(a);return (uint32_t(high)<<16)|read16(a+2);}
    void write8(int8_t value,int address){
        if(!nativeLowBase){const auto a=unsigned(address)&0xffffff;clockMem(a,lastwrite);if(bus)bus->write(a,uint8_t(value));else memory[a]=uint8_t(value);return;}
        const auto a=unsigned(address)&0xffffff;clockMem(a,lastwrite);
        if(a>=0xb8e8 && a<0x200000 && nativeLowWritable){memory[0xa00000+a]=uint8_t(value);return;}
        if((a>=0xa0b8e8 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00)){memory[a]=uint8_t(value);return;}
        if(bus)bus->write(a,uint8_t(value));else memory[a]=uint8_t(value);
    }
    void write16(int16_t v,int a){write8(int8_t(v>>8),a);write8(int8_t(v),a+1);}
    void write32(int32_t v,int a){write16(int16_t(v>>16),a);write16(int16_t(v),a+2);}
    void pushL(uint32_t v){auto sp=getSP()-4;write32(int32_t(v),sp);setSP(sp);}
    void pushW(uint16_t v){auto sp=getSP()-2;write16(int16_t(v),sp);setSP(sp);}
    uint32_t popL(){auto sp=getSP();setSP(sp+4);return read32(sp);}
    uint16_t popW(){auto sp=getSP();setSP(sp+2);return read16(sp);}
    void pushPC(uint32_t target,bool flags=false){auto v=uint32_t(pcoff(target))&0xffffff;pushL(v|(flags?uint32_t(ccr)<<24:0));}
    uint32_t popPC(bool flags=false){auto v=popL();if(flags)ccr=uint8_t(v>>24);return makepc(v&0xffffff);}
    uint32_t loadPCFrom(uint32_t a){return makepc(read32(a)&0xffffff);}
    int8_t add8(int a,int b,int carry=0,bool keepZero=false){
        const auto v=controlArithmetic::add(int8_t(a),int8_t(b),ccr,unsigned(carry),keepZero);ccr=v.flags;return v.value;
    }
    int8_t sub8(int a,int b,int carry=0,bool keepZero=false){
        const auto v=controlArithmetic::subtract(int8_t(a),int8_t(b),ccr,unsigned(carry),keepZero);ccr=v.flags;return v.value;
    }
    int16_t add16(int a,int b){const auto v=controlArithmetic::add(int16_t(a),int16_t(b),ccr);ccr=v.flags;return v.value;}
    int16_t sub16(int a,int b){const auto v=controlArithmetic::subtract(int16_t(a),int16_t(b),ccr);ccr=v.flags;return v.value;}
    int32_t add32(int a,int b,bool noFlags=false){
        const auto v=controlArithmetic::add(int32_t(a),int32_t(b),ccr);if(!noFlags)ccr=v.flags;return v.value;
    }
    int32_t sub32(int a,int b,bool noFlags=false){
        const auto v=controlArithmetic::subtract(int32_t(a),int32_t(b),ccr);if(!noFlags)ccr=v.flags;return v.value;
    }
private:
    friend class ControllerReference;
    friend class RuntimeController;
    friend class StateStoreMenuForeground;
    friend class StateProgramBrowserForeground;
    friend class ClockSyncForeground;
    friend class StateDeferredPartsForeground;
    friend class StateRefreshForeground;
    friend class StateFactoryForeground;
    friend class StateProgramLoadForeground;
    friend class StatePlaybackModeForeground;
    friend class StatePartNavigationForeground;
    friend class StateEditActionForeground;
    friend class StateSceneToggleForeground;
    friend class StateSceneEditForeground;
    friend class StateSceneForeground;
    friend class StateVoiceRefreshForeground;
    friend class GlobalUpdateForeground;
    friend class ParameterUploadForeground;
    friend class StateScreenForeground;
    friend class PanelSettingsForeground;
    friend class StateFieldForeground;
    friend class StateSaveForeground;
    friend class StateApplyForeground;
    friend class StateDisplayForeground;
    friend class StatePanelForeground;
    friend class StateMenuForeground;
    friend class StateStoreForeground;
    friend class StateEventsForeground;
    friend class PanelControlsForeground;
    friend class PanelScanForeground;
    friend class SerialForeground;
    friend class StateResetForeground;
    // Temporary edge to the remaining compatibility controller. Native jobs
    // supply transfer timestamps themselves; these accesses do not clock a CPU.
    uint8_t readResource(uint32_t a) { a &= 0xffffff; return bus ? bus->read(a) : memory[a]; }
    void writeResource(uint32_t a,uint8_t v) { a &= 0xffffff; if(bus)bus->write(a,v);else memory[a]=v; }
    ControllerBus* bus=nullptr;
    std::unique_ptr<ControllerReference> reference;
};
}
