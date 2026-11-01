// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
std::vector<uint8_t> bytes(const char* p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
static_assert(ks::Board::dspTime(uint64_t(ks::Board::ControllerClockHz)*7200)==ks::DSP::ClockHz*7200,"Clock conversion overflows in long sessions");
int main(int argc,char** argv){if(argc!=5)return 2;try{
 auto flash=bytes(argv[3]);auto init=std::vector<uint8_t>(flash.begin()+0x2c800,flash.begin()+0x2c900);
 // A private INIT-only test fixture avoids irrelevant librarian uploads; original
 // reset, waveform loader, UART, HDI and DSP processing remain active.
 std::fill(flash.begin()+0x20000,flash.begin()+0x45800,0xff);std::copy(init.begin(),init.end(),flash.begin()+0x2c800);
 ks::Board b(bytes(argv[1]),bytes(argv[2]),flash);if(!b.boot())throw std::runtime_error("Boot failed");
 auto midi=[&](std::initializer_list<uint8_t> v){if(!b.enqueueMidi(v.begin(),v.size()))throw std::runtime_error("MIDI overflow");};
 midi({0xb0,7,127,72,127,73,0,74,0,75,0,76,0,91,0,92,0,93,0,90,0,108,0,111,0,105,127,99,0,98,50,6,0});
 for(unsigned n=0;n<32768;++n)b.renderSample();
 const auto ca=b.dsp(0).cycles(),cb=b.dsp(1).cycles(),ch=ks::test::comparisonCpu(b).cycles,fa=b.dsp(0).audioFramesProduced(),fb=b.dsp(1).audioFramesProduced();
 const auto under=b.serialUnderruns,over=b.serialOverflows;
 std::ofstream out(argv[4],std::ios::binary);uint64_t samples=0;size_t maxDepth=0;
 auto render=[&](unsigned n){for(unsigned i=0;i<n;++i){auto frame=b.renderSample();out.write(reinterpret_cast<const char*>(frame.data()),sizeof(frame));++samples;for(auto depth:b.serialDepth())maxDepth=std::max(maxDepth,depth);}};
 render(45340);midi({0x90,69,100});render(45340*3);midi({0x80,69,0});render(45340);
 for(uint8_t key:{36,60,96}){midi({0xb0,120,0});render(8192);const auto begin=samples;midi({0x90,key,100});render(45340*2);midi({0x80,key,0});std::cout<<"key="<<int(key)<<" begin="<<begin<<" end="<<samples<<std::endl;}
 midi({0xb0,120,0});render(8192);const auto chordBegin=samples;
 for(uint8_t key:{48,52,55,60,64,67})midi({0x90,key,80});render(45340*3);midi({0xb0,120,0});
 std::cout<<"chord_begin="<<chordBegin<<" chord_end="<<samples<<std::endl;
 const auto da=b.dsp(0).audioFramesProduced()-fa,db=b.dsp(1).audioFramesProduced()-fb;
 std::cout<<"sample_rate="<<ks::DSP::SampleRate<<" H8_cycles="<<ks::test::comparisonCpu(b).cycles-ch<<" A_cycles="<<b.dsp(0).cycles()-ca<<" B_cycles="<<b.dsp(1).cycles()-cb<<" A_frames="<<da<<" B_frames="<<db<<" frames="<<samples<<" max_link_depth="<<maxDepth<<std::endl;
 if(da!=samples || db!=samples || maxDepth>1 || b.serialUnderruns!=under || b.serialOverflows!=over)throw std::runtime_error("Serial clock/link drift, underflow or overflow");
 std::cout<<"PASS: shared serial slots, equal A/B frame counts, no lost link frames, long-session clock conversion"<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}return 0;}
