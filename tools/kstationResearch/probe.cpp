#include "kstationHardware.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <sys/resource.h>

namespace {
using Clock=std::chrono::steady_clock;
bool recording=false;
std::array<uint64_t,65536> pcCount{},pcCycles{},portReads{},portWrites{};
uint64_t portHash=14695981039346656037ull;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double cpuSeconds(){rusage r{};require(getrusage(RUSAGE_SELF,&r)==0,"getrusage failed");return r.ru_utime.tv_sec+r.ru_utime.tv_usec*1e-6+r.ru_stime.tv_sec+r.ru_stime.tv_usec*1e-6;}
void hashWord(uint64_t& h,uint64_t v){for(unsigned i=0;i<8;++i){h^=uint8_t(v>>(i*8));h*=1099511628211ull;}}
std::vector<uint8_t> load(const std::string& path){std::ifstream f(path,std::ios::binary);require(bool(f),"Cannot read firmware");return {std::istreambuf_iterator<char>(f),{}};}
void send(kstation::Hardware& board,std::initializer_list<uint8_t> bytes){require(board.enqueueMidi(bytes.begin(),bytes.size()),"MIDI queue full");}
struct Event{uint64_t frame;std::array<uint8_t,3>bytes;unsigned size;};
std::vector<Event> events(const std::string& scenario,uint64_t frames){
 std::vector<Event> e;
 auto add=[&](uint64_t f,uint8_t a,uint8_t b,uint8_t c){e.push_back({f,{a,b,c},3});};
 const auto rate=kstation::DSP::SampleRate;
 if(scenario=="sweep")for(uint64_t f=0,n=0;f<frames;f+=uint64_t(rate/50),++n)add(f,0xb0,105,uint8_t(n%254<128?n%254:254-n%254));
 if(scenario=="notes")for(uint64_t f=0,n=0;f<frames;f+=uint64_t(rate/8),++n){add(f,0x90,uint8_t(48+n%24),100);if(f+uint64_t(rate/16)<frames)add(f+uint64_t(rate/16),0x80,uint8_t(48+n%24),0);}
 if(scenario=="program")for(uint64_t f=0;f<frames;f+=uint64_t(rate/2)){e.push_back({f,{0xc0,0,0},2});}
 return e;
}
void setup(kstation::Hardware& board,const std::string& scenario){
 // Existing firmware-generated Init sound. Dry sustained oscillator, exact CC
 // values retained in before/after snapshots. No direct guest state mutation.
 send(board,{0xb0,7,127});send(board,{0xb0,9,56});send(board,{0xb0,108,0});
 send(board,{0xb0,110,127});send(board,{0xb0,111,0});
 send(board,{0xb0,91,0});send(board,{0xb0,92,0});send(board,{0xb0,93,0});
 send(board,{0xb0,105,127});send(board,{0xb0,87,3});send(board,{0xb0,89,0});
 for(unsigned i=0;i<8192;++i)board.renderSample();
 if(scenario=="arp"){send(board,{0xb0,87,3});send(board,{0xb0,88,64});send(board,{0xb0,89,12});}
 unsigned keys=scenario=="idle"||scenario=="notes"?0:scenario=="held1"?1:8;
 for(unsigned n=0;n<keys;++n)send(board,{0x90,uint8_t(48+n*3),100});
}
struct Result{uint64_t hash=14695981039346656037ull,frames=0;double peak=0,squares=0,wall=0,cpu=0,maxBlock=0;uint64_t overBudget=0;};
Result render(kstation::Hardware& board,uint64_t frames,const std::vector<Event>& events,bool paced,unsigned block,std::vector<int32_t>* pcm=nullptr){
 Result r;size_t next=0;const auto begin=Clock::now();const double before=cpuSeconds();
 for(uint64_t cursor=0;cursor<frames;){const auto blockBegin=Clock::now();const auto end=std::min(frames,cursor+block);
  for(;cursor<end;++cursor){while(next<events.size()&&events[next].frame==cursor){require(board.enqueueMidi(events[next].bytes.data(),events[next].size),"Event MIDI queue full");++next;}
   const auto frame=board.renderSample();for(auto v:frame){const double x=v/8388608.0;require(std::isfinite(x),"Nonfinite PCM");r.peak=std::max(r.peak,std::abs(x));r.squares+=x*x;hashWord(r.hash,uint32_t(v));if(pcm)pcm->push_back(v);}
  }
  const double spent=std::chrono::duration<double>(Clock::now()-blockBegin).count();r.maxBlock=std::max(r.maxBlock,spent);if(spent>block/kstation::DSP::SampleRate)++r.overBudget;
  if(paced)std::this_thread::sleep_until(begin+std::chrono::nanoseconds(int64_t(cursor*1e9/kstation::DSP::SampleRate)));
 }
 r.cpu=cpuSeconds()-before;r.wall=std::chrono::duration<double>(Clock::now()-begin).count();r.frames=frames;return r;
}
void byteFile(const std::string&path,const void* data,size_t bytes){std::ofstream f(path,std::ios::binary);f.write(static_cast<const char*>(data),std::streamsize(bytes));require(bool(f),"Cannot write evidence");}
}

