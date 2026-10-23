#include "ksLoaderProgramCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
LoaderProgramPartialTest partialTest=nullptr;using T=LoaderProgramTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,48> points{{{0xdfa,10},{0xe04,2},{0xe06,6},{0xe0c,2},{0xe0e,2},{0xe10,2},{0xe12,2},{0xe14,2},{0xe16,2},{0xe18,2},{0xe1a,2},{0xe1c,2},{0xe1e,2},{0xe20,2},{0xe22,6},{0xe28,4},{0xe2c,2},{0xe2e,2},{0xe30,2},{0xe32,2},{0xe34,2},{0xe36,2},{0xe38,4},{0xe3c,2},{0xe3e,4},{0xe42,2},{0xe44,4},{0xe48,2},{0xe4a,2},{0xe4c,2},{0xe4e,2},{0xe50,4},{0xe54,2},{0xe56,6},{0xe5c,2},{0xe5e,6},{0xe64,4},{0xe68,2},{0xe6a,4},{0xe6e,2},{0xe70,4},{0xe74,2},{0xe76,2},{0xe78,2},{0xe7a,2},{0xe7c,2},{0xe7e,2},{0xe80,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setLoaderProgramPartialForTests(LoaderProgramPartialTest value){partialTest=value;}
bool LoaderProgramCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xdfa:kind=T::Kind::Image;break;case 0xe1e:kind=T::Kind::Pages;break;case 0xe56:kind=T::Kind::Result;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native LoaderProgram capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[1].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint16_t(r[6].e));const auto stage=task.stage();
 const unsigned ref=(uint32_t(r[5].er)+388)&0xffffff;unsigned store=0;int previous=-1;for(unsigned i=0;i<4;++i){unsigned at=(ref+i)&0xffffff;store+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field)->uint32_t{return field==T::Read::Size?uint32_t(h.read32(uint32_t(r[5].er)+388)):uint16_t(h.read16(0xffed6a));};auto write=[&](T::Write,uint16_t value){f.store16(0xffed6a,int16_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native LoaderProgram admission differs");
 switch(stage){case S::ReadImageSize:f.move(r[0].er,task.size());break;
case S::IncludeChecksum:r[0].er=int32_t(uint32_t(r[0].er)+0x4);break;
case S::CompareImageLimit:h.sub32(r[0].er,0x16000);break;
case S::BindPageHigh:f.move(r[1].r,r[0].e);break;
case S::MovePageHigh:f.move(r[1].rh,r[1].rl);break;
case S::BindPageLow:f.move(r[1].rl,r[0].rh);break;
case S::TestPartialPage:f.move(r[0].rl,r[0].rl);break;
case S::IncludePartialPage:f.inc(r[1].r,1);break;
case S::ProgramPages:f.call(0xe1e);break;
case S::ReturnImage:f.ret();break;
case S::BindPageTotal:f.move(r[0].e,r[1].r);break;
case S::ClearPageProgress:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::ProgramPage:f.call(0x11ae);break;
case S::TestPageAddress:f.move(r[6].e,r[6].e);break;
case S::NextPageProgress:f.inc(r[0].r,1);break;
case S::BindProgressByte:f.move(r[2].rl,r[0].rl);break;
case S::MaskProgressByte:r[2].rl&=0x7;value(h,r[2].rl);break;
case S::SaveProgressLong:f.push(r[0].er);break;
case S::SavePageCount:f.push(r[1].r);break;
case S::ShowPageProgress:f.call(0xb2c);break;
case S::RestorePageCount:f.pop(r[1].r);break;
case S::RestoreProgressLong:f.pop(r[0].er);break;
case S::TickPageCount:f.inc(r[1].r,-1);break;
case S::ShowProgramResult:f.call(0xe56);break;
case S::ReturnPages:f.ret();break;
case S::ShowImageError:f.call(0xb6a);break;
case S::ReadFailures:f.move(r[0].r,task.failures());break;
case S::BindSuccessNotice:f.move(r[6].er,0x22e);break;
case S::ShowSuccessNotice:f.call(0x9b8);break;
case S::SaveSuccessWord:f.push(r[0].r);break;
case S::BindSuccessOuter:f.move(r[0].r,0xc8);break;
case S::SaveSuccessOuter:f.push(r[0].r);break;
case S::BindSuccessInner:f.move(r[0].r,0xa67);break;
case S::TickSuccessInner:f.inc(r[0].r,-1);break;
case S::RestoreSuccessOuter:f.pop(r[0].r);break;
case S::TickSuccessOuter:f.inc(r[0].r,-1);break;
case S::RestoreSuccessWord:f.pop(r[0].r);break;
case S::ReturnResult:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LoaderProgram clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
