#include "kstationH8.h"
#include "kstationMidiQueue.h"
#include "kstationHdiWait.h"
#include "kstationNativeTimers.h"
#include "baseLib/sha256.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>
#include "h8s/h8sdevices.hpp"

namespace kstation
{
    namespace
    {
        class KStationTimers final : public Timers
        {
        public:
            void tick()
            {
                // H8/300H TMDR.MDF selects phase counting for channel 2.
                // Firmware 8BA reads TCNT2 as the program rotary encoder.
                // No external quadrature transitions means no count change;
                // the shared legacy timer implementation ignores this mode.
                const auto start=Timers::read(0xffff60);
                const bool phaseCounting=(Timers::read(0xffff62)&0x40)!=0;
                if(phaseCounting)Timers::write(0xffff60,start&~4);
                Timers::tick();
                if(phaseCounting)Timers::write(0xffff60,start);
            }
        };
        template<size_t Size> struct ByteQueue
        {
            std::array<uint8_t,Size> data{};
            size_t read=0,write=0;
            bool push(uint8_t v) {if(write-read==Size)return false;data[(write++)%Size]=v;return true;}
            bool pop(uint8_t& v) {if(read==write)return false;v=data[(read++)%Size];return true;}
            size_t available() const {return Size-(write-read);}
            void clear(){read=write=0;}
        };
    }

    struct H8::Impl final : H8SDevice
    {
        std::unique_ptr<Stage1Cpu> cpu=std::make_unique<Stage1Cpu>();
        std::unique_ptr<KStationTimers> timers;
        std::unique_ptr<NativeTimers> nativeTimers;
        const Stage1Mode mode;
        bool nativeWaitReady=false;
        uint64_t nativeWaitSteps=0;
        HostCallbacks host;
        std::vector<uint8_t> rom,flash,initialFlash;
        std::array<uint8_t,256> regs{};
        std::array<char,128> lcd{};
        std::array<uint8_t,64> lcdCharacters{};
        uint8_t lcdAddress=0,lcdData=0;
        bool lcdCgram=false;
        MidiSerialQueue<8192> midiIn;ByteQueue<8192> midiOut;
        uint64_t nextRx=0,txReady=0,adcReady=0,instructions=0,hostReads=0,hostWrites=0,midiReceived=0;
        bool overlay=false,enteredMain=false,ready=false,reconstructedDefaults=false;
        unsigned flashCommand=0,pageRemaining=0;
        uint64_t flashRevision=0;

