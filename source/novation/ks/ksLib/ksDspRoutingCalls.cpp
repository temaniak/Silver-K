#include "ksDspRoutingCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
DspRoutingPartialTest partialTest=nullptr;
using K=DspRoutingTask::Kind;using S=DspRoutingTask::Stage;
struct Point {unsigned location,length;};
Point point(const DspRoutingTask& task){
 const auto kind=task.kind();const bool b=kind==K::VoiceB;
 switch(task.stage()){
 case S::ReadPrimary:return {kind==K::PrimeRelative?0x77dcu:0x785au,6};
 case S::TestSign:return {kind==K::PrimeRelative?0x77e2u:0x7860u,2};
 case S::AdmitSign:return {kind==K::PrimeRelative?0x77e4u:0x7862u,2};
 case S::ReadAlternate:return {kind==K::PrimeRelative?0x77e6u:kind==K::WriteRelative?0x7864u:0x784cu,6};
 case S::ResolveAddress:return {kind==K::PrimeRelative?0x77ecu:kind==K::WriteRelative?0x786au:0x7852u,2};
 case S::DispatchWindow:return {kind==K::PrimeRelative?0x77eeu:kind==K::WriteRelative?0x786cu:0x7854u,4};
 case S::ReadVoice:return {b?0x77f4u:0x781au,6};case S::ReadBase:return {0x7820,6};
 case S::Difference:return {0x7826,2};case S::AdmitDifference:return {0x7828,2};case S::ClampDifference:return {0x782a,2};
 case S::NormalizeVoice:return {b?0x77fau:0x782cu,2};case S::PrimaryFactor:return {b?0x77fcu:0x782eu,4};
 case S::PrimaryProduct:return {b?0x7800u:0x7832u,2};case S::PublishPrimary:return {b?0x7802u:0x7834u,6};
 case S::AlternateFactor:return {b?0x7808u:0x783au,4};case S::AlternateProduct:return {b?0x780cu:0x783eu,2};
 case S::PublishAlternate:return {b?0x780eu:0x7840u,6};case S::DispatchSelect:return {b?0x7814u:0x7846u,4};
 case S::PublishAddress:return {0x24b2,6};case S::DispatchAddress:return {0x24b8,4};
 case S::SelectPort:return {kind==K::WriteB?0x24beu:0x24c6u,6};case S::SelectDelay:return {0x24c4,2};
 case S::ReadCached:return {0x24cc,6};case S::CompareCached:return {0x24d2,2};case S::DispatchWrite:return {0x24d4,4};
 case S::AwaitWindow:case S::AwaitSelect:case S::AwaitAddress:case S::Complete:
  switch(kind){case K::PrimeRelative:return {0x77f2,2};case K::WriteRelative:return {0x7870,2};case K::ReadAlternate:return {0x7858,2};
  case K::VoiceB:return {0x7818,2};case K::VoiceA:return {0x784a,2};case K::PrimeAbsolute:return {0x24bc,2};default:break;}break;
 case S::EmitWrite:return {0x24d8,2};
 }throw std::runtime_error("Invalid native DSP routing stage");
}
std::optional<K> root(unsigned pc){switch(pc){case 0x77dc:return K::PrimeRelative;case 0x785a:return K::WriteRelative;
 case 0x784c:return K::ReadAlternate;case 0x77f4:return K::VoiceB;case 0x781a:return K::VoiceA;
 case 0x24b2:return K::PrimeAbsolute;case 0x24be:return K::WriteB;case 0x24c6:return K::WriteA;case 0x24cc:return K::WriteAbsolute;default:return std::nullopt;}}
}
void setDspRoutingPartialForTests(DspRoutingPartialTest fn){partialTest=fn;}
bool DspRoutingCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}
 // 24cc is also an interior continuation of explicit A/B selection.
 if(const auto kind=root(pc);kind&&(!selected||point((*selected)->task).location!=pc)){
  if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native DSP routing capacity exceeded");selected->emplace(*kind,uint16_t(h.regs[4].r),h.getSP());++started;
 }
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;if(task.request()!=DspRoutingTask::Request::None)task.resumeChild();
 const auto stage=task.stage();const auto kind=task.kind();Boundary clocks(h,pc,point(task).length);auto& v=h.regs;
 if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 const auto cost=task.nextClocks();
 auto input=[&](DspRoutingTask::Read what)->uint16_t{using R=DspRoutingTask::Read;
  switch(what){case R::PrimaryOffset:return h.read16(0x174a8);case R::AlternateOffset:return h.read16(0x174aa);
  case R::VoiceB:return uint8_t(h.read8(0x174a7));case R::VoiceA:return uint8_t(h.read8(0x17526));case R::VoiceBase:return uint8_t(h.read8(0x167ba));
  case R::CachedAddress:return h.read16(0x159f2);}return 0;};
 auto output=[&](DspRoutingTask::Write what,uint16_t data){using W=DspRoutingTask::Write;h.write16(int16_t(data),what==W::PrimaryOffset?0x174a8:what==W::AlternateOffset?0x174aa:0x159f2);};
 const auto progress=task.advanceBefore(cost,input,output);if(progress.clocks!=cost||progress.boundaries!=1)throw std::runtime_error("Native DSP routing duration mismatch");
 auto call=[&](unsigned target){clocks.call(target);h.pc=h.makepc(clocks.next);};
 switch(stage){
 case S::ReadPrimary:case S::ReadAlternate:v[0].r=int16_t(task.offset());value(h,v[0].r);break;
 case S::TestSign:value(h,int16_t(task.relative()));break;
 case S::ResolveAddress:h.add16(int16_t(task.relative()),int16_t(task.offset()));v[4].r=int16_t(task.address());break;
 case S::DispatchWindow:call(kind==K::PrimeRelative?0x24b2:kind==K::WriteRelative?0x24cc:0x2560);return true;
 case S::ReadVoice:v[1].rl=int8_t(task.voice());value(h,v[1].rl);break;
 case S::ReadBase:v[0].rl=int8_t(task.baseVoice());value(h,v[0].rl);break;
 case S::Difference:h.sub8(int8_t(task.voice()),int8_t(task.baseVoice()));v[1].rl=int8_t(task.difference());break;
 case S::ClampDifference:v[1].rl=0;value(h,0);break;
 case S::NormalizeVoice:v[1].r=kind==K::VoiceB?task.voice():task.difference();value(h,v[1].r);break;
 case S::PrimaryFactor:v[0].r=int16_t(task.primaryFactor());value(h,v[0].r);break;
 case S::AlternateFactor:v[0].r=int16_t(task.alternateFactor());value(h,v[0].r);break;
 case S::PrimaryProduct:case S::AlternateProduct:v[0].er=int32_t(task.product());h.clockI(20);break;
 case S::PublishPrimary:case S::PublishAlternate:value(h,int16_t(task.product()));break;
 case S::DispatchSelect:call(kind==K::VoiceB?0x248e:0x24a0);return true;
 case S::PublishAddress:value(h,int16_t(task.address()));break;
 case S::DispatchAddress:call(0x2416);return true;
 case S::SelectPort:v[5].er=kind==K::WriteB?0x600000:0x400000;value(h,v[5].er);break;
 case S::ReadCached:v[0].r=int16_t(task.cached());value(h,v[0].r);break;
 case S::CompareCached:h.sub16(int16_t(task.address()),int16_t(task.cached()));break;
 case S::DispatchWrite:call(0x2416);selected->reset();++completed;return true;
 default:break;
 }
 h.pc=h.makepc(point(task).location);return true;
}
}
