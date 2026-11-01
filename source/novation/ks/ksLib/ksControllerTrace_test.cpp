#include "ksControllerTrace.h"
#include "ksRecordedController_test.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"Invalid trace/controller operation accepted");}
}
int main(){try{
    ks::ControllerRecorder record(17,1000,8,4);
    record.beginSlice(1000);record.port(1002,0x1234,0,4,6,false,true);record.port(1010,0x1238,1,4,6,false,true);
    record.port(1015,0x123a,0,10,0x12,true,true);record.port(1018,0x123c,0,12,0x34,true,true);record.port(1020,0x123e,0,14,0x56,true,true);record.endSlice(1128);
    record.beginSlice(1128);record.port(1130,0x1240,1,2,0x80,true,true);record.endSlice(1258);
    auto tape=record.finish();tape->validate();check(tape->events.size()==6 && tape->slices.size()==2,"Recording counts differ");
    record.beginSlice(1258);record.endSlice(1386);check(tape->slices.size()==2,"Sealed recording mutated");
    const auto path="ks-controller-synthetic-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".kst";
    tape->save(path);auto loaded=ks::ControllerTape::load(path);check(loaded->digest()==tape->digest(),"Round trip differs");rejects([&]{tape->save(path);});
    ks::RecordedController controller(loaded,17,1000);unsigned reads=0,writes=0;uint32_t word=0;
    auto read=[&](unsigned dsp,unsigned offset){check(dsp<2 && offset==4,"Wrong read route");++reads;return uint8_t(6);};
    auto write=[&](unsigned dsp,unsigned offset,uint8_t value){++writes;if(!dsp)word=(word<<8)|value;else check(offset==2 && value==0x80,"Wrong second DSP write");return true;};
    check(controller.advance(128,read,write)==1128 && controller.advance(128,read,write)==1258,"Slice target changed");
    check(reads==2 && writes==4 && word==0x123456 && controller.checkedPorts()==6,"Port reconstruction differs");
    rejects([&]{controller.advance(128,read,write);});rejects([&]{ks::RecordedController bad(loaded,18,1000);});
    ks::RecordedController badRead(loaded,17,1000);rejects([&]{badRead.advance(128,[](unsigned,unsigned){return uint8_t(0);},write);});rejects([&]{badRead.advance(128,read,write);});
    ks::RecordedController badWrite(loaded,17,1000);rejects([&]{badWrite.advance(128,read,[](unsigned,unsigned,uint8_t){return false;});});
    auto malformed=*loaded;malformed.events[0].dsp=2;rejects([&]{malformed.validate();});
    malformed=*loaded;malformed.slices[1].first=100;rejects([&]{malformed.validate();});
    ks::ControllerRecorder full(0,0,0,1);full.beginSlice(0);full.port(0,0,0,4,0,false,true);full.endSlice(128);rejects([&]{full.finish()->validate();});
    {std::fstream corrupt(path,std::ios::in|std::ios::out|std::ios::binary);corrupt.seekp(56+loaded->slices.size()*24+12);corrupt.put(char(1));}
    rejects([&]{ks::ControllerTape::load(path);});
    std::cout<<"PASS: both DSP routes, byte reconstruction, exact slices, checked responses, terminal mismatch/exhaustion, bounded recorder, sealed tape, portable file round-trip/checksum and overwrite refusal\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
