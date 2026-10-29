#pragma once
#include "kstationParameterInputTask.h"
#include "kstationNativeMidiPump.h"
#include <array>
#include <stdexcept>
namespace kstation {
class NativeParameterInput final {
    using T=PackedControllerTask;using S=T::Stage;using I=ParameterInputTask;using P=I::Stage;
    struct Job{T packed;I input;bool converting=false;};
public:
    struct CodeRange{uint32_t begin,end;};
    inline static constexpr std::array<CodeRange,2>CodeRanges{{{0x575a,0x5778},{0x57b8,0x581e}}};
    static bool candidate(uint32_t at){return (at>=0x575a&&at<0x5778)||(at>=0x57b8&&at<0x581e);}
    void reset(){depth_=0;last_={ParameterInputResult::Ordinary,0,0};}bool active()const{return depth_!=0;}
    bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;const auto at=uint32_t(c.getPC());
        if(depth_){auto&j=jobs_[depth_-1];if(!j.converting&&j.input.awaitingConversion()&&at==0x575c)j.input.resumeConversion(last_);}
        if(!depth_||boundary(jobs_[depth_-1])!=at){
            if(at!=0x575a&&at!=0x5768&&at!=0x576a&&at!=0x57b8)return false;
            if(depth_==jobs_.size())throw std::logic_error("Native K parameter input continuation capacity");
            Job j;j.converting=at==0x57b8;j.packed=T(uint8_t(c.reg8(14)),uint8_t(c.reg8(6)),uint32_t(c.reg32(1)));
            j.input=I(uint8_t(c.reg8(14)),at==0x575a?I::Entry::Controller:at==0x5768?I::Entry::BiasedController:I::Entry::Indexed,uint8_t(c.reg8(6)));jobs_[depth_++]=j;
        }
        auto&j=jobs_[depth_-1];if(boundary(j)!=at)return false;if(c.pending_irqs)c.read8(0xfffff2);
        const auto len=length(j);const auto begin=c.cycles;c.fetchClocks(len);c.pc=c.makepc(int(at+1));
        if(!j.converting){const auto p=j.input.stage();I::Credits cr;cr.call=3+bus(c.getSP()-4,4,c.writeHistory());cr.ret=5+bus(c.getSP(),4,int(at+len-1));
            cr.mapping=12+bus(0xbcc6+j.input.index(),1,int(at+len-1));const auto cost=j.input.nextClocks(cr);
            switch(p){case P::Convert:midi_projection::call(c,0x57b8,0x575c,false);break;
            case P::ReturnIgnored:case P::ReturnRange:midi_projection::ret(c);break;
            case P::CompareRange:midi_projection::arithmetic(c,uint8_t(c.reg8(14)),50,8,true);break;
            case P::Bias:c.reg8(14)=int8_t(midi_projection::arithmetic(c,uint8_t(c.reg8(14)),128,8,false));break;
            case P::ClearIndex:c.reg32(0)=int32_t(midi_projection::arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(0)),32,true));break;
            case P::CopyIndex:c.reg8(8)=c.reg8(14);midi_projection::val(c,c.reg8(8));break;
            case P::ClearOffset:c.reg32(1)=int32_t(midi_projection::arithmetic(c,uint32_t(c.reg32(1)),uint32_t(c.reg32(1)),32,true));break;
            default:break;}
            const auto used=j.input.advanceBefore(cost,cr,[&](unsigned index){c.reg8(9)=c.read8(int(0xbcc6+index));midi_projection::val(c,c.reg8(9));return uint8_t(c.reg8(9));});
            if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K parameter input clocks differ");
            if(j.input.complete()){if(j.input.updates()){const auto result=j.input.output();if(result.offset!=uint32_t(c.reg32(1))||result.value!=uint8_t(c.reg8(6)))throw std::logic_error("Native K parameter input semantic result differs");c.pc=c.makepc(0x5778);}--depth_;}else if(!j.input.awaitingConversion())c.pc=c.makepc(int(boundary(j)));return true;
        }
        auto&t=j.packed;const auto phase=t.stage();T::Credits cr;
        cr.descriptor=15+bus(0xbdb2+t.descriptorIndex(),4,int(at+len-1));
        cr.metadata=5+bus(t.cursor(),1,int(at+len-1));cr.destination=3+bus(t.cursor(),1,int(at+len-1));
        cr.packed=12+bus(0xba5c+t.offset(),1,int(at+len-1));cr.ret=5+bus(c.getSP(),4,int(at+len-1));
        const auto cost=t.nextClocks(cr);
        switch(phase){
        case S::CompareLower:midi_projection::arithmetic(c,uint8_t(c.reg8(14)),104,8,true);break;
        case S::CompareUpper:midi_projection::arithmetic(c,uint8_t(c.reg8(14)),107,8,true);break;
        case S::ClearOrdinary:case S::ClearResult:clear(c,8,8);break;
        case S::ClearIndex:clear(c,0,32);break;
        case S::CopyController:c.reg8(8)=c.reg8(14);midi_projection::val(c,c.reg8(8));break;
        case S::NormalizeController:c.reg8(8)=int8_t(midi_projection::arithmetic(c,uint8_t(c.reg8(8)),0x98,8,false));break;
        case S::ScaleFirst:case S::ScaleSecond:shift(c,8,true);break;
        case S::ClearValue:clear(c,2,32);break;
        case S::CopyValue:c.reg8(10)=c.reg8(6);midi_projection::val(c,c.reg8(10));break;
        case S::ShiftValue:shift(c,10,false);break;
        case S::DecrementShift:decrement(c,9);break;
        case S::CompareLimit:midi_projection::arithmetic(c,uint8_t(c.reg8(9)),uint8_t(c.reg8(10)),8,true);break;
        case S::BoundaryResult:c.reg8(8)=1;midi_projection::val(c,c.reg8(8));break;
        case S::ApplyMask:c.reg8(6)=int8_t(uint8_t(c.reg8(6))&uint8_t(c.reg8(14)));midi_projection::val(c,c.reg8(6));break;
        case S::InvertMask:c.reg8(14)=int8_t(~uint8_t(c.reg8(14)));midi_projection::val(c,c.reg8(14));break;
        case S::RoundSelector:c.reg8(10)=int8_t(uint8_t(c.reg8(10))&0xfe);midi_projection::val(c,c.reg8(10));break;
        case S::SelectTuple:c.reg32(0)=int32_t(midi_projection::arithmetic(c,uint32_t(c.reg32(0)),uint32_t(c.reg32(2)),32,false));break;
        case S::LeftShiftValue:shift(c,6,true);break;
        case S::RotateMask:{auto v=uint8_t(c.reg8(14));c.ccr=uint8_t((c.ccr&~1)|(v>>7));c.reg8(14)=int8_t(uint8_t((v<<1)|(v>>7)));midi_projection::val(c,c.reg8(14));break;}
        case S::DecrementLeftShift:decrement(c,10);break;
        case S::ClearOffset:clear(c,1,32);break;
        case S::Preserve:c.reg8(8)=int8_t(uint8_t(c.reg8(8))&uint8_t(c.reg8(14)));midi_projection::val(c,c.reg8(8));break;
        case S::Merge:c.reg8(6)=int8_t(uint8_t(c.reg8(6))|uint8_t(c.reg8(8)));midi_projection::val(c,c.reg8(6));break;
        case S::SetResult:{const int old=c.reg8(8),v=old+1;midi_projection::val(c,v);if((~old&v)&0x80)c.ccr|=2;c.reg8(8)=int8_t(v);break;}
        case S::ReturnOrdinary:case S::ReturnBoundary:case S::ReturnTransformed:midi_projection::ret(c);break;
        default:break;}
        const auto used=t.advanceBefore(cost,cr,[&](T::Read field)->uint32_t{
            if(field==T::Read::Descriptor){c.reg32(0)=int32_t(c.read32(int(0xbdb2+t.descriptorIndex())));midi_projection::val(c,c.reg32(0));return uint32_t(c.reg32(0));}
            if(field==T::Read::Packed){c.reg8(8)=c.read8(0xba5c+t.offset());midi_projection::val(c,c.reg8(8));return uint8_t(c.reg8(8));}
            const int r=field==T::Read::Mask?14:field==T::Read::Tag?1:field==T::Read::LeftShift?10:9;
            c.reg8(r)=c.read8(int(t.cursor()));if(field!=T::Read::Offset){c.reg32(0)=int32_t(uint32_t(c.reg32(0))+1);c.clockI(2);}midi_projection::val(c,c.reg8(r));return uint8_t(c.reg8(r));
        });
        if(used!=cost||c.cycles-begin!=cost)throw std::logic_error("Native K packed controller clocks differ");
        if(t.complete()){last_=t.output();if(last_.kind==ParameterInputResult::Transformed&&(last_.offset!=uint32_t(c.reg32(1))||last_.value!=uint8_t(c.reg8(6))))throw std::logic_error("Native K packed controller semantic result differs");--depth_;}else c.pc=c.makepc(int(boundary(j)));return true;
    }
