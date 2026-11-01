#include "ksControllerDeviceState.h"
#include "ksControllerImageValidation.h"
#include "ksNativeControllerExecutor.h"
#include "ksNativeStartupController.h"
#include "ksBoard.h"
#include "ksBoardNativeOwner.h"
#include "ksDevice.h"
#include "ksRom.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
namespace {
template<class T,class=void>struct HasRegisters:std::false_type{};
template<class T>struct HasRegisters<T,std::void_t<decltype(std::declval<T&>().regs)>>:std::true_type{};
template<class T,class=void>struct HasPc:std::false_type{};
template<class T>struct HasPc<T,std::void_t<decltype(std::declval<T&>().pc)>>:std::true_type{};
template<class T,class=void>struct HasCcr:std::false_type{};
template<class T>struct HasCcr<T,std::void_t<decltype(std::declval<T&>().ccr)>>:std::true_type{};
template<class T,class=void>struct HasExr:std::false_type{};
template<class T>struct HasExr<T,std::void_t<decltype(std::declval<T&>().exr)>>:std::true_type{};
template<class T,class=void>struct HasStack:std::false_type{};
template<class T>struct HasStack<T,std::void_t<decltype(std::declval<T&>().getSP())>>:std::true_type{};
static_assert(!HasRegisters<ks::ControllerDeviceState>::value&&!HasPc<ks::ControllerDeviceState>::value&&!HasCcr<ks::ControllerDeviceState>::value&&!HasExr<ks::ControllerDeviceState>::value&&!HasStack<ks::ControllerDeviceState>::value,"CPU architecture returned to native physical state");
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
struct GuardBus final:ks::ControllerResourceBus{
 unsigned reads=0,writes=0;
 uint8_t read(uint32_t)override{++reads;throw std::runtime_error("Unadmitted native read");}
 void write(uint32_t,uint8_t)override{++writes;throw std::runtime_error("Unadmitted native write");}
};
std::vector<uint8_t>bytes(const char*path){std::ifstream f(path,std::ios::binary);check(bool(f),"Missing external test fixture");return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char**argv){try{
 check(!ks::boardComparisonAvailable(),"Production linked a comparison factory");
 check(ks::Device::environmentControllerMode()==ks::ControllerMode::Native,"Production default is not semantic C++");
 {
  auto device=std::make_unique<ks::ControllerDeviceState>();auto main=std::make_unique<ks::NativeControllerExecutor>();auto startup=std::make_unique<ks::NativeStartupController>();GuardBus bus;device->attachResourceBus(&bus);
  device->cycles=123;device->pending_irqs=1ull<<32;device->interruptPriority[32]=6;device->interruptMode2=true;device->memory[0xa12345]=0x5a;
  check(main->nextClocks(*device)==0&&main->advanceBefore(*device,~0u)==0&&main->advanceNextPhase(*device)==0,"Unadmitted Main advanced");
  check(startup->nextClocks(*device)==0&&startup->advanceBefore(*device,~0u)==0,"Unadmitted startup advanced");
  check(device->cycles==123&&device->pending_irqs==(1ull<<32)&&device->memory[0xa12345]==0x5a&&!bus.reads&&!bus.writes,"Rejected work changed physical resources");
  for(const auto size:{0u,1u,31u}){
   std::array<uint8_t,32>shortImage{};bool rejected=false;
   try{ks::controllerImage::verifyRom(shortImage.data(),size);}catch(const std::runtime_error&){rejected=true;}
   check(rejected,"Short image admitted");
  }
 }
 if(argc!=1&&argc!=4)return 77;
 if(argc==4){
  ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid external OS fixture");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
  auto board=std::make_unique<ks::Board>(rom.data(),wave,flash,true,true);
  check(board->nativeOwnerState(),"Semantic owner missing");
  // Status is published unconditionally after the phase tick, so caller-written
  // diagnostic flags cannot make a fresh cold owner appear ready.
  board->mainEntered=board->ready=true;const auto initialCycles=board->controllerCycles();board->advance(1);board->discardAudio();unsigned phases=1;
  check(board->controllerCycles()>initialCycles&&!board->mainEntered&&!board->ready&&board->mainEntered==board->nativeOwnerState()->mainEntered()&&board->ready==board->nativeOwnerState()->ready(),"Cold phase status did not repair forced Board flags");
  while(board->nativeOwnerState()->playback().metrics().mainReturns==0&&phases<40000000&&board->controllerCycles()<ks::Board::ControllerClockHz*10ull){board->advance(1);board->discardAudio();++phases;}
  check(board->ready&&board->mainEntered&&board->nativeOwnerState()->playbackPublished(),"Native startup did not reach Main");
  check(board->nativeOwnerState()->playback().metrics().mainReturns==1&&!board->h8InterpretedSteps(),"Native first Main did not complete");
  const auto&result=board->nativeOwnerState()->startup().result();check(result.known(),"Startup domain values missing");
  std::cout<<"PASS: native-only Board completed startup and first Main; phases="<<phases<<" cycles="<<board->controllerCycles()<<"\n";
 }
 std::cout<<"PASS: production has CPU-free physical state, no comparison factory, native default, immutable rejection before image admission\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
