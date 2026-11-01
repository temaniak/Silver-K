#include "ksPanelAdcScanCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
PanelAdcScanPartialTest partialTest=nullptr;using T=PanelAdcScanTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,46> points{{{0x152e,2},{0x1530,6},{0x1536,2},{0x1538,2},{0x153a,4},{0x153e,2},{0x1540,4},{0x1544,2},{0x1546,4},{0x154a,2},{0x154c,2},{0x154e,2},{0x1550,2},{0x1552,2},{0x1554,2},{0x1556,6},{0x155c,2},{0x155e,2},{0x1560,2},{0x1562,2},{0x1564,2},{0x1566,4},{0x156a,4},{0x156e,2},{0x1570,6},{0x1576,2},{0x1624,2},{0x1626,2},{0x1628,6},{0x162e,2},{0x1630,2},{0x1632,2},{0x1634,6},{0x163a,2},{0x163c,2},{0x163e,2},{0x1640,2},{0x1642,2},{0x1644,2},{0x1646,6},{0x164c,2},{0x164e,2},{0x1650,2},{0x1652,2},{0x1654,6},{0x165a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned address,int previous){address&=0xffffff;const bool fresh=(previous&1)||address!=(unsigned(previous)|1u);return address<0x400000?(fresh?3:0):address>=0xfffd10&&address<0xffffa0?(fresh?1:0):address>=0xffffa0?1:3;}
}
void setPanelAdcScanPartialForTests(PanelAdcScanPartialTest value){partialTest=value;}
bool PanelAdcScanCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x152e||pc==0x1624||pc==0x1646||(pc==0x156e&&!slot)){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native ADC scan capacity exceeded");
        slot->emplace(pc==0x152e?T::Kind::Initialize:pc==0x156e?T::Kind::ResetLast:pc==0x1624?T::Kind::Start:T::Kind::Mux,h.getSP());++started;
    }
    if(!slot||point((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
    const unsigned countWrite=byteClocks(0x156e3,h.lastwrite);
    const auto cost=task.nextClocks(countWrite);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
    auto read=[&](T::Read)->uint8_t{return uint8_t(h.read8(0x156e2));};
    auto write=[&](T::Write field,uint8_t value){f.store8(field==T::Write::Group?0x156e2:field==T::Write::Last?0x156e4:field==T::Write::Status?0xffff98:field==T::Write::Mux?0xffff6e:0x156e3,int8_t(value));};
    if(task.advanceBefore(cost,read,write,countWrite)!=cost)throw std::runtime_error("Native ADC scan admission differs");
    switch(stage){
    case S::SetInitialGroup:f.move(r[0].rl,0x24);break;
    case S::SetPasses:f.move(r[0].rl,0x10);break;
    case S::SavePass:f.push(r[0].r);break;
    case S::StartPass:f.call(0x1624);break;
    case S::SaveDelayBase:f.push(r[0].r);break;
    case S::SetDelayPasses:f.move(r[0].r,0x1);break;
    case S::SaveDelayPasses:f.push(r[0].r);break;
    case S::SetDelay:f.move(r[0].r,0xa67);break;
    case S::DecrementDelay:f.inc(r[0].r,-1);break;
    case S::RestoreDelayPasses:f.pop(r[0].r);break;
    case S::DecrementDelayPasses:f.inc(r[0].r,-1);break;
    case S::RestoreDelayBase:f.pop(r[0].r);break;
    case S::ReadCompletedGroup:f.move(r[0].rl,task.group());break;
    case S::CompareCompletedGroup:h.sub8(r[0].rl,0x20);break;
    case S::RestorePass:f.pop(r[0].r);break;
    case S::DecrementPass:f.inc(r[0].rl,-1);break;
    case S::Snapshot:f.call(0x158c);break;
    case S::ResetControls:f.call(0x1578);break;
    case S::SetLast:f.move(r[0].rl,0xffffffff);break;
    case S::ReturnInitial:f.ret();break;
    case S::SaveStartBase:f.push(r[0].r);break;
    case S::SaveStartAux:f.push(r[1].r);break;
    case S::ReadStartGroup:f.move(r[0].rl,task.group());break;
    case S::CompareStartGroup:h.sub8(r[0].rl,0x20);break;
    case S::ClearStartGroup:f.move(r[0].rl,0x0);break;
    case S::SelectMux:f.call(0x1646);break;
    case S::SetConversion:f.move(r[0].rl,0x73);break;
    case S::RestoreStartAux:f.pop(r[1].r);break;
    case S::RestoreStartBase:f.pop(r[0].r);break;
    case S::ReturnStart:f.ret();break;
    case S::ReadMuxGroup:f.move(r[0].rl,task.group());break;
    case S::HalfMux:f.shift(r[0].rl,1,false);break;
    case S::QuarterMux:f.shift(r[0].rl,1,false);break;
    case S::SetSettling:f.move(r[0].rh,0x9);break;
    case S::ReturnMux:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native ADC scan clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
