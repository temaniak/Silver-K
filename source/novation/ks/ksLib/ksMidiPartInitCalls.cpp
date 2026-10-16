#include "ksMidiPartInitCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {MidiPartInitPartialTest partialTest=nullptr;using P=MidiPartInitTask;using PS=P::Stage;using C=MidiControlInitTask;using CS=C::Stage;
struct Point{unsigned location,length;};
Point point(const P& t){switch(t.stage()){
 case PS::ClearPart:return {0x31dc,2};case PS::SetParts:return {0x31de,2};case PS::PublishPart:return {0x31e0,6};case PS::DispatchPart:return {0x31e6,4};case PS::AwaitPart:case PS::BindData:return {0x31ea,2};case PS::SetSentinel:return {0x31ec,2};case PS::PublishSentinel:return {0x31ee + 8*t.field(),8};case PS::ReadReference:return {0x3226,8};case PS::PublishReference:return {0x322e,10};case PS::LocateData:return {0x3238,6};case PS::SetDataCount:return {0x323e,4};case PS::ClearDataValue:return {0x3242,2};case PS::PublishData:return {0x3244,2};case PS::AdvanceData:return {0x3246,2};case PS::TickData:return {0x3248,2};case PS::AdmitData:return {0x324a,2};case PS::TickPart:return {0x324c,2};case PS::TickParts:return {0x324e,2};case PS::AdmitParts:return {0x3250,2};case PS::Finish:return {0x3252,2};default:throw std::runtime_error("Invalid MIDI part initialization stage");}}
