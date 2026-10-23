#include "ksStartupBannerCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
StartupBannerPartialTest partialTest=nullptr;using T=StartupBannerTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,40> points{{{0xb6c,6},{0xb72,6},{0xb78,2},{0xb7a,6},{0xb80,4},{0xb84,6},{0xb8a,2},{0xb8c,2},{0xb8e,4},{0xb92,2},{0xb94,2},{0xb96,2},{0xb98,4},{0xb9c,2},{0xb9e,4},{0xba2,2},{0xba4,2},{0xba6,2},{0xba8,2},{0xbaa,2},{0xbac,2},{0xbae,2},{0xbb0,2},{0xbb2,2},{0xbb4,2},{0xbb6,4},{0xbba,2},{0xbbc,2},{0xbbe,2},{0xbc0,2},{0xbc2,4},{0xbc6,2},{0xbc8,4},{0xbcc,2},{0xbce,2},{0xbd0,2},{0xbd2,2},{0xbd4,2},{0xbd6,2},{0xbd8,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setStartupBannerPartialForTests(StartupBannerPartialTest value){partialTest=value;}
bool StartupBannerCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xb6c:kind=T::Kind::Banner;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native StartupBanner capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteBannerDirection||stage==S::WriteIdleDirection?0xfffeb1:0xffff62,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field)->uint8_t{return uint8_t(h.read8(field==T::Read::Hardware?0xffec14:0xffff51));};auto write=[&](T::Write field,uint8_t v){f.store8(field==T::Write::Direction?0xfffeb1:0xffff62,int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native StartupBanner admission differs");
 switch(stage){case S::BindDefaultBanner:f.move(r[6].er,0x2d4);break;
case S::ReadBannerHardware:f.move(r[0].rl,task.hardware());break;
case S::BindAlternateBanner:f.move(r[6].er,0x2e2);break;
case S::ShowBanner:f.call(0x9b8);break;
case S::BindVersionBanner:f.move(r[6].er,0x1b8);break;
case S::ShowVersion:f.call(0xbde);break;
case S::ClearBannerPorts:f.move(r[0].rl,0x0);break;
case S::BindBannerMux:f.move(r[0].rl,0x25);break;
case S::SaveDebounceWord:f.push(r[0].r);break;
case S::BindDebounceCount:f.move(r[0].r,0x1);break;
case S::SaveDebounceCount:f.push(r[0].r);break;
case S::BindDebounceInner:f.move(r[0].r,0xa67);break;
case S::DecrementDebounceInner:f.inc(r[0].r,-1);break;
case S::RestoreDebounceCount:f.pop(r[0].r);break;
case S::DecrementDebounceCount:f.inc(r[0].r,-1);break;
case S::RestoreDebounceWord:f.pop(r[0].r);break;
case S::ReadBannerKeys:f.move(r[1].rl,task.keys());break;
case S::BindIdleMux:f.move(r[0].rl,0x30);break;
case S::BindIdleDirection:f.move(r[0].rl,0xffffffff);break;
case S::TestBannerKey:flag(h,4,(uint8_t(r[1].rl)&(1u<<0x6))==0);break;
case S::InitializeBannerNotice:f.call(0xc00);break;
case S::SaveStartupWord:f.push(r[0].r);break;
case S::BindStartupCount:f.move(r[0].r,0x1f4);break;
case S::SaveStartupCount:f.push(r[0].r);break;
case S::BindStartupInner:f.move(r[0].r,0xa67);break;
case S::DecrementStartupInner:f.inc(r[0].r,-1);break;
case S::RestoreStartupCount:f.pop(r[0].r);break;
case S::DecrementStartupCount:f.inc(r[0].r,-1);break;
case S::RestoreStartupWord:f.pop(r[0].r);break;
case S::ReturnBanner:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native StartupBanner clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
