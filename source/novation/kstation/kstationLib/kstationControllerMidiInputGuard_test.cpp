#include "kstationControllerMidiControlRuntime.h"
#include "kstationControllerProgramRuntime.h"
#include "kstationControllerMidiSysExRuntime.h"
#include "kstationControllerMidiInitRuntime.h"
#include "kstationControllerStartupKeyboardRuntime.h"
#include "kstationControllerStartupServicesRuntime.h"
#include "kstationControllerStartupDisplayRuntime.h"
#include "kstationControllerStartupVolumeRuntime.h"
#include "kstationControllerUiEncoderRuntime.h"
#include "kstationControllerUiEncoderRouteRuntime.h"
#include "kstationControllerUiModeRuntime.h"
#include "kstationControllerAdcEventRuntime.h"
#include "kstationControllerMainUiAdcRuntime.h"
#include "kstationControllerMainUiKeyRuntime.h"
#include "kstationControllerPanelMidiRuntime.h"
#include "kstationControllerForegroundClockRuntime.h"
#include "kstationControllerPeriodicUiRuntime.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
template<class Runtime>bool test(const std::vector<uint8_t>&rom,unsigned&checks){auto d=std::make_unique<kstation::ControllerDeviceState>();d->loadMemory(rom.data(),rom.size(),0);d->loadMemory(rom.data(),rom.size(),0x400000);for(auto r:Runtime::CodeRanges){for(uint32_t base:{0u,0x400000u})for(auto edge:{r.begin,r.end-1}){auto at=base+edge;d->memory[at]^=1;Runtime bad;if(bad.bindVerifiedFirmware(*d,rom.data(),rom.size()))return false;d->memory[at]^=1;++checks;}}Runtime good;if(!good.bindVerifiedFirmware(*d,rom.data(),rom.size()))return false;for(auto r:Runtime::CodeRanges){for(uint32_t base:{0u,0x400000u,0x1000000u,0x1400000u}){bool rejected=false;try{d->writeResource(base+r.begin,0);}catch(const std::logic_error&){rejected=true;}if(!rejected)return false;++checks;for(unsigned width:{2u,4u}){auto at=base+r.begin-(width-1);auto before=d->memory[at&0xffffff];rejected=false;try{if(width==2)d->write16(at,0x1234);else d->write32(at,0x12345678);}catch(const std::logic_error&){rejected=true;}if(!rejected||d->memory[at&0xffffff]!=before)return false;++checks;}}}return true;}
template<class K>bool testBoot(const std::vector<uint8_t>&rom,unsigned&checks){for(auto range:K::CodeRanges)for(auto edge:{range.begin,range.end-1})for(uint32_t base:{0u,0x400000u}){auto d=std::make_unique<kstation::ControllerDeviceState>();d->loadMemory(rom.data(),rom.size(),0);d->loadMemory(rom.data(),rom.size(),0x400000);d->memory[base+edge]^=1;K k;bool low=k.bindBootResources(*d,rom.data(),rom.size());if(base==0){if(low)return false;}else if(!low||k.bindMirroredFirmware(*d,rom.data(),rom.size()))return false;++checks;}auto d=std::make_unique<kstation::ControllerDeviceState>();d->loadMemory(rom.data(),rom.size(),0);d->loadMemory(rom.data(),rom.size(),0x400000);K k;if(!k.bindBootResources(*d,rom.data(),rom.size())||!k.bindMirroredFirmware(*d,rom.data(),rom.size()))return false;for(auto r:K::CodeRanges)for(uint32_t base:{0u,0x400000u,0x1000000u,0x1400000u})for(unsigned width:{1u,2u,4u}){auto a=base+r.begin-(width-1);auto old=d->memory[a&0xffffff];bool rejected=false;try{if(width==1)d->write8(a,0);else if(width==2)d->write16(a,0);else d->write32(a,0);}catch(const std::logic_error&){rejected=true;}if(!rejected||old!=d->memory[a&0xffffff])return false;++checks;}return true;}
int main(int argc,char**argv){if(argc!=2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};if(rom.size()!=49728)return 1;unsigned checks=0;if(!test<kstation::ControllerProgramRuntime>(rom,checks)||!test<kstation::ControllerMidiControlRuntime>(rom,checks)||!test<kstation::ControllerMidiSysExRuntime>(rom,checks)||!test<kstation::ControllerMidiInitRuntime>(rom,checks)||!testBoot<kstation::ControllerStartupKeyboardRuntime>(rom,checks)||!testBoot<kstation::ControllerStartupServicesRuntime>(rom,checks)||!testBoot<kstation::ControllerStartupVolumeRuntime>(rom,checks)||!test<kstation::ControllerStartupDisplayRuntime>(rom,checks)||!test<kstation::ControllerUiEncoderRuntime>(rom,checks)||!test<kstation::ControllerUiEncoderRouteRuntime>(rom,checks)||!test<kstation::ControllerUiModeRuntime>(rom,checks)||!test<kstation::ControllerAdcEventRuntime>(rom,checks)||!test<kstation::ControllerMainUiAdcRuntime>(rom,checks)||!test<kstation::ControllerMainUiKeyRuntime>(rom,checks)||!test<kstation::ControllerPanelMidiRuntime>(rom,checks)||!test<kstation::ControllerForegroundClockRuntime>(rom,checks)||!test<kstation::ControllerPeriodicUiRuntime>(rom,checks))return 2;std::printf("{\"passed\":true,\"tamper_and_wrapped_write_checks\":%u}\n",checks);}