Point point(const C& t){switch(t.stage()){
 case CS::BindControls:return {0x3254,6};case CS::SetParts:return {0x325a,2};case CS::BindPatch:return {0x325c,6};case CS::BindTemplate:return {0x3262,6};case CS::SetBytes:return {0x3268,4};case CS::CaptureDestination:return {0x326c,2};case CS::ReadByte:return {0x326e,2};case CS::PublishByte:return {0x3270,2};case CS::AdvanceByte:return {0x3272,2};case CS::TickByte:return {0x3274,2};case CS::AdmitByte:return {0x3276,2};case CS::PublishPatch:return {0x3278,10};case CS::AdvancePatch:return {0x3282,6};case CS::TickPart:return {0x3288,2};case CS::AdmitPart:return {0x328a,2};case CS::Finish:return {0x328c,2};default:throw std::runtime_error("Invalid MIDI control initialization stage");}}
}
void setMidiPartInitPartialForTests(MidiPartInitPartialTest fn){partialTest=fn;}
bool MidiPartInitCalls::step(ControllerCpu& h){using namespace controllerDetail;const auto pc=unsigned(h.getPC());auto& v=h.regs;auto partial=[&]{if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;};
 if(pc<0x3254){
  std::optional<Call<P>>* selected=nullptr;for(auto& c:parts)if(c&&c->token==h.getSP()){selected=&c;break;}
  if(pc==0x31dc){if(!selected)for(auto& c:parts)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI part initialization capacity exceeded");selected->emplace(h.getSP());++partStarted;}
  if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}auto& task=(*selected)->task;
  if(task.awaitingPart()){const auto a=uint32_t(v[0].er);if(a<0x160d2||a>=0x160d2 + 256*334||(a-0x160d2)%334)throw std::runtime_error("Invalid MIDI initialization part binding");task.resumePart(uint8_t((a-0x160d2)/334));}
  const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const auto base=0x160d2 + unsigned(task.part())*334;
  auto address=[&](P::Write field,unsigned offset){return field==P::Write::SelectedPart?0x17526:base+offset;};auto timing=[&](P::Write field,unsigned offset){const auto at=address(field,offset);return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(task.publicationKind(),task.publicationOffset()):3);
  auto read=[&]{return uint32_t(h.read32(0xffee78));};auto write=[&](P::Write field,unsigned offset,uint32_t data){if(field==P::Write::Reference)h.write32(int32_t(data),base+offset);else h.write8(int8_t(data),address(field,offset));};
  if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("MIDI part initialization deadline mismatch");
  switch(stage){case PS::ClearPart:v[3].rl=0;value(h,v[3].rl);break;case PS::SetParts:v[3].rh=5;value(h,v[3].rh);break;case PS::PublishPart:value(h,int8_t(task.index()));break;
  case PS::DispatchPart:clocks.call(0x4000);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI initialization dispatch clocks differ");return true;
  case PS::BindData:v[1].er=int32_t(base);value(h,v[1].er);break;case PS::SetSentinel:v[2].rl=-1;value(h,v[2].rl);break;case PS::PublishSentinel:value(h,-1);break;
  case PS::ReadReference:v[4].er=int32_t(task.reference());value(h,v[4].er);break;case PS::PublishReference:value(h,int32_t(task.reference()));break;
  case PS::LocateData:v[1].er=h.add32(v[1].er,60);break;case PS::SetDataCount:v[2].e=253;value(h,v[2].e);break;case PS::ClearDataValue:v[2].rl=0;value(h,v[2].rl);break;case PS::PublishData:value(h,0);break;
  case PS::AdvanceData:clocks.inc(v[1].er,1);break;case PS::TickData:clocks.inc(v[2].e,-1);break;case PS::TickPart:clocks.inc(v[3].rl,1);break;case PS::TickParts:clocks.inc(v[3].rh,-1);break;
  case PS::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI part initialization return clocks differ");selected->reset();++partCompleted;return true;default:break;}
  if(h.cycles-begin!=cost)throw std::runtime_error("MIDI part initialization bus clocks differ");h.pc=h.makepc(point(task).location);return true;
 }
 std::optional<Call<C>>* selected=nullptr;for(auto& c:controls)if(c&&c->token==h.getSP()){selected=&c;break;}
 if(pc==0x3254){if(!selected)for(auto& c:controls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native MIDI control initialization capacity exceeded");selected->emplace(h.getSP());++controlStarted;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}auto& task=(*selected)->task;
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);auto timing=[&](unsigned part,unsigned offset){const auto at=0x174ae + part*20+offset;return (h.lastwrite&1)||at!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(task.part(),task.offset()):3);
 auto read=[&](unsigned offset){return uint8_t(h.read8(0xf304 + offset));};auto write=[&](C::Write field,unsigned part,unsigned offset,uint32_t data){const auto at=0x174ae + part*20+offset;if(field==C::Write::Patch)h.write32(int32_t(data),at);else h.write8(int8_t(data),at);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("MIDI control initialization deadline mismatch");
 switch(stage){case CS::BindControls:v[1].er=0x174ae;value(h,v[1].er);break;case CS::SetParts:v[2].rl=5;value(h,v[2].rl);break;case CS::BindPatch:v[3].er=0x168fc;value(h,v[3].er);break;case CS::BindTemplate:v[0].er=0xf304;value(h,v[0].er);break;case CS::SetBytes:v[2].e=20;value(h,v[2].e);break;case CS::CaptureDestination:v[4].er=v[1].er;value(h,v[4].er);break;
 case CS::ReadByte:v[2].rh=int8_t(task.byte());value(h,v[2].rh);++v[0].er;h.clockI(2);break;case CS::PublishByte:value(h,int8_t(task.byte()));break;case CS::AdvanceByte:clocks.inc(v[1].er,1);break;case CS::TickByte:clocks.inc(v[2].e,-1);break;
 case CS::PublishPatch:value(h,int32_t(task.patch()));break;case CS::AdvancePatch:v[3].er=h.add32(v[3].er,256);break;case CS::TickPart:clocks.inc(v[2].rl,-1);break;
 case CS::Finish:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("MIDI control initialization return clocks differ");selected->reset();++controlCompleted;return true;default:break;}
 if(h.cycles-begin!=cost)throw std::runtime_error("MIDI control initialization bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
