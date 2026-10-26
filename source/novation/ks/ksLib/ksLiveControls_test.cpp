// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
std::vector<uint8_t> bytes(const char* p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char** argv){if(argc!=4)return 2;try{
 ks::Board b(bytes(argv[1]),bytes(argv[2]),bytes(argv[3]));if(!b.boot())throw std::runtime_error("Boot failed");
 auto midi=[&](std::initializer_list<uint8_t> m){if(!b.enqueueMidi(m.begin(),m.size()))throw std::runtime_error("MIDI overflow");};
 auto render=[&](const char* label){double energy=0,difference=0;int previous=0;for(unsigned i=0;i<45340;++i){auto a=b.renderSample();if(i>8192){double v=a[0]/8388608.,d=(a[0]-previous)/8388608.;energy+=v*v;difference+=d*d;}previous=a[0];}const std::array<double,2> measured{sqrt(energy/37147),sqrt(difference/37147)};std::cout<<label<<" RMS="<<measured[0]<<" diff="<<measured[1]<<" tick="<<ks::test::comparisonCpu(b).read32(0xffec10)<<std::endl;return measured;};
 midi({0xb0,7,127,72,127,73,0,74,0,75,0,76,0,91,0,92,0,93,0,108,0,111,0,105,127});render("setup");
 if(ks::test::comparisonCpu(b).read32(0xffec10)==0)throw std::runtime_error("Original TPU0 tick does not advance");
 midi({0x90,69,100});auto high=render("held high");midi({0xb0,105,20});auto low=render("held low");midi({0xb0,105,127});auto restored=render("held restored");
 if(!(low[1]<high[1]*0.4 && restored[1]>low[1]*2))throw std::runtime_error("Held voice did not respond to cutoff");
 midi({0xb0,72,0});auto muted=render("held level zero");midi({0xb0,72,127});auto level=render("held level restored");
 if(!(muted[0]<high[0]*0.5 && level[0]>muted[0]*2))throw std::runtime_error("Held voice did not respond to mixer");
 midi({0x80,69,0});render("off");std::cout<<"PASS: TPU0 tick, original UART filter and mixer response without retrigger"<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}return 0;}
