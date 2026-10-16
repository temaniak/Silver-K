#include "ksPatchExpandCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {namespace{PatchExpandPartialTest partialTest=nullptr;using T=PatchExpandTask;using S=T::Stage;struct Point{unsigned location,length;};Point point(const T& t){switch(t.stage()){case S::RetainData:return {0xb06c,4};case S::RetainCursor:return {0xb070,4};case S::RetainValue:return {0xb074,4};case S::BindTemplate:return {0xb078,6};case S::BindDestination:return {0xb07e,2};case S::LocateDestination:return {0xb080,6};case S::SetCopyCount:return {0xb086,4};case S::ReadTemplate:return {0xb08a,2};case S::PublishTemplate:return {0xb08c,2};case S::AdvanceCopy:return {0xb08e,2};case S::TickCopy:return {0xb090,2};case S::AdmitCopy:return {0xb092,2};case S::SetZeroCount:return {0xb094,4};case S::ClearValue:return {0xb098,2};case S::PublishZero:return {0xb09a,2};case S::AdvanceZero:return {0xb09c,2};case S::TickZero:return {0xb09e,2};case S::AdmitZero:return {0xb0a0,2};case S::ReadOscillator:return {0xb0a2,8};case S::CaptureOscillator0:return {0xb0aa,2};case S::MaskOscillator0:return {0xb0ac,2};case S::PublishOscillator0:return {0xb0ae,8};case S::QuarterOscillator0:return {0xb0b6,2};case S::QuarterOscillator1:return {0xb0b8,2};case S::CaptureOscillator1:return {0xb0ba,2};case S::MaskOscillator1:return {0xb0bc,2};case S::PublishOscillator1:return {0xb0be,8};case S::QuarterOscillator2:return {0xb0c6,2};case S::QuarterOscillator3:return {0xb0c8,2};case S::CaptureOscillator2:return {0xb0ca,2};case S::MaskOscillator2:return {0xb0cc,2};case S::PublishOscillator2:return {0xb0ce,8};case S::QuarterOscillator4:return {0xb0d6,2};case S::QuarterOscillator5:return {0xb0d8,2};case S::MaskOscillator6:return {0xb0da,2};case S::PublishOscillator6:return {0xb0dc,8};case S::ReadUnison:return {0xb0e4,8};case S::QuarterUnison0:return {0xb0ec,2};case S::QuarterUnison1:return {0xb0ee,2};case S::CaptureUnison0:return {0xb0f0,2};case S::MaskUnison0:return {0xb0f2,2};case S::TickUnison0:return {0xb0f4,2};case S::PublishUnison0:return {0xb0f6,8};case S::QuarterUnison2:return {0xb0fe,2};case S::QuarterUnison3:return {0xb100,2};case S::CaptureUnison1:return {0xb102,2};case S::MaskUnison1:return {0xb104,2};case S::TickUnison1:return {0xb106,2};case S::PublishUnison1:return {0xb108,8};case S::ReadFlags:return {0xb110,8};case S::MaskFlags:return {0xb118,2};case S::PublishFlags:return {0xb11a,8};case S::ReadGate:return {0xb122,8};case S::MaskGate:return {0xb12a,2};case S::HalfGate:return {0xb12c,2};case S::PublishGate:return {0xb12e,8};case S::ReadOffset:return {0xb136,8};case S::AdmitOffset:return {0xb13e,2};case S::SetOffsetBase:return {0xb140,2};case S::SubtractOffset:return {0xb142,2};case S::AdmitOffsetSign:return {0xb144,2};case S::ClearOffset:return {0xb146,2};case S::PublishOffset:return {0xb148,8};case S::ReadCommand:return {0xb150,6};case S::CompareLegacy:return {0xb156,2};case S::AdmitLegacy:return {0xb158,2};case S::ClearLegacyField:return {0xb15a,2};case S::PublishLegacyField:return {0xb15c,8};case S::SetLegacyMode:return {0xb164,2};case S::PublishLegacyMode:return {0xb166,8};case S::ReleaseValue:return {0xb16e,4};case S::ReleaseCursor:return {0xb172,4};case S::ReleaseData:return {0xb176,4};case S::Finish:return {0xb17a,2};default:throw std::runtime_error("Invalid patch expansion stage");}}}
void setPatchExpandPartialForTests(PatchExpandPartialTest fn){partialTest=fn;}
bool PatchExpandCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}if(pc==0xb06c){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Patch expansion capacity exceeded");selected->emplace(uint32_t(v[5].er),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& t=call.task;const auto stage=t.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(t).length);uint32_t readData=0;
 auto publicationAddress=[&](S stage)->uint32_t{switch(stage){case S::PublishTemplate:return t.patch()+128+t.index();case S::PublishZero:return t.patch()+128+t.index();case S::PublishOscillator0:return t.patch()+128;case S::PublishOscillator1:return t.patch()+129;case S::PublishOscillator2:return t.patch()+130;case S::PublishOscillator6:return t.patch()+5;case S::PublishUnison0:return t.patch()+131;case S::PublishUnison1:return t.patch()+132;case S::PublishFlags:return t.patch()+79;case S::PublishGate:return t.patch()+147;case S::PublishOffset:return t.patch()+125;case S::PublishLegacyField:return t.patch()+43;case S::PublishLegacyMode:return t.patch()+151;default:throw std::runtime_error("Invalid patch publication");}};auto timing=[&](S stage){const auto at=publicationAddress(stage)&0xffffff;if(at>=0x400000)return 3u;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=t.nextClocks(t.publication()?timing(stage):3);
 auto read=[&](S stage)->uint32_t{switch(stage){case S::ReadTemplate:return uint8_t(h.read8(0xf428+t.index()));case S::ReadOscillator:return uint8_t(h.read8(t.patch()+5));case S::ReadUnison:return uint8_t(h.read8(t.patch()+78));case S::ReadFlags:return uint8_t(h.read8(t.patch()+79));case S::ReadGate:return uint8_t(h.read8(t.patch()+80));case S::ReadOffset:return uint8_t(h.read8(t.patch()+125));case S::ReadCommand:return uint8_t(h.read8(0x1603b));default:throw std::runtime_error("Invalid patch expansion read");}};auto capture=[&](S stage){return readData=read(stage);};auto write=[&](S stage,uint32_t data){switch(stage){case S::PublishTemplate:h.write8(int8_t(data),t.patch()+128+t.index());break;case S::PublishZero:h.write8(int8_t(data),t.patch()+128+t.index());break;case S::PublishOscillator0:h.write8(int8_t(data),t.patch()+128);break;case S::PublishOscillator1:h.write8(int8_t(data),t.patch()+129);break;case S::PublishOscillator2:h.write8(int8_t(data),t.patch()+130);break;case S::PublishOscillator6:h.write8(int8_t(data),t.patch()+5);break;case S::PublishUnison0:h.write8(int8_t(data),t.patch()+131);break;case S::PublishUnison1:h.write8(int8_t(data),t.patch()+132);break;case S::PublishFlags:h.write8(int8_t(data),t.patch()+79);break;case S::PublishGate:h.write8(int8_t(data),t.patch()+147);break;case S::PublishOffset:h.write8(int8_t(data),t.patch()+125);break;case S::PublishLegacyField:h.write8(int8_t(data),t.patch()+43);break;case S::PublishLegacyMode:h.write8(int8_t(data),t.patch()+151);break;default:throw std::runtime_error("Invalid patch expansion write");}};if(t.advanceBefore(cost,capture,write,timing)!=cost)throw std::runtime_error("Patch expansion deadline mismatch");switch(stage){
case S::RetainData:{clocks.push(v[0].er);call.depth+=4;break;}
case S::RetainCursor:{clocks.push(v[1].er);call.depth+=4;break;}
case S::RetainValue:{clocks.push(v[2].er);call.depth+=4;break;}
case S::BindTemplate:{clocks.move(v[0].er,0xf428);break;}
case S::BindDestination:{clocks.move(v[1].er,v[5].er);break;}
case S::LocateDestination:{v[1].er=h.add32(v[1].er,0x80);break;}
case S::SetCopyCount:{clocks.move(v[2].e,0x51);break;}
case S::ReadTemplate:{clocks.move(v[2].rl,int8_t(readData));v[0].er+=1;h.clockI(2);break;}
case S::PublishTemplate:{value(h,v[2].rl);break;}
case S::AdvanceCopy:{clocks.inc(v[1].er,1);break;}
case S::TickCopy:{clocks.inc(v[2].e,-1);break;}
case S::AdmitCopy:{;break;}
case S::SetZeroCount:{clocks.move(v[2].e,0x2f);break;}
case S::ClearValue:{clocks.move(v[2].rl,0x0);break;}
case S::PublishZero:{value(h,v[2].rl);break;}
case S::AdvanceZero:{clocks.inc(v[1].er,1);break;}
case S::TickZero:{clocks.inc(v[2].e,-1);break;}
case S::AdmitZero:{;break;}
case S::ReadOscillator:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::CaptureOscillator0:{clocks.move(v[0].rh,v[0].rl);break;}
case S::MaskOscillator0:{v[0].rh&=0x3;value(h,v[0].rh);break;}
case S::PublishOscillator0:{value(h,v[0].rh);break;}
case S::QuarterOscillator0:{clocks.shift(v[0].rl,1,false);break;}
case S::QuarterOscillator1:{clocks.shift(v[0].rl,1,false);break;}
case S::CaptureOscillator1:{clocks.move(v[0].rh,v[0].rl);break;}
case S::MaskOscillator1:{v[0].rh&=0x3;value(h,v[0].rh);break;}
case S::PublishOscillator1:{value(h,v[0].rh);break;}
case S::QuarterOscillator2:{clocks.shift(v[0].rl,1,false);break;}
case S::QuarterOscillator3:{clocks.shift(v[0].rl,1,false);break;}
case S::CaptureOscillator2:{clocks.move(v[0].rh,v[0].rl);break;}
case S::MaskOscillator2:{v[0].rh&=0x3;value(h,v[0].rh);break;}
case S::PublishOscillator2:{value(h,v[0].rh);break;}
case S::QuarterOscillator4:{clocks.shift(v[0].rl,1,false);break;}
case S::QuarterOscillator5:{clocks.shift(v[0].rl,1,false);break;}
case S::MaskOscillator6:{v[0].rl&=0x1;value(h,v[0].rl);break;}
case S::PublishOscillator6:{value(h,v[0].rl);break;}
case S::ReadUnison:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::QuarterUnison0:{clocks.shift(v[0].rl,1,false);break;}
case S::QuarterUnison1:{clocks.shift(v[0].rl,1,false);break;}
case S::CaptureUnison0:{clocks.move(v[0].rh,v[0].rl);break;}
case S::MaskUnison0:{v[0].rh&=0x3;value(h,v[0].rh);break;}
case S::TickUnison0:{clocks.inc(v[0].rh,1);break;}
case S::PublishUnison0:{value(h,v[0].rh);break;}
case S::QuarterUnison2:{clocks.shift(v[0].rl,1,false);break;}
case S::QuarterUnison3:{clocks.shift(v[0].rl,1,false);break;}
case S::CaptureUnison1:{clocks.move(v[0].rh,v[0].rl);break;}
case S::MaskUnison1:{v[0].rh&=0x3;value(h,v[0].rh);break;}
case S::TickUnison1:{clocks.inc(v[0].rh,1);break;}
case S::PublishUnison1:{value(h,v[0].rh);break;}
case S::ReadFlags:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::MaskFlags:{v[0].rl&=0xfffffff6;value(h,v[0].rl);break;}
case S::PublishFlags:{value(h,v[0].rl);break;}
case S::ReadGate:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::MaskGate:{v[0].rl&=0x78;value(h,v[0].rl);break;}
case S::HalfGate:{clocks.shift(v[0].rl,1,false);break;}
case S::PublishGate:{value(h,v[0].rl);break;}
case S::ReadOffset:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::AdmitOffset:{;break;}
case S::SetOffsetBase:{clocks.move(v[0].rh,0x40);break;}
case S::SubtractOffset:{v[0].rl=h.sub8(v[0].rl,v[0].rh);break;}
case S::AdmitOffsetSign:{;break;}
case S::ClearOffset:{clocks.move(v[0].rl,0x0);break;}
case S::PublishOffset:{value(h,v[0].rl);break;}
case S::ReadCommand:{clocks.move(v[0].rl,int8_t(readData));break;}
case S::CompareLegacy:{h.sub8(v[0].rl,0x40);break;}
case S::AdmitLegacy:{;break;}
case S::ClearLegacyField:{clocks.move(v[0].rl,0x0);break;}
case S::PublishLegacyField:{value(h,v[0].rl);break;}
case S::SetLegacyMode:{clocks.move(v[0].rl,0x41);break;}
case S::PublishLegacyMode:{value(h,v[0].rl);break;}
case S::ReleaseValue:{clocks.pop(v[2].er);call.depth-=4;break;}
case S::ReleaseCursor:{clocks.pop(v[1].er);call.depth-=4;break;}
case S::ReleaseData:{clocks.pop(v[0].er);call.depth-=4;break;}
case S::Finish:{clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Patch expansion return timing");selected->reset();++completed;return true;break;}
default:break;}if(h.cycles-begin!=cost)throw std::runtime_error("Patch expansion bus timing");h.pc=h.makepc(point(t).location);return true;}}
