#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const char* path){std::ifstream f(path,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
}
int main(int argc,char** argv){try{
 if(argc!=4){std::cout<<"SKIP: supply KS OS, synthetic Flash and Wave ROM\n";return 77;}
 const ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
 auto flash=bytes(argv[2]);check(flash.size()==0x80000,"Invalid Flash");const auto wave=bytes(argv[3]);
 for(unsigned slot=0;slot<50;++slot)std::copy_n(flash.data()+0x20000,256,flash.data()+0x20000+(500+slot)*256);
 auto board=std::make_unique<ks::Board>(os.data(),wave,flash,true,true);check(board->boot(),"Native reset failed");
 auto render=[&](unsigned n){while(n--)board->renderSample();};
 const uint8_t kit[]{0xb0,32,39,0xc0,0};check(board->enqueueMidi(kit,sizeof(kit)),"Kit rejected");render(16000);
 bool fast=true;auto select=[&](uint8_t note,const char* scenario){
  const auto received=board->midiReceived,cycle=board->controllerCycles();
  check(board->selectDrumNote(0,note),"Selection rejected");
  unsigned frames=0;while(board->drumEditBusy() && frames<ks::DSP::SampleRate){board->renderSample();++frames;}
  check(!board->drumEditBusy() && board->state().parts[0][188]==note-36,"Wrong selected drum");
  const double ms=1000.0*(board->controllerCycles()-cycle)/ks::Board::ControllerClockHz;
  std::cout<<scenario<<": "<<ms<<" ms, UART bytes="<<board->midiReceived-received<<"\n"<<std::flush;
  fast=fast && ms<20;
 };
 select(36,"same key");select(37,"unchanged key");select(36,"unchanged return");
 const auto initial=board->state().parts[0];
 const uint8_t cutoff[]{0xb0,105,31};check(board->enqueueMidi(cutoff,sizeof(cutoff)),"Cutoff rejected");
 select(37,"voice edited");check(board->state().parts[0][46]==initial[46],"Next voice changed");
 select(36,"return to voice edit");check(board->state().parts[0][46]==31,"Voice edit lost");
 // Delay level is CC 92, canonical offset 100, shared through slot 49.
 const uint8_t effects[]{0xb0,92,99};check(board->enqueueMidi(effects,sizeof(effects)),"FX rejected");
 select(37,"FX edited");check(board->state().parts[0][100]==99,"Shared FX lost");
 const uint8_t both[]{0xb0,105,52,0xb0,92,71};check(board->enqueueMidi(both,sizeof(both)),"Combined edit rejected");
 select(36,"voice and FX edited");check(board->state().parts[0][46]==31 && board->state().parts[0][100]==71,"Combined edit lost");
 select(37,"return to second edit");check(board->state().parts[0][46]==52,"Second edit lost");
 for(unsigned n=0;n<16;++n)select(uint8_t(36+(n&1)),"rapid alternating keys");
 const auto saved=board->saveUserFlash();const auto base=0x20000-0x1fc00+500*256;
 check(saved[base+46]==31 && saved[base+256+46]==52 && saved[base+49*256+100]==71,"Saved map lost voice/FX edits");
 check(board->h8InterpretedSteps()==0,"Hidden H8 fallback");
 check(fast,"Drum Edit exceeds 20 ms of guest time (host/driver latency excluded)");
 std::cout<<"PASS: bounded key selection, both voice edits/shared FX, saved Flash, zero H8 steps\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
