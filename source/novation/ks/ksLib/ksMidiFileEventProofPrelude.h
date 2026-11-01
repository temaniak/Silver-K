#include "ksMidiFileEventCompatibility.h"
#include "ksMidiFileEventBindings.h"
#include "ksNativeMidiDomainBindings.h"

#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>
namespace{bool tracking=false;unsigned allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
using Row=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Original:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice{std::vector<Row>events;uint64_t dspReads=0,lcdReads=0,uartReads=0,dspBusy=0,lcdBusy=0,uartBusy=0;uint8_t read(uint32_t a)override{const auto v=a==0x400004||a==0x600004?uint8_t(dspReads++<dspBusy?0:255):a==0xffff51?uint8_t(lcdReads++<lcdBusy?128:0):a==0xffff8c?uint8_t(uartReads++<uartBusy?state->memory[a]&127:state->memory[a]|128):state->memory[a];events.emplace_back(false,a,v,state->cycles);return v;}void write(uint32_t a,uint8_t v)override{state->memory[a]=v;events.emplace_back(true,a,v,state->cycles);}};
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}struct A{std::array<uint32_t,7>values{};uint8_t flags=0;};
struct FullResolver{bool buttonDescriptor(unsigned,unsigned,unsigned){throw std::logic_error("Unexpected preview button descriptor");}ks::StateCentralActionsCoordinator::ButtonAction menuAction(unsigned,unsigned){throw std::logic_error("Unexpected preview menu action");}static void require(bool b,const char*m){if(!b)throw std::logic_error(m);}ks::ScheduledTimerTask::ActionHandle boundAction(ks::StateCentralActionsService::ActionKind k)const{constexpr unsigned targets[]{0x5006,0x55d4,0x55e2,0x46da,0x471a,0x4a80,0x4ac2,0x5e7c,0x5ebe,0x5e80};return{targets[unsigned(k)]};}ks::StateFieldCoordinator::Handler field(uint8_t c,uint32_t raw)const{constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};require(c<9&&raw==targets[c],"Field binding");return ks::StateFieldCoordinator::Handler(c);}bool schema(uint8_t c,uint32_t raw)const{return (c==0||c==2||c==3||c==4||c==6)&&(field(c,raw),true);}bool operator()(uint8_t c,uint32_t raw)const{return schema(c,raw);}bool packing(uint32_t raw)const{constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(auto a:roots)if((raw&0xffffff)==a)return true;return false;}ks::StatePackFieldsTask::Kind operator()(uint32_t raw)const{constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return ks::StatePackFieldsTask::Kind(n);throw std::logic_error("Packing binding");}};
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}


struct FileResolver:FullResolver{ks::MidiFileEventCoordinator::Kind fileMeta(uint8_t i,uint8_t t,uint32_t h)const{return ks::MidiFileEventBindings{}.fileMeta(i,t,h);}};
