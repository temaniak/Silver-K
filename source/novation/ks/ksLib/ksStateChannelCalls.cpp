#include "ksStateChannelCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
StateChannelPartialTest partialTest=nullptr;
struct Point{unsigned pc,length;};
constexpr std::array<Point,20> points{{
    {0x43a8,6},{0x43ae,2},{0x43b0,6},{0x43b6,2},{0x43b8,2},
    {0x43ba,6},{0x43c0,2},{0x43c2,6},{0x43c8,8},{0x43d0,8},
    {0x43d8,6},{0x43de,8},{0x43e6,2},{0x43e8,4},{0x43ec,6},
    {0x43f2,2},{0x43f4,4},{0x43f8,8},{0x4400,4},{0x4404,2}
}};
}
void setStateChannelPartialForTests(StateChannelPartialTest value){partialTest=value;}
bool StateChannelCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using T=StateChannelTask;using S=T::Stage;
    const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x43a8||pc==0x43c2||pc==0x43e8){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native state channel capacity exceeded");
        slot->emplace(pc==0x43a8?T::Kind::ModeStatus:pc==0x43c2?T::Kind::GlobalMirror:T::Kind::SelectedChannel,h.getSP());++started;
    }
    const auto pointOf=[](const T& task){return points[unsigned(task.awaitingControls()?S::ReadSelectedChannel:task.stage())];};
    if(!slot||pointOf((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingControls())task.resumeControls();
    const auto stage=task.stage();const auto point=pointOf(task);
    const unsigned address=stage==S::WriteMessage?0x167d6:stage==S::WriteGlobalChannel?0x168fc+181:0x174ae + 11;
    const unsigned writeClocks=(h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3:0;
    const auto cost=task.nextClocks(writeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point.length);
    auto read=[&](T::Read field)->uint8_t{
        switch(field){
        case T::Read::Mode:return uint8_t(h.read8(0x167ba));
        case T::Read::MultiStatus:return uint8_t(h.read8(0x167d5));
        case T::Read::GlobalChannel:return uint8_t(h.read8(0x168fc+242));
        case T::Read::SelectedPart:return uint8_t(h.read8(0x16798));
        case T::Read::SelectedChannel:return uint8_t(h.read8(0x174ae + 20*task.part()+11));
        }return 0;
    };
    auto write=[&](T::Write field,uint8_t value){
        f.store8(field==T::Write::Message?0x167d6:field==T::Write::GlobalChannel?0x168fc+181:0x174ae + 11,int8_t(value));
    };
    if(task.advanceBefore(cost,read,write,writeClocks)!=cost)throw std::runtime_error("Native state channel admission differs");
    switch(stage){
    case S::ReadMode:f.move(r[0].rl,task.mode());break;
    case S::ReadMultiStatus:f.move(r[0].rl,task.multiStatus());break;
    case S::PrepareMessage:f.move(r[0].rl,int8_t(0x91));break;
    case S::SelectGlobals:f.move(r[0].er,0x168fc);break;
    case S::ReadGlobalChannel:f.move(r[1].rl,task.channel());break;
    case S::SelectMirror:f.move(r[0].er,0x174ae);break;
    case S::AdmitContext:f.push(r[0].er);break;
    case S::ReadSelectedPart:f.move(r[0].rl,task.part());break;
    case S::BindSelectedPart:f.move(r[6].rh,r[0].rl);break;
    case S::LookupControls:f.call(0xa670);break;
    case S::ReadSelectedChannel:f.move(r[6].rl,task.channel());break;
    case S::ReleaseContext:f.pop(r[0].er);break;
    case S::ReturnMode:case S::ReturnGlobal:case S::ReturnSelected:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native state channel clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingControls())call.token=h.getSP();
    h.pc=h.makepc(task.awaitingControls()?f.next:pointOf(task).pc);return true;
}
}