private:
    static void clear(Stage1Cpu&c,unsigned r,unsigned bits){if(bits==32)c.reg32(r)=int32_t(midi_projection::arithmetic(c,uint32_t(c.reg32(r)),uint32_t(c.reg32(r)),32,true));else c.reg8(r)=int8_t(midi_projection::arithmetic(c,uint8_t(c.reg8(r)),uint8_t(c.reg8(r)),8,true));}
    static void decrement(Stage1Cpu&c,unsigned r){const int old=c.reg8(r),value=old-1;midi_projection::val(c,value);if((old&~value)&0x80)c.ccr|=2;c.reg8(r)=int8_t(value);}
    static void shift(Stage1Cpu&c,unsigned r,bool left){const auto old=uint8_t(c.reg8(r));c.ccr=uint8_t((c.ccr&~1)|(left?old>>7:old&1));c.reg8(r)=int8_t(left?uint8_t(old<<1):uint8_t(old>>1));midi_projection::val(c,c.reg8(r));}
    static uint32_t boundary(const Job&j){if(!j.converting){if(j.input.awaitingConversion()||j.input.complete())return 0;constexpr uint32_t sites[]{0x575a,0x575c,0x575e,0x5760,0x5762,0x5764,0x5766,0x5768,0x576a,0x576c,0x576e,0x5770};return sites[unsigned(j.input.stage())];}
        if(j.packed.complete())return 0;constexpr uint32_t sites[]{0x57b8,0x57ba,0x57bc,0x57be,0x57c0,0x57c2,0x57c4,0x57c6,0x57c8,0x57ca,0x57cc,0x57ce,0x57d8,0x57da,0x57dc,0x57de,0x57e0,0x57e2,0x57e4,0x57e6,0x57e8,0x57ea,0x57ec,0x57ee,0x57f0,0x57f2,0x57f4,0x57f6,0x57f8,0x57fa,0x57fc,0x57fe,0x5800,0x5802,0x5804,0x5806,0x5808,0x580a,0x580c,0x5814,0x5816,0x5818,0x581a,0x581c};return sites[unsigned(j.packed.stage())];
    }
    static unsigned length(const Job&j){if(!j.converting)return j.input.stage()==P::ReadMapping?8:2;
        const auto s=j.packed.stage();return s==S::ReadDescriptor?10:s==S::ReadPacked?8:2;}
    static unsigned bus(uint32_t at,unsigned n,int last){unsigned cost=0;for(unsigned i=0;i<n;++i){const auto a=(at+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((last&1)||a!=uint32_t(last|1))cost+=a<0x400000?3:1;last=int(a);}else{cost+=a>=0xffffa0?1:3;last=-1;}}return cost;}
    std::array<Job,4>jobs_{};unsigned depth_=0;ParameterInputValue last_{ParameterInputResult::Ordinary,0,0};
};
}
