// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
static std::vector<uint8_t> bytes(const char* p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char** argv){if(argc!=5 && argc!=6)return 2;try{
 auto flash=bytes(argv[3]);auto fixture=flash;std::fill(fixture.begin()+0x20000,fixture.begin()+0x45800,0xff);std::copy_n(flash.begin()+0x2c800,256,fixture.begin()+0x2c800);
 ks::Board b(bytes(argv[1]),bytes(argv[2]),fixture);if(!b.boot())throw std::runtime_error("Boot failed");
 auto midi=[&](std::initializer_list<uint8_t> v){if(!b.enqueueMidi(v.begin(),v.size()))throw std::runtime_error("MIDI overflow");};
 if(argc==6){
  midi({0xb0,7,127,0xb0,91,0,0xb0,92,0,0xb0,93,0,0x90,36,64,0x90,69,64,0x90,96,64});for(unsigned n=0;n<8192;++n)b.renderSample();
  if(std::string(argv[5])=="paired")midi({0x80,36,0,0x80,69,0,0x80,96,0});
  midi({0xb0,120,0});for(unsigned n=0;n<45340;++n)b.renderSample();
 }
 for(unsigned patch:{27,29,30}){
  midi({0xb0,120,0});for(unsigned i=0;i<45340;++i)b.renderSample();
  std::array<uint8_t,273> msg{0xb0,32,33,0xf0,0,0x20,0x29,1,0x42,0x7f,0,0,0x11,2,0,0};std::copy_n(flash.begin()+0x20000+patch*256,256,msg.begin()+16);msg.back()=0xf7;
  b.enqueueMidi(msg.data(),msg.size());for(unsigned i=0;i<45340;++i)b.renderSample();
  std::string name(reinterpret_cast<const char*>(flash.data()+0x20000+patch*256+151),16);std::cout<<"patch="<<patch+100<<" name="<<name<<std::endl;
  std::ofstream out(std::filesystem::path(argv[4])/ (std::to_string(100+patch)+".raw"),std::ios::binary);unsigned sample=0;
  auto render=[&](unsigned frames,const char* stage){const auto begin=sample;double energy=0;for(unsigned n=0;n<frames;++n){auto a=b.renderSample();out.write(reinterpret_cast<const char*>(a.data()),sizeof(a));energy+=double(a[0])*a[0]/(8388608.*8388608.);++sample;}std::cout<<stage<<" begin="<<begin<<" end="<<sample<<" rms="<<sqrt(energy/frames)<<" tick="<<ks::test::comparisonCpu(b).read32(0xffec10)<<std::endl;};
  render(45340,"idle");midi({0x90,48,100});render(45340*3,"on48");midi({0x80,48,0});render(45340*4,"off48");midi({0xb0,123,0});render(45340,"allnotesoff");midi({0xb0,120,0});render(45340,"allsoundoff");
  for(uint8_t note:{60,48,67}){midi({0x90,note,100});render(45340,"repeat-on");midi({0x80,note,0});render(45340,"repeat-off");}midi({0xb0,120,0});render(45340,"end");
 }
 std::cout<<"serial_depth="<<b.serialDepth()[0]<<","<<b.serialDepth()[1]<<" underruns="<<b.serialUnderruns[0]<<","<<b.serialUnderruns[1]<<" overflows="<<b.serialOverflows[0]<<","<<b.serialOverflows[1]<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}return 0;}
