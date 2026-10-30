#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerTypes.h"
#include "kstationControllerTimers.h"
#include "kstationMidiQueue.h"
#include "baseLib/sha256.h"
#include <algorithm>
#include <array>
#include <functional>
#include <vector>
#include <utility>
namespace kstation {
// Firmware data, board resources and peripheral clocks. This class never
// constructs a CPU and contains no instruction fetch or controller dispatcher.
class ControllerBoard final : public ControllerResourceBus {
 friend struct NativeBoardAccess;
 template<size_t N>struct ByteQueue{std::array<uint8_t,N>data{};size_t head=0,tail=0;bool push(uint8_t v){if(head-tail==N)return false;data[(head++)%N]=v;return true;}bool pop(uint8_t&v){if(head==tail)return false;v=data[(tail++)%N];return true;}void clear(){head=tail=0;}};
public:
 static constexpr size_t FlashSize=0x40000;
 ControllerBoard(ControllerDeviceState&state,const std::vector<uint8_t>&image,ControllerHostCallbacks callbacks,const std::vector<uint8_t>&storage={})
 :state_(state),host(std::move(callbacks)),rom(image){
  constexpr baseLib::SHA256 supported("2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07");
  if(image.size()!=49728||baseLib::SHA256(image)!=supported)throw std::invalid_argument("Native K requires verified OS2.1.01");
  if(!host.read||!host.write)throw std::invalid_argument("Missing native DSP host callbacks");
  if(!storage.empty()&&storage.size()!=FlashSize)throw std::invalid_argument("Wrong K flash size");
  initialFlash=storage.empty()?std::vector<uint8_t>(FlashSize,0xff):storage;
  if(std::all_of(initialFlash.begin()+0x1ff00,initialFlash.begin()+0x20000,[](uint8_t b){return b==0xff;})){
   std::fill(initialFlash.begin()+0x1ff00,initialFlash.begin()+0x20000,0);
   std::copy(image.begin()+0xbb12,image.begin()+0xbb29,initialFlash.begin()+0x1ff00);
   std::copy(image.begin()+0xba5c,image.begin()+0xbadc,initialFlash.begin()+0x20000);
   std::copy(image.begin()+0xba5c,image.begin()+0xbadc,initialFlash.begin()+0x26400);reconstructedDefaults=true;
  }
  uint32_t checksum=0;for(size_t i=0;i<image.size();i+=2)checksum+=(uint32_t(image[i])<<8)|image[i+1];
  for(int shift:{24,16,8,0})rom.push_back(uint8_t(checksum>>shift));
  std::copy(rom.begin(),rom.end(),initialFlash.begin()+0x10000);
  if(state_.hasImmutableBindings())throw std::logic_error("K board requires fresh controller state");
  state_.attachBus(this);reset();
 }
 void reset(){
  if(state_.hasImmutableBindings())throw std::logic_error("Cannot reset bound K board; recreate controller owners");
  if(host.resetDSP)host.resetDSP(true);
  state_.resetForBoot();state_.loadMemory(rom.data(),rom.size(),0);
  flash=initialFlash;regs.fill(0);lcd.fill(' ');lcdCharacters.fill(0);lcdAddress=lcdData=0;lcdCgram=false;
  regs[0xb4]=0x84;regs[0xce]=0xff;regs[0xc7]=0xff;
  overlay=false;flashCommand=pageRemaining=0;flashRevision=0;
  nextRx=txReady=adcReady=hostReads=hostWrites=midiReceived=0;midiIn.clear();midiOut.clear();timers_=ControllerTimers{};
 }
 bool mapped(uint32_t a)const override{return a<0x10000||(a>=0x200000&&a<0x240000)||(a>=0x400000&&a<0x410000)||(a>=0x600000&&a<0x600010)||a>=0xffff1c;}
 void tickAfterPhase(){timers_.tick(state_);tickSerial();}
 bool enqueueMidi(const uint8_t*bytes,size_t n){return midiIn.push(bytes,n);}bool popMidiOut(uint8_t&v){return midiOut.pop(v);}
 uint8_t readMemory(uint32_t a)const{return raw(a);}
 bool ramOverlay()const{return overlay;}bool initializedFromFirmware()const{return reconstructedDefaults;}
 const std::array<char,128>&lcdText()const{return lcd;}
 const std::vector<uint8_t>&flashData()const{return flash;}uint64_t storageRevision()const{return flashRevision;}
 uint64_t hostReadCount()const{return hostReads;}uint64_t hostWriteCount()const{return hostWrites;}uint64_t midiReceivedCount()const{return midiReceived;}
 ControllerTimers&timers(){return timers_;}
        uint8_t raw(uint32_t address) const
        {
            address&=0xffffff;
            if(address<0x10000)return overlay?state_.memory[0x400000+address]:(address<rom.size()?rom[address]:0xff);
            if(address>=0x200000 && address<0x240000)return flash[address-0x200000];
            return state_.memory[address];
        }
        uint8_t read(uint32_t address) override
        {
            if(address>=0xffff60&&address<0xffffa0)return timers_.read(address);
            if(address<0x10000 || (address>=0x200000 && address<0x240000))return raw(address);
            if(address>=0x400000&&address<0x410000)return state_.memory[address];
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
            if(address>=0xffff60&&address<0xffffa0){timers_.write(address,value);return;}
            if(address<0x10000){if(overlay){
                if(state_.isImmutable(0x400000+address))throw std::logic_error("K immutable controller overlay");
                state_.memory[0x400000+address]=value;
            }return;}
            if(address>=0x400000&&address<0x410000){
                if(state_.isImmutable(address))throw std::logic_error("K immutable controller alias");
                state_.memory[address]=value;return;
            }
            if(address>=0x200000 && address<0x240000){writeFlash(address,value);return;}
            if(address>=0x600000 && address<0x600010)
            {
                ++hostWrites;
                if(!host.write(address-0x600000,value))throw std::runtime_error("Native K wrote a busy DSP HDI08 host port");
                return;
            }
            const auto r=address&255;
            const auto old=regs[r];
            regs[r]=value;
            if(r==0xcf){overlay=(value&1)==0;if(expectedBus_&&state_.bus_==expectedBus_)state_.lowRead_=overlay?state_.memory+0x400000:nullptr;} // Original P8 bank select.
            else if(r==0xd2 && ((old^value)&2) && host.resetDSP)host.resetDSP((value&2)==0);
            else if(r==0xc7)lcdData=value;
            else if(r==0xd3 && (old&0x80) && !(value&0x80) && !(value&0x10) && (value&0x40))lcdLatch();
            else if(r==0xb2 && (value&0x20) && !(old&0x20))regs[0xb4]|=0x84;
            else if(r==0xb3)
            {
                if(!midiOut.push(value))throw std::runtime_error("K-Station MIDI output queue overflow");
                regs[0xb4]&=0x7b;
                txReady=state_.cycles+5120;
            }
            else if(r==0xe8 && (value&0x20) && !(old&0x20))adcReady=state_.cycles+1064;
        }
        void tickSerial()
        {
            const auto now=state_.cycles;
            // Four-channel scan, normal-speed conversion. Exact part-specific
            // ADC divider is provisional until a K-Station MCU marking is known.
            if(adcReady && now>=adcReady && (regs[0xe8]&0x20) && !(regs[0xe8]&0x80))
            {
                regs[0xe8]|=0x80;adcReady=now+1064;
                if(regs[0xe8]&0x40)state_.requestInterrupt(60);
                if(!(regs[0xe8]&0x10))regs[0xe8]&=~0x20;
            }
            if(txReady && now>=txReady)
            {
                regs[0xb4]|=0x84;txReady=0;
                if(regs[0xb2]&0x80)state_.requestInterrupt(54);
                if(regs[0xb2]&4)state_.requestInterrupt(55);
            }
            if((regs[0xb2]&0x10) && now>=nextRx && !(regs[0xb4]&0x40))
            {
                uint8_t byte;
                if(midiIn.pop(byte))
                {
                    regs[0xb5]=byte;regs[0xb4]|=0x40;
                    nextRx=now+5120;++midiReceived;
                    if(regs[0xb2]&0x40)state_.requestInterrupt(53);
                }
            }
        }
private:
 ControllerDeviceState&state_;ControllerHostCallbacks host;
 std::vector<uint8_t>rom,flash,initialFlash;
 std::array<uint8_t,256>regs{};std::array<char,128>lcd{};std::array<uint8_t,64>lcdCharacters{};
 uint8_t lcdAddress=0,lcdData=0;bool lcdCgram=false;
 MidiSerialQueue<8192>midiIn;ByteQueue<8192>midiOut;ControllerTimers timers_;
 uint64_t nextRx=0,txReady=0,adcReady=0,hostReads=0,hostWrites=0,midiReceived=0;
 ControllerResourceBus*expectedBus_=nullptr;
 bool overlay=false,reconstructedDefaults=false;unsigned flashCommand=0,pageRemaining=0;uint64_t flashRevision=0;
};
}
