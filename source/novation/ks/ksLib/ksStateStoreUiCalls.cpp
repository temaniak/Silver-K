#include "ksStateStoreUiCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
StateStoreUiPartialTest partialTest=nullptr;using T=StateStoreUiTask;using S=T::Stage;
struct Point{unsigned pc,length;};
constexpr std::array<Point,40> points{{
    {0x47b8,4},{0x47bc,4},{0x47c0,6},{0x47c6,4},{0x47ca,2},{0x47cc,6},{0x47d2,4},
    {0x47d6,4},{0x47da,8},{0x47e2,6},{0x47e8,8},{0x47f0,4},{0x47f4,2},{0x47f6,6},
    {0x47fc,4},{0x4800,6},{0x4806,6},{0x480c,4},{0x4810,4},{0x4814,2},{0x4816,6},
    {0x481c,4},{0x4820,6},{0x4826,4},{0x482a,4},{0x482e,2},{0x4830,2},{0x4832,2},
    {0x4834,6},{0x483a,2},{0x483c,6},{0x4842,4},{0x4846,4},{0x484a,2},{0x484c,6},
    {0x4852,4},{0x4856,2},{0x4858,2},{0x485a,6},{0x4860,2}
}};
Point point(const T& t){
    if(t.awaitingTail())return{t.tail()==T::Tail::ResetPanel?0x48ccu:t.tail()==T::Tail::Display?0x55b8u:0x48beu,0};
    return points[unsigned(t.awaitingChild()?t.resumeStage():t.stage())];
}
}
void setStateStoreUiPartialForTests(StateStoreUiPartialTest value){partialTest=value;}
void StateStoreUiCalls::retireReturns(ControllerCpu& h){
    for(auto& c:calls_)if(c&&c->token==h.getSP()&&c->task.awaitingTail()&&point(c->task).pc==unsigned(h.getPC())){c->task.retireTail();c.reset();++completed;}
}
bool StateStoreUiCalls::step(ControllerCpu& h){
    using namespace controllerDetail;retireReturns(h);const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    std::optional<T::Kind> kind;
    switch(pc){case 0x47b8:kind=T::Kind::CommitMulti;break;case 0x47d6:kind=T::Kind::CommitSingle;break;
    case 0x482a:kind=T::Kind::BeginProgram;break;case 0x484a:kind=T::Kind::BeginName;break;case 0x4858:kind=T::Kind::BeginStore;break;default:break;}
    if(kind){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native store UI capacity exceeded");slot->emplace(*kind,h.getSP());++started;
    }
    if(!slot||point((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingChild())task.resumeChild(uint8_t(r[0].rl));const auto stage=task.stage();
    const unsigned address=(stage==S::WriteCategory?task.current()+176:stage==S::WriteMultiDirty||stage==S::WriteProgramDirty?0x16799:0x16790)&0xffffff;
    const unsigned storeClocks=address<0x400000&&!(h.lastwrite&1)&&address==(unsigned(h.lastwrite)|1u)?0:3;
    const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
    auto read=[&](T::Read field)->uint32_t{
        if(field==T::Read::Current)return uint32_t(h.read32(0x167bc));
        return uint8_t(h.read8(field==T::Read::Category?0x167da:0x167ba));
    };
    auto write=[&](T::Write,uint8_t value){f.store8(address,int8_t(value));};
    if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native store UI admission differs");
    switch(stage){
    case S::SerializeMulti:f.call(0x6e8a);break;case S::SaveMulti:f.call(0x71a6);break;
    case S::SelectMultiMessage:f.move(r[6].er,0x14e93);break;case S::DisplayMulti:f.call(0x55b8);break;
    case S::ClearMultiPage:case S::ClearSinglePage:case S::ClearMultiDirty:case S::ClearProgramDirty:f.move(r[0].rl,0);break;
    case S::FinishMulti:case S::SelectReload:case S::FinishSingle:case S::FinishProgram:h.clockI(2);break;
    case S::PrepareSingle:f.call(0x6e7a);break;case S::ReadCurrent:f.move(r[1].er,task.current());break;
    case S::ReadCategory:f.move(r[0].rl,task.category());break;case S::SaveSingle:f.call(0x6494);break;
    case S::ResetSinglePanel:f.call(0x48cc);break;case S::SelectSingleMessage:f.move(r[6].er,0x14e21);break;
    case S::ReadMode:f.move(r[0].rl,task.mode());break;case S::ReloadMulti:f.call(0x6ff4);break;
    case S::RefreshMultiName:case S::RefreshProgramName:f.call(0x559e);break;
    case S::SelectReloadMessage:f.move(r[6].er,0x14e46);break;case S::CheckProtection:f.call(0x66bc);break;
    case S::ReturnProtected:f.ret();break;case S::SetProgramPage:f.move(r[0].rl,1);break;
    case S::SetNamePage:f.move(r[0].rl,2);break;case S::BeginNameEditor:f.call(0x756a);break;
    case S::SetStorePage:f.move(r[0].rl,3);break;default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native store UI clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
