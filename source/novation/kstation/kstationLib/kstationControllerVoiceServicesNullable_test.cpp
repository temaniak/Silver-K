#include "kstationControllerVoiceServices.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

using namespace kstation;
struct Fixture {
 std::unique_ptr<ControllerDeviceState> state=std::make_unique<ControllerDeviceState>();
 ControllerForegroundRuntime foreground; ControllerDspTransferRuntime dsp;
 ControllerInterruptRuntime irq; ControllerVoiceServices voice;
 bool bind(const std::vector<uint8_t>& rom){
  state->loadMemory(rom.data(),rom.size(),0);state->loadMemory(rom.data(),rom.size(),0x400000);
  return foreground.bindVerifiedFirmware(*state,rom.data(),rom.size())
   && dsp.bindVerifiedFirmware(*state,rom.data(),rom.size())
   && irq.bindVerifiedFirmware(*state,rom.data(),rom.size())
   && voice.bindAll(*state,rom.data(),rom.size());
 }
};
int main(int argc,char**argv){
 if(argc!=2)return 77;std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t>rom{std::istreambuf_iterator<char>(file),{}};if(rom.size()!=49728)return 1;
 Fixture a,b;if(!a.bind(rom)||!b.bind(rom))return 2;
 using R=ControllerVoiceServices::Route;
 ControllerVoiceServices unbound;ControllerDspTransferRuntime unboundDsp;
 if(unbound.routeForeground(a.foreground,*a.state,a.dsp,nullptr)!=R::Invalid
  ||a.voice.routeForeground(a.foreground,*b.state,a.dsp,nullptr)!=R::Invalid
  ||a.voice.routeForeground(a.foreground,*a.state,unboundDsp,nullptr)!=R::Invalid)return 3;
 if(a.foreground.request().kind!=ControllerForegroundRuntime::RequestKind::None||b.foreground.request().kind!=ControllerForegroundRuntime::RequestKind::None)return 4;
 if(a.voice.routeForeground(a.foreground,*a.state,a.dsp,nullptr)!=b.voice.routeForeground(b.foreground,*b.state,b.dsp))return 5;
 ControllerForegroundRuntime::Request global;global.kind=ControllerForegroundRuntime::RequestKind::VoiceGlobal;
 if(a.voice.routeForeground(a.foreground,*a.state,a.dsp,&global)!=R::Ready
  ||b.voice.routeForeground(b.foreground,*b.state,b.dsp,&global)!=R::Ready)return 6;
 unsigned phases=0;
 while(a.voice.globalActive()&&b.voice.globalActive()&&phases<10000){
  auto x=a.voice.routeForeground(a.foreground,*a.state,a.dsp,nullptr);
  auto y=b.voice.routeForeground(b.foreground,*b.state,b.dsp);
  if(x!=y)return 7;
  auto p=a.voice.advanceForeground(a.foreground,*a.state,a.irq,a.dsp,128);
  auto q=b.voice.advanceForeground(b.foreground,*b.state,b.irq,b.dsp,128);
  if(p.clocks!=q.clocks||p.blocked!=q.blocked||p.iteration!=q.iteration
    ||a.state->cycles!=b.state->cycles||a.state->pendingIrqs!=b.state->pendingIrqs)return 8;
  if(!p.clocks)break;++phases;
 }
 if(!phases||a.voice.globalActive()!=b.voice.globalActive())return 9;
 std::printf("PASS nullable routing guards and active global equivalence: %u phases\n",phases);
}