        Impl(const std::vector<uint8_t>& image,HostCallbacks callbacks,const std::vector<uint8_t>& storage,Stage1Mode stage1)
        : mode(stage1),host(std::move(callbacks)),rom(image)
        {
            if(image.size()!=49728)throw std::invalid_argument("K-Station requires decoded OS 2.1.01");
            if(mode!=Stage1Mode::Original)
            {
                constexpr baseLib::SHA256 supported("2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07");
                if(baseLib::SHA256(image)!=supported)throw std::invalid_argument("Native K stage 1 requires verified OS 2.1.01");
            }
            if(!host.read || !host.write)throw std::invalid_argument("Missing DSP HDI08 callbacks");
            if(!storage.empty() && storage.size()!=FlashSize)throw std::invalid_argument("Wrong K-Station flash size");
            initialFlash=storage.empty()?std::vector<uint8_t>(FlashSize,0xff):storage;
            const bool erasedGlobals=std::all_of(initialFlash.begin()+0x1ff00,initialFlash.begin()+0x20000,
                                                [](uint8_t b){return b==0xff;});
            if(erasedGlobals)
            {
                // Explicit initialization mode, not a claimed factory-bank dump.
                // These defaults and the 128-byte initialized patch are contained
                // in the supplied OS. All other program slots remain erased.
                std::fill(initialFlash.begin()+0x1ff00,initialFlash.begin()+0x20000,0);
                std::copy(image.begin()+0xbb12,image.begin()+0xbb29,initialFlash.begin()+0x1ff00);
                std::copy(image.begin()+0xba5c,image.begin()+0xbadc,initialFlash.begin()+0x20000);
                std::copy(image.begin()+0xba5c,image.begin()+0xbadc,initialFlash.begin()+0x26400);
                reconstructedDefaults=true;
            }
            uint32_t checksum=0;
            for(size_t i=0;i<image.size();i+=2)checksum+=(uint32_t(image[i])<<8)|image[i+1];
            // The update payload excludes the physical flash-container checksum.
            // Rebuild that verified transport field; execute the original loader.
            for(int shift:{24,16,8,0})rom.push_back(static_cast<uint8_t>(checksum>>shift));
            std::copy(rom.begin(),rom.end(),initialFlash.begin()+0x10000);
            cpu->memmap(this,0,0x10000);
            cpu->memmap(this,0x200000,FlashSize);
            cpu->memmap(this,0x600000,16);
            cpu->memmap(this,0xffff1c,0xe4);
            // Protect both the low overlay window and its physical RAM alias.
            // All normal H8 stores, including 24-bit wrapped addresses, must
            // reach the same immutable native-wait code guard after handoff.
            if(mode==Stage1Mode::TimersAndHdi)cpu->memmap(this,0x401672,0x12e);
            reset();
        }
        void reset()
        {
            if(host.resetDSP)host.resetDSP(true);
            std::memset(cpu->memory,0,sizeof(cpu->memory));
            for(auto& r:cpu->regs)r.er=0;
            cpu->loadmem(rom.data(),static_cast<uint32_t>(rom.size()),0);
            flash=initialFlash;regs.fill(0);lcd.fill(' ');lcdCharacters.fill(0);lcdAddress=lcdData=0;lcdCgram=false;
            regs[0xb4]=0x84;regs[0xce]=0xff;regs[0xc7]=0xff;
            overlay=enteredMain=ready=false;flashCommand=pageRemaining=0;
            nativeWaitReady=false;nativeWaitSteps=0;
            instructions=hostReads=hostWrites=midiReceived=0;nextRx=txReady=adcReady=0;
            midiIn.clear();midiOut.clear();
            if(mode==Stage1Mode::Original)
            {
                nativeTimers.reset();timers=std::make_unique<KStationTimers>();
                cpu->memmap(timers.get(),0xffff60,64);
            }
            else
            {
                timers.reset();nativeTimers=std::make_unique<NativeTimers>();
                cpu->memmap(nativeTimers.get(),0xffff60,64);
            }
            cpu->boot();
        }
        uint8_t raw(uint32_t address) const
        {
            address&=0xffffff;
            if(address<0x10000)return overlay?cpu->memory[0x400000+address]:(address<rom.size()?rom[address]:0xff);
            if(address>=0x200000 && address<0x240000)return flash[address-0x200000];
            return cpu->memory[address];
        }
        uint8_t read(uint32_t address) override
        {
            if(address<0x10000 || (address>=0x200000 && address<0x240000))return raw(address);
            if(address>=0x401672&&address<0x4017a0)return cpu->memory[address];
            if(address>=0x600000 && address<0x600010){++hostReads;return host.read(address-0x600000);}
            const auto r=address&255;
            if(r==0xc7 && !(regs[0xc5]&0xff))
            {
                if(regs[0xd3]&0x40)return (regs[0xd3]&8)?
                    (lcdCgram?lcdCharacters[lcdAddress&63]:static_cast<uint8_t>(lcd[lcdAddress&127])):lcdAddress;
                return 0xff; // All keyboard/front-panel switches released.
            }
            if(r==0xce)return 0xff; // All front-panel switches released.
            // Firmware 1BD0 scans eight encoder groups through PBDR and
            // compares these quadrature pairs with its zeroed A856 history.
            if(r==0xd7)return 0; // Stationary encoder pairs; no synthetic turn.
            if(r>=0xe0 && r<0xe8)return (r&1)?0:0x80;
            return regs[r];
        }
        void writeFlash(uint32_t address,uint8_t value)
        {
            const auto offset=address-0x200000, unlock=offset&0x7ffff;
            if(flashCommand==3)
            {
                // Firmware BC0 issues AA/55/A0 then writes a complete 256-byte
                // page. Its page-program interface overwrites existing data.
                flash[offset]=value;
                if(--pageRemaining==0){flashCommand=0;++flashRevision;}
                return;
            }
            if(flashCommand==6 && value==0x30)
            {
                // Byte-program command family used by the firmware. Sector
                // layout needs a confirmed flash part before exposing erasure.
                throw std::runtime_error("K-Station flash sector erase requires a confirmed flash chip layout");
            }
            if(flashCommand==0 && unlock==0x5555 && value==0xaa)flashCommand=1;
            else if(flashCommand==1 && unlock==0x2aaa && value==0x55)flashCommand=2;
            else if(flashCommand==2 && unlock==0x5555 && value==0xa0){flashCommand=3;pageRemaining=256;}
            else if(flashCommand==2 && unlock==0x5555 && value==0x80)flashCommand=4;
            else if(flashCommand==4 && unlock==0x5555 && value==0xaa)flashCommand=5;
            else if(flashCommand==5 && unlock==0x2aaa && value==0x55)flashCommand=6;
            else flashCommand=0;
        }
        void lcdLatch()
        {
            if(regs[0xd3]&8)
            {
                if(lcdCgram)lcdCharacters[lcdAddress&63]=lcdData&31;
                else lcd[lcdAddress&127]=static_cast<char>(lcdData);
                lcdAddress=(lcdAddress+1)&(lcdCgram?63:127);
            }
            else if(lcdData&0x80){lcdAddress=lcdData&127;lcdCgram=false;}
            else if(lcdData&0x40){lcdAddress=lcdData&63;lcdCgram=true;}
            else if(lcdData==1){lcd.fill(' ');lcdAddress=0;lcdCgram=false;}
            else if(lcdData==2){lcdAddress=0;lcdCgram=false;}
        }
        void write(uint32_t address,uint8_t value) override
        {
            if(address<0x10000){if(overlay){
                if(nativeWaitReady&&address>=0x1672&&address<0x17a0)
                    throw std::runtime_error("Cannot modify verified native K wait code");
                cpu->memory[0x400000+address]=value;
            }return;}
            if(address>=0x401672&&address<0x4017a0){
                if(nativeWaitReady)throw std::runtime_error("Cannot modify native K wait physical alias");
                cpu->memory[address]=value;return;
            }
            if(address>=0x200000 && address<0x240000){writeFlash(address,value);return;}
            if(address>=0x600000 && address<0x600010)
            {
                ++hostWrites;
                if(!host.write(address-0x600000,value))throw std::runtime_error("H8 wrote a busy DSP HDI08 host port");
                return;
            }
            const auto r=address&255;
            const auto old=regs[r];
            regs[r]=value;
            if(r==0xcf)overlay=(value&1)==0; // Original P8 bank select.
            else if(r==0xd2 && ((old^value)&2) && host.resetDSP)host.resetDSP((value&2)==0);
            else if(r==0xc7)lcdData=value;
            else if(r==0xd3 && (old&0x80) && !(value&0x80) && !(value&0x10) && (value&0x40))lcdLatch();
            else if(r==0xb2 && (value&0x20) && !(old&0x20))regs[0xb4]|=0x84;
            else if(r==0xb3)
            {
                if(!midiOut.push(value))throw std::runtime_error("K-Station MIDI output queue overflow");
                regs[0xb4]&=0x7b;
                txReady=cpu->cycles+5120;
            }
            else if(r==0xe8 && (value&0x20) && !(old&0x20))adcReady=cpu->cycles+1064;
        }
        void tickSerial()
        {
            const auto now=cpu->cycles;
            // Four-channel scan, normal-speed conversion. Exact part-specific
            // ADC divider is provisional until a K-Station MCU marking is known.
            if(adcReady && now>=adcReady && (regs[0xe8]&0x20) && !(regs[0xe8]&0x80))
            {
                regs[0xe8]|=0x80;adcReady=now+1064;
                if(regs[0xe8]&0x40)cpu->interrupt(60);
                if(!(regs[0xe8]&0x10))regs[0xe8]&=~0x20;
            }
            if(txReady && now>=txReady)
            {
                regs[0xb4]|=0x84;txReady=0;
                if(regs[0xb2]&0x80)cpu->interrupt(54);
                if(regs[0xb2]&4)cpu->interrupt(55);
            }
            if((regs[0xb2]&0x10) && now>=nextRx && !(regs[0xb4]&0x40))
            {
                uint8_t byte;
                if(midiIn.pop(byte))
                {
                    regs[0xb5]=byte;regs[0xb4]|=0x40;
                    nextRx=now+5120;++midiReceived;
                    if(regs[0xb2]&0x40)cpu->interrupt(53);
                }
            }
        }
        template<bool Native> uint64_t execCore(uint32_t budget)
        {
            const auto begin=cpu->cycles;
            while(cpu->cycles-begin<budget)
            {
                const auto pc=static_cast<uint32_t>(cpu->getPC());
                if(pc==0x3a58)enteredMain=true;
                if(enteredMain && pc==0x3ae4)
                {
                    bool valid=raw(0xcd82)>=1 && raw(0xcd82)<=4 && raw(0xcd83)<100;
                    for(unsigned i=0;i<128 && valid;++i)valid=raw(0xba5c+i)<128;
                    ready=valid;
                    if constexpr(Native)if(ready&&mode==Stage1Mode::TimersAndHdi&&!nativeWaitReady)
                    {
                        if(!overlay||!std::equal(rom.begin()+0x1672,rom.begin()+0x17a0,cpu->memory+0x401672))
                            throw std::runtime_error("Native K HDI handoff code mismatch");
                        nativeWaitReady=true;
                    }
                }
                if constexpr(Native)
                {
                    if(nativeWaitReady&&NativeHdiWait::step(*cpu))++nativeWaitSteps;
                    else cpu->step();
                }
                else cpu->step();
                ++instructions;
                if constexpr(Native)nativeTimers->tick();else timers->tick();
                tickSerial();
            }
            return cpu->cycles-begin;
        }
        uint64_t exec(uint32_t budget){return mode==Stage1Mode::Original?execCore<false>(budget):execCore<true>(budget);}
    };

