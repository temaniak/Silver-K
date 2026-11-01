#include "ksKeySearchCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
KeySearchPartialTest partialTest=nullptr;
using Task=nativeControl::KeySearchTask;
// Source locations below describe only the old caller projection and bus trace.
// They never select the native result, next observed byte or completion time.
enum class Projection {CopyKey,CopyBit,MaskBit,MaskGroup,CopyIndex,Index4,Index2,Index1,IndexWord,IndexLong,IndexAddress,
    Read,ByteDecision,AdjustBit,CompareEnd,EndDecision,ObserveBit,BitDecision,Select,SelectedDecision,MovePointer,ResetBit,MoveGroup,GroupDecision,Return};
struct Point {unsigned location;Projection projection;int bit=0;};
Point point(Task::Cursor c){
    using P=Projection;
    if(c.phase==Task::Phase::Complete)return {c.forward?0x31aau:0x31dau,P::Return};
    if(c.phase==Task::Phase::Admission){
        static constexpr Projection order[]{P::CopyKey,P::CopyBit,P::MaskBit,P::MaskGroup,P::CopyIndex,P::Index4,P::Index2,P::Index1,P::IndexWord,P::IndexLong,P::IndexAddress};
        return {(c.forward?0x317au:0x31acu)+2*c.offset,order[c.offset]};
    }
    if(c.phase==Task::Phase::ReadByte)return {c.forward?0x3190u:0x31c2u,P::Read};
    if(!c.offset)return {c.forward?0x3192u:0x31c4u,P::ByteDecision};
    unsigned at=c.offset-1;
    const bool found=c.candidate>=int(c.group*8) && c.candidate<int(c.group*8+8);
    if(c.observedByte){
        const unsigned attempts=c.forward?unsigned((found?c.candidate%8:7)-c.afterBit):unsigned(c.afterBit-(found?c.candidate%8:0));
        const unsigned stride=c.forward?5u:4u;
        if(at<attempts*stride){
            const int bit=c.afterBit+(c.forward?1:-1)*int(at/stride+1);const unsigned part=at%stride;
            if(c.forward){
                static constexpr Projection ops[]{P::AdjustBit,P::CompareEnd,P::EndDecision,P::ObserveBit,P::BitDecision};
                return {0x3194+2*part,ops[part],bit};
            }
            static constexpr Projection ops[]{P::AdjustBit,P::EndDecision,P::ObserveBit,P::BitDecision};
            return {0x31c6+2*part,ops[part],bit};
        }
        at-=attempts*stride;
        if(found)return {unsigned((c.forward?0x319e:0x31ce)+2*at),at?P::SelectedDecision:P::Select,c.candidate%8};
        const unsigned end=c.forward?3u:2u;
        if(at<end){
            if(c.forward){static constexpr Projection ops[]{P::AdjustBit,P::CompareEnd,P::EndDecision};return {0x3194+2*at,ops[at],8};}
            return {0x31c6+2*at,at?P::EndDecision:P::AdjustBit,-1};
        }
        at-=end;
    }
    static constexpr Projection skip[]{P::MovePointer,P::ResetBit,P::MoveGroup,P::GroupDecision};
    return {unsigned((c.forward?0x31a2:0x31d2)+2*at),skip[at]};
}
}
void setKeySearchPartialForTests(KeySearchPartialTest fn){partialTest=fn;}
bool KeySearchCalls::step(ControllerCpu& h){
    using namespace controllerDetail;using P=Projection;
    const unsigned pc=unsigned(h.getPC());
    std::optional<Call>* selected=nullptr;
    for(auto& c:calls)if(c && c->callerToken==h.getSP()){selected=&c;break;}
    if(pc==0x317a || pc==0x31ac){
        if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
        if(!selected)throw std::runtime_error("Native KS key-call capacity exceeded");
        selected->emplace(pc==0x317a,uint8_t(h.regs[5].rl),uint32_t(h.regs[3].er),h.getSP());++started;
    }
    if(!selected || point((*selected)->task.cursor()).location!=pc){
        // Only comparison tests can reconstruct pre-existing arbitrary partial
        // H8 frames; a normal native call always owns its typed task from entry.
        if(selected)selected->reset();
        if(partialTest && partialTest(h)){++partialEntries;return true;}
        return false;
    }
    auto& call=**selected;const auto cursor=call.task.cursor();const auto p=point(cursor);
    Boundary clocks(h,p.location,2);auto& r=h.regs;
    if(p.projection==P::Return){
        --h.indent;h.clockI(2);h.pc=h.popPC();selected->reset();++completed;return true;
    }
    const auto cost=call.task.nextBoundaryClocks();
    const unsigned elapsed=call.task.advanceBefore(cost,[&](unsigned index){
        const auto byte=uint8_t(h.read8(call.storage+index));r[2].rl=int8_t(byte);value(h,r[2].rl);return byte;
    });
    if(elapsed!=cost)throw std::runtime_error("Native KS key-call clock mismatch");
    // Retained callers and IRQ saves still observe this projection. It is
    // output-only: none of these operands/flags drives KeySearchTask.
    switch(p.projection){
    case P::CopyKey:r[4].rl=int8_t(cursor.group*8+unsigned(cursor.afterBit));value(h,r[4].rl);break;
    case P::CopyBit:r[4].rh=int8_t(cursor.group*8+unsigned(cursor.afterBit));value(h,r[4].rh);break;
    case P::MaskBit:r[4].rh=int8_t(cursor.afterBit);value(h,r[4].rh);break;
    case P::MaskGroup:r[4].rl=int8_t(cursor.group*8);value(h,r[4].rl);break;
    case P::CopyIndex:r[1].rl=int8_t(cursor.group*8);value(h,r[1].rl);break;
    case P::Index4:case P::Index2:case P::Index1:{
        const unsigned shift=p.projection==P::Index4?1:p.projection==P::Index2?2:3;
        const unsigned v=cursor.group*8;flag(h,1,v&(1u<<(shift-1)));r[1].rl=int8_t(v>>shift);value(h,r[1].rl);break;
    }
    case P::IndexWord:r[1].r=int16_t(cursor.group);value(h,r[1].r);break;
    case P::IndexLong:r[1].er=int32_t(cursor.group);value(h,r[1].er);break;
    case P::IndexAddress:r[1].er=h.add32(int32_t(cursor.group),int32_t(call.storage));break;
    case P::AdjustBit:{
        const int old=p.bit+(cursor.forward?-1:1),v=p.bit;
        r[4].rh=int8_t(v);value(h,v);
        flag(h,2,(cursor.forward?(~uint32_t(old)&uint32_t(v)):(uint32_t(old)&~uint32_t(v)))&128u);break;
    }
    case P::CompareEnd:h.sub8(p.bit,8);break;
    case P::ObserveBit:flag(h,4,!(cursor.observedByte&(1u<<unsigned(p.bit))));break;
    case P::Select:r[4].rl=int8_t(cursor.candidate);value(h,r[4].rl);break;
    case P::MovePointer:r[1].er=int32_t(call.storage+cursor.storageIndex+(cursor.forward?1u:unsigned(-1)));break;
    case P::ResetBit:r[4].rh=int8_t(cursor.forward?-1:8);value(h,r[4].rh);break;
    case P::MoveGroup:r[4].rl=cursor.forward?h.add8(int(cursor.group*8),8):h.sub8(int(cursor.group*8),8);break;
    default:break; // Semantic waiting/branch admission changes no caller value.
    }
    h.pc=point(call.task.cursor()).location;return true;
}
}
