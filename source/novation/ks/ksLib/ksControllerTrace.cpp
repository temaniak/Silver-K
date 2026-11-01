#include "ksControllerTrace.h"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <cstring>

namespace ks {
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void put(std::ostream& s,uint64_t v,unsigned bytes){for(unsigned n=0;n<bytes;++n)s.put(char(v>>(n*8)));}
uint64_t get(std::istream& s,unsigned bytes){uint64_t v=0;for(unsigned n=0;n<bytes;++n){const auto b=s.get();require(b!=EOF,"Truncated KS controller trace");v|=uint64_t(uint8_t(b))<<(n*8);}return v;}
}
void ControllerTape::validate()const {
    require(complete && !slices.empty(),"KS controller trace incomplete/overflowed");
    require(events.size()<=MaxEvents && slices.size()<=MaxSlices,"KS controller trace exceeds limits");
    uint64_t cycle=beginCycle;size_t first=0;
    for(const auto& slice:slices){
        require(slice.begin==cycle && slice.end>slice.begin && slice.end-slice.begin<4096,"Invalid KS controller slice clock");
        require(slice.first==first && slice.count<=events.size()-first,"Invalid KS controller event range");
        uint64_t portCycle=slice.begin;
        for(size_t n=first;n<first+slice.count;++n){const auto& event=events[n];
            require(event.dsp<2 && event.offset<16 && event.flags<=3 && event.pc<0x1000000,"Invalid KS controller port event");
            require(event.cycle>=portCycle && event.cycle<=slice.end,"Invalid KS controller event clock");portCycle=event.cycle;
        }
        first+=slice.count;cycle=slice.end;
    }
    require(first==events.size() && cycle==endCycle,"Invalid KS controller trace end");
}
void ControllerTape::save(const std::string& path)const {
    validate();std::ifstream existing(path,std::ios::binary);require(!existing.good(),"Refusing to overwrite KS controller trace");
    std::ofstream s(path,std::ios::binary);require(bool(s),"Cannot create KS controller trace");s.write("KSHTR001",8);
    for(auto v:{checkpoint,beginCycle,endCycle,uint64_t(events.size()),uint64_t(slices.size())})put(s,v,8);
    put(s,digest(),8);
    for(const auto& slice:slices){put(s,slice.begin,8);put(s,slice.end,8);put(s,slice.first,4);put(s,slice.count,4);}
    for(const auto& event:events){put(s,event.cycle,8);put(s,event.pc,4);put(s,event.dsp,1);put(s,event.offset,1);put(s,event.value,1);put(s,event.flags,1);}
    s.flush();require(bool(s),"Cannot write KS controller trace");
}
std::shared_ptr<ControllerTape> ControllerTape::load(const std::string& path){
    std::ifstream s(path,std::ios::binary|std::ios::ate);require(bool(s),"Cannot open KS controller trace");
    const std::streamoff bytes=s.tellg();require(bytes>=56 && bytes<=std::streamoff(56+MaxEvents*16+MaxSlices*24),"Invalid KS controller trace size");
    s.seekg(0);char magic[8]{};s.read(magic,8);require(std::memcmp(magic,"KSHTR001",8)==0,"Unknown KS controller trace format");
    auto result=std::make_shared<ControllerTape>();result->checkpoint=get(s,8);result->beginCycle=get(s,8);result->endCycle=get(s,8);
    const auto events=get(s,8),slices=get(s,8);require(events<=MaxEvents && slices<=MaxSlices,"KS controller trace exceeds limits");
    const auto expectedDigest=get(s,8);
    require(bytes==std::streamoff(56+events*16+slices*24),"Invalid KS controller trace length");
    result->slices.resize(size_t(slices));result->events.resize(size_t(events));
    for(auto& v:result->slices){v.begin=get(s,8);v.end=get(s,8);v.first=uint32_t(get(s,4));v.count=uint32_t(get(s,4));}
    for(auto& v:result->events){v.cycle=get(s,8);v.pc=uint32_t(get(s,4));v.dsp=uint8_t(get(s,1));v.offset=uint8_t(get(s,1));v.value=uint8_t(get(s,1));v.flags=uint8_t(get(s,1));}
    result->complete=true;result->validate();require(result->digest()==expectedDigest,"KS controller trace checksum differs");return result;
}
uint64_t ControllerTape::digest()const noexcept {
    uint64_t hash=14695981039346656037ull;auto word=[&](uint64_t v){for(unsigned n=0;n<8;++n){hash^=uint8_t(v>>(n*8));hash*=1099511628211ull;}};
    word(checkpoint);word(beginCycle);word(endCycle);word(events.size());word(slices.size());
    for(const auto& v:slices){word(v.begin);word(v.end);word(v.first);word(v.count);}
    for(const auto& v:events){word(v.cycle);word(v.pc);word(v.dsp);word(v.offset);word(v.value);word(v.flags);}return hash;
}
ControllerRecorder::ControllerRecorder(uint64_t checkpoint,uint64_t begin,size_t events,size_t slices)
 :tape(std::make_shared<ControllerTape>()),eventLimit(events),sliceLimit(slices){
    require(events<=ControllerTape::MaxEvents && slices<=ControllerTape::MaxSlices,"KS trace capacity exceeds limit");
    tape->checkpoint=checkpoint;tape->beginCycle=tape->endCycle=begin;
    tape->events.reserve(events);tape->slices.reserve(slices);
}
void ControllerRecorder::beginSlice(uint64_t cycle) noexcept {
    if(overflow || sealed)return;
    if(active || cycle!=tape->endCycle || tape->slices.size()==sliceLimit){overflow=true;return;}
    pending={cycle,cycle,uint32_t(tape->events.size()),0};active=true;
}
void ControllerRecorder::port(uint64_t cycle,uint32_t pc,unsigned dsp,unsigned offset,uint8_t value,bool write,bool accepted) noexcept {
    if(overflow || sealed)return;
    if(!active || tape->events.size()==eventLimit){overflow=true;return;}
    tape->events.push_back({cycle,pc,uint8_t(dsp),uint8_t(offset),value,uint8_t((write?1:0)|(accepted?2:0))});
}
void ControllerRecorder::endSlice(uint64_t cycle) noexcept {
    if(overflow || sealed)return;
    if(!active){overflow=true;return;}pending.end=cycle;pending.count=uint32_t(tape->events.size()-pending.first);
    tape->slices.push_back(pending);tape->endCycle=cycle;active=false;
}
std::shared_ptr<const ControllerTape> ControllerRecorder::finish(){
    if(!sealed){tape->complete=!overflow && !active && !tape->slices.empty();sealed=true;}return tape;
}
}
