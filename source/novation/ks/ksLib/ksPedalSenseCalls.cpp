#include "ksPedalSenseCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PedalSensePartialTest partialTest=nullptr;using T=PedalSenseTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,24> points{{{0x13ae,2},{0x13b0,2},{0x13b2,2},{0x13b4,2},{0x13b6,6},{0x13bc,2},{0x13be,4},{0x13c2,6},{0x13c8,8},{0x13d0,2},{0x13d2,2},{0x13d4,2},{0x13d6,6},{0x13dc,6},{0x13e2,2},{0x13e4,2},{0x13e6,2},{0x13e8,2},{0x13ea,2},{0x13ec,2},{0x13ee,2},{0x13f0,2},{0x13f2,4},{0x13f6,2}}};
}
void setPedalSensePartialForTests(PedalSensePartialTest value){partialTest=value;}
bool PedalSenseCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x13ae||pc==0x13b4||pc==0x13be){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native pedal capacity exceeded");
        slot->emplace(pc==0x13ae?T::Kind::Switches:pc==0x13b4?T::Kind::Reset:T::Kind::Pedal,h.getSP());++started;
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(field==T::Read::Switches?0xffff53:field==T::Read::Polarity?0x168fc+254:field==T::Read::Port?0xffff50:0x1537a));};
    auto write=[&](uint8_t value){f.store8(0x1537a,int8_t(value));};
    if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native pedal admission differs");
    switch(stage){
    case S::ReadSwitches:f.move(r[6].rl,task.switches());break;
    case S::MaskSwitches:r[6].rl&=0x30;value(h,r[6].rl);break;
    case S::ReturnSwitches:f.ret();break;
    case S::SetReset:f.move(r[0].rl,0xffffffff);break;
    case S::ReturnReset:f.ret();break;
    case S::SaveScratch:f.push(r[0].er);break;
    case S::SelectGlobals:f.move(r[0].er,0x168fc);break;
    case S::ReadPolarity:f.move(r[0].rh,task.polarity());break;
    case S::ReadPort:f.move(r[0].rl,task.port());break;
    case S::RotatePolarity:f.rotate(r[0].rh,2,false,false);break;
    case S::NormalizePort:r[0].rl^=r[0].rh;value(h,r[0].rl);break;
    case S::ReadHistory:f.move(r[0].rh,task.history());break;
    case S::DetectTransition:r[0].rh^=r[0].rl;value(h,r[0].rh);break;
    case S::SetUnchanged:f.move(r[6].rh,0xffffff80);break;
    case S::TestTransition:flag(h,4,(uint8_t(r[0].rh)&(1u<<0x6))==0);break;
    case S::SetReleased:f.move(r[6].rh,0x0);break;
    case S::TestReleased:flag(h,4,(uint8_t(r[0].rl)&(1u<<0x6))==0);break;
    case S::SetPressed:f.move(r[6].rh,0x7f);break;
    case S::RestoreScratch:f.pop(r[0].er);break;
    case S::ReturnPedal:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native pedal clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    call.token=h.getSP();h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
