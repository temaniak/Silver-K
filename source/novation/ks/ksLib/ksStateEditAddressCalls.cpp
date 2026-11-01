#include "ksStateEditAddressCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
StateEditAddressPartialTest partialTest=nullptr;
using T=StateEditAddressTask;using S=T::Stage;
struct Point{unsigned pc,length;};
constexpr std::array<Point,25> points{{
    {0x52ac,2},{0x52ae,8},{0x52b6,2},{0x52b8,2},{0x52ba,8},
    {0x52c2,2},{0x52c4,2},{0x52c6,2},{0x52c8,2},{0x52ca,2},
    {0x52cc,8},{0x52d4,2},{0x52d6,2},{0x52d8,8},{0x52e0,2},
    {0x52e2,2},{0x52e6,2},{0x52e8,8},{0x52f0,8},{0x52f8,2},
    {0x52fa,2},{0x52fc,2},{0x52fe,8},{0x5306,6},{0x530c,2}
}};
}
void setStateEditAddressPartialForTests(StateEditAddressPartialTest value){partialTest=value;}
bool StateEditAddressCalls::step(ControllerCpu& h){
    using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    if(pc==0x52ac||pc==0x52e6||pc==0x52fc){
        // Indexed descriptors can select the direct branch in the same call.
        const bool continuation=slot&&(*slot)->task.stage()==S::ZeroDirect&&pc==0x52e6;
        if(!continuation){
            if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
            if(!slot)throw std::runtime_error("Native edit address capacity exceeded");
            slot->emplace(pc==0x52ac?T::Kind::Indexed:pc==0x52e6?T::Kind::Direct:T::Kind::Global,h.getSP(),uint32_t(h.regs[0].er));++started;
        }
    }
    if(!slot||points[unsigned((*slot)->task.stage())].pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& task=(*slot)->task;auto& r=h.regs;const auto stage=task.stage();
    const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,points[unsigned(stage)].length);
    auto read=[&](T::Read field)->uint32_t{
        switch(field){
        case T::Read::Selector:return uint8_t(h.read8(task.descriptor()+2));
        case T::Read::IndexBase:case T::Read::TargetBase:case T::Read::DirectBase:return uint32_t(h.read32(0x167bc));
        case T::Read::Index:return uint8_t(h.read8(task.selectorAddress()));
        case T::Read::MappedOffset:return uint8_t(h.read8(task.mappingAddress()));
        case T::Read::DirectOffset:case T::Read::GlobalOffset:return uint8_t(h.read8(task.descriptor()+3));
        }return 0;
    };
    if(task.advanceBefore(cost,read)!=cost)throw std::runtime_error("Native edit address admission differs");
    switch(stage){
    case S::ZeroSelector:case S::ZeroDirect:case S::ZeroGlobal:r[1].er=h.sub32(r[1].er,r[1].er);break;
    case S::ReadSelector:f.move(r[1].rl,task.selector());break;
    case S::CompareSentinel:h.sub8(r[1].rl,0xff);break;
    case S::ReadIndexBase:case S::ReadDirectBase:case S::ReadTargetBase:f.move(r[2].er,task.base());break;
    case S::LocateSelector:r[1].er=h.add32(r[1].er,r[2].er);break;
    case S::ReadIndex:f.move(r[2].rl,task.index());break;
    case S::MaskIndex:r[2].r&=0xff;value(h,r[2].r);break;
    case S::NormalizeIndex:r[2].er&=0xffff;value(h,r[2].er);break;
    case S::LocateMapping:r[2].er=h.add32(r[2].er,r[0].er);break;
    case S::ReadMappedOffset:case S::ReadDirectOffset:case S::ReadGlobalOffset:f.move(r[1].rl,task.offset());break;
    case S::MaskOffset:r[1].r&=0xff;value(h,r[1].r);break;
    case S::NormalizeOffset:r[1].er&=0xffff;value(h,r[1].er);break;
    case S::LocateTarget:case S::LocateDirect:r[1].er=h.add32(r[1].er,r[2].er);break;
    case S::LocateGlobal:r[1].er=h.add32(r[1].er,0x168fc);break;
    case S::ReturnIndexed:case S::ReturnDirect:case S::ReturnGlobal:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native edit address clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    h.pc=h.makepc(points[unsigned(task.stage())].pc);return true;
}
}
