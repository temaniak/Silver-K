#include "kstationH8.h"
#include "kstationHardware.h"
#include "h8s/h8s.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace kstation {struct HardwareStage1Comparison{static void advance(Hardware&b){b.advance();}};}
namespace {void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: decoded K firmware required\n";return 77;}
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};check(rom.size()==49728,"ROM size");
 auto bad=rom;bad.back()^=1;bool rejected=false;
 try{kstation::H8 h(bad,{[](uint32_t){return uint8_t(6);},[](uint32_t,uint8_t){return true;},{}},{},kstation::H8::Stage1Mode::TimersAndHdi);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"Unverified image accepted");unsigned guards=1;
 {
  kstation::Hardware board(rom,{},kstation::H8::Stage1Mode::TimersAndHdi);check(board.boot(),"Guard boot");auto& cpu=board.controller().core();const auto byte=board.controller().readMemory(0x1672);
  for(uint32_t address:{0x1672u,0x401672u,0x1401672u}){
   rejected=false;try{cpu.write8(uint8_t(byte^1),int(address));}catch(const std::runtime_error&){rejected=true;}
   check(rejected&&board.controller().readMemory(0x1672)==byte,"Code/alias self-write guard");++guards;
  }
  rejected=false;try{cpu.write32(0,0x401672);}catch(const std::runtime_error&){rejected=true;}check(rejected,"Wide alias store accepted");++guards;
  board.controller().reset();cpu.write8(0x5a,0x401672);check(cpu.memory[0x401672]==0x5a,"Reset retained stale immutable lock");++guards;
 }
 {
  kstation::Hardware board(rom,{},kstation::H8::Stage1Mode::TimersAndHdi);
  while(!board.controller().debugState().ramOverlay){check(board.controller().core().cycles<160000000,"Overlay timeout");kstation::HardwareStage1Comparison::advance(board);}
  check(!board.controller().ready(),"Missed pre-handoff boundary");
  // Alter only the otherwise unvisited retry displacement, through the actual
  // physical store path, after the loader has published the overlay.
  board.controller().core().write8(uint8_t(rom[0x1695]^2),0x401695);
  rejected=false;try{board.boot();}catch(const std::runtime_error&e){rejected=std::string(e.what()).find("handoff code mismatch")!=std::string::npos;}
  check(rejected,"Modified overlay was admitted");++guards;
 }
 std::cout<<"PASS stage1 image/physical/low/wrapped/wide/reset/handoff guards="<<guards<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
