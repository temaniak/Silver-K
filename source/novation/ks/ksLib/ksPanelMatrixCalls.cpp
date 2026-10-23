#include "ksPanelMatrixCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
PanelMatrixPartialTest partialTest=nullptr;using T=PanelMatrixTask;using S=T::Stage;
struct Point{unsigned pc,length;};
constexpr std::array<Point,56> points{{
    {0x13f8,2},
    {0x13fa,2},
    {0x13fc,2},
    {0x1456,4},
    {0x145a,4},
    {0x145e,4},
    {0x1462,4},
    {0x1466,4},
    {0x146a,4},
    {0x146e,2},
    {0x1470,4},
    {0x1474,6},
    {0x147a,2},
    {0x147c,2},
    {0x147e,2},
    {0x1480,2},
    {0x1482,2},
    {0x1484,4},
    {0x1488,2},
    {0x148a,2},
    {0x148c,2},
    {0x148e,2},
    {0x1490,2},
    {0x1492,2},
    {0x1494,2},
    {0x1496,2},
    {0x1498,4},
    {0x149c,2},
    {0x149e,2},
    {0x14a0,2},
    {0x14a2,2},
    {0x14a4,2},
    {0x14a6,2},
    {0x14a8,2},
    {0x14aa,2},
    {0x14ac,2},
    {0x14ae,2},
    {0x14b0,2},
    {0x14b2,2},
    {0x14b4,2},
    {0x14b6,2},
    {0x14b8,2},
    {0x14ba,2},
    {0x14bc,2},
    {0x14be,2},
    {0x14c0,2},
    {0x14c2,2},
    {0x14c4,2},
    {0x14c6,4},
    {0x14ca,4},
    {0x14ce,4},
    {0x14d2,4},
    {0x14d6,4},
    {0x14da,4},
    {0x14de,4},
    {0x14e2,2}
}};
Point point(const T& t){return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];}
unsigned byteClocks(unsigned address,int previous){address&=0xffffff;const bool fresh=(previous&1)||address!=(unsigned(previous)|1u);return address<0x400000?(fresh?3:0):address>=0xfffd10&&address<0xffffa0?(fresh?1:0):address>=0xffffa0?1:3;}
}
void setPanelMatrixPartialForTests(PanelMatrixPartialTest value){partialTest=value;}
bool PanelMatrixCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x13f8||pc==0x1456){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native panel matrix capacity exceeded");
        slot->emplace(pc==0x13f8?T::Kind::Initialize:T::Kind::Scan,h.getSP());++started;
    }
    if(!slot||point((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
    const unsigned address=stage==S::WriteHistory?0x15400+task.row():stage==S::WriteMux||stage==S::WriteRestoredMux?0xffff62:0xfffeb1;
    const auto storeClocks=byteClocks(address,h.lastwrite),cost=task.nextClocks(storeClocks);const auto before=h.cycles;
    RuntimeBoundary f(h,pc,point(task).length);
    auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(field==T::Read::Previous?0x15400+task.row():0xffff51));};
    auto write=[&](T::Write,uint8_t value){f.store8(address,int8_t(value));};
    if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native panel matrix admission differs");
    switch(stage){
    case S::ScanInitial:f.call(0x1456);break;
    case S::ResetQueue:f.call(0x13fe);break;
    case S::ReturnInitial:f.ret();break;
    case S::SaveScratch:f.push(r[0].er);break;
    case S::SaveMux:f.push(r[2].er);break;
    case S::SaveBits:f.push(r[3].er);break;
    case S::SaveIndex:f.push(r[4].er);break;
    case S::SaveTarget:f.push(r[5].er);break;
    case S::SaveEvent:f.push(r[6].er);break;
    case S::DisableGate:f.move(r[0].rl,0x0);break;
    case S::SelectHistory:f.move(r[0].er,0x15400);break;
    case S::ClearKey:f.move(r[6].rl,0x0);break;
    case S::SelectMux:f.move(r[2].rl,0x20);break;
    case S::SetRows:f.move(r[2].rh,0x6);break;
    case S::SaveDelay:f.push(r[0].r);break;
    case S::SetDelay:f.move(r[0].r,0xc);break;
    case S::DecrementDelay:f.inc(r[0].r,-1);break;
    case S::RestoreDelay:f.pop(r[0].r);break;
    case S::ReadPrevious:f.move(r[3].rl,task.previous());break;
    case S::ReadSample:f.move(r[3].rh,task.sample());break;
    case S::CompareRow:r[3].rl^=r[3].rh;value(h,r[3].rl);break;
    case S::SetBits:f.move(r[3].e,0x8);break;
    case S::ShiftBits:f.shift(r[3].r,1,false);break;
    case S::SetReleased:f.move(r[6].rh,0x0);break;
    case S::TestPressed:flag(h,4,(uint8_t(r[3].rl)&(1u<<0x7))==0);break;
    case S::SetPressed:f.move(r[6].rh,0x1);break;
    case S::RetainEvent:f.move(r[6].e,r[6].r);break;
    case S::PublishEvent:f.call(0x14e4);break;
    case S::RestoreEvent:f.move(r[6].r,r[6].e);break;
    case S::AdvanceKey:f.inc(r[6].rl,1);break;
    case S::DecrementBits:f.inc(r[3].e,-1);break;
    case S::AdvanceUnchanged:r[6].rl=h.add8(r[6].rl,0x8);break;
    case S::AdvanceMux:f.inc(r[2].rl,1);break;
    case S::AdvanceHistory:f.inc(r[0].er,1);break;
    case S::DecrementRows:f.inc(r[2].rh,-1);break;
    case S::RestoreMuxValue:f.move(r[0].rl,0x30);break;
    case S::EnableGate:f.move(r[0].rl,0xffffffff);break;
    case S::RestoreSavedEvent:f.pop(r[6].er);break;
    case S::RestoreTarget:f.pop(r[5].er);break;
    case S::RestoreIndex:f.pop(r[4].er);break;
    case S::RestoreBits:f.pop(r[3].er);break;
    case S::RestoreMux:f.pop(r[2].er);break;
    case S::RestoreScratch:f.pop(r[0].er);break;
    case S::ReturnScan:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native panel matrix clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
