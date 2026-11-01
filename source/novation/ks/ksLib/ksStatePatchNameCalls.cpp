#include "ksStatePatchNameCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StatePatchNameTask;using S=T::Stage;StatePatchNamePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,36> points{{{0x6e42,8},{0x6e4a,6},{0x6e50,2},{0x6e52,6},{0x6e58,6},{0x6e5e,6},{0x6e64,2},{0x6e66,4},{0x6e6a,2},{0x6e6c,2},{0x6e6e,2},{0x6e70,2},{0x6e72,2},{0x6e74,2},{0x6e76,2},{0x6e78,2},{0x6e7a,8},{0x6e82,6},{0x6e88,2},{0x6e8a,6},{0x6e90,6},{0x6e96,6},{0x6e9c,4},{0x6ea0,2},{0x6ea2,2},{0x6ea4,2},{0x6ea6,2},{0x6ea8,2},{0x6eaa,2},{0x6eac,2},{0x6eae,2},{0x6eb0,8},{0x6eb8,2},{0x6eba,8},{0x6ec2,2},{0x6ec4,2}}};Point point(const T& t){return points[unsigned(t.stage())];}bool root(unsigned p){switch(p){case 0x6e42:case 0x6e52:case 0x6e5e:case 0x6e7a:case 0x6e8a:case 0x6e96:case 0x6eb0:case 0x6eba:case 0x6ec4:return true;default:return false;}}T::Entry entry(unsigned p){switch(p){case 0x6e42:return T::Entry::ReadPatch;case 0x6e52:return T::Entry::ReadMulti;case 0x6e5e:return T::Entry::ReadProvided;case 0x6e7a:return T::Entry::WritePatch;case 0x6e8a:return T::Entry::WriteMulti;case 0x6e96:return T::Entry::WriteProvided;case 0x6eb0:return T::Entry::WriteSelected;case 0x6eba:return T::Entry::ReadSelected;default:return T::Entry::Empty;}}}
void setStatePatchNamePartialForTests(StatePatchNamePartialTest f){partialTest=f;}
bool StatePatchNameCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(root(pc)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native patch-name capacity exceeded");selected->emplace(entry(pc),h.getSP(),uint32_t(h.regs[0].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;const auto stage=task.stage();const unsigned target=unsigned(stage==S::WriteName?call.storage+task.byteOffset():0x1733c+task.byteOffset())&0xffffff;const unsigned storeClocks=target<0x400000?((h.lastwrite&1)||int(target)!=(h.lastwrite|1)?3:0):target>=0xffffa0?1:target>=0xfffd10?((h.lastwrite&1)||int(target)!=(h.lastwrite|1)?1:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::CurrentBase)return observed=call.storage=uint32_t(h.read32(0x167bc));if(what==T::Read::SelectedBase)return observed=call.storage=uint32_t(h.read32(0x1748e));return observed=uint8_t(h.read8(int32_t(what==T::Read::Name?call.storage+task.byteOffset():0x1733c+task.byteOffset())));};
 auto write=[&](T::Write what,uint8_t v){f.store8(int32_t(what==T::Write::Name?call.storage+task.byteOffset():0x1733c+task.byteOffset()),v);};if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native name admission differs");switch(stage){case S::ReadPatchBase:{f.move(r[0].er,observed);break;}
case S::AddPatchName:{r[0].er=h.add32(r[0].er,0x97);call.storage+=0x97;break;}
case S::JumpRead:{;break;}
case S::SetReadMulti:{f.move(r[0].er,0x169fc);call.storage=0x169fc;break;}
case S::AddReadMulti:{r[0].er=h.add32(r[0].er,0xc0);call.storage+=0xc0;break;}
case S::SetReadScratch:{f.move(r[1].er,0x1733c);break;}
case S::CaptureScratch:{f.move(r[6].er,r[1].er);break;}
case S::SetReadLength:{f.move(r[2].e,0x10);break;}
case S::ReadName:{f.move(r[2].rl,observed);r[0].er+=1;h.clockI(2);break;}
case S::WriteScratch:{;break;}
case S::NextScratch:{f.inc(r[1].er,1);break;}
case S::DecrementRead:{f.inc(r[2].e,-1);break;}
case S::AdmitRead:{;break;}
case S::ClearReadTerm:{f.move(r[2].rl,0x0);break;}
case S::WriteReadTerm:{;break;}
case S::ReturnRead:{f.ret();break;}
case S::ReadWriteBase:{f.move(r[0].er,observed);break;}
case S::AddWritePatch:{r[0].er=h.add32(r[0].er,0x97);call.storage+=0x97;break;}
case S::JumpWrite:{;break;}
case S::SetWriteMulti:{f.move(r[0].er,0x169fc);call.storage=0x169fc;break;}
case S::AddWriteMulti:{r[0].er=h.add32(r[0].er,0xc0);call.storage+=0xc0;break;}
case S::SetWriteScratch:{f.move(r[1].er,0x1733c);break;}
case S::SetWriteLength:{f.move(r[2].e,0x10);break;}
case S::ReadScratch:{f.move(r[2].rl,observed);r[1].er+=1;h.clockI(2);break;}
case S::WriteName:{;break;}
case S::NextName:{f.inc(r[0].er,1);break;}
case S::DecrementWrite:{f.inc(r[2].e,-1);break;}
case S::AdmitWrite:{;break;}
case S::ClearWriteTerm:{f.move(r[2].rl,0x0);break;}
case S::WriteWriteTerm:{;break;}
case S::ReturnWrite:{f.ret();break;}
case S::ReadSelectedWrite:{f.move(r[0].er,observed);break;}
case S::JumpSelectedWrite:{;break;}
case S::ReadSelectedRead:{f.move(r[0].er,observed);break;}
case S::JumpSelectedRead:{;break;}
case S::ReturnEmpty:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native name clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(point(task).location);return true;}}
