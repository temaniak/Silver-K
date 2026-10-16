#include "ksLiveMidiController.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>

namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Memory final:H8SDevice {
    bool overlay=true,rejectCode=false;std::array<uint8_t,4096> rom{};uint64_t ioReads=0,ioWrites=0;
    static bool ram(unsigned a){return (a>=0xa00000 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00);}
    static bool io(unsigned a){return a>=0xfffe00||(a>=0x400000 && a<0x400010)||(a>=0x600000 && a<0x600010);}
    uint8_t read(uint32_t a)override{
        if(a<0x200000)return overlay?state->memory[0xa00000+a]:a<rom.size()?rom[a]:0xff;
        if(ram(a))return state->memory[a];
        if(io(a))return uint8_t(state->memory[a]^uint8_t(ioReads++));
        throw std::runtime_error("Unmapped test memory");
    }
    void write(uint32_t a,uint8_t v)override{
        auto code=a>=0xa00000 && a<0xc00000?a-0xa00000:a;
        if(rejectCode && code>=0x490 && code<0xb8e8)throw std::logic_error("Protected code");
        if(a<0x200000){if(overlay)state->memory[0xa00000+a]=v;return;}
        if(ram(a)){state->memory[a]=v;return;}
        if(io(a)){++ioWrites;state->memory[a]=v;return;}
        throw std::runtime_error("Unmapped test memory");
    }
};
}
int main(){try{
    auto reference=std::make_unique<ks::ControllerCpu>(),native=std::make_unique<ks::ControllerCpu>();
    Memory x,y;for(unsigned n=0;n<x.rom.size();++n)x.rom[n]=y.rom[n]=uint8_t(n*17);
    reference->memmap(&x,0,1<<24);native->memmap(&y,0,1<<24);
    std::mt19937 random(0x4b534d45);uint64_t operations=0;
    for(bool overlay:{true,false,true}){
        x.overlay=y.overlay=overlay;native->nativeLowBase=overlay?native->memory+0xa00000:y.rom.data();
        native->nativeLowSize=overlay?0x200000:unsigned(y.rom.size());native->nativeLowWritable=overlay;
        for(unsigned n=0;n<50000;++n){
            const unsigned bases[]{0x16000,0xa16000,0xffec00,0x400000,0x600000,0xffff80,0x100,0xa00100};
            const auto type=random()%8;const int address=int(bases[type]+random()%(type==3 || type==4?12:128));
            const auto value=int32_t(random());
            switch(random()%6){
            case 0:check(reference->read8(address)==native->read8(address),"Native byte read differs");break;
            case 1:check(reference->read16(address)==native->read16(address),"Native word read differs");break;
            case 2:check(reference->read32(address)==native->read32(address),"Native long read differs");break;
            case 3:reference->write8(int8_t(value),address);native->write8(int8_t(value),address);break;
            case 4:reference->write16(int16_t(value),address);native->write16(int16_t(value),address);break;
            case 5:reference->write32(value,address);native->write32(value,address);break;
            }
            ++operations;check(reference->cycles==native->cycles && reference->lastread==native->lastread && reference->lastwrite==native->lastwrite,"Native RAM bus timing differs");
        }
    }
    // Unaligned and crossing reads exercise actual H8 bus clocks/callbacks,
    // including partial reads that throw and the 24-bit address wrap.
    for(bool overlay:{true,false}){
        x.overlay=y.overlay=overlay;native->nativeLowBase=overlay?native->memory+0xa00000:y.rom.data();
        native->nativeLowSize=overlay?0x200000:unsigned(y.rom.size());native->nativeLowWritable=overlay;
        for(unsigned address:{0u,1u,4093u,4094u,4095u,0x1ffffdu,0x1ffffeu,0x1fffffu,
            0xa00001u,0xbffffdu,0xbffffeu,0xbfffffu,0xffdbffu,0xffdc01u,0xfffbfdu,
            0xfffbfeu,0xfffbffu,0xfffc00u,0x3fffffu,0x400001u,0x40000du,0x40000fu,
            0x60000fu,0xfffffeu,0xffffffu})for(unsigned width:{2u,4u}){
            uint32_t a=0,b=0;bool ax=false,bx=false;
            try{a=width==2?reference->read16(int(address)):reference->read32(int(address));}catch(const std::runtime_error&){ax=true;}
            try{b=width==2?native->read16(int(address)):native->read32(int(address));}catch(const std::runtime_error&){bx=true;}
            check(ax==bx && (ax || a==b),"Native crossing read result/exception differs");
            check(reference->cycles==native->cycles && reference->lastread==native->lastread,"Native crossing read clocks differ");
            check(x.ioReads==y.ioReads,"Native crossing read bypassed IO");++operations;
        }
    }
    check(std::equal(std::begin(reference->memory),std::end(reference->memory),std::begin(native->memory)),"Native final RAM/IO bytes differ");
    check(x.ioReads==y.ioReads && x.ioWrites==y.ioWrites && x.ioReads>0 && x.ioWrites>0,"Native RAM path bypassed MMIO side effects");
    y.rejectCode=true;bool rejected=false;const auto old=native->memory[0xa00562];
    try{native->write8(55,0x562);}catch(const std::logic_error&){rejected=true;}
    check(rejected && native->memory[0xa00562]==old,"Native RAM path bypassed code protection");
    rejected=false;try{native->read8(0x300000);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"Native RAM path accepted unmapped memory");
    std::cout<<"PASS: "<<operations<<" native RAM/ROM-overlay operations, bytes/endianness/clocks/IO side effects exact; code and unmapped-access guards\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
