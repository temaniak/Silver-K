#include "ksDspWorkers.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct Context {
    std::atomic<unsigned> entered{0};
    std::array<int64_t,2> results{};
    std::array<std::thread::id,2> ids{};
    bool throwA=false,throwB=false,requireOverlap=false;
    static void execute(void* p,unsigned i,int64_t target,bool sampled){
        auto& c=*static_cast<Context*>(p);c.ids[i]=std::this_thread::get_id();
        if(c.requireOverlap){c.entered.fetch_add(1);while(c.entered.load()!=2)std::this_thread::yield();}
        c.results[i]=target+(sampled?1:0);
        if((i==0 && c.throwA)||(i==1 && c.throwB))throw std::runtime_error("injected DSP failure");
    }
};
}
int main(){try{
    Context c;
    {
        ks::DspWorkers workers(&c,Context::execute,48000,64);
        c.requireOverlap=true;workers.run({17,23},true);c.requireOverlap=false;
        check(c.results==std::array<int64_t,2>{18,24},"Payload visibility");
        check(c.ids[0]!=c.ids[1] && c.ids[1]!=std::this_thread::get_id(),"DSPs did not use distinct execution threads");
#ifdef __APPLE__
        check(c.ids[0]==std::this_thread::get_id(),"macOS DSP A did not use the coordinator");
#else
        check(c.ids[0]!=std::this_thread::get_id(),"DSP A did not use its helper");
#endif
        for(int64_t n=0;n<10000;++n){workers.run({n,n+7},false);check(c.results==std::array<int64_t,2>{n,n+7},"Lost slice/completion");}
        for(unsigned mask=1;mask<=3;++mask){
            c.throwA=bool(mask&1);c.throwB=bool(mask&2);bool caught=false;
            try{workers.run({50,60},false);}catch(const std::runtime_error&){caught=true;}
            check(caught && c.results==std::array<int64_t,2>{50,60},"Failure did not drain both workers");
        }
        c.throwA=c.throwB=false;workers.configure(44100,512);workers.run({70,80},false);
        check(c.results==std::array<int64_t,2>{70,80},"Recovery after failed slice");
        for(unsigned n=0;n<20;++n){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            workers.run({90+n,100+n},false);check(c.results==std::array<int64_t,2>{90+n,100+n},"Lost wake after idle");
        }
    }
    for(unsigned n=0;n<20;++n){ks::DspWorkers idle(&c,Context::execute,48000,128);}
    std::cout<<"PASS: two overlapping DSP execution threads, platform ownership, 10000 handoffs, failure drain/recovery, idle wake and shutdown\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
