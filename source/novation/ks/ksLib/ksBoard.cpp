#include "ksBoard.h"
#include "ksBoardNativeOwner.h"
#include "ksControllerTimers.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <sstream>
namespace ks {
Board::Board(const std::vector<uint8_t>& os,const std::vector<uint8_t>& bank,const std::vector<uint8_t>& storage,bool isolateParts,bool nativeStartup)
 : rom(os),flash(0x80000,0xff),waves(bank) {
    if(os.size()!=87360 || bank.size()!=WaveSize)throw std::invalid_argument("KS OS/bank size mismatch");
    // Update excludes container checksum. Rebuild its transport field, which
    // original reset code verifies while copying Flash to SRAM.
    uint32_t sum=0;for(size_t n=0;n<os.size();n+=2)sum+=(os[n]<<8)|os[n+1];
    for(int s:{24,16,8,0})rom.push_back(sum>>s);
    if(!storage.empty()){if(storage.size()!=flash.size())throw std::invalid_argument("KS flash size mismatch");flash=storage;}
    std::copy(rom.begin(),rom.end(),flash.begin()+0x4000);
    if(!nativeStartup&&boardComparisonAvailable())comparison=createBoardComparison(this,rom);
    if(comparison)device=&comparison->resources();
    else{nativeDevice=std::make_unique<ControllerDeviceState>();device=nativeDevice.get();}
    device->attachResourceBus(this);
    device->loadmem(rom.data(),uint32_t(rom.size()),0);
    io[0x17c]=io[0x184]=io[0x18c]=0x84;
    io[0x16f]=1;
    // H8S TPU TMDR0 reset: reserved bits 7:6 are 1, MD3:0=0 (normal counting).
    // A nonzero mode disabled the original 2 kHz tick and held-voice updates.
    io[0x1d1]=0xc0;
    for(unsigned n=0;n<128;++n)device->interruptPriority[n]=7;
    for(unsigned n=0xc4;n<=0xce;++n)io[n]=0x77;
    for(auto& d:dsps){d=std::make_unique<DSP>();d->peripherals().getEsaiClock().setEnabled(false);}
    // Voice DMA emits parts 1/2 on B TX3, parts 3/4 on B TX2,
    // with the first part in slot 1. A's effects mixer expects parts 1..4
    // consecutively in RX2 slots 0/1 then RX3 slots 0/1 (P:0x340..349).
    dsps[0]->serialRead=[this](dsp56k::Audio::RxFrame& f){DSP::SerialFrame b{};if(!bToA.pop(b) && serialSlaveStarted)++serialUnderruns[0];for(unsigned s=0;s<2;++s){f[s][2]=b[1-s][3];f[s][3]=b[1-s][2];}};
    dsps[1]->serialRead=[this](dsp56k::Audio::RxFrame& f){DSP::SerialFrame a{};if(!aToB.pop(a))++serialUnderruns[1];for(unsigned s=0;s<2;++s)f[s][0]=a[s][1];};
    dsps[0]->serialWrite=[this](const DSP::SerialFrame& f){if(serialSlaveStarted && !aToB.push(f))++serialOverflows[0];};
    dsps[1]->serialWrite=[this](const DSP::SerialFrame& f){if(!bToA.push(f))++serialOverflows[1];AudioFrame a{};for(unsigned d=0;d<2;++d)for(unsigned t=0;t<6;++t)for(unsigned s=0;s<2;++s){auto v=d?f[s][t]:dsps[0]->serialTx[s][t];a[d*12+t*2+s]=(v&0x800000)?int32_t(v)-0x1000000:int32_t(v);}audio.push(a);};
    placeholder.enabled=std::all_of(waves.begin(),waves.end(),[](uint8_t v){return v==0;});
    for(unsigned n=0;n<600;++n)std::copy_n(flash.data()+0x20000+n*256,256,programDesired[n].begin());
    for(unsigned n=0;n<100;++n)std::copy_n(flash.data()+0x45800+n*1024,1024,performanceDesired[n].begin());
    partMidi.enabled=isolateParts;
    partMidi.globalChannel=flash[0x1ff08]&15;
    if(isolateParts)for(unsigned n=0;n<600;++n){auto* p=flash.data()+0x20000+n*256;if(p[125]>30 && p[125]<128)p[125]=0;}
    if(isolateParts)for(unsigned n=0;n<100;++n)for(unsigned p=0;p<PartCount;++p)
    {auto* program=flash.data()+0x45800+n*1024+p*256;program[181]=uint8_t(p);if(program[125]>30 && program[125]<128)program[125]=0;}
    // Prepare the private Flash image before reset. Rewriting every sampled
    // preset over the guest UART used millions of emulated audio samples at
    // each activation. Keep the desired bytes for snapshots and state saves.
    if(placeholder.enabled)muteStoredPrograms();
    if(comparison)comparison->initialize();
    else{
        device->nativeLowBase=rom.data();device->nativeLowSize=unsigned(rom.size());
        nativeOwner=std::make_unique<BoardNativeOwner>(*device,rom.data(),unsigned(rom.size()));
    }
}
Board::~Board()=default;
bool Board::nativeMapping()const{return comparison?comparison->nativeMapping():bool(nativeOwner);}
bool Board::protectedCode()const{return comparison?comparison->protectedCode():(nativeOwner&&nativeOwner->protectedCode());}
// Zero is an explicit "no CPU location" diagnostic; it never drives execution.
// Any finite test-pair admission waits for independently exact physical data.
uint32_t Board::traceDiagnostic()const{return comparison?comparison->diagnosticLocation():0;}
void Board::resetControllerMetrics(){if(comparison)comparison->resetMetrics();else nativeOwner->resetMetrics();}
void Board::enableLiveMidiController(bool enabled){if(!comparison)throw std::logic_error("KS comparison controller unavailable");comparison->select(enabled?BoardComparisonMode::Hybrid:BoardComparisonMode::Original);}
void Board::enableNativeController(bool enabled){if(comparison){comparison->select(enabled?BoardComparisonMode::LegacyCompatibility:BoardComparisonMode::Original);return;}if(!enabled)throw std::logic_error("KS comparison controller unavailable");}
void Board::startControllerTrace(size_t events,size_t slices){if(controllerRecorder||(comparison&&!comparison->allowsLiveInput()))throw std::logic_error("KS trace already active");controllerRecorder=std::make_unique<ControllerRecorder>(controllerCheckpoint(),device->cycles,events,slices);}
void Board::startRecordedController(std::shared_ptr<const ControllerTape> tape){if(!comparison||controllerRecorder)throw std::logic_error("KS replay requires explicit comparison backend");comparison->startRecorded(std::move(tape),controllerCheckpoint());}

uint8_t Board::read(uint32_t a){
    if(a<0x200000){if(overlay)return device->memory[0xa00000+a];return a<rom.size()?rom[a]:0xff;}
    if(a>=0x800000 && a<0x880000){
        if(a>=WaveBase && a<WaveBase+WaveSize){++waveReads;bankLow=std::min(bankLow,uint64_t(a));bankHigh=std::max(bankHigh,uint64_t(a));return waves[a-WaveBase];}
        return flash[a-0x800000];
    }
    if((a>=0xa00000 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00))return device->memory[a];
    if((a>=0x400000 && a<0x400010)||(a>=0x600000 && a<0x600010)){
        const unsigned i=a>=0x600000;const auto value=dsps[i]->hostRead(a&15);
        if(controllerRecorder)controllerRecorder->port(device->cycles,traceDiagnostic(),i,a&15,value,false,true);
        return value;
    }
    if(a>=0xfffe00){
        if(a==0xffff51)return ((io[0x162]&0xf7)==0x32 || (io[0x162]&0xf7)==0x33)?0:0xff; // LCD not busy / keys released (mux refined below).
        if(a==0xffff50 || a==0xffff53 || a==0xffff5d)return 0xff;
        if(a>=0xffff90 && a<0xffff98)return (a&1)?0:0x80;
        return io[a&511];
    }
    std::ostringstream s;s<<"Unmapped KS read 0x"<<std::hex<<a<<" cycle="<<device->cycles;throw std::runtime_error(s.str());
}
void Board::write(uint32_t a,uint8_t v){
    if(protectedCode()){
        const uint32_t code=a>=0xa00000 && a<0xc00000?a-0xa00000:a;
        if((code>=0x170 && code<0x172)||(code>=0x490 && code<0x2a0c)||(code>=0x2a18 && code<0xb8e8))
            throw std::runtime_error("Native KS controller cannot modify verified executable SRAM");
    }
    if(a<0x200000){if(overlay)device->memory[0xa00000+a]=v;return;}
    if((a>=0xa00000 && a<0xc00000)||(a>=0xffdc00 && a<0xfffc00)){device->memory[a]=v;return;}
    if(a>=0x800000 && a<0x880000){
        auto off=a-0x800000;
        if(a>=WaveBase && a<WaveBase+WaveSize)throw std::runtime_error("KS attempted wave ROM write");
        if(flashCommand==3){flash[off]=v;if(!--pageRemaining){flashCommand=0;++flashRevision;}return;}
        if(flashCommand==0 && (off&0x7ffff)==0x5555 && v==0xaa)flashCommand=1;
        else if(flashCommand==1 && (off&0x7ffff)==0x2aaa && v==0x55)flashCommand=2;
        else if(flashCommand==2 && (off&0x7ffff)==0x5555 && v==0xa0){flashCommand=3;pageRemaining=256;}
        else throw std::runtime_error("Unimplemented KS flash command");
        return;
    }
    if((a>=0x400000 && a<0x400010)||(a>=0x600000 && a<0x600010)){
        const unsigned i=a>=0x600000;++hostWrites[i];const bool accepted=dsps[i]->hostWrite(a&15,v);
        if(controllerRecorder)controllerRecorder->port(device->cycles,traceDiagnostic(),i,a&15,v,true,accepted);
        if(!accepted)throw std::runtime_error("KS HDI write while busy");return;
    }
    if(a>=0xfffe00){
        auto off=a&511;auto old=io[off];io[off]=v;
        if(a==0xffff6f){overlay=!(v&1);if(nativeMapping()){device->nativeLowBase=overlay?device->memory+0xa00000:rom.data();device->nativeLowSize=overlay?0x200000:unsigned(rom.size());device->nativeLowWritable=overlay;}}
        if(a==0xffff39)device->interruptMode2=(v&0x30)==0x20;
        if(a==0xffff8b){midiOut.push(v);io[0x18c]&=0x7b;txDue=device->cycles+8000;}
        if(a==0xffff98 && (v&0x20) && !(old&0x20))adcDue=device->cycles+1064;
        // KS sets TPU0 to interrupt priority 6 and ADC/SCI2 to 1.
        // Decode guest-written IPR fields rather than forcing guest state.
        if(a==0xfffec9)device->interruptPriority[32]=(v>>4)&7;
        if(a==0xfffec8)device->interruptPriority[28]=v&7;
        if(a==0xfffece)for(int n=88;n<=91;++n)device->interruptPriority[n]=v&7;
        return;
    }
    std::ostringstream s;s<<"Unmapped KS write 0x"<<std::hex<<a<<" cycle="<<device->cycles;throw std::runtime_error(s.str());
}
void Board::tickNative(uint64_t previous){
    auto now=device->cycles;
    if(txDue && now>=txDue){txDue=0;io[0x18c]|=0x84;if(io[0x18a]&0x80)device->interrupt(90);if(io[0x18a]&4)device->interrupt(91);}
    if(nativeQueuedMidi && (io[0x18a]&0x10) && now>=rxDue && !(io[0x18c]&0x40)){uint8_t b;if(midiIn.pop(b)){--nativeQueuedMidi;io[0x18d]=b;io[0x18c]|=0x40;rxDue=now+8000;++midiReceived;if(io[0x18a]&0x40)device->interrupt(89);}}

    if((io[0x1c0]&1) && (io[0x1d1]&15)==0){
        unsigned shift=unsigned(io[0x1d0]&7)*2;
        if(shift<8){auto count=(io[0x1d6]<<8)|io[0x1d7];auto compare=(io[0x1d8]<<8)|io[0x1d9];
            const auto timer=advanceControllerTimer(uint16_t(count),uint16_t(compare),(now>>shift)-(previous>>shift),(io[0x1d0]&0x60)==0x20);
            count=timer.count;if(timer.matched){io[0x1d5]|=1;if(io[0x1d4]&1)device->interrupt(32);}
            io[0x1d6]=count>>8;io[0x1d7]=count;
        }
    }
    // OS 2.1.02 measures incoming MIDI-clock intervals with TPU1 TCNT1
    // (0xffffe6). It selects TPSC=6, normal mode, no compare clearing:
    // the H8S/2357 TPU clock table defines this as the CPU clock / 256.
    // Leaving this counter at zero held the firmware PLL at its initial tempo.
    if((io[0x1c0]&2) && (io[0x1e1]&15)==0 && io[0x1e0]==6){
        const uint64_t elapsed=(now>>8)-(previous>>8);
        const uint64_t count=((uint16_t(io[0x1e6])<<8)|io[0x1e7])+elapsed;
        io[0x1e6]=uint8_t(count>>8);io[0x1e7]=uint8_t(count);
        if(count>65535){io[0x1e5]|=4;if(io[0x1e4]&4)device->interrupt(42);}
    }
    if(adcDue && now>=adcDue && (io[0x198]&0x20) && !(io[0x198]&0x80)){
        io[0x198]|=0x80;adcDue=now+1064;if(io[0x198]&0x40)device->interrupt(28);if(!(io[0x198]&0x10))io[0x198]&=~0x20;
    }
}
bool Board::enqueueMidi(const uint8_t* bytes,size_t count){
    if(comparison&&!comparison->allowsLiveInput())return false;
    // Leave enough UART space for a complete routed packet and the zero-bank
    // adapter's injected controls. A rejected byte has not entered either parser.
    for(size_t n=0;n<count;++n){if(bytes[n]>=0xf8){if(!midiIn.push(bytes[n]))return false;continue;}if(midiIn.size()>65536-4096)return false;if(!partMidi.accept(bytes[n],
        [this](const uint8_t* b,size_t len){return enqueueNativeMidi(b,len);},
        [this](uint8_t program){return performanceDesired[program].data();},
        [this](const uint8_t* message,size_t){if(message[12]<100)std::copy_n(message+13,1024,performanceDesired[message[12]].begin());}))return false;}
    return true;
}
bool Board::enqueuePartMidi(unsigned part,const uint8_t* bytes,size_t count){
    if(comparison&&!comparison->allowsLiveInput())return false;
    if(!partMidi.enabled || !partMidi.performance)return enqueueMidi(bytes,count);
    if(part>=PartCount || (count!=3 && count!=9))return false;
    if(midiIn.size()>65536-4096)return false;
    if(count==9 && bytes[4]==98 && bytes[5]==86 && bytes[7]==6){partMidi.channels[part]=bytes[8]&15;return true;}
    std::array<uint8_t,9> routed{};std::copy_n(bytes,count,routed.begin());
    for(size_t n=0;n<count;n+=3)routed[n]=uint8_t((routed[n]&0xf0)|part);
    return enqueueNativeMidi(routed.data(),count);
}
bool Board::enqueueNativeMidi(const uint8_t* bytes,size_t count){
    for(size_t n=0;n<count;++n)if(!placeholder.accept(bytes[n], [this](const uint8_t* b,size_t len){return midiIn.push(b,len);}, [this](uint8_t ch,uint8_t bank,uint8_t program)->uint16_t {
        if(bank==65 && program<100){
            auto programs=performanceDesired[program];
            if(partMidi.enabled)for(unsigned p=0;p<PartCount;++p)programs[p*256+181]=uint8_t(p);
            placeholder.seedPerformance(programs.data());
            uint16_t channels=0;for(unsigned part=0;part<4;++part)channels|=1u<<(programs[part*256+181]&15);return channels;
        }
        if(bank>=33 && bank<=40){bank-=32;ch=placeholder.globalChannel;}
        if(bank<1 || bank>8 || program>=(bank<=4?100:50))return 0;
        const auto index=bank<=4?(bank-1)*100+program:400+(bank-5)*50+program;
        placeholder.seed(ch,programDesired[index].data());return 1u<<ch;
    },[this](uint8_t* message,size_t length){adaptStoredProgram(message,length);}))return false;return true;
}
bool Board::boot(){if(comparison&&!comparison->allowsLiveInput())throw std::logic_error("Recorded KS controller cannot boot H8");while(device->cycles<ControllerClockHz*10ull && !ready){advance();audio.reset();}if(!ready)return false;
    std::array<uint8_t,256> p{};for(unsigned n=0;n<256;++n)p[n]=read(0x168fc+n);
    const unsigned bank=flash[0x1ff02],program=flash[0x1ff03];
    if(placeholder.enabled && bank>=1 && bank<=8 && program<(bank<=4?100u:50u)){
        const unsigned index=bank<=4?(bank-1)*100+program:400+(bank-5)*50+program;
        PlaceholderMidi::restoreProgram(p.data(),programDesired[index].data());
    }
    for(uint8_t ch=0;ch<16;++ch){placeholder.seed(ch,p.data());placeholder.bank[ch]=placeholder.activeBank[ch]=flash[0x1ff02];}
    placeholder.globalChannel=flash[0x1ff08]&15;
    if(partMidi.enabled && read(0x167ba) && flash[0x1ff01]<100)partMidi.seed(performanceDesired[flash[0x1ff01]].data());
    if(placeholder.enabled && read(0x167ba)){
        State current;current.performance=true;const auto index=flash[0x1ff01];
        if(index<100)for(unsigned part=0;part<4;++part)std::copy_n(performanceDesired[index].data()+part*256,256,current.parts[part].begin());
        const auto message=stateMessage(current);enqueueMidi(message.data(),message.size());
    }
    if(placeholder.enabled)placeholder.mute(placeholder.globalChannel,[this](const uint8_t* bytes,size_t n){return midiIn.push(bytes,n);});
    for(unsigned n=0;n<16384;++n)renderSample();audio.reset();return true;}
void Board::adaptStoredProgram(uint8_t* message,size_t length) {
    constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};
    if(length<14 || !std::equal(std::begin(prefix),std::end(prefix),message))return;
    if(length==1038 && message[7]==3 && message[12]<100){
        if(!partMidi.enabled)std::copy_n(message+13,1024,performanceDesired[message[12]].begin());
        for(unsigned part=0;part<4;++part)PlaceholderMidi::muteProgram(message+13+part*256);return;
    }
    if(length!=270 || message[7]!=1)return;
    const auto bank=message[8]==1?message[11]:(placeholder.activeBank[placeholder.globalChannel]&31);
    if(bank<1 || bank>8 || message[12]>=(bank<=4?100:50))return;
    const auto index=bank<=4?(bank-1)*100+message[12]:400+(bank-5)*50+message[12];
    auto& original=programDesired[index];
    std::copy_n(message+13,256,original.begin());PlaceholderMidi::muteProgram(message+13);
}
void Board::muteStoredPrograms() {
    // Program Changes and drum keys still load through the original firmware.
    // Only its private zero-bank Flash has the same muted modulation fields
    // that live MIDI loads receive; the input Flash and wave ROM stay intact.
    for(unsigned n=0;n<600;++n){const auto& p=programDesired[n];
        if(std::any_of(p.begin(),p.end(),[](uint8_t v){return v>=128;}))continue;
        PlaceholderMidi::muteProgram(flash.data()+0x20000+n*256);
    }
    for(unsigned n=0;n<100;++n){const auto& p=performanceDesired[n];
        if(std::any_of(p.begin(),p.end(),[](uint8_t v){return v>=128;}))continue;
        for(unsigned part=0;part<4;++part)PlaceholderMidi::muteProgram(flash.data()+0x45800+n*1024+part*256);
    }
}
std::vector<uint8_t> Board::saveUserFlash() const {
    std::vector<uint8_t> saved;copyUserFlash(saved);return saved;
}
void Board::copyUserFlash(std::vector<uint8_t>& saved) const {
    if(comparison&&!comparison->allowsLiveInput())throw std::logic_error("Recorded KS controller has no live Flash API");
    saved.assign(flash.begin()+0x1fc00,flash.begin()+0x60000);
    if(placeholder.enabled)for(unsigned n=0;n<600;++n)PlaceholderMidi::restoreProgram(saved.data()+0x20000-0x1fc00+n*256,programDesired[n].data());
    if(placeholder.enabled)for(unsigned n=0;n<100;++n)for(unsigned part=0;part<4;++part)
        PlaceholderMidi::restoreProgram(saved.data()+0x45800-0x1fc00+n*1024+part*256,performanceDesired[n].data()+part*256);
    if(partMidi.enabled)for(unsigned n=0;n<100;++n)for(unsigned part=0;part<PartCount;++part)
        saved[0x45800-0x1fc00+n*1024+part*256+181]=performanceDesired[n][part*256+181];
}
State Board::state() {
    if(comparison&&!comparison->allowsLiveInput())throw std::logic_error("Recorded KS controller has no live state API");
    State result;result.performance=read(0x167ba)!=0;result.globalChannel=placeholder.globalChannel;
    result.activeMask=result.performance?(read(0x167d3)&15):1;
    for(unsigned part=0;part<(result.performance?PartCount:1);++part){auto& p=result.parts[part];const unsigned index=result.performance?part+1:0;
        for(unsigned n=0;n<256;++n)p[n]=read(0x168fc+index*256+n);
        for(unsigned n=0;n<8;++n)p[179+n]=read(0x174ae + 20*index+9+n);
        packMirrors(p);if(result.performance)placeholder.restorePart(part,p.data());else restoreDesiredLevels(p.data());
        if(partMidi.enabled && result.performance)p[181]=partMidi.channels[part];
    }
    if(result.performance){auto& first=result.parts[0];first[189]=read(0x167d2)&15;first[190]=result.activeMask;
        first[191]=read(0x168fc+191);first[208]=read(0x168fc+208);}
    return result;
}
Board::AudioFrame Board::renderSample(){AudioFrame a{};const auto begin=controllerCycles();while(!audio.pop(a)){advance();if(controllerCycles()-begin>ControllerClockHz)throw std::runtime_error("KS serial output timeout");}return a;}
void Board::enableDspWorkers(int rate,unsigned block){
    if(dspWorkers)return;
    if(!started[0] || !started[1])throw std::logic_error("KS DSP workers require completed sequential boot");
    dspWorkers=std::make_unique<DspWorkers>(this,[](void* board,unsigned i,int64_t target,bool sampled){
        static_cast<Board*>(board)->catchUpDsp(i,target,sampled);
    },rate,block);
}
void Board::catchUpDsp(unsigned i,int64_t target,bool sampled){
    using namespace baseLib::performance;
    Scope dspTiming(performance,i?Metric::DspB:Metric::DspA,0,0,true,sampled);
    // ESAI clocks stay disabled; all cross-DSP callbacks are serviced by the
    // coordinator AFTER both catch-ups, exactly as in sequential advance().
    dsps[i]->catchUpToCycle(target);
}
std::shared_ptr<const ControllerTape> Board::finishControllerTrace(){
    if(!controllerRecorder)throw std::logic_error("No KS controller recording");
    auto result=controllerRecorder->finish();controllerRecorder.reset();return result;
}
void Board::advance(uint32_t budget){
    using namespace baseLib::performance;
    const bool sample=performance && performance->mode()==2 && ((++diagnosticAdvances&255)==0);
    Scope controllerTiming(performance,Metric::H8,0,0,true,sample&&(!comparison||comparison->allowsLiveInput()));
    const auto begin=device->cycles;if(controllerRecorder)controllerRecorder->beginSlice(begin);
    if(comparison)comparison->beginSlice(device->cycles,budget);
    if(comparison&&!comparison->allowsLiveInput()){
        // Fixture replay retains its independent checked tape clock. It never
        // advances the comparison CPU or the live peripheral timer owner.
        comparison->advanceNextPhase();comparison->tickAfterPhase(device->cycles);
    }else{
    if(!comparison)nativeQueuedMidi=uint32_t(midiIn.size());
    while(device->cycles-begin<budget){
        const auto before=device->cycles;
        if(comparison){
            comparison->advanceNextPhase();comparison->tickAfterPhase(before);
            mainEntered=comparison->mainEntered();ready=comparison->ready();
        }else{
            const auto cost=nativeOwner->nextClocks(*device);
            if(!cost||nativeOwner->advanceBefore(*device,cost)!=cost)throw std::logic_error("Native KS owner has no admitted phase");
            tickNative(before);mainEntered=nativeOwner->mainEntered();ready=nativeOwner->ready();
        }
        if(device->cycles<=before)throw std::logic_error("KS controller phase did not advance");
    }
    }
    if(controllerRecorder)controllerRecorder->endSlice(device->cycles);
    controllerTiming.finish();
    const auto nowController=controllerCycles();
    if(dspWorkers){
        const std::array<int64_t,2> targets{int64_t(dspTime(nowController))-epochs[0],int64_t(dspTime(nowController))-epochs[1]};
        Scope barrierTiming(performance,Metric::DspBarrier,0,0,true,sample);
        dspWorkers->run(targets,sample);
    }else for(unsigned order=0;order<2;++order){unsigned i=1-order;if(dsps[i]->bootFinished()){
        Scope dspTiming(performance,i?Metric::DspB:Metric::DspA,0,0,true,sample);
        if(!started[i]){epochs[i]=int64_t(dspTime(nowController))-int64_t(dsps[i]->cycles());started[i]=true;if(i==1)aToB.reset();}
        const auto target=int64_t(dspTime(nowController))-epochs[i];
        dsps[i]->catchUpToCycle(target);
    }}
    // Both DSPs share the master's serial clock and frame sync. Independent
    // peripheral clocks can stretch differently when interrupt work is pending.
    auto& master=dsps[0]->peripherals().getEsai();
    Scope serialTiming(performance,Metric::Serial,0,0,true,sample);
    const uint64_t serialTime=dspTime(nowController);
    if(!serialNext && master.hasEnabledTransmitters())serialNext=serialTime+DSP::SerialSlotCycles;
    while(serialNext && serialTime>=serialNext){
        for(unsigned i=0;i<2;++i){auto& esai=dsps[i]->peripherals().getEsai();
            if(i==1 && !serialSlaveStarted){if((serialSlot&1) || !esai.hasEnabledTransmitters())continue;serialSlaveStarted=true;aToB.reset();aToB.push(dsps[0]->serialTx);}
            esai.execTX();esai.execRX();
        }
        ++serialSlot;serialNext+=DSP::SerialSlotCycles;
    }
}
}
