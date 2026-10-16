#include "ksDspStartupCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {
DspStartupPartialTest partialTest=nullptr;
struct Point {unsigned location,length,depth;};
Point point(const DspResetTask& task){
 using S=DspResetTask::Stage;using D=DspSettlingTask::Stage;
 switch(task.stage()){
 case S::Admission:return {0x136e,2,0};case S::ReadControl:return {0x1370,2,2};
 case S::Clear:return {0x1372,2,2};case S::WriteLow:return {0x1374,2,2};
 case S::Set:return {0x138e,2,2};case S::WriteHigh:return {0x1390,2,2};
 case S::Release:return {0x13aa,2,2};case S::Complete:return {0x13ac,2,0};
 case S::SettleLow:case S::SettleHigh:{
  const unsigned offset=task.stage()==S::SettleLow?0u:0x1cu;
  Point p{};switch(task.settling().stage()){
  case D::Admission:p={0x1376,2,2};break;case D::OuterSetup:p={0x1378,4,4};break;
  case D::InnerAdmission:p={0x137c,2,4};break;case D::InnerSetup:p={0x137e,4,6};break;
  case D::Tick:p={0x1382,2,6};break;case D::Gate:p={0x1384,2,6};break;
  case D::InnerRelease:p={0x1386,2,6};break;case D::OuterTick:p={0x1388,2,4};break;
  case D::OuterGate:p={0x138a,2,4};break;case D::Release:p={0x138c,2,4};break;
  case D::Complete:throw std::runtime_error("Unreleased native DSP settling task");
  }p.location+=offset;return p;
 }
 }throw std::runtime_error("Invalid native DSP reset stage");
}
Point point(const DspStartupTask& task){
 using S=DspStartupTask::Stage;
 switch(task.stage()){
 case S::Reset:return {0x230c,4,0};case S::AwaitReset:case S::PortA:return {0x2310,6,0};
 case S::ImageA:return {0x2316,6,0};case S::SetupA:return {0x231c,2,0};
 case S::AwaitA:case S::PortB:return {0x231e,6,0};case S::ImageB:return {0x2324,6,0};
 case S::SetupB:return {0x232a,2,0};case S::AwaitB:case S::SelectA:return {0x232c,4,0};
 case S::AwaitSelect:case S::Complete:return {0x2330,2,0};
 }throw std::runtime_error("Invalid native DSP startup stage");
}
Point point(const DspSelectTask& task){
 using S=DspSelectTask::Stage;const unsigned offset=task.port()==DspPort::B?0u:0x12u;
 switch(task.stage()){
 case S::Select:return {0x248e + offset,6,0};case S::Invalidate:return {0x2494 + offset,4,0};
 case S::Publish:return {0x2498 + offset,6,0};case S::Complete:return {0x249e + offset,2,0};
 }throw std::runtime_error("Invalid native DSP selection stage");
}
}
void setDspStartupPartialForTests(DspStartupPartialTest fn){partialTest=fn;}
bool DspStartupCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());
 auto partial=[&]{if(partialTest && partialTest(h)){++partialEntries;return true;}return false;};
 if(pc>=0x136e && pc<=0x13ac){
  using S=DspResetTask::Stage;using D=DspSettlingTask::Stage;std::optional<Reset>* selected=nullptr;
  for(auto& c:resets)if(c && c->token-point(c->task).depth==h.getSP()){selected=&c;break;}
  if(pc==0x136e){if(!selected)for(auto& c:resets)if(!c){selected=&c;break;}
   if(!selected)throw std::runtime_error("Native DSP reset capacity exceeded");selected->emplace(h.getSP());++resetStarted;}
  if(!selected || point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;const auto stage=task.stage();const auto delay=task.settling().stage();const auto p=point(task);Boundary clocks(h,pc,p.length);auto& v=h.regs;
  if(task.complete()){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++resetCompleted;return true;}
  const auto cost=task.nextClocks();const auto progress=task.advanceBefore(cost,[&]{return h.read8(0xffff6f);},[&](uint8_t data){h.write8(int8_t(data),0xffff6f);});
  if(progress.clocks!=cost||progress.boundaries!=1)throw std::runtime_error("Native DSP reset duration mismatch");
  switch(stage){
  case S::Admission:clocks.push(v[6].r);break;case S::ReadControl:v[6].rh=int8_t(task.control());value(h,v[6].rh);break;
  case S::Clear:case S::Set:v[6].rh=int8_t(task.control());break;
  case S::WriteLow:case S::WriteHigh:value(h,int8_t(task.control()));break;
  case S::Release:clocks.pop(v[6].r);break;
  case S::SettleLow:case S::SettleHigh:switch(delay){
   case D::Admission:case D::InnerAdmission:clocks.push(v[0].r);break;
   case D::OuterSetup:v[0].r=int16_t(task.settling().outer());value(h,v[0].r);break;
   case D::InnerSetup:v[0].r=int16_t(task.settling().inner());value(h,v[0].r);break;
   case D::Tick:v[0].r=int16_t(task.settling().inner());value(h,v[0].r);flag(h,2,false);break;
   case D::OuterTick:v[0].r=int16_t(task.settling().outer());value(h,v[0].r);flag(h,2,false);break;
   case D::InnerRelease:case D::Release:clocks.pop(v[0].r);break;default:break;
  }break;default:break;
  }h.pc=h.makepc(point(task).location);return true;
 }
 if(pc>=0x230c && pc<=0x2330){
  using S=DspStartupTask::Stage;std::optional<Startup>* selected=nullptr;
  for(auto& c:startups)if(c && c->token==h.getSP()){selected=&c;break;}
  if(pc==0x230c){if(!selected)for(auto& c:startups)if(!c){selected=&c;break;}
   if(!selected)throw std::runtime_error("Native DSP startup capacity exceeded");selected->emplace(h.getSP());++startupStarted;}
  if(!selected || point((*selected)->task).location!=pc){if(selected)selected->reset();return partial();}
  auto& task=(*selected)->task;if(task.request()!=DspStartupTask::Request::None)task.resumeChild();
  const auto stage=task.stage();Boundary clocks(h,pc,point(task).length);auto& v=h.regs;
  if(stage==S::Complete){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++startupCompleted;return true;}
  if(!task.advanceBefore(task.nextClocks()))throw std::runtime_error("Native DSP startup admission mismatch");
  switch(stage){
  case S::Reset:clocks.call(0x136e);h.pc=h.makepc(clocks.next);return true;
  case S::PortA:v[5].er=0x400000;value(h,v[5].er);break;
  case S::PortB:v[5].er=0x600000;value(h,v[5].er);break;
  case S::ImageA:v[4].er=0xbad6;value(h,v[4].er);break;
  case S::ImageB:v[4].er=0xd0c3;value(h,v[4].er);break;
  case S::SetupA:case S::SetupB:clocks.call(0x2332);h.pc=h.makepc(clocks.next);return true;
  case S::SelectA:clocks.call(0x24a0);h.pc=h.makepc(clocks.next);return true;default:break;
  }h.pc=h.makepc(point(task).location);return true;
 }return false;
}
bool DspSelectCalls::step(ControllerCpu& h){
 using namespace controllerDetail;using S=DspSelectTask::Stage;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;
 for(auto& c:calls)if(c && c->token==h.getSP()){selected=&c;break;}
 if(pc==0x248e||pc==0x24a0){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}
  if(!selected)throw std::runtime_error("Native DSP selection capacity exceeded");selected->emplace(pc==0x248e?DspPort::B:DspPort::A,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest && partialTest(h)){++partialEntries;return true;}return false;}
 auto& task=(*selected)->task;const auto stage=task.stage();Boundary clocks(h,pc,point(task).length);
 if(stage==S::Complete){clocks.ret();h.pc=h.makepc(clocks.next);selected->reset();++completed;return true;}
 const auto cost=task.nextClocks();if(task.advanceBefore(cost,[&](DspPort,uint16_t invalid){h.write16(int16_t(invalid),0x159f2);})!=cost)throw std::runtime_error("Native DSP selection duration mismatch");
 switch(stage){case S::Select:h.regs[5].er=task.port()==DspPort::A?0x400000:0x600000;value(h,h.regs[5].er);break;
 case S::Invalidate:h.regs[0].r=int16_t(task.invalidAddress());value(h,h.regs[0].r);break;
 case S::Publish:value(h,int16_t(task.invalidAddress()));break;default:break;}
 h.pc=h.makepc(point(task).location);return true;
}
}
