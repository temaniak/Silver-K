#include "ksStatePackRefreshCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePackRefreshTask;using S=T::Stage;StatePackRefreshPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,18> points{{{0x687e,2},{0x6880,2},{0x6882,2},{0x6884,2},{0x6886,2},{0x6888,2},{0x688a,2},{0x688c,2},{0x688e,2},{0x6890,2},{0x6892,2},{0x6894,2},{0x6896,2},{0x6898,10},{0x68a2,8},{0x68aa,2},{0x68ac,2},{0x68ae,2}}};Point point(const T& t){return t.awaitingRecipe()?Point{0x68ac,2}:points[unsigned(t.stage())];}}
void setStatePackRefreshPartialForTests(StatePackRefreshPartialTest f){partialTest=f;}
bool StatePackRefreshCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x687e||pc==0x688e)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native pack-refresh capacity exceeded");selected->emplace(pc==0x687e?T::Entry::All:T::Entry::One,uint8_t(h.regs[2].rl),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingRecipe())task.resumeRecipe(uint8_t(r[0].rl),uint32_t(r[1].er));const auto stage=task.stage();const unsigned address=(call.source+task.field())&0xffffff;const unsigned storeClocks=address<0x400000?((h.lastwrite&1)||int(address)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::Base)return observed=call.source=uint32_t(h.read32(0x167bc));observed=call.recipe=uint32_t(h.read32(71306+task.descriptorOffset()));constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned i=0;i<6;++i)if((observed&0xffffff)==roots[i])return i;throw std::runtime_error("Unsupported native packed-field descriptor");};auto write=[&](uint32_t field,uint8_t value){f.store8(call.source+field,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native pack-refresh admission differs");switch(stage){case S::BeginAll:{f.move(r[2].rl,0x1);break;}
case S::DispatchEntry:{f.call(0x688e);break;}
case S::AddTargetBase:{r[1].er=h.add32(r[1].er,r[4].er);break;}
case S::WritePacked:{;break;}
case S::NextGroup:{f.inc(r[2].rl,1);break;}
case S::CompareGroup:{h.sub8(r[2].rl,0x6);break;}
case S::AdmitGroup:{;break;}
case S::ReturnAll:{f.ret();break;}
case S::RetainGroup:{f.push(r[2].r);break;}
case S::PreviousGroup:{f.inc(r[2].rl,-1);break;}
case S::ScaleGroup:{f.shift(r[2].rl,2,true);break;}
case S::NormalizeWord:{r[2].r&=0xff;value(h,r[2].r);break;}
case S::NormalizeIndex:{r[2].er&=0xffff;value(h,r[2].er);break;}
case S::ReadRecipe:{f.move(r[2].er,observed);break;}
case S::ReadBase:{f.move(r[4].er,observed);break;}
case S::DispatchRecipe:{f.call((uint32_t(r[2].er)&0xffffff));break;}
case S::RestoreGroup:{f.pop(r[2].r);break;}
case S::ReturnEntry:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native pack-refresh clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingRecipe())call.token=h.getSP();h.pc=h.makepc(task.awaitingRecipe()?f.next:point(task).location);return true;}}
