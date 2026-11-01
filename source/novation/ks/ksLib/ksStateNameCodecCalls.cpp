#include "ksStateNameCodecCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateNameCodecTask;using S=T::Stage;StateNameCodecPartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,41> points{{{0x75e8,2},{0x75ea,6},{0x75f0,6},{0x75f6,2},{0x75f8,2},{0x760e,2},{0x7610,2},{0x7612,2},{0x7614,2},{0x7616,2},{0x7618,2},{0x761a,2},{0x761c,2},{0x761e,2},{0x7620,2},{0x7622,2},{0x7624,2},{0x7626,2},{0x7628,2},{0x762a,2},{0x762c,2},{0x762e,2},{0x7630,2},{0x7632,2},{0x7634,2},{0x7636,6},{0x763c,6},{0x7642,2},{0x7644,6},{0x764a,2},{0x764c,2},{0x764e,2},{0x7650,2},{0x7652,2},{0x7654,2},{0x7656,2},{0x7658,2},{0x765a,6},{0x7660,2},{0x7662,2},{0x7664,2}}};Point point(const T& t){return points[unsigned(t.stage())];}}
void setStateNameCodecPartialForTests(StateNameCodecPartialTest f){partialTest=f;}
bool StateNameCodecCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(pc==0x75e8||pc==0x760e||pc==0x7644){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native name-codec capacity exceeded");selected->emplace(pc==0x75e8?T::Kind::Read:pc==0x760e?T::Kind::Decode:T::Kind::Encode,uint8_t(h.regs[1].rl),h.getSP());++started;}if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();const unsigned target=stage==S::WriteCase?0x167d8:0x167d9;const unsigned storeClocks=(h.lastwrite&1)||int(target)!=(h.lastwrite|1)?3:0;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;auto read=[&](T::Read what){return observed=uint8_t(h.read8(what==T::Read::Cursor?0x167d7:what==T::Read::Character?0x1733c+task.cursor():what==T::Read::Index?0x167d9:0x167d8));};auto write=[&](T::Write what,uint8_t value){f.store8(what==T::Write::Case?0x167d8:0x167d9,value);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native name-codec admission differs");switch(stage){case S::ClearCursor:{r[0].er=h.sub32(r[0].er,r[0].er);break;}
case S::ReadCursor:{f.move(r[0].rl,observed);break;}
case S::AddNameBase:{r[0].er=h.add32(r[0].er,0x1733c);break;}
case S::ReadCharacter:{f.move(r[1].rl,observed);break;}
case S::ReturnCharacter:{f.ret();break;}
case S::CompareSpace:{h.sub8(r[1].rl,0x20);break;}
case S::AdmitSpace:{;break;}
case S::SetSpaceIndex:{f.move(r[1].rl,0x0);break;}
case S::SkipSpace:{;break;}
case S::CompareUpperStart:{h.sub8(r[1].rl,0x41);break;}
case S::AdmitUpperStart:{;break;}
case S::CompareUpperEnd:{h.sub8(r[1].rl,0x5a);break;}
case S::AdmitUpperEnd:{;break;}
case S::SetUpperIndex:{r[1].rl=h.add8(r[1].rl,0xffffffc0);break;}
case S::SetUpperCase:{f.move(r[1].rh,0x1);break;}
case S::SkipUpper:{;break;}
case S::CompareLowerStart:{h.sub8(r[1].rl,0x61);break;}
case S::AdmitLowerStart:{;break;}
case S::CompareLowerEnd:{h.sub8(r[1].rl,0x7a);break;}
case S::AdmitLowerEnd:{;break;}
case S::SetLowerIndex:{r[1].rl=h.add8(r[1].rl,0xffffffa0);break;}
case S::SetLowerCase:{f.move(r[1].rh,0x0);break;}
case S::SkipLower:{;break;}
case S::SetOtherIndex:{r[1].rl=h.add8(r[1].rl,0xfffffffa);break;}
case S::SkipOther:{;break;}
case S::WriteCase:{;break;}
case S::WriteIndex:{;break;}
case S::ReturnIndex:{f.ret();break;}
case S::ReadIndex:{f.move(r[1].rl,observed);break;}
case S::AdmitIndex:{;break;}
case S::SetSpace:{f.move(r[1].rl,0x20);break;}
case S::SkipEncodedSpace:{;break;}
case S::CompareOther:{h.sub8(r[1].rl,0x1b);break;}
case S::AdmitOther:{;break;}
case S::SetOtherCharacter:{r[1].rl=h.add8(r[1].rl,0x6);break;}
case S::SkipOtherCharacter:{;break;}
case S::SetUpperCharacter:{r[1].rl=h.add8(r[1].rl,0x40);break;}
case S::ReadCase:{f.move(r[1].rh,observed);break;}
case S::AdmitCase:{;break;}
case S::SetLowerCharacter:{r[1].rl=h.add8(r[1].rl,0x20);break;}
case S::ReturnEncoded:{f.ret();break;} default:break;}if(h.cycles-before!=cost)throw std::runtime_error("Native name-codec clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(point(task).location);return true;}}
