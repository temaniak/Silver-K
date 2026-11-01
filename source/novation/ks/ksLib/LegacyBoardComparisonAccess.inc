class BoardComparisonAccess final {
public:
 static void referenceTick(Board&board,uint64_t previous){
    auto now=board.device->cycles;
    if(board.txDue && now>=board.txDue){board.txDue=0;board.io[0x18c]|=0x84;if(board.io[0x18a]&0x80)board.device->interrupt(90);if(board.io[0x18a]&4)board.device->interrupt(91);}
    if((board.io[0x18a]&0x10) && now>=board.rxDue && !(board.io[0x18c]&0x40)){uint8_t b;if(board.midiIn.pop(b)){board.io[0x18d]=b;board.io[0x18c]|=0x40;board.rxDue=now+8000;++board.midiReceived;if(board.io[0x18a]&0x40)board.device->interrupt(89);}}

    if((board.io[0x1c0]&1) && (board.io[0x1d1]&15)==0){
        unsigned shift=unsigned(board.io[0x1d0]&7)*2;
        if(shift<8){auto count=(board.io[0x1d6]<<8)|board.io[0x1d7];auto compare=(board.io[0x1d8]<<8)|board.io[0x1d9];
            for(uint64_t n=previous>>shift;n<(now>>shift);++n){if(++count==compare){board.io[0x1d5]|=1;if(board.io[0x1d4]&1)board.device->interrupt(32);if((board.io[0x1d0]&0x60)==0x20)count=0;}count&=65535;}
            board.io[0x1d6]=count>>8;board.io[0x1d7]=count;
        }
    }
    // OS 2.1.02 measures incoming MIDI-clock intervals with TPU1 TCNT1
    // (0xffffe6). It selects TPSC=6, normal mode, no compare clearing:
    // the H8S/2357 TPU clock table defines this as the CPU clock / 256.
    // Leaving this counter at zero held the firmware PLL at its initial tempo.
    if((board.io[0x1c0]&2) && (board.io[0x1e1]&15)==0 && board.io[0x1e0]==6){
        const uint64_t elapsed=(now>>8)-(previous>>8);
        const uint64_t count=((uint16_t(board.io[0x1e6])<<8)|board.io[0x1e7])+elapsed;
        board.io[0x1e6]=uint8_t(count>>8);board.io[0x1e7]=uint8_t(count);
        if(count>65535){board.io[0x1e5]|=4;if(board.io[0x1e4]&4)board.device->interrupt(42);}
    }
    if(board.adcDue && now>=board.adcDue && (board.io[0x198]&0x20) && !(board.io[0x198]&0x80)){
        board.io[0x198]|=0x80;board.adcDue=now+1064;if(board.io[0x198]&0x40)board.device->interrupt(28);if(!(board.io[0x198]&0x10))board.io[0x198]&=~0x20;
    }

 }
 static uint64_t nextEvent(const Board&board,uint64_t limit){
    const auto now=board.device->cycles;
    if(board.txDue)limit=std::min(limit,board.txDue);
    if(board.nativeQueuedMidi && (board.io[0x18a]&0x10) && !(board.io[0x18c]&0x40))
        limit=std::min(limit,board.rxDue);
    if(board.adcDue && (board.io[0x198]&0x20) && !(board.io[0x198]&0x80))
        limit=std::min(limit,board.adcDue);
    if((board.io[0x1c0]&1) && (board.io[0x1d1]&15)==0) {
        const unsigned shift=unsigned(board.io[0x1d0]&7)*2;
        const unsigned count=(unsigned(board.io[0x1d6])<<8)|board.io[0x1d7];
        const unsigned compare=(unsigned(board.io[0x1d8])<<8)|board.io[0x1d9];
        if(shift<8 && compare) {
            const uint64_t distance=compare>count?compare-count:65536ull+compare-count;
            limit=std::min(limit,((now>>shift)+distance)<<shift);
        }
    }
    if((board.io[0x1c0]&2) && (board.io[0x1e1]&15)==0 && board.io[0x1e0]==6) {
        const unsigned count=(unsigned(board.io[0x1e6])<<8)|board.io[0x1e7];
        limit=std::min(limit,((now>>8)+65536-count)<<8);
    }
    return limit;

 }
 static uint64_t checkpoint(const Board&board){
    uint64_t hash=14695981039346656037ull;
    auto byte=[&](uint8_t v){hash^=v;hash*=1099511628211ull;};
    auto word=[&](uint64_t v){for(unsigned n=0;n<8;++n)byte(uint8_t(v>>(n*8)));};
    for(const auto* image:{&board.rom,&board.flash,&board.waves})for(auto v:*image)byte(v);
    for(unsigned n=0xa00000;n<0xc00000;++n)byte(board.device->memory[n]);
    for(unsigned n=0xffdc00;n<0xfffc00;++n)byte(board.device->memory[n]);
    for(auto v:board.io)byte(v);
    word(board.device->cycles);word(int32_t(board.comparison->diagnosticLocation()));word(board.serialNext);word(board.serialSlot);word(board.serialSlaveStarted);
    word(board.midiIn.size());word(board.audio.size());word(board.aToB.size());word(board.bToA.size());word(board.partMidi.enabled);
    for(unsigned i=0;i<2;++i){word(board.dsps[i]->cycles());word(board.dsps[i]->instructions());word(uint64_t(board.epochs[i]));word(board.started[i]);
        for(const auto& slot:board.dsps[i]->serialTx)for(auto v:slot)word(v);
    }
    return hash;

 }
 static void nativeTick(Board&board,uint64_t before){board.tickNative(before);}
 static bool started(const Board&board){return board.started[0]&&board.started[1];}
 static bool recording(const Board&board){return bool(board.controllerRecorder);}
 static void snapshotUART(Board&board){board.nativeQueuedMidi=uint32_t(board.midiIn.size());}
 static uint8_t hostRead(Board&board,unsigned i,unsigned offset){return board.dsps[i]->hostRead(offset);}
 static bool hostWrite(Board&board,unsigned i,unsigned offset,uint8_t value){++board.hostWrites[i];return board.dsps[i]->hostWrite(offset,value);}
 static BoardComparisonBackend*backend(Board&board){return board.comparison.get();}
 static const BoardComparisonBackend*backend(const Board&board){return board.comparison.get();}
};