    H8::H8(const std::vector<uint8_t>& image,HostCallbacks cb,const std::vector<uint8_t>& flash,Stage1Mode mode)
    :m_impl(std::make_unique<Impl>(image,std::move(cb),flash,mode)){}
    H8::~H8()=default;
    void H8::reset(){m_impl->reset();}
    uint64_t H8::exec(uint32_t budget){return m_impl->exec(budget);}
    bool H8::enqueueMidi(const uint8_t* bytes,size_t size)
    {
        return m_impl->midiIn.push(bytes,size);
    }
    bool H8::popMidiOut(uint8_t& byte){return m_impl->midiOut.pop(byte);}
    bool H8::bootReady() const{return m_impl->ready;}
    uint8_t H8::readMemory(uint32_t address) const{return m_impl->raw(address);}
    void H8::copyCurrentPatch(uint8_t* p) const
    {
        for(unsigned i=0;i<128;++i)p[i]=m_impl->raw(0xba5c+i);
        // Local panel edits operate on decoded mirrors. Serialize these only
        // into the caller's snapshot, following original 4F48/525C; no guest
        // memory writes or calls. Preserve bits outside each proven field.
        struct Mirror {uint8_t packed,decoded,mask,shift,bias;};
        static constexpr Mirror fields[]{
            {0,0x81,7,0,0},{0,0x80,0x18,3,0},{0,0x82,0x60,5,0},
            {5,0x83,3,0,0},{5,0x84,0x0c,2,0},{5,0x85,0x30,4,0},{5,0x86,0x40,6,0},
            {6,0x87,3,0,63},{6,0x88,0x0c,2,63},{6,0x89,0x30,4,63},{6,0x8a,0x40,6,0},
            {0x4e,0x9b,1,0,0},{0x4e,0x9c,2,1,0},{0x4e,0x8b,0x0c,2,0},{0x4e,0x8c,0x30,4,0},
            {0x4f,0xa0,1,0,0},{0x4f,0xa1,2,1,0},{0x4f,0xa2,4,2,0},
            {0x4f,0xa3,8,3,0},{0x4f,0xa4,0x10,4,0},{0x4f,0xa5,0x20,5,0},
            {0x50,0x9d,1,0,0},{0x50,0x9e,2,1,0},{0x50,0x9f,4,2,0},{0x50,0x8d,0x78,3,0},
            {0x58,0x8e,3,0,0},{0x58,0x8f,4,2,0},{0x58,0xa6,8,3,0},
            {0x58,0xa7,0x10,4,0},{0x58,0x90,0x60,5,0},
            {0x79,0x91,7,0,0},{0x79,0x92,0x18,3,0},
            {0x7a,0x93,3,0,0},{0x7a,0x94,0x0c,2,0},{0x7a,0x95,0x30,4,0},
            {0x7b,0x96,7,0,0},{0x7b,0x98,8,3,0},{0x7b,0x97,0x10,4,0},
            {0x7b,0x99,0x20,5,0},{0x7b,0x9a,0x40,6,0},
            {0x7f,0xa8,3,0,0},{0x7f,0xa9,0x0c,2,0},{0x7f,0xaa,0x30,4,0},{0x7f,0xab,0x40,6,0},
            {0x7e,0xac,7,0,0},{0x7e,0xad,0x78,3,0}
        };
        for(const auto& f:fields)
        {
            const auto value=static_cast<uint8_t>(m_impl->raw(0xba5c+f.decoded)-f.bias);
            p[f.packed]=(p[f.packed]&~f.mask)|((value<<f.shift)&f.mask);
        }
    }
    std::vector<uint8_t> H8::saveFlash() const{return m_impl->flash;}
    void H8::copyFlash(std::vector<uint8_t>& out) const{out.assign(m_impl->flash.begin(),m_impl->flash.end());}
    uint64_t H8::flashRevision() const{return m_impl->flashRevision;}
    bool H8::hasProgram(uint8_t bank,uint8_t program) const
    {
        if(bank<1 || bank>4 || program>=100)return false;
        const size_t offset=0x26400+((size_t(bank)-1)*100+program)*128;
        for(size_t i=0;i<128;++i)if(m_impl->flash[offset+i]>=128)return false;
        return true;
    }
    h8state& H8::core(){return *m_impl->cpu;}
    H8::DebugState H8::debugState() const
    {
        DebugState s;
        s.pc=m_impl->cpu->getPC();s.sp=m_impl->cpu->getSP();s.cycles=m_impl->cpu->cycles;
        s.instructions=m_impl->instructions;s.hostReads=m_impl->hostReads;s.hostWrites=m_impl->hostWrites;
        s.interpretedSteps=m_impl->instructions-m_impl->nativeWaitSteps;s.nativeWaitSteps=m_impl->nativeWaitSteps;
        s.midiReceived=m_impl->midiReceived;s.ramOverlay=m_impl->overlay;s.mainEntered=m_impl->enteredMain;
        s.initializedFromFirmware=m_impl->reconstructedDefaults;
        s.currentBank=m_impl->raw(0xcd82);s.currentProgram=m_impl->raw(0xcd83);
        s.lcd.assign(m_impl->lcd.begin(),m_impl->lcd.end());
        return s;
    }
}
