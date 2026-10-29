#include "kstationHardware.h"
#include "kstationNativeSerialTx.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace kstation {struct HardwareStage1Comparison{static void advance(Hardware& b){b.advance();}};}
namespace {void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: decoded K firmware required\n";return 77;}
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};check(rom.size()==49728,"ROM size");unsigned guards=0;
 {
  kstation::Hardware board(rom,{},kstation::H8::Stage1Mode::TimersHdiIrqAndSerialTx);check(board.boot(),"TX mode boot");auto& cpu=board.controller().core();
  for(const auto& range:kstation::NativeSerialTx::CodeRanges)for(uint32_t low:{range.begin,range.end-1})for(uint32_t base:{0u,0x400000u,0x1400000u}){
   const auto byte=board.controller().readMemory(low);bool rejected=false;
   try{cpu.write8(uint8_t(byte^1),int(base+low));}catch(const std::runtime_error&){rejected=true;}
   check(rejected&&board.controller().readMemory(low)==byte,"TX low/physical/wrapped store admitted");++guards;
  }
  for(const auto& range:kstation::NativeSerialTx::CodeRanges){bool rejected=false;const auto byte=board.controller().readMemory(range.begin);try{cpu.write32(0,int(0x400000+range.begin));}catch(const std::runtime_error&){rejected=true;}check(rejected&&board.controller().readMemory(range.begin)==byte,"TX wide store admitted");++guards;}
  board.controller().reset();cpu.write8(0x5a,0x400b54);check(cpu.memory[0x400b54]==0x5a,"Reset retained TX code lock");check(board.controller().debugState().nativeSerialTxSteps==0,"Reset retained TX metrics");++guards;
 }
 // TX interrupt is not produced by the model during ordinary boot, so this tests
 // real pre-handoff stores without changing an executed bootstrap path.
 for(uint32_t low:{0xdbu,0xb55u}){
  kstation::Hardware board(rom,{},kstation::H8::Stage1Mode::TimersHdiIrqAndSerialTx);
  while(!board.controller().debugState().ramOverlay){check(board.controller().core().cycles<160000000,"Overlay timeout");kstation::HardwareStage1Comparison::advance(board);}
  check(!board.controller().ready(),"Missed handoff");board.controller().core().write8(uint8_t(rom[low]^2),int(0x400000+low));
  bool rejected=false;try{board.boot();}catch(const std::runtime_error&e){rejected=std::string(e.what()).find("TX handoff code mismatch")!=std::string::npos;}
  check(rejected,"Modified TX vector/body admitted at handoff");++guards;
 }
 std::cout<<"PASS TX code/vector/alias/reset/handoff guards="<<guards<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
