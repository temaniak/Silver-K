#include "ksStateGlobalsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateGlobalsTask;using S=T::Stage;StateGlobalsPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,9> boot{{{0x69c6,6},{0x69cc,6},{0x69d2,4},{0x69d6,2},{0x69d8,2},{0x69da,2},{0x69dc,2},{0x69de,2},{0x69e0,2}}},factory{{{0x6a68,6},{0x6a6e,6},{0x6a74,4},{0x6a78,2},{0x6a7a,2},{0x6a7c,2},{0x6a7e,2},{0x6a80,2},{0x6aac,2}}},backup{{{0x6aae,6},{0x6ab4,6},{0x6aba,4},{0x6abe,2},{0x6ac0,2},{0x6ac2,2},{0x6ac4,2},{0x6ac6,2},{0x6ac8,2}}};constexpr std::array<Point,11> rest{{{0x6a82,6},{0x6a88,6},{0x6a8e,4},{0x6a92,2},{0x6a94,2},{0x6a96,2},{0x6a98,2},{0x6a9a,2},{0x6a9c,6},{0x6aa2,2},{0x6aa4,8}}};constexpr std::array<Point,9> live{{{0x6aca,4},{0x6ace,8},{0x6ad6,2},{0x6ad8,2},{0x6ada,2},{0x6adc,2},{0x6ade,2},{0x6ae0,4},{0x6ae4,2}}};Point point(const T& t){const auto p=t.stage();if(p<=S::AdmitMore)return(t.kind()==T::Kind::BootDefaults?boot:t.kind()==T::Kind::Factory?factory:backup)[unsigned(p)];if(p==S::ReturnCopy)return(t.kind()==T::Kind::BootDefaults?boot:t.kind()==T::Kind::Factory?factory:backup)[8];if(p>=S::SetFactorySource&&p<=S::WriteGlobal)return rest[unsigned(p)-unsigned(S::SetFactorySource)];if(p==S::AwaitInvalid)return {0x6ae4,2};if(p>=S::SetLiveCount&&p<=S::ReturnLive)return live[unsigned(p)-unsigned(S::SetLiveCount)];return {0,0};}
uint32_t source(const T& t){return t.kind()==T::Kind::BootDefaults?0x81ff00:t.kind()==T::Kind::RestoreBackup?0x16dfc:0x169e6;}uint32_t target(const T& t){return t.kind()==T::Kind::Factory?0x16dfc:0x169e6;}}
void setStateGlobalsPartialForTests(StateGlobalsPartialTest f){partialTest=f;}
bool StateGlobalsCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x69c6||pc==0x6a68||pc==0x6aae||pc==0x6aca){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native globals capacity exceeded");selected->emplace(pc==0x69c6?T::Kind::BootDefaults:pc==0x6a68?T::Kind::Factory:pc==0x6aae?T::Kind::RestoreBackup:T::Kind::InvalidateLive,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingInvalid())task.resumeInvalid();const auto stage=task.stage();const uint32_t address=stage==S::WriteFactoryByte?0x169e6+task.byteOffset():stage==S::WriteGlobal?0x168fc+243:stage==S::WriteInvalid?call.live+task.byteOffset():target(task)+task.byteOffset();const unsigned physical=address&0xffffff;const unsigned storeClocks=physical<0x400000?((h.lastwrite&1)||int(physical)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::LiveBase)return observed=call.live=uint32_t(h.read32(0x167c0));return observed=uint8_t(h.read8((what==T::Read::FactoryByte?0x81fe00:source(task))+task.byteOffset()));};auto write=[&](T::Write what,uint8_t v){f.store8(what==T::Write::FactoryByte?0x169e6+task.byteOffset():what==T::Write::FactoryMarker?0x168fc+243:what==T::Write::Invalid?call.live+task.byteOffset():target(task)+task.byteOffset(),v);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native globals admission differs");switch(pc){case 0x69c6:{f.move(r[0].er,0x81ff00);break;}
case 0x69cc:{f.move(r[1].er,0x169e6);break;}
case 0x69d2:{f.move(r[2].e,0x15);break;}
case 0x69d6:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case 0x69d8:{;break;}
case 0x69da:{f.inc(r[1].er,1);break;}
case 0x69dc:{f.inc(r[2].e,-1);break;}
case 0x69de:{;break;}
case 0x69e0:{f.ret();break;}
case 0x6a68:{f.move(r[0].er,0x169e6);break;}
case 0x6a6e:{f.move(r[1].er,0x16dfc);break;}
case 0x6a74:{f.move(r[2].e,0x15);break;}
case 0x6a78:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case 0x6a7a:{;break;}
case 0x6a7c:{f.inc(r[1].er,1);break;}
case 0x6a7e:{f.inc(r[2].e,-1);break;}
case 0x6a80:{;break;}
case 0x6a82:{f.move(r[0].er,0x81fe00);break;}
case 0x6a88:{f.move(r[1].er,0x169e6);break;}
case 0x6a8e:{f.move(r[2].e,0x15);break;}
case 0x6a92:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case 0x6a94:{;break;}
case 0x6a96:{f.inc(r[1].er,1);break;}
case 0x6a98:{f.inc(r[2].e,-1);break;}
case 0x6a9a:{;break;}
case 0x6a9c:{f.move(r[0].er,0x168fc);break;}
case 0x6aa2:{f.move(r[1].rl,0x1);break;}
case 0x6aa4:{;break;}
case 0x6aac:{f.ret();break;}
case 0x6aae:{f.move(r[0].er,0x16dfc);break;}
case 0x6ab4:{f.move(r[1].er,0x169e6);break;}
case 0x6aba:{f.move(r[2].e,0x15);break;}
case 0x6abe:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case 0x6ac0:{;break;}
case 0x6ac2:{f.inc(r[1].er,1);break;}
case 0x6ac4:{f.inc(r[2].e,-1);break;}
case 0x6ac6:{;break;}
case 0x6ac8:{f.ret();break;}
case 0x6aca:{f.move(r[1].e,0xff);break;}
case 0x6ace:{f.move(r[0].er,observed);break;}
case 0x6ad6:{f.move(r[1].rl,0xffffffff);break;}
case 0x6ad8:{;break;}
case 0x6ada:{f.inc(r[0].er,1);break;}
case 0x6adc:{f.inc(r[1].e,-1);break;}
case 0x6ade:{;break;}
case 0x6ae0:{f.call(0x156e);break;}
case 0x6ae4:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native globals clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingInvalid()?f.next:point(task).location);return true;}}
