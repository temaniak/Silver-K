#include "ksPanelInputCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>

namespace ks {
namespace {
PanelInputPartialTest partialTest=nullptr;
using T=PanelInputTask;using S=T::Stage;using K=T::Kind;
struct Point{unsigned pc,length;};
Point point(const T& t){
    const bool c=t.controls();const unsigned pop=t.kind()==K::ControlPeek?0x21e2:c?0x15da:0x1412;
    switch(t.awaitingControl()?S::RestorePopAux:t.stage()){
    case S::SaveReset:return{c?0x1578u:0x13feu,2};
    case S::ZeroReset:return{c?0x157au:0x1400u,2};
    case S::ResetProducer:return{c?0x157cu:0x1402u,6};
    case S::ResetConsumer:return{c?0x1582u:0x1408u,6};
    case S::RestoreReset:return{c?0x1588u:0x140eu,2};
    case S::ReturnReset:return{c?0x158au:0x1410u,2};
    case S::SavePop:return{pop,4};
    case S::ReadConsumer:return{pop+4,6};
    case S::ReadProducer:return{pop+10,6};
    case S::CompareIndices:return{pop+16,2};
    case S::SelectEmpty:return{pop+18,2};
    case S::EmptyResult:return{pop+20,c?2u:4u};
    case S::RestoreEmpty:return{pop+(c?22u:24u),4};
    case S::ReturnEmpty:return{pop+(c?26u:28u),2};
    case S::SavePopAux:return{pop+(c?28u:30u),4};
    case S::SelectPopBuffer:return{pop+(c?32u:34u),6};
    case S::NormalizePop:return{pop+(c?38u:40u),2};
    case S::LocatePop:return{pop+(c?40u:42u),2};
    case S::AdvanceConsumer:return{pop+(c?42u:44u),2};
    case S::WrapConsumer:return{pop+(c?44u:46u),4};
    case S::PublishConsumer:return{pop+(c?48u:50u),6};
    case S::ReadEvent:return{pop+(c?54u:56u),2};
    case S::SetAvailable:return{pop+56,4};
    case S::ConvertControl:return{pop+60,4};
    case S::RestorePopAux:return{pop+(t.kind()==K::ControlPop?64u:c?60u:58u),4};
    case S::RestorePop:return{pop+(t.kind()==K::ControlPop?68u:c?64u:62u),4};
    case S::ReturnPop:return{pop+(t.kind()==K::ControlPop?72u:c?68u:66u),2};
    case S::SavePushAux:return{0x1848,4};case S::SavePush:return{0x184c,4};
    case S::ZeroKey:return{c?0x1850u:0x14e4u,2};case S::CaptureKey:return{c?0x1852u:0x14e6u,2};
    case S::ReadMapping:return{c?0x1854u:0x14e8u,8};case S::SelectMapping:return{0x185c,2};
    case S::ReadPushProducer:return{c?0x185eu:0x14f0u,6};case S::NormalizePush:return{c?0x1864u:0x14f6u,2};
    case S::SelectPushBuffer:return{c?0x1866u:0x14f8u,6};case S::LocatePush:return{c?0x186cu:0x14feu,2};
    case S::WriteEvent:return{c?0x186eu:0x1500u,2};case S::AdvanceProducer:return{c?0x1870u:0x1502u,2};
    case S::WrapProducer:return{c?0x1872u:0x1504u,4};case S::PublishProducer:return{c?0x1876u:0x1508u,6};
    case S::RestorePush:return{0x187c,4};case S::RestorePushAux:return{0x1880,4};
    case S::ReturnPush:return{c?0x1884u:0x150eu,2};
    default:throw std::runtime_error("Invalid native panel input stage");
    }
}
}
void setPanelInputPartialForTests(PanelInputPartialTest value){partialTest=value;}
bool PanelInputCalls::step(ControllerCpu& h){
    using namespace controllerDetail;
    const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;
    for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
    std::optional<K> kind;
    switch(pc){case 0x13fe:kind=K::ButtonReset;break;case 0x1412:kind=K::ButtonPop;break;case 0x14e4:kind=K::ButtonPush;break;
    case 0x1578:kind=K::ControlReset;break;case 0x15da:kind=K::ControlPop;break;case 0x1848:kind=K::ControlPush;break;case 0x21e2:kind=K::ControlPeek;break;default:break;}
    if(kind){
        if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}
        if(!slot)throw std::runtime_error("Native panel input capacity exceeded");
        slot->emplace(*kind,h.getSP(),uint16_t(*kind==K::ControlPush?h.regs[1].r:h.regs[6].r));++started;
    }
    if(!slot||point((*slot)->task).pc!=pc){
        if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;
    }
    auto& call=**slot;auto& task=call.task;auto& r=h.regs;
    if(task.awaitingControl())task.resumeControl();
    const auto stage=task.stage();const auto p=point(task);
    const unsigned eventClocks=task.eventIndex()&1?6:3;const auto cost=task.nextClocks(eventClocks);
    const auto before=h.cycles;RuntimeBoundary f(h,pc,p.length);
    const unsigned producer=task.controls()?0x15406:0x1537c,consumer=producer+2,buffer=producer+4;
    auto read=[&](T::Read field)->uint16_t{
        switch(field){case T::Read::Producer:return uint16_t(h.read16(producer));case T::Read::Consumer:return uint16_t(h.read16(consumer));
        case T::Read::Mapping:return uint8_t(h.read8((task.controls()?0xb918:0xb8e8)+task.key()));
        case T::Read::Event:return uint16_t(h.read16(buffer+task.eventIndex()));}return 0;
    };
    auto write=[&](T::Write field,uint16_t value){f.store16(field==T::Write::Producer?producer:field==T::Write::Consumer?consumer:buffer+task.eventIndex(),int16_t(value));};
    if(task.advanceBefore(cost,read,write,eventClocks)!=cost)throw std::runtime_error("Native panel input admission differs");
    const bool c=task.controls();
    switch(stage){
    case S::SaveReset:f.push(r[0].r);break;case S::ZeroReset:r[0].r=h.sub16(r[0].r,r[0].r);break;
    case S::RestoreReset:f.pop(r[0].r);break;
    case S::SavePop:f.push(r[0].er);break;case S::ReadConsumer:f.move(r[0].r,task.consumer());break;
    case S::ReadProducer:f.move(r[0].e,task.producer());break;case S::CompareIndices:h.sub16(r[0].r,r[0].e);break;
    case S::EmptyResult:if(c)r[6].er=h.sub32(r[6].er,r[6].er);else f.move(r[6].r,0xff00);break;
    case S::RestoreEmpty:case S::RestorePop:f.pop(r[0].er);break;
    case S::SavePopAux:f.push(r[1].er);break;case S::SelectPopBuffer:f.move(r[1].er,buffer);break;
    case S::NormalizePop:r[0].er&=0xffff;value(h,r[0].er);break;case S::LocatePop:r[1].er=h.add32(r[1].er,r[0].er);break;
    case S::AdvanceConsumer:f.inc(r[0].r,2);break;case S::WrapConsumer:r[0].r&=task.mask();value(h,r[0].r);break;
    case S::ReadEvent:f.move(r[6].r,task.event());break;case S::SetAvailable:f.move(r[6].e,1);break;
    case S::ConvertControl:f.call(0x20fc);break;case S::RestorePopAux:f.pop(r[1].er);break;
    case S::SavePushAux:f.push(r[4].er);break;case S::SavePush:f.push(r[5].er);break;
    case S::ZeroKey:if(c)r[4].er=h.sub32(r[4].er,r[4].er);else r[5].er=h.sub32(r[5].er,r[5].er);break;
    case S::CaptureKey:if(c)f.move(r[4].rl,task.key());else f.move(r[5].rl,task.key());break;
    case S::ReadMapping:if(c)f.move(r[1].rl,task.mapping());else f.move(r[6].rl,task.mapping());break;
    case S::ReadPushProducer:f.move(r[4].r,task.producer());break;
    case S::NormalizePush:r[4].er&=0xffff;value(h,r[4].er);break;
    case S::SelectPushBuffer:f.move(r[5].er,buffer);break;case S::LocatePush:r[5].er=h.add32(r[5].er,r[4].er);break;
    case S::AdvanceProducer:f.inc(r[4].r,2);break;case S::WrapProducer:r[4].r&=task.mask();value(h,r[4].r);break;
    case S::RestorePush:f.pop(r[5].er);break;case S::RestorePushAux:f.pop(r[4].er);break;
    case S::ReturnReset:case S::ReturnEmpty:case S::ReturnPop:case S::ReturnPush:f.ret();break;
    default:break;
    }
    if(h.cycles-before!=cost)throw std::runtime_error("Native panel input clocks differ");
    if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
    if(!task.awaitingControl())call.token=h.getSP();
    h.pc=h.makepc(task.awaitingControl()?f.next:point(task).pc);return true;
}
}
