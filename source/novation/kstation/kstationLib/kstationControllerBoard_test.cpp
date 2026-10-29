#include "kstationControllerBoard.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <new>
namespace{bool watch=false;uint64_t allocations=0;void check(bool x,const char*w){if(!x)throw std::runtime_error(w);}}
void*operator new(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(int argc,char**argv){try{if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"Private image");auto state=std::make_unique<kstation::ControllerDeviceState>();uint64_t resets=0,reads=0,writes=0;bool asserted=false;
 kstation::ControllerBoard board(*state,rom,{[&](uint32_t a){++reads;return uint8_t(a+17);},[&](uint32_t,uint8_t){++writes;return true;},[&](bool a){++resets;asserted=a;}});
 check(state->cycles==0&&resets==1&&asserted&&board.initializedFromFirmware(),"Native board cold reset contract");
 check(board.readMemory(0x21ff00)==rom[0xbb12]&&board.readMemory(0x220000)==rom[0xba5c]&&board.readMemory(0x226400)==rom[0xba5c],"Firmware reconstructed defaults");
 watch=true;
 // Live overlay and low/physical RAM aliases, before the immutable handoff.
 auto original=board.readMemory(0xc000);state->write8(0xc000,0x55);check(board.readMemory(0xc000)==original,"ROM write changed read mapping");state->loadMemory(rom.data(),rom.size(),0x400000);state->write8(0xffffcf,0);state->write8(0xc000,0x55);check(board.ramOverlay()&&board.readMemory(0xc000)==0x55&&state->memory[0x40c000]==0x55,"Native overlay alias");
 check(state->read8(0x600002)==19,"HDI read");state->write8(0x60000a,0xab);check(reads==1&&writes==1,"HDI callbacks");
 // Real UART slots, realtime-byte priority, reception latch and serial IRQ.
 uint8_t wire[]{0x90,60,100,0xf8};check(board.enqueueMidi(wire,4),"UART enqueue");state->write8(0xffffb2,0x50);board.tickAfterPhase();check(state->read8(0xffffb5)==0xf8&&(state->pendingIrqs&(uint64_t(1)<<53)),"UART realtime admission");state->write8(0xffffb4,0x84);state->pendingIrqs=0;state->cycles+=5120;board.tickAfterPhase();check(state->read8(0xffffb5)==0x90&&(state->pendingIrqs&(uint64_t(1)<<53)),"UART normal admission");
 state->write8(0xffffb2,0xa4);state->write8(0xffffb3,0x7f);uint8_t byte=0;check(board.popMidiOut(byte)&&byte==0x7f&&!board.popMidiOut(byte),"UART transmit stream");state->pendingIrqs=0;state->cycles+=5119;board.tickAfterPhase();check(!(state->pendingIrqs&((uint64_t(1)<<54)|(uint64_t(1)<<55))),"UART transmit early");++state->cycles;board.tickAfterPhase();check((state->pendingIrqs&((uint64_t(1)<<54)|(uint64_t(1)<<55)))==((uint64_t(1)<<54)|(uint64_t(1)<<55)),"UART transmit deadline");
 state->write8(0xffffe8,0x70);state->pendingIrqs=0;state->cycles+=1063;board.tickAfterPhase();check(!(state->pendingIrqs&(uint64_t(1)<<60)),"ADC early");++state->cycles;board.tickAfterPhase();check((state->pendingIrqs&(uint64_t(1)<<60))&&(state->read8(0xffffe8)&0xa0)==0xa0&&state->read8(0xffffe0)==0x80&&state->read8(0xffffe1)==0,"ADC conversion");
 // LCD commands and character RAM traverse the same GPIO edge protocol.
 auto lcd=[&](uint8_t v,bool data){state->write8(0xffffc5,0xff);state->write8(0xffffc7,v);state->write8(0xffffd3,uint8_t(0xc0|(data?8:0)));state->write8(0xffffd3,uint8_t(0x40|(data?8:0)));};lcd(0x80,false);lcd('K',true);lcd(0x80,false);state->write8(0xffffc5,0);state->write8(0xffffd3,0x48);check(state->read8(0xffffc7)=='K'&&board.lcdText()[0]=='K',"LCD GPIO data");
 // Full page programming, including the original unlock and revision policy.
 state->write8(0x205555,0xaa);state->write8(0x202aaa,0x55);state->write8(0x205555,0xa0);for(unsigned i=0;i<256;++i)state->write8(0x21fe00+i,uint8_t(i^0x5a));check(board.storageRevision()==1,"Flash revision");for(unsigned i=0;i<256;++i)check(board.readMemory(0x21fe00+i)==uint8_t(i^0x5a),"Flash page data");
 watch=false;check(!allocations,"Native board processing allocated");check(state->protectImmutable(0x20,0x22)&&state->protectImmutable(0x400020,0x400022),"Board immutable fixture");auto before=state->cycles,beforeResets=resets;bool resetRejected=false,writeRejected=false;try{board.reset();}catch(const std::logic_error&){resetRejected=true;}try{state->write16(0x1000020,0);}catch(const std::logic_error&){writeRejected=true;}check(resetRejected&&writeRejected&&state->cycles==before&&resets==beforeResets,"Bound reset/write mutated board");
 std::puts("{\"passed\":true,\"processing_allocations\":0,\"native_cpu_constructions\":0}");return 0;
}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
