#include "kstationH8.h"
#include "kstationHardware.h"
#include "kstationNativeDevelopmentServices.h"
#include "kstationNativeAdc.h"
#include "kstationNativeForeground.h"
#include "kstationNativeDspTransfer.h"
#include "kstationNativeDspStartup.h"
#include "kstationNativeStartup.h"
#include "kstationNativeFlashState.h"
#include "kstationNativePatchUnpack.h"
#include "kstationNativeMidiEvents.h"
#include "kstationNativeVoiceAllocation.h"
#include "kstationNativeVoiceUpdate.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace kstation {struct HardwareStage1Comparison{static void advance(Hardware&b){b.advance();}};}
namespace {void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(input),{}};check(rom.size()==49728,"Private K image required");unsigned guards=0;
 using Mode=kstation::H8::Stage1Mode;
 {auto altered=rom;altered[0x200]^=1;bool rejected=false;try{kstation::Hardware board(altered,{},Mode::ControllerDevelopment);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Wrong development image admitted");++guards;}
 {
  kstation::Hardware board(rom,{},Mode::ControllerDevelopment);check(board.boot(),"Development boot");auto&cpu=board.controller().core();
  const auto ranges=[&](const auto&entries){for(auto r:entries){
   for(uint32_t low:{r.begin,r.end-1})for(uint32_t base:{0u,0x400000u,0x1400000u}){const auto byte=board.controller().readMemory(low);bool rejected=false;try{cpu.write8(int8_t(byte^1),int(base+low));}catch(const std::runtime_error&){rejected=true;}check(rejected&&board.controller().readMemory(low)==byte,"Development code/table alias store admitted");++guards;}
   const auto byte=board.controller().readMemory(r.begin);bool rejected=false;try{cpu.write32(0,int(0x400000+r.begin));}catch(const std::runtime_error&){rejected=true;}check(rejected&&board.controller().readMemory(r.begin)==byte,"Development wide code store admitted");++guards;
  }};
  ranges(kstation::NativeAdc::CodeRanges);ranges(kstation::NativeForeground::CodeRanges);ranges(kstation::NativeDspTransfer::CodeRanges);
  ranges(kstation::NativeDspStartup::CodeRanges);ranges(kstation::NativeStartup::CodeRanges);ranges(kstation::NativeFlashState::CodeRanges);
  ranges(kstation::NativePatchUnpack::CodeRanges);ranges(kstation::NativeMidiEvents::CodeRanges);ranges(kstation::NativeVoiceAllocation::CodeRanges);ranges(kstation::NativeVoiceUpdate::CodeRanges);
  kstation::NativeDevelopmentServices::ranges([&](uint32_t begin,uint32_t end){struct R{uint32_t begin,end;};ranges(std::array<R,1>{{{begin,end}}});});
  board.controller().reset();cpu.write8(0x5a,0x40065a);const auto state=board.controller().debugState();
  check(cpu.memory[0x40065a]==0x5a&&!state.nativeAdcSteps&&!state.nativeDspTransferSteps&&!state.nativeForegroundSteps&&!state.nativeStartupSteps&&!state.nativeFlashStateSteps&&!state.nativePatchUnpackSteps&&!state.nativeMidiEventSteps&&!state.nativeVoiceAllocationSteps&&!state.nativeVoiceUpdateSteps,"Development reset retained state/lock");++guards;
 }
 // Changing the ignored high byte of vector60 preserves original mode0 IRQ
 // admission during bootstrap. Fixed-image verification must still reject it.
 {
  kstation::Hardware board(rom,{},Mode::ControllerDevelopment);
  while(!board.controller().debugState().ramOverlay){check(board.controller().core().cycles<160000000,"Overlay timeout");kstation::HardwareStage1Comparison::advance(board);}
  check(!board.controller().ready(),"Missed development handoff");board.controller().core().write8(int8_t(rom[0xf0]^1),0x4000f0);
  bool rejected=false;try{board.boot();}catch(const std::runtime_error&e){const std::string message=e.what();rejected=message.find("code mismatch")!=std::string::npos;}check(rejected,"Changed ADC vector admitted at development handoff");++guards;
 }
 std::cout<<"PASS development image/code/vector/table/alias/wide/reset/handoff guards="<<guards<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
