#include "kstationControllerForegroundRuntime.h"
#include "kstationControllerVoiceParameterRuntime.h"
#include "kstationControllerArpVoiceStartRuntime.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
int main(int argc,char**argv){if(argc!=2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};if(rom.size()!=49728)return 1;auto d=std::make_unique<kstation::ControllerDeviceState>();d->loadMemory(rom.data(),rom.size(),0);d->loadMemory(rom.data(),rom.size(),0x400000);for(uint32_t a:{0xc06cu,0xc144u,0xc14au,0xc16bu,0x40c06cu,0x40c144u,0x40c16bu})d->memory[a]^=0x5a;kstation::ControllerForegroundRuntime fg;kstation::ControllerVoiceParameterRuntime vp;kstation::ControllerArpVoiceStartRuntime arp;if(!fg.bindVerifiedFirmware(*d,rom.data(),rom.size())||!vp.bindVerifiedFirmware(*d,rom.data(),rom.size())||!arp.bindVerifiedFirmware(*d,rom.data(),rom.size()))return 2;unsigned writes=0;for(uint32_t base:{0u,0x400000u,0x1000000u,0x1400000u}){for(uint32_t a:{0xc06cu,0xc144u,0xc14au,0xc16bu}){d->write8(base+a,0xa5);if(d->readResource(base+a)!=0xa5)return 3;++writes;}d->write16(base+0xc144,0x1234);d->write32(base+0xc14a,0x12345678);if(d->readResource(base+0xc144)!=0x12||d->readResource(base+0xc14d)!=0x78)return 4;writes+=2;}std::printf("{\"passed\":true,\"live_writes\":%u,\"three_owners_bound_after_mutation\":true}\n",writes);}
