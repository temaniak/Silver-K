#include "ksStateStorageCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateStorageTask;using S=T::Stage;StateStoragePartialTest partialTest=nullptr;struct Point{unsigned location,length;};constexpr std::array<Point,39> points{{{0x629e,6},{0x62a4,2},{0x62a6,6},{0x62ac,4},{0x62b0,2},{0x6fcc,2},{0x6fce,4},{0x6fd2,2},{0x6fd4,2},{0x6fd6,6},{0x6fdc,8},{0x6fe4,6},{0x6fea,8},{0x6ff2,2},{0x7190,6},{0x7196,2},{0x7198,4},{0x719c,2},{0x719e,6},{0x71a4,2},{0x7364,6},{0x736a,6},{0x7370,2},{0x7372,6},{0x7378,2},{0x74d4,6},{0x74da,8},{0x74e2,6},{0x74e8,8},{0x74f0,6},{0x74f6,8},{0x74fe,2},{0x7500,6},{0x7506,8},{0x750e,6},{0x7514,8},{0x751c,6},{0x7522,8},{0x752a,2}}};Point point(const T& t){return t.awaitingPart()?Point{0x62b0,2}:points[unsigned(t.stage())];}bool root(unsigned p){return p==0x629e||p==0x6fcc||p==0x7190||p==0x7364||p==0x74d4||p==0x7500;}T::Kind kind(unsigned p){return p==0x629e?T::Kind::BindCurrent:p==0x6fcc?T::Kind::BindPart:p==0x7190?T::Kind::MultiAddress:p==0x7364?T::Kind::Dirty:p==0x74d4?T::Kind::NameTables:T::Kind::Tables;}}
void setStateStoragePartialForTests(StateStoragePartialTest f){partialTest=f;}
bool StateStorageCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if(root(pc)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native state-storage capacity exceeded");selected->emplace(kind(pc),uint8_t(h.regs[0].rl),h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& task=(**selected).task;auto& r=h.regs;if(task.awaitingPart())task.resumePart();const auto stage=task.stage();const auto cost=task.nextClocks();const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{return observed=uint8_t(h.read8(what==T::Read::Mode?0x167ba:what==T::Read::Part||what==T::Read::DirtyPart?0x17526:what==T::Read::Bank?0x167ab:0x1679a));};auto write=[&](T::Write what,uint32_t value){if(what==T::Write::Dirty){f.store8(0x1679a,uint8_t(value));return;}const uint32_t address=what==T::Write::CurrentBase?0x167bc:what==T::Write::LiveBase?0x167c0:what==T::Write::FieldTable?0x167c4:what==T::Write::LimitTable?0x167c8:0x167cc;f.store32(int32_t(address),value);};if(task.advanceBefore(cost,read,write)!=cost)throw std::runtime_error("Native storage admission differs");switch(stage){case S::ReadMode:{f.move(r[0].rl,observed);break;}
case S::AdmitMode:{;break;}
case S::ReadPart:{f.move(r[0].rl,observed);break;}
case S::BindPartChild:{f.call(0x6fcc);break;}
case S::ReturnCurrent:{f.ret();break;}
case S::NormalizePart:{r[0].r&=0xff;value(h,r[0].r);break;}
case S::SetPartStride:{f.move(r[0].e,0x100);break;}
case S::ScalePart:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::CopyPartOffset:{f.move(r[1].er,r[0].er);break;}
case S::AddCurrentBase:{r[0].er=h.add32(r[0].er,0x168fc);break;}
case S::WriteCurrentBase:{;break;}
case S::AddLiveBase:{r[1].er=h.add32(r[1].er,0x16e12);break;}
case S::WriteLiveBase:{;break;}
case S::ReturnPart:{f.ret();break;}
case S::ReadBank:{f.move(r[0].rl,observed);break;}
case S::NormalizeBank:{r[0].r&=0xff;value(h,r[0].r);break;}
case S::SetBankStride:{f.move(r[0].e,0x400);break;}
case S::ScaleBank:{r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);break;}
case S::AddMultiBase:{r[0].er=h.add32(r[0].er,0x845800);break;}
case S::ReturnMulti:{f.ret();break;}
case S::ReadDirty:{f.move(r[0].rl,observed);break;}
case S::ReadDirtyPart:{f.move(r[0].rh,observed);break;}
case S::SetDirty:{r[0].rl|=(1u<<(uint8_t(r[0].rh)&7));break;}
case S::WriteDirty:{;break;}
case S::ReturnDirty:{f.ret();break;}
case S::SetNameFieldTable:{f.move(r[0].er,0x11210);break;}
case S::WriteNameFieldTable:{;break;}
case S::SetNameLimitTable:{f.move(r[0].er,0x112db);break;}
case S::WriteNameLimitTable:{;break;}
case S::SetNameDisplayTable:{f.move(r[0].er,0x1142c);break;}
case S::WriteNameDisplayTable:{;break;}
case S::ReturnNameTables:{f.ret();break;}
case S::SetFieldTable:{f.move(r[0].er,0x11143);break;}
case S::WriteFieldTable:{;break;}
case S::SetLimitTable:{f.move(r[0].er,0x11299);break;}
case S::WriteLimitTable:{;break;}
case S::SetDisplayTable:{f.move(r[0].er,0x1141f);break;}
case S::WriteDisplayTable:{;break;}
case S::ReturnTables:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native storage clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}h.pc=h.makepc(task.awaitingPart()?f.next:point(task).location);return true;}}
