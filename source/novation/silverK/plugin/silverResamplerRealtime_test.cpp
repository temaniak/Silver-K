#include "synthLib/resamplerInOut.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>

namespace {
thread_local bool inAudio=false;
thread_local size_t allocations=0;
}
void* operator new(size_t n){if(inAudio)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{std::free(p);}

int main(){
 // A non-integer rate pairing can need one more input-padding sample long
 // after startup. Check minutes of streaming, rather than only cold prewarm.
 constexpr unsigned rate=48000,block=256,seconds=300;
 synthLib::ResamplerInOut converter(1,2);converter.setSamplerates(rate,float(112000000./2304.));
 std::array<float,block> left{},right{},zero{};
 synthLib::TAudioInputs input{};input[0]=zero.data();synthLib::TAudioOutputs output{};output[0]=left.data();output[1]=right.data();
 synthLib::ResamplerInOut::TMidiVec midi,response;uint64_t deviceSamples=0,hash=14695981039346656037ull;
 synthLib::ResamplerInOut::TProcessFunc render=[&](const auto&,const auto& out,size_t count,const auto&,auto&){
  for(size_t n=0;n<count;++n){const auto value=float(std::sin(deviceSamples++*.01));out[0][n]=out[1][n]=value;}
 };
 bool finite=true;
 for(unsigned frame=0;frame<rate*seconds;frame+=block){
  inAudio=true;converter.process(input,output,midi,response,block,render);inAudio=false;
  for(unsigned n=0;n<block;++n){finite=finite && std::isfinite(left[n]) && left[n]==right[n];uint32_t word;std::memcpy(&word,&left[n],sizeof(word));hash^=word;hash*=1099511628211ull;}
 }
 std::cout<<"seconds="<<seconds<<" callback_allocations="<<allocations<<" input_latency="<<converter.getInputLatency()<<" output_latency="<<converter.getOutputLatency()<<" device_samples="<<deviceSamples<<" audio_hash="<<hash<<'\n';
 if(allocations || !finite){std::cerr<<"FAIL: long resampler stream allocated in callback or produced invalid stereo audio\n";return 1;}
 std::cout<<"PASS: long non-integer-rate stream, finite stereo output, no C++ callback allocations\n";return 0;
}
