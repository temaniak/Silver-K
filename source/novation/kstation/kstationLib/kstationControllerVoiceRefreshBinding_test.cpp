#include "kstationControllerVoiceRefreshRuntime.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
struct Range{uint32_t begin,end;};
int main(int argc,char**argv){if(argc!=2)return 77;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(f),{}};if(rom.size()!=49728)return 1;constexpr std::array<Range,5>ranges{{{0x785c,0x79a4},{0x72ba,0x72ce},{0x72e2,0x7302},{0x5ba0,0x5bb8},{0x5bb8,0x5bec}}};auto d=std::make_unique<kstation::ControllerDeviceState>();d->loadMemory(rom.data(),rom.size(),0);d->loadMemory(rom.data(),rom.size(),0x400000);unsigned rejected=0,guards=0;for(auto r:ranges)for(uint32_t alias:{0u,0x400000u})for(uint32_t at:{r.begin,r.end-1}){auto a=alias+at;d->memory[a]^=1;kstation::ControllerVoiceRefreshRuntime candidate;if(candidate.bindVerifiedFirmware(*d,rom.data(),rom.size()))return 2;d->memory[a]^=1;++rejected;}kstation::ControllerVoiceRefreshRuntime fg;if(!fg.bindVerifiedFirmware(*d,rom.data(),rom.size())||fg.bindVerifiedFirmware(*d,rom.data(),rom.size()))return 3;for(auto r:ranges)for(uint32_t alias:{0u,0x400000u})for(uint32_t at:{r.begin,r.end-1}){auto a=alias+at;auto value=d->memory[a];auto cycles=d->cycles;try{d->write8(a,uint8_t(value^1));return 4;}catch(const std::logic_error&){}if(value!=d->memory[a]||cycles!=d->cycles)return 5;++guards;}std::printf("PASS voice refresh verified binding: %u mismatched boundaries rejected, %u immutable low/physical guards\n",rejected,guards);}
