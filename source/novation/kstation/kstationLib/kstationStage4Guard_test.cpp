#include "kstationH8.h"
#include "kstationHardware.h"
#include "kstationNativeClock.h"
#include "kstationNativeTimer1.h"
#include "kstationNativeSerialRx.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace kstation {struct HardwareStage1Comparison{static void advance(Hardware& b){b.advance();}};}
namespace {void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: external decoded K firmware required\n";return 77;}
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(f),{}};check(rom.size()==49728,"ROM size");unsigned guards=0;
 {auto bad=rom;bad[0x200]^=1;bool rejected=false;try{kstation::Hardware b(bad,{},kstation::H8::Stage1Mode::Timer1AndRx);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Wrong image admitted");++guards;}
 {
  kstation::Hardware b(rom,{},kstation::H8::Stage1Mode::Timer1AndRx);check(b.boot(),"Stage4 boot");auto&cpu=b.controller().core();
  const auto verify=[&](const auto&ranges){for(const auto&r:ranges){
   for(uint32_t low:{r.begin,r.end-1})for(uint32_t base:{0u,0x400000u,0x1400000u}){const auto byte=b.controller().readMemory(low);bool rejected=false;try{cpu.write8(uint8_t(byte^1),int(base+low));}catch(const std::runtime_error&){rejected=true;}check(rejected&&b.controller().readMemory(low)==byte,"New graph alias code store admitted");++guards;}
   const auto byte=b.controller().readMemory(r.begin);bool rejected=false;try{cpu.write32(0,int(0x400000+r.begin));}catch(const std::runtime_error&){rejected=true;}check(rejected&&b.controller().readMemory(r.begin)==byte,"New graph wide code store admitted");++guards;
  }};
  verify(kstation::NativeClock::CodeRanges);verify(kstation::NativeTimer1::CodeRanges);verify(kstation::NativeSerialRx::CodeRanges);
  b.controller().reset();cpu.write8(0x5a,0x400a70);const auto d=b.controller().debugState();check(cpu.memory[0x400a70]==0x5a&&d.nativeRxSteps==0&&d.nativeTimer1Steps==0&&d.nativeClockSteps==0,"Reset retained code lock or metrics");++guards;
 }
 // No RX traffic during boot, so these real pre-handoff stores do not corrupt
 // an executed bootstrap path. Timer1/clock code is verified by the same gate.
 for(uint32_t low:{0xd7u,0xa7du}){
  kstation::Hardware b(rom,{},kstation::H8::Stage1Mode::Timer1AndRx);
  while(!b.controller().debugState().ramOverlay){check(b.controller().core().cycles<160000000,"Overlay timeout");kstation::HardwareStage1Comparison::advance(b);}
  check(!b.controller().ready(),"Missed handoff");b.controller().core().write8(uint8_t(rom[low]^2),int(0x400000+low));
  bool rejected=false;try{b.boot();}catch(const std::runtime_error&e){rejected=std::string(e.what()).find("Timer1/RX handoff code mismatch")!=std::string::npos;}check(rejected,"Changed RX code/vector admitted at handoff");++guards;
 }
 std::cout<<"PASS Timer1/RX/clock image/code/vector/alias/reset/handoff guards="<<guards<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
