#include "ksNativeArpRandomService.h"
#include "ksNativeArpRandomAbi.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{using T=ks::NativeArpRandomService;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;for(unsigned seed=0;seed<256;++seed)for(unsigned count:{0u,1u,127u,255u}){const uint32_t counter=seed*0x1020304u;const uint16_t initial=uint16_t(seed*257),inc=uint16_t(seed*127);const auto word=[&](unsigned a,uint16_t v){m[a]=uint8_t(v>>8);m[a+1]=uint8_t(v);};for(unsigned n=0;n<4;++n)m[0xffec10+n]=uint8_t(counter>>(24-8*n));word(0x15c30,initial);word(0x15c2e,inc);m[0x160d2+307]=uint8_t(count);tracking=true;{T s({0});while(!s.complete()){const auto cost=s.nextClocks();check(cost,"Random stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;check(!s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;})&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Random budget mutation");++rejects;check(s.advanceBefore(cost,[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Random read span/time");++reads;return m[at];},[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Random write span/time");++writes;m[at]=v;})==cost,"Random phase admission");++phases;}const uint16_t expected=uint16_t(uint16_t(counter)*uint32_t(initial)+inc);check(s.summary().published==expected&&s.summary().index==uint16_t(count*uint32_t(expected)>>16),"Random independent product/index oracle");struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;}a;projectNativeArpRandom(a,s);check(a.values[6]==uint32_t(s.summary().index)*0x10001u,"Random final result");}tracking=false;++graphs;}check(!allocations,"Random heap");std::cout<<"PASS Pure NativeArpRandom graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
