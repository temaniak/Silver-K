#include "ksLoaderPacketsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
LoaderPacketsPartialTest partialTest=nullptr;using T=LoaderPacketsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,55> points{{{0xe82,4},{0xe86,4},{0xe8a,2},{0xe8c,2},{0xe8e,4},{0xe92,4},{0xe96,6},{0xe9c,4},{0xea0,6},{0xea6,4},{0xeaa,4},{0xeae,6},{0xeb4,4},{0xeb8,8},{0xec0,2},{0xec2,2},{0xec4,4},{0xec8,4},{0xecc,2},{0xece,2},{0xed0,4},{0xed4,4},{0xed8,6},{0xede,2},{0xee0,2},{0xee2,4},{0xee6,6},{0xeec,2},{0xeee,2},{0xef0,4},{0xef4,4},{0xef8,2},{0xefa,2},{0xefc,2},{0xefe,4},{0xf02,2},{0xf04,2},{0xf06,4},{0xf0a,4},{0xf0e,6},{0xf14,2},{0xf16,2},{0xf18,4},{0xf1c,6},{0xf22,2},{0xf24,2},{0xf26,2},{0xf28,2},{0xf2a,2},{0xf2c,2},{0xf2e,2},{0xf30,2},{0xf32,2},{0xf34,2},{0xf36,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setLoaderPacketsPartialForTests(LoaderPacketsPartialTest value){partialTest=value;}
bool LoaderPacketsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xe82:kind=T::Kind::Header;break;case 0xec4:kind=T::Kind::Footer;break;case 0xefc:kind=T::Kind::Block;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native LoaderPackets capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[4].r));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[0].rl),uint32_t(r[1].er),uint32_t(r[3].er),uint32_t(r[6].er));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::StoreHeaderModel?0xffec2a:0xffec2b,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field){return uint8_t(h.read8(field==T::Read::Model?0xffec2a:0xffec2b));};auto write=[&](T::Write field,uint32_t value){switch(field){case T::Write::Model:f.store8(0xffec2a,int8_t(value));break;case T::Write::Device:f.store8(0xffec2b,int8_t(value));break;case T::Write::Count:f.store16(0xffec2c,int16_t(value));break;case T::Write::Size:f.store32(0xffec2e,int32_t(value));break;}};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native LoaderPackets admission differs");
 switch(stage){case S::FindHeader:f.call(0xf38);break;
case S::ReadHeaderTag:f.call(0x10d0);break;
case S::CompareHeaderTag:h.sub8(r[0].rl,0x7f);break;
case S::ReadHeaderReserved:f.call(0x10d0);break;
case S::ReadHeaderModel:f.call(0x10d0);break;
case S::ReadHeaderDevice:f.call(0x10d0);break;
case S::ReadHeaderUnused:f.call(0xf58);break;
case S::ReadHeaderCount:f.call(0xf58);break;
case S::ReadHeaderSize:f.call(0xf6a);break;
case S::AcceptHeader:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::ReturnHeader:f.ret();break;
case S::FindFooter:f.call(0xf38);break;
case S::ReadFooterTag:f.call(0x10d0);break;
case S::CompareFooterTag:h.sub8(r[0].rl,0x7d);break;
case S::ReadFooterReserved:f.call(0x10d0);break;
case S::ReadFooterModel:f.call(0x10d0);break;
case S::ReadSavedFooterModel:f.move(r[0].rh,task.expected());break;
case S::CompareFooterModel:h.sub8(r[0].rl,r[0].rh);break;
case S::ReadFooterDevice:f.call(0x10d0);break;
case S::ReadSavedFooterDevice:f.move(r[0].rh,task.expected());break;
case S::CompareFooterDevice:h.sub8(r[0].rl,r[0].rh);break;
case S::ReadFooterUnused:f.call(0xf58);break;
case S::ReadFooterChecksum:f.call(0xf6a);break;
case S::AcceptFooter:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::ReturnFooter:f.ret();break;
case S::FindBlock:f.call(0xf38);break;
case S::ReadBlockTag:f.call(0x10d0);break;
case S::CompareBlockTag:h.sub8(r[0].rl,0x7e);break;
case S::ReadBlockReserved:f.call(0x10d0);break;
case S::ReadBlockModel:f.call(0x10d0);break;
case S::ReadSavedBlockModel:f.move(r[0].rh,task.expected());break;
case S::CompareBlockModel:h.sub8(r[0].rl,r[0].rh);break;
case S::ReadBlockDevice:f.call(0x10d0);break;
case S::ReadSavedBlockDevice:f.move(r[0].rh,task.expected());break;
case S::CompareBlockDevice:h.sub8(r[0].rl,r[0].rh);break;
case S::ReadBlockUnused:f.call(0xf58);break;
case S::ReadBlockIndex:f.call(0xf58);break;
case S::CompareBlockIndex:h.sub16(r[1].r,r[4].r);break;
case S::ReadBlockGroups:f.call(0xf58);break;
case S::DecodeBlock:f.call(0xf8c);break;
case S::ReadBlockChecksum:f.call(0xf6a);break;
case S::AcceptBlock:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::ReturnBlock:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LoaderPackets clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
