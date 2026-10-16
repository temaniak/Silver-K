#include "ksMidiFileTimeCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks{namespace{MidiFileTimePartialTest partialTest=nullptr;using T=MidiFileTimeTask;using K=T::Kind;using S=T::Stage;struct Point{unsigned location,length;};Point point(const T&t){switch(t.stage()){case S::DelayReadDelta:return{0xb2e0,2};case S::DelaySetShift:return{0xb2e2,2};case S::DelayShift:return{0xb2e4,2};case S::DelayTickShift:return{0xb2e6,2};case S::DelayAdmitShift:return{0xb2e8,2};case S::DelayReadDeadline:return{0xb2ea,8};case S::DelayAddDelta:return{0xb2f2,2};case S::DelayPublishDeadline:return{0xb2f4,8};case S::DelayFinish:return{0xb2fc,2};case S::DueReadNow:return{0xb2fe,8};case S::DueReadDeadline:return{0xb306,8};case S::DueCompare:return{0xb30e,2};case S::DueFinish:return{0xb310,2};case S::VlqClear:return{0xb312,2};case S::VlqReadFirst:return{0xb314,2};case S::VlqAdmitFirst:return{0xb316,2};case S::VlqMaskFirst:return{0xb318,2};case S::VlqPositionFirst:return{0xb31a,2};case S::VlqClearLow:return{0xb31c,2};case S::VlqScaleFirst:return{0xb31e,2};case S::VlqClearSecond:return{0xb320,2};case S::VlqReadSecond:return{0xb322,2};case S::VlqAdmitSecond:return{0xb324,2};case S::VlqMaskSecond:return{0xb326,2};case S::VlqAddSecond:return{0xb328,2};case S::VlqMultiplier:return{0xb32a,4};case S::VlqScaleSecond:return{0xb32e,2};case S::VlqClearThird:return{0xb330,2};case S::VlqReadThird:return{0xb332,2};case S::VlqAdmitThird:return{0xb334,2};case S::VlqMaskThird:return{0xb336,2};case S::VlqAddThird:return{0xb338,2};case S::VlqSetShift:return{0xb33a,2};case S::VlqShift:return{0xb33c,2};case S::VlqTickShift:return{0xb33e,2};case S::VlqAdmitShift:return{0xb340,2};case S::VlqClearFourth:return{0xb342,2};case S::VlqReadFourth:return{0xb344,2};case S::VlqAddLast:return{0xb346,2};case S::VlqFinish:return{0xb348,2};case S::TempoShift0:return{0xb3c0,2};case S::TempoShift1:return{0xb3c2,2};case S::TempoShift2:return{0xb3c4,2};case S::TempoShift3:return{0xb3c6,2};case S::TempoReadDivision:return{0xb3c8,6};case S::TempoDivision:return{0xb3ce,4};case S::TempoUnit:return{0xb3d2,4};case S::TempoScaleDivision:return{0xb3d6,2};case S::TempoRate:return{0xb3d8,4};case S::TempoScaleRate:return{0xb3dc,2};case S::TempoDivide:return{0xb3de,2};case S::TempoTruncate:return{0xb3e0,2};case S::TempoPublish:return{0xb3e2,8};case S::TempoFinish:return{0xb3ea,2};case S::ArpReadDelta:return{0xb494,4};case S::ArpSetShift:return{0xb498,2};case S::ArpShift:return{0xb49a,2};case S::ArpTickShift:return{0xb49c,2};case S::ArpAdmitShift:return{0xb49e,2};case S::ArpReadDeadline:return{0xb4a0,10};case S::ArpAddDelta:return{0xb4aa,2};case S::ArpPublishDeadline:return{0xb4ac,10};case S::ArpFinish:return{0xb4b6,2};default:throw std::runtime_error("Invalid MIDI file time stage");}}}void setMidiFileTimePartialForTests(MidiFileTimePartialTest f){partialTest=f;}
bool MidiFileTimeCalls::step(ControllerCpu&h){using namespace controllerDetail;auto pc=unsigned(h.getPC());auto&v=h.regs;std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}std::optional<K>kind;switch(pc){case 0xb2e0:kind=K::Delay;break;case 0xb2fe:kind=K::Due;break;case 0xb312:kind=K::Vlq;break;case 0xb3c0:kind=K::Tempo;break;case 0xb494:kind=K::ArpDelay;break;default:break;}if(kind&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("MIDI file time capacity exceeded");selected->emplace(*kind,uint32_t(v[4].er),uint32_t(v[0].er),uint32_t(v[0].er),h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto&t=(**selected).task;if(t.awaitingChild())t.resumeVlq(uint32_t(v[5].er),uint32_t(v[4].er));auto stage=t.stage();auto begin=h.cycles;RuntimeBoundary clocks(h,pc,point(t).length);uint32_t readData=0;auto timing=[&](S stage){uint32_t at=0;unsigned width=0;bool store=false;switch(stage){case S::DelayReadDeadline:at=0x17322;width=4;store=false;break;case S::DueReadNow:at=0xffee82;width=4;store=false;break;case S::DueReadDeadline:at=0x17322;width=4;store=false;break;case S::VlqReadFirst:at=t.stream();width=1;store=false;break;case S::VlqReadSecond:at=t.stream();width=1;store=false;break;case S::VlqReadThird:at=t.stream();width=1;store=false;break;case S::VlqReadFourth:at=t.stream();width=1;store=false;break;case S::TempoReadDivision:at=0x17326;width=2;store=false;break;case S::ArpReadDeadline:at=t.timeRecord()+56;width=4;store=false;break;case S::DelayPublishDeadline:at=0x17322;width=4;store=true;break;case S::TempoPublish:at=0xffee86;width=4;store=true;break;case S::ArpPublishDeadline:at=t.timeRecord()+56;width=4;store=true;break;default:return 0u;}int last=store?h.lastwrite:h.lastread;unsigned cost=0;for(unsigned i=0;i<width;++i){auto addr=(at+i)&0xffffff;cost+=addr>=0x400000||(last&1)||addr!=(unsigned(last)|1u)?3u:0u;last=int(addr);}return cost;};auto cost=t.nextClocks(timing(stage));auto read=[&](S stage)->uint32_t{switch(stage){case S::DelayReadDeadline:return uint32_t(h.read32(0x17322));case S::DueReadNow:return uint32_t(h.read32(0xffee82));case S::DueReadDeadline:return uint32_t(h.read32(0x17322));case S::VlqReadFirst:return uint8_t(h.read8(t.stream()));case S::VlqReadSecond:return uint8_t(h.read8(t.stream()));case S::VlqReadThird:return uint8_t(h.read8(t.stream()));case S::VlqReadFourth:return uint8_t(h.read8(t.stream()));case S::TempoReadDivision:return uint16_t(h.read16(0x17326));case S::ArpReadDeadline:return uint32_t(h.read32(t.timeRecord()+56));default:throw std::runtime_error("Invalid MIDI file time read");}};auto capture=[&](S s){return readData=read(s);};auto write=[&](S stage,uint32_t data){switch(stage){case S::DelayPublishDeadline:h.write32(int32_t(data),0x17322);break;case S::TempoPublish:h.write32(int32_t(data),0xffee86);break;case S::ArpPublishDeadline:h.write32(int32_t(data),t.timeRecord()+56);break;default:throw std::runtime_error("Invalid MIDI file time write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("MIDI file time deadline mismatch");switch(stage){
case S::DelayReadDelta:{clocks.call(0xb312);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time call timing");return true;break;}
case S::DelaySetShift:{clocks.move(v[1].rl,0xc);break;}
case S::DelayShift:{clocks.shift(v[5].er,1,true);break;}
case S::DelayTickShift:{clocks.inc(v[1].rl,-1);break;}
case S::DelayAdmitShift:{;break;}
case S::DelayReadDeadline:{clocks.move(v[1].er,int32_t(readData));break;}
case S::DelayAddDelta:{v[1].er=h.add32(v[1].er,v[5].er);break;}
case S::DelayPublishDeadline:{value(h,v[1].er);break;}
case S::DelayFinish:{clocks.ret();if(stage==S::VlqFinish&&(t.value()!=uint32_t(v[5].er)||t.stream()!=uint32_t(v[4].er)))throw std::runtime_error("Typed VLQ result differs");if(stage==S::TempoFinish&&t.period()!=uint32_t(v[1].er))throw std::runtime_error("Typed tempo period differs");if(stage==S::DueFinish&&t.due()!=!(h.ccr&1))throw std::runtime_error("Typed unsigned due reply differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time return timing");selected->reset();++completed;return true;break;}
case S::DueReadNow:{clocks.move(v[0].er,int32_t(readData));break;}
case S::DueReadDeadline:{clocks.move(v[1].er,int32_t(readData));break;}
case S::DueCompare:{h.sub32(v[0].er,v[1].er);break;}
case S::DueFinish:{clocks.ret();if(stage==S::VlqFinish&&(t.value()!=uint32_t(v[5].er)||t.stream()!=uint32_t(v[4].er)))throw std::runtime_error("Typed VLQ result differs");if(stage==S::TempoFinish&&t.period()!=uint32_t(v[1].er))throw std::runtime_error("Typed tempo period differs");if(stage==S::DueFinish&&t.due()!=!(h.ccr&1))throw std::runtime_error("Typed unsigned due reply differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time return timing");selected->reset();++completed;return true;break;}
case S::VlqClear:{v[5].er=h.sub32(v[5].er,v[5].er);break;}
case S::VlqReadFirst:{clocks.move(v[5].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::VlqAdmitFirst:{;break;}
case S::VlqMaskFirst:{v[5].rl&=0x7f;value(h,v[5].rl);break;}
case S::VlqPositionFirst:{clocks.move(v[5].rh,v[5].rl);break;}
case S::VlqClearLow:{clocks.move(v[5].rl,0x0);break;}
case S::VlqScaleFirst:{clocks.shift(v[5].r,1,false);break;}
case S::VlqClearSecond:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::VlqReadSecond:{clocks.move(v[1].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::VlqAdmitSecond:{;break;}
case S::VlqMaskSecond:{v[1].rl&=0x7f;value(h,v[1].rl);break;}
case S::VlqAddSecond:{v[5].r=h.add16(v[5].r,v[1].r);break;}
case S::VlqMultiplier:{clocks.move(v[5].e,0x80);break;}
case S::VlqScaleSecond:{v[5].er=uint16_t(v[5].er)*uint32_t(uint16_t(v[5].e));h.clockI(20);break;}
case S::VlqClearThird:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::VlqReadThird:{clocks.move(v[1].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::VlqAdmitThird:{;break;}
case S::VlqMaskThird:{v[1].rl&=0x7f;value(h,v[1].rl);break;}
case S::VlqAddThird:{v[5].er=h.add32(v[5].er,v[1].er);break;}
case S::VlqSetShift:{clocks.move(v[1].rl,0x7);break;}
case S::VlqShift:{clocks.shift(v[5].er,1,true);break;}
case S::VlqTickShift:{clocks.inc(v[1].rl,-1);break;}
case S::VlqAdmitShift:{;break;}
case S::VlqClearFourth:{v[1].er=h.sub32(v[1].er,v[1].er);break;}
case S::VlqReadFourth:{clocks.move(v[1].rl,int8_t(readData));v[4].er+=1;h.clockI(2);break;}
case S::VlqAddLast:{v[5].er=h.add32(v[5].er,v[1].er);break;}
case S::VlqFinish:{clocks.ret();if(stage==S::VlqFinish&&(t.value()!=uint32_t(v[5].er)||t.stream()!=uint32_t(v[4].er)))throw std::runtime_error("Typed VLQ result differs");if(stage==S::TempoFinish&&t.period()!=uint32_t(v[1].er))throw std::runtime_error("Typed tempo period differs");if(stage==S::DueFinish&&t.due()!=!(h.ccr&1))throw std::runtime_error("Typed unsigned due reply differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time return timing");selected->reset();++completed;return true;break;}
case S::TempoShift0:{clocks.shift(v[0].er,1,false);break;}
case S::TempoShift1:{clocks.shift(v[0].er,1,false);break;}
case S::TempoShift2:{clocks.shift(v[0].er,1,false);break;}
case S::TempoShift3:{clocks.shift(v[0].er,1,false);break;}
case S::TempoReadDivision:{clocks.move(v[1].r,int16_t(readData));break;}
case S::TempoDivision:{clocks.move(v[1].r,0x180);break;}
case S::TempoUnit:{clocks.move(v[1].e,0x10);break;}
case S::TempoScaleDivision:{v[1].er=uint16_t(v[1].er)*uint32_t(uint16_t(v[1].e));h.clockI(20);break;}
case S::TempoRate:{clocks.move(v[1].e,0x3e80);break;}
case S::TempoScaleRate:{v[1].er=uint16_t(v[1].er)*uint32_t(uint16_t(v[1].e));h.clockI(20);break;}
case S::TempoDivide:{clocks.divideUnsigned(v[0].r,v[1].er);break;}
case S::TempoTruncate:{v[1].er&=0xffff;value(h,v[1].er);break;}
case S::TempoPublish:{value(h,v[1].er);break;}
case S::TempoFinish:{clocks.ret();if(stage==S::VlqFinish&&(t.value()!=uint32_t(v[5].er)||t.stream()!=uint32_t(v[4].er)))throw std::runtime_error("Typed VLQ result differs");if(stage==S::TempoFinish&&t.period()!=uint32_t(v[1].er))throw std::runtime_error("Typed tempo period differs");if(stage==S::DueFinish&&t.due()!=!(h.ccr&1))throw std::runtime_error("Typed unsigned due reply differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time return timing");selected->reset();++completed;return true;break;}
case S::ArpReadDelta:{clocks.call(0xb312);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time call timing");return true;break;}
case S::ArpSetShift:{clocks.move(v[1].rl,0xc);break;}
case S::ArpShift:{clocks.shift(v[5].er,1,true);break;}
case S::ArpTickShift:{clocks.inc(v[1].rl,-1);break;}
case S::ArpAdmitShift:{;break;}
case S::ArpReadDeadline:{clocks.move(v[1].er,int32_t(readData));break;}
case S::ArpAddDelta:{v[1].er=h.add32(v[1].er,v[5].er);break;}
case S::ArpPublishDeadline:{value(h,v[1].er);break;}
case S::ArpFinish:{clocks.ret();if(stage==S::VlqFinish&&(t.value()!=uint32_t(v[5].er)||t.stream()!=uint32_t(v[4].er)))throw std::runtime_error("Typed VLQ result differs");if(stage==S::TempoFinish&&t.period()!=uint32_t(v[1].er))throw std::runtime_error("Typed tempo period differs");if(stage==S::DueFinish&&t.due()!=!(h.ccr&1))throw std::runtime_error("Typed unsigned due reply differs");h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("MIDI file time bus timing");h.pc=h.makepc(point(t).location);return true;}}
