#include "ksLoaderInputsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
LoaderInputsPartialTest partialTest=nullptr;using T=LoaderInputsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,101> points{{{0xc0e,2},{0xc10,4},{0xc14,2},{0xc16,2},{0xc18,2},{0xc1a,4},{0xc1e,2},{0xc20,4},{0xc24,2},{0xc26,2},{0xc28,2},{0xc2a,2},{0xc2c,2},{0xc2e,2},{0xc30,2},{0xc32,2},{0xc34,2},{0xc36,2},{0xc38,4},{0xc3c,2},{0xc3e,4},{0xc42,2},{0xc44,2},{0xc46,2},{0xc48,2},{0xc4a,2},{0xc4c,2},{0xc4e,2},{0xc50,2},{0xc52,2},{0xc54,2},{0xc56,2},{0xc58,4},{0xc5c,2},{0xc5e,4},{0xc62,2},{0xc64,2},{0xc66,2},{0xc68,2},{0xc6a,2},{0xc6c,2},{0xc6e,2},{0xc70,2},{0xc72,2},{0xc74,2},{0xc76,4},{0xc7a,2},{0xc7c,4},{0xc80,2},{0xc82,2},{0xc84,2},{0xc86,2},{0xc88,2},{0xc8a,2},{0xc8c,2},{0xc8e,2},{0xc90,2},{0xc92,2},{0xc94,4},{0xc98,2},{0xc9a,4},{0xc9e,6},{0xca4,2},{0xca6,2},{0xca8,4},{0xcac,2},{0xcae,4},{0xcb2,2},{0xcb4,2},{0xcb6,2},{0xcb8,2},{0xcba,2},{0xcbc,2},{0xcbe,4},{0xcc2,6},{0xcc8,2},{0xcca,2},{0xccc,4},{0xcd0,2},{0xcd2,2},{0xcd4,4},{0xcd8,2},{0xcda,4},{0xcde,2},{0xce0,2},{0xce2,2},{0xce4,2},{0xce6,2},{0xce8,2},{0xcea,4},{0xcee,2},{0xcf0,2},{0xcf2,2},{0xcf4,2},{0xcf6,6},{0xcfc,2},{0xcfe,2},{0xd00,2},{0xd02,2},{0xd04,6},{0xd0a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setLoaderInputsPartialForTests(LoaderInputsPartialTest value){partialTest=value;}
bool LoaderInputsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xc0e:kind=T::Kind::Sample;break;case 0xc9a:kind=T::Kind::Released;break;case 0xccc:kind=T::Kind::Stable;break;case 0xcf4:kind=T::Kind::Pressed;break;case 0xd00:kind=T::Kind::Confirm;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native LoaderInputs capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[1].er));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteInputDirection||stage==S::WriteIdleDirection?0xfffeb1:0xffff62,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read){return uint8_t(h.read8(0xffff51));};auto write=[&](T::Write field,uint8_t value){f.store8(field==T::Write::Mux?0xffff62:0xfffeb1,int8_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native LoaderInputs admission differs");
 switch(stage){case S::DisableInputs:f.move(r[0].rl,0x0);break;
case S::BindFirstBank:f.move(r[0].rl,0x22);break;
case S::SaveFirstWord:f.push(r[0].r);break;
case S::BindFirstOuter:f.move(r[0].r,0x1);break;
case S::SaveFirstOuter:f.push(r[0].r);break;
case S::BindFirstInner:f.move(r[0].r,0xa67);break;
case S::TickFirstInner:f.inc(r[0].r,-1);break;
case S::RestoreFirstOuter:f.pop(r[0].r);break;
case S::TickFirstOuter:f.inc(r[0].r,-1);break;
case S::RestoreFirstWord:f.pop(r[0].r);break;
case S::ReadFirstBank:f.move(r[1].rh,uint8_t(task.sample()>>8));break;
case S::BindSecondBank:f.move(r[0].rl,0x23);break;
case S::SaveSecondWord:f.push(r[0].r);break;
case S::BindSecondOuter:f.move(r[0].r,0x1);break;
case S::SaveSecondOuter:f.push(r[0].r);break;
case S::BindSecondInner:f.move(r[0].r,0xa67);break;
case S::TickSecondInner:f.inc(r[0].r,-1);break;
case S::RestoreSecondOuter:f.pop(r[0].r);break;
case S::TickSecondOuter:f.inc(r[0].r,-1);break;
case S::RestoreSecondWord:f.pop(r[0].r);break;
case S::ReadSecondBank:f.move(r[1].rl,uint8_t(task.sample()));break;
case S::RetainFirstPair:f.move(r[1].e,r[1].r);break;
case S::BindThirdBank:f.move(r[0].rl,0x24);break;
case S::SaveThirdWord:f.push(r[0].r);break;
case S::BindThirdOuter:f.move(r[0].r,0x1);break;
case S::SaveThirdOuter:f.push(r[0].r);break;
case S::BindThirdInner:f.move(r[0].r,0xa67);break;
case S::TickThirdInner:f.inc(r[0].r,-1);break;
case S::RestoreThirdOuter:f.pop(r[0].r);break;
case S::TickThirdOuter:f.inc(r[0].r,-1);break;
case S::RestoreThirdWord:f.pop(r[0].r);break;
case S::ReadThirdBank:f.move(r[1].rh,uint8_t(task.sample()>>8));break;
case S::BindFourthBank:f.move(r[0].rl,0x25);break;
case S::SaveFourthWord:f.push(r[0].r);break;
case S::BindFourthOuter:f.move(r[0].r,0x1);break;
case S::SaveFourthOuter:f.push(r[0].r);break;
case S::BindFourthInner:f.move(r[0].r,0xa67);break;
case S::TickFourthInner:f.inc(r[0].r,-1);break;
case S::RestoreFourthOuter:f.pop(r[0].r);break;
case S::TickFourthOuter:f.inc(r[0].r,-1);break;
case S::RestoreFourthWord:f.pop(r[0].r);break;
case S::ReadFourthBank:f.move(r[1].rl,uint8_t(task.sample()));break;
case S::BindIdleBank:f.move(r[0].rl,0x30);break;
case S::EnableInputs:f.move(r[0].rl,0xffffffff);break;
case S::ReturnInputs:f.ret();break;
case S::ReadReleased:f.call(0xc0e);break;
case S::CompareReleased:h.sub32(r[1].er,0xffffffff);break;
case S::SaveReleaseWord:f.push(r[0].r);break;
case S::BindReleaseOuter:f.move(r[0].r,0xa);break;
case S::SaveReleaseOuter:f.push(r[0].r);break;
case S::BindReleaseInner:f.move(r[0].r,0xa67);break;
case S::TickReleaseInner:f.inc(r[0].r,-1);break;
case S::RestoreReleaseOuter:f.pop(r[0].r);break;
case S::TickReleaseOuter:f.inc(r[0].r,-1);break;
case S::RestoreReleaseWord:f.pop(r[0].r);break;
case S::ReadReleasedAgain:f.call(0xc0e);break;
case S::CompareReleasedAgain:h.sub32(r[1].er,0xffffffff);break;
case S::ReturnReleased:f.ret();break;
case S::ReadStable:f.call(0xc0e);break;
case S::RetainStable:f.move(r[2].er,r[1].er);break;
case S::SaveStableWord:f.push(r[0].r);break;
case S::BindStableOuter:f.move(r[0].r,0xa);break;
case S::SaveStableOuter:f.push(r[0].r);break;
case S::BindStableInner:f.move(r[0].r,0xa67);break;
case S::TickStableInner:f.inc(r[0].r,-1);break;
case S::RestoreStableOuter:f.pop(r[0].r);break;
case S::TickStableOuter:f.inc(r[0].r,-1);break;
case S::RestoreStableWord:f.pop(r[0].r);break;
case S::ReadStableAgain:f.call(0xc0e);break;
case S::CompareStable:h.sub32(r[1].er,r[2].er);break;
case S::ReturnStable:f.ret();break;
case S::ReadPressed:f.call(0xccc);break;
case S::ComparePressed:h.sub32(r[1].er,0xffffffff);break;
case S::ReturnPressed:f.ret();break;
case S::WaitRelease:f.call(0xc9a);break;
case S::WaitPress:f.call(0xcf4);break;
case S::CompareConfirmation:h.sub32(r[1].er,0xffffffef);break;
case S::ReturnConfirmation:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LoaderInputs clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
