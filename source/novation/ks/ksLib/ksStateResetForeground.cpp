#include "ksStateResetForeground.h"
#include "ksControllerState.h"
#include <stdexcept>

namespace ks {
bool StateResetForeground::step(ControllerCpu& h){
    using K=StateResetService::Kind;using C=StateResetService::Component;
    if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.interruptMode2)return false;
    if(!job_){
        if(h.indent<=0)return false; // A reset returns through an existing outer caller.
        K kind;
        switch(h.getPC()){
        case 0x4486:kind=K::State;break;
        case 0x13fe:kind=K::Buttons;break;
        case 0x1578:kind=K::Controls;break;
        case 0x1efe:kind=K::Keyboard;break;
        default:return false;
        }
        const auto stack=h.getSP();
        if((stack&1)||stack<0xfff400||stack>0xfffbfc)return false;
        const int16_t saved=kind==K::State||kind==K::Keyboard?h.regs[6].r:h.regs[0].r;
        const auto flags=uint8_t((h.ccr&0xd0)|(saved<0?8:saved==0?4:0));
        job_.emplace(kind,stack,h.indent,flags);++started;++kinds[unsigned(kind)];
    }
    auto& job=*job_;if(job.depth!=h.indent)return false;
    const auto cost=job.service.nextClocks();const auto start=h.cycles;
    const auto before=job.service.component();const bool childStart=job.childStart;job.childStart=false;
    const auto history=[](uint32_t address){return address<0x400000||(address>=0xfffd10&&address<0xffffa0)?int(address):-1;};
    const auto used=job.service.advanceBefore(cost,[&](uint32_t address,unsigned offset){
        h.cycles=start+offset;h.lastread=history(address);return h.readResource(address);
    },[&](uint32_t address,uint8_t value,unsigned offset){
        h.cycles=start+offset;h.lastwrite=history(address);h.writeResource(address,value);
    });
    if(!cost||used!=cost)throw std::logic_error("State reset phase admission differs");
    if(childStart)h.lastwrite=-1;
    if(before==C::State&&job.service.component()!=C::State){h.lastwrite=-1;job.childStart=true;}
    components|=job.service.components();++phases;
    if(job.service.complete()){
        // This frame belongs to the still-compatible outer caller. Internal
        // reset calls never wrote a frame and the graph never changed SP/PC.
        uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
        h.setSP(job.caller+4);h.pc=h.makepc(target&0xffffff);h.ccr=job.flags;--h.indent;
        job_.reset();++completed;
    }
    h.cycles=start+cost;return true;
}
}
