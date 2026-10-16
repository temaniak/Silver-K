#include "ksVoiceTableInitCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{namespace{VoiceTableInitPartialTest partialTest=nullptr;using T=VoiceTableInitTask;using S=T::Stage;using W=T::Field;struct Point{unsigned location,length;};Point point(S s){switch(s){case S::ClearIndex:return {0xa6d4,2};case S::SetCount:return {0xa6d6,2};case S::BindSlots:return {0xa6d8,6};case S::DefaultNote:return {0xa6de,2};case S::DefaultDistance:return {0xa6e0,2};case S::ClearCounter:return {0xa6e2,2};case S::PublishNote:return {0xa6e4,8};case S::PublishTransposed:return {0xa6ec,8};case S::PublishDistance:return {0xa6f4,8};case S::PublishCounter:return {0xa6fc,10};case S::PublishReleased:return {0xa706,8};case S::ReadMapping:return {0xa70e,8};case S::PublishMapping:return {0xa716,8};case S::AdmitLowerSign:return {0xa71e,2};case S::SetNegativeLower:return {0xa720,2};case S::SkipLower:return {0xa722,2};case S::SetPositiveLower:return {0xa724,2};case S::CompareLowerSplit:return {0xa726,2};case S::AdmitLowerSplit:return {0xa728,2};case S::SetWideLower:return {0xa72a,2};case S::ComputeLower:return {0xa72c,2};case S::PublishLower:return {0xa72e,8};case S::ObserveUpperSign:return {0xa736,2};case S::AdmitUpperSign:return {0xa738,2};case S::SetPositiveUpper:return {0xa73a,2};case S::SkipUpper:return {0xa73c,2};case S::SetNegativeUpper:return {0xa73e,2};case S::CompareUpperSplit:return {0xa740,2};case S::AdmitUpperSplit:return {0xa742,2};case S::SetWideUpper:return {0xa744,2};case S::ComputeUpper:return {0xa746,2};case S::PublishUpper:return {0xa748,8};case S::TickIndex:return {0xa750,2};case S::MaskIndex:return {0xa752,2};case S::AdvanceSlot:return {0xa754,6};case S::TickCount:return {0xa75a,2};case S::AdmitCount:return {0xa75c,4};case S::Finish:return {0xa760,2};default:throw std::runtime_error("Invalid native voice-table setup stage");}}
unsigned address(W w,unsigned slot){unsigned offset=0;switch(w){case W::Note:offset=5;break;case W::Transposed:offset=13;break;case W::Distance:offset=7;break;case W::Counter:offset=8;break;case W::Released:offset=17;break;case W::Mapping:offset=14;break;case W::Lower:offset=15;break;case W::Upper:offset=16;break;}return 0x17560+20*slot+offset;}}
void setVoiceTableInitPartialForTests(VoiceTableInitPartialTest fn){partialTest=fn;}
bool VoiceTableInitCalls::step(ControllerCpu&h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>*selected=nullptr;for(auto&c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
if(pc==0xa6d4){if(selected)throw std::runtime_error("Overlapping native voice-table setup");for(auto&c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native voice-table setup capacity exceeded");selected->emplace(h.getSP());++started;}
if(!selected||point((*selected)->task.stage()).location!=pc){if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
auto&task=(*selected)->task;auto&v=h.regs;auto stage=task.stage();const auto before=h.cycles;Boundary f(h,pc,point(stage).length);auto timing=[&](W w,unsigned slot){const auto a=address(w,slot);return (a&1)&&h.lastwrite==int(a-1)?0u:3u;};auto cost=task.nextClocks(task.publication()?timing(task.publicationField(),task.index()):3u);
if(task.advanceBefore(cost,[&](unsigned index){return uint8_t(h.read8(0x1535a+index));},[&](W w,unsigned slot,unsigned data){const auto a=address(w,slot);if(w==W::Counter)h.write32(int32_t(data),a);else h.write8(int8_t(data),a);},timing)!=cost)throw std::runtime_error("Native voice-table setup admission mismatch");
switch(stage){
case S::ClearIndex:v[4].er=h.sub32(v[4].er,v[4].er);break;
case S::SetCount:f.move(v[5].rl,0xe);break;
case S::BindSlots:f.move(v[3].er,0x17560);break;
case S::DefaultNote:f.move(v[0].rl,0x3c);break;
case S::DefaultDistance:f.move(v[0].rh,0x40);break;
case S::ClearCounter:v[1].er=h.sub32(v[1].er,v[1].er);break;
case S::PublishNote:value(h,v[0].rl);break;
case S::PublishTransposed:value(h,v[0].rl);break;
case S::PublishDistance:value(h,v[0].rh);break;
case S::PublishCounter:value(h,v[1].er);break;
case S::PublishReleased:value(h,v[1].rl);break;
case S::ReadMapping:f.move(v[2].rl,task.mapping());break;
case S::PublishMapping:value(h,v[2].rl);break;
case S::AdmitLowerSign:f.branch(!(h.ccr&8),0xa724);break;
case S::SetNegativeLower:f.move(v[2].rh,0xf);break;
case S::SkipLower:f.branch(true,0xa72c);break;
case S::SetPositiveLower:f.move(v[2].rh,0x27);break;
case S::CompareLowerSplit:h.sub8(v[2].rl,0x7);break;
case S::AdmitLowerSplit:f.branch(bool(h.ccr&5),0xa72c);break;
case S::SetWideLower:f.move(v[2].rh,0x37);break;
case S::ComputeLower:v[2].rh=h.sub8(v[2].rh,v[2].rl);break;
case S::PublishLower:value(h,v[2].rh);break;
case S::ObserveUpperSign:f.move(v[2].rl,v[2].rl);break;
case S::AdmitUpperSign:f.branch(bool(h.ccr&8),0xa73e);break;
case S::SetPositiveUpper:f.move(v[2].rh,0xffffffef);break;
case S::SkipUpper:f.branch(true,0xa746);break;
case S::SetNegativeUpper:f.move(v[2].rh,0xffffffd7);break;
case S::CompareUpperSplit:h.sub8(v[2].rl,0xfffffff8);break;
case S::AdmitUpperSplit:f.branch(!(((h.ccr>>2)^h.ccr)&2),0xa746);break;
case S::SetWideUpper:f.move(v[2].rh,0xffffffc7);break;
case S::ComputeUpper:v[2].rh=h.sub8(v[2].rh,v[2].rl);break;
case S::PublishUpper:value(h,v[2].rh);break;
case S::TickIndex:f.inc(v[4].rl,1);break;
case S::MaskIndex:v[4].rl&=0x1f;value(h,v[4].rl);break;
case S::AdvanceSlot:v[3].er=h.add32(v[3].er,0x14);break;
case S::TickCount:f.inc(v[5].rl,-1);break;
case S::AdmitCount:f.branch(!(h.ccr&4),0xa6e4);break;
case S::Finish:f.ret();break;
default:break;}
if(h.cycles-before!=cost)throw std::runtime_error("Native voice-table setup clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;}else h.pc=h.makepc(point(task.stage()).location);return true;}}