void kResearchStep(unsigned pc,unsigned long long begin,unsigned long long end){if(recording&&pc<pcCount.size()){++pcCount[pc];pcCycles[pc]+=end-begin;}}
void kResearchPort(unsigned pc,unsigned long long cycle,unsigned address,unsigned value,bool write){if(!recording)return;if(pc<portReads.size())++(write?portWrites[pc]:portReads[pc]);hashWord(portHash,cycle);hashWord(portHash,pc);hashWord(portHash,address);hashWord(portHash,value);hashWord(portHash,write);}

int main(int argc,char** argv){try{
 require(argc==9,"Usage: probe firmware scenario seconds mode(off|capture|detailed|observe) pacing(offline|paced) block output-prefix warm-seconds");
 const auto firmware=load(argv[1]);require(firmware.size()==49728,"Decoded firmware expected; validate SHA in runner");
 const std::string scenario=argv[2],mode=argv[4],prefix=argv[7];
 require(scenario=="idle"||scenario=="held1"||scenario=="held8"||scenario=="notes"||scenario=="sweep"||scenario=="arp"||scenario=="program","Unknown scenario");
 require(mode=="off"||mode=="capture"||mode=="detailed"||mode=="observe","Unknown mode");
#ifndef K_RESEARCH_OBSERVE
 require(mode!="observe","Use observer executable");
#endif
 const double seconds=std::stod(argv[3]),warmSeconds=std::stod(argv[8]);const unsigned block=std::stoul(argv[6]);require(seconds>0&&seconds<=120&&warmSeconds>=0&&block>=16&&block<=2048,"Bad duration/block");
 const bool paced=std::string(argv[5])=="paced";require(paced||std::string(argv[5])=="offline","Unknown pacing");
 kstation::Hardware board(firmware);require(board.boot(),"Boot failed");setup(board,scenario);
 const auto warmFrames=uint64_t(warmSeconds*kstation::DSP::SampleRate);render(board,warmFrames,events(scenario,warmFrames),paced,block);
 std::array<uint8_t,128> beforePatch{};board.controller().copyCurrentPatch(beforePatch.data());
 const auto before=board.controller().debugState();const auto beforeDsp=board.dsp().instructions(),beforeCycles=board.dsp().cycles(),beforeDrop=board.dsp().audioFramesDropped();
 baseLib::performance::Diagnostics diagnostics;
 if(mode=="detailed"){diagnostics.start(true);board.setPerformanceDiagnostics(&diagnostics);}
 const auto frames=uint64_t(seconds*kstation::DSP::SampleRate);const auto passage=events(scenario,frames);
 std::vector<int32_t> pcm;std::vector<int32_t>* capture=nullptr;
 if(mode=="observe"||mode=="capture"){pcm.reserve(frames*2);capture=&pcm;recording=mode=="observe";}
 const auto result=render(board,frames,passage,paced,block,capture);recording=false;diagnostics.stop();board.setPerformanceDiagnostics(nullptr);
 const auto after=board.controller().debugState();std::array<uint8_t,128> patch{};board.controller().copyCurrentPatch(patch.data());
 const auto h8=diagnostics.read(baseLib::performance::Metric::H8),dsp=diagnostics.read(baseLib::performance::Metric::DspA);
 if(capture)byteFile(prefix+".pcm",pcm.data(),pcm.size()*sizeof(pcm[0]));
 if(mode=="observe"){
  std::ofstream hist(prefix+".pc.tsv");hist<<"pc\tinstructions\tguest_cycles\thdi_reads\thdi_writes\n";
  for(unsigned n=0;n<pcCount.size();++n)if(pcCount[n]||portReads[n]||portWrites[n])hist<<std::hex<<n<<std::dec<<'\t'<<pcCount[n]<<'\t'<<pcCycles[n]<<'\t'<<portReads[n]<<'\t'<<portWrites[n]<<'\n';
 }
 const auto flash=board.controller().saveFlash();uint64_t flashHash=14695981039346656037ull;for(auto b:flash)hashWord(flashHash,b);
 const auto& core=board.controller().core();uint64_t memoryHash=14695981039346656037ull;for(auto b:core.memory)hashWord(memoryHash,b);
 uint64_t midiHash=14695981039346656037ull,midiCount=0;uint8_t midiByte;while(board.popMidiOut(midiByte)){hashWord(midiHash,midiByte);++midiCount;}
 if(capture){byteFile(prefix+".memory",core.memory,sizeof(core.memory));byteFile(prefix+".flash",flash.data(),flash.size());}
 std::ofstream out(prefix+".json");out<<std::setprecision(12)<<"{\n\"schema\":\"kstation.research.v1\",\n\"scenario\":\""<<scenario<<"\",\"mode\":\""<<mode<<"\",\"pacing\":\""<<(paced?"paced":"offline")<<"\",\n";
 out<<"\"block\":"<<block<<",\"native_rate\":"<<kstation::DSP::SampleRate<<",\"frames\":"<<result.frames<<",\"audio_seconds\":"<<result.frames/kstation::DSP::SampleRate<<",\"warm_seconds\":"<<warmSeconds<<",\n";
 out<<"\"process_cpu_seconds\":"<<result.cpu<<",\"wall_seconds\":"<<result.wall<<",\"cpu_percent_one_core\":"<<100*result.cpu/(result.frames/kstation::DSP::SampleRate)<<",\"peak\":"<<result.peak<<",\"rms\":"<<sqrt(result.squares/(2*frames))<<",\"pcm_fnv\":\""<<result.hash<<"\",\n";
 out<<"\"h8_instructions\":"<<after.instructions-before.instructions<<",\"h8_cycles\":"<<after.cycles-before.cycles<<",\"dsp_instructions\":"<<board.dsp().instructions()-beforeDsp<<",\"dsp_cycles\":"<<board.dsp().cycles()-beforeCycles<<",\"hdi_reads\":"<<after.hostReads-before.hostReads<<",\"hdi_writes\":"<<after.hostWrites-before.hostWrites<<",\"midi_received\":"<<after.midiReceived-before.midiReceived<<",\n";
 out<<"\"dropped_frames\":"<<board.dsp().audioFramesDropped()-beforeDrop<<",\"block_compute_over_budget\":"<<result.overBudget<<",\"max_block_compute_seconds\":"<<result.maxBlock<<",\"h8_sample_count\":"<<h8.count<<",\"dsp_sample_count\":"<<dsp.count<<",\"h8_sampled_wall_ns\":"<<h8.total<<",\"dsp_sampled_wall_ns\":"<<dsp.total<<",\"flash_fnv\":\""<<flashHash<<"\",\"observed_port_fnv\":\""<<portHash<<"\",\"memory_fnv\":\""<<memoryHash<<"\",\"midi_out_count\":"<<midiCount<<",\"midi_out_fnv\":\""<<midiHash<<"\",\n";
 out<<"\"before_patch\":[";for(unsigned i=0;i<128;++i)out<<(i?",":"")<<unsigned(beforePatch[i]);out<<"],\"after_patch\":[";for(unsigned i=0;i<128;++i)out<<(i?",":"")<<unsigned(patch[i]);out<<"],\"bank\":"<<unsigned(after.currentBank)<<",\"program\":"<<unsigned(after.currentProgram)<<"\n}\n";
 require(bool(out),"Cannot save JSON");std::cout<<scenario<<" "<<mode<<" cpu="<<100*result.cpu/(frames/kstation::DSP::SampleRate)<<"% peak="<<result.peak<<" dropped="<<board.dsp().audioFramesDropped()-beforeDrop<<"\n";
 require(board.dsp().audioFramesDropped()==beforeDrop,"Dropped native frames");require(scenario=="idle"||result.peak>1e-5,"Silent sounding scenario");
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
