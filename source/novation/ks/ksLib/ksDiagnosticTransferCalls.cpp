#include "ksDiagnosticTransferCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
DiagnosticTransferPartialTest partialTest=nullptr;using T=DiagnosticTransferTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,89> points{{{0x18f8,6},{0x18fe,4},{0x1902,6},{0x1908,4},{0x190c,6},{0x1912,2},{0x1914,2},{0x1916,6},{0x191c,6},{0x1922,2},{0x1924,2},{0x1926,2},{0x1928,6},{0x192e,8},{0x1936,4},{0x193a,2},{0x193c,2},{0x193e,6},{0x1944,2},{0x1946,4},{0x194a,2},{0x194c,2},{0x194e,2},{0x1950,2},{0x1952,4},{0x1956,2},{0x1958,4},{0x195c,2},{0x195e,4},{0x1962,6},{0x1968,4},{0x196c,6},{0x1972,4},{0x1976,2},{0x1978,4},{0x197c,6},{0x1982,4},{0x1986,8},{0x198e,4},{0x1992,2},{0x1994,4},{0x1998,2},{0x199a,4},{0x199e,2},{0x19a0,4},{0x19a4,2},{0x19a6,4},{0x19aa,2},{0x19ac,4},{0x19b0,2},{0x19b2,4},{0x19b6,6},{0x19bc,4},{0x19c0,6},{0x19c6,4},{0x19ca,2},{0x19cc,4},{0x19d0,2},{0x19d2,4},{0x19d6,2},{0x19d8,4},{0x19dc,2},{0x19de,4},{0x19e2,2},{0x19e4,4},{0x19e8,2},{0x19ea,2},{0x19ec,2},{0x19ee,4},{0x19f2,2},{0x19f4,4},{0x19f8,6},{0x19fe,4},{0x1a02,6},{0x1a08,4},{0x1a0c,2},{0x1a0e,2},{0x1a10,2},{0x1a12,2},{0x1a14,4},{0x1a18,2},{0x1a1a,2},{0x1a1c,2},{0x1a1e,2},{0x1a20,2},{0x1a22,2},{0x1a24,2},{0x1a26,4},{0x1a2a,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setDiagnosticTransferPartialForTests(DiagnosticTransferPartialTest value){partialTest=value;}
bool DiagnosticTransferCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x18f8:kind=T::Kind::Dump;break;case 0x1952:kind=T::Kind::Header;break;case 0x19a6:kind=T::Kind::Footer;break;case 0x19ea:kind=T::Kind::Block;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native DiagnosticTransfer capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[4].r),uint32_t(h.regs[5].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[3].er));const auto stage=task.stage();
 const auto store=byteClocks(stage==S::StoreDumpModel?0x156e6:0x156e7,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read f)->uint32_t{switch(f){case T::Read::Model:return uint8_t(h.read8(0x8041b8));case T::Read::Device:return uint8_t(h.read8(0x8041b9));case T::Read::SavedModel:return uint8_t(h.read8(0x156e6));case T::Read::SavedDevice:return uint8_t(h.read8(0x156e7));case T::Read::Blocks:return uint16_t(h.read16(0x804188));case T::Read::Size:return uint32_t(h.read32(0x804184));}return 0;};auto write=[&](T::Write field,uint8_t v){f.store8(field==T::Write::Model?0x156e6:0x156e7,int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native DiagnosticTransfer admission differs");
 switch(stage){case S::BindDumpNotice:f.move(r[6].er,0xbaab);break;
case S::ShowDumpNotice:f.call(0x9b8);break;
case S::BindDumpName:f.move(r[6].er,0x8041b8);break;
case S::ShowDumpName:f.call(0xbda);break;
case S::BindDumpIdentity:f.move(r[6].er,0x8041b8);break;
case S::ReadDumpModel:f.move(r[0].rl,task.model());r[6].er+=1;h.clockI(2);break;
case S::ReadDumpDevice:f.move(r[0].rh,task.device());r[6].er+=1;h.clockI(2);break;
case S::SendDumpHeader:f.call(0x1952);break;
case S::ClearDumpChecksum:r[5].er=h.sub32(r[5].er,r[5].er);break;
case S::ClearDumpBlock:r[4].r=h.sub16(r[4].r,r[4].r);break;
case S::BindDumpData:f.move(r[6].er,0x804000);break;
case S::ReadDumpBlocks:f.move(r[4].e,task.remaining());break;
case S::SendDumpBlock:f.call(0x19ea);break;
case S::AddDumpChecksum:r[5].er=h.add32(r[5].er,r[3].er);break;
case S::NextDumpBlock:f.inc(r[4].r,1);break;
case S::ReadDumpBlockTotal:f.move(r[0].e,task.total());break;
case S::BindDumpProgress:f.move(r[0].r,r[4].r);break;
case S::ShowDumpProgress:f.call(0xb2c);break;
case S::DecrementDumpBlocks:f.inc(r[4].e,-1);break;
case S::SendDumpFooter:f.call(0x19a6);break;
case S::ReturnDump:f.ret();break;
case S::BeginHeader:f.call(0x1a2c);break;
case S::BindHeaderTag:f.move(r[0].rh,0x7f);break;
case S::SendHeaderTag:f.call(0x119c);break;
case S::ClearHeaderFlags:r[0].rh=h.sub8(r[0].rh,r[0].rh);break;
case S::SendHeaderFlags:f.call(0x119c);break;
case S::ReadHeaderModel:f.move(r[0].rh,task.model());break;
case S::SendHeaderModel:f.call(0x119c);break;
case S::ReadHeaderDevice:f.move(r[0].rh,task.device());break;
case S::SendHeaderDevice:f.call(0x119c);break;
case S::ClearHeaderZero:r[1].r=h.sub16(r[1].r,r[1].r);break;
case S::SendHeaderZero:f.call(0x1a46);break;
case S::ReadHeaderBlocks:f.move(r[1].r,task.total());break;
case S::SendHeaderBlocks:f.call(0x1a46);break;
case S::ReadHeaderSize:f.move(r[1].er,task.size());break;
case S::SendHeaderSize:f.call(0x1a58);break;
case S::ClearHeaderReservedOne:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::SendHeaderReservedOne:f.call(0x1a58);break;
case S::ClearHeaderReservedTwo:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::SendHeaderReservedTwo:f.call(0x1a58);break;
case S::BindHeaderEnd:f.move(r[0].rh,0xfffffff7);break;
case S::SendHeaderEnd:f.call(0x119c);break;
case S::ReturnHeader:f.ret();break;
case S::BeginFooter:f.call(0x1a2c);break;
case S::BindFooterTag:f.move(r[0].rh,0x7d);break;
case S::SendFooterTag:f.call(0x119c);break;
case S::ClearFooterFlags:r[0].rh=h.sub8(r[0].rh,r[0].rh);break;
case S::SendFooterFlags:f.call(0x119c);break;
case S::ReadFooterModel:f.move(r[0].rh,task.model());break;
case S::SendFooterModel:f.call(0x119c);break;
case S::ReadFooterDevice:f.move(r[0].rh,task.device());break;
case S::SendFooterDevice:f.call(0x119c);break;
case S::ClearFooterZero:r[1].r=h.sub16(r[1].r,r[1].r);break;
case S::SendFooterZero:f.call(0x1a46);break;
case S::BindFooterChecksum:f.move(r[1].er,r[5].er);break;
case S::SendFooterChecksum:f.call(0x1a58);break;
case S::ClearFooterReservedOne:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::SendFooterReservedOne:f.call(0x1a58);break;
case S::ClearFooterReservedTwo:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::SendFooterReservedTwo:f.call(0x1a58);break;
case S::BindFooterEnd:f.move(r[0].rh,0xfffffff7);break;
case S::SendFooterEnd:f.call(0x119c);break;
case S::ReturnFooter:f.ret();break;
case S::BeginBlock:f.call(0x1a2c);break;
case S::BindBlockTag:f.move(r[0].rh,0x7e);break;
case S::SendBlockTag:f.call(0x119c);break;
case S::ClearBlockFlags:r[0].rh=h.sub8(r[0].rh,r[0].rh);break;
case S::SendBlockFlags:f.call(0x119c);break;
case S::ReadBlockModel:f.move(r[0].rh,task.model());break;
case S::SendBlockModel:f.call(0x119c);break;
case S::ReadBlockDevice:f.move(r[0].rh,task.device());break;
case S::SendBlockDevice:f.call(0x119c);break;
case S::ClearBlockZero:r[1].r=h.sub16(r[1].r,r[1].r);break;
case S::SendBlockZero:f.call(0x1a46);break;
case S::BindBlockIndex:f.move(r[1].r,r[4].r);break;
case S::SendBlockIndex:f.call(0x1a46);break;
case S::BindBlockSize:f.move(r[1].r,0x20);break;
case S::SendBlockSize:f.call(0x1a46);break;
case S::EncodeBlockData:f.call(0x1a8a);break;
case S::BindBlockChecksum:f.move(r[1].er,r[3].er);break;
case S::SendBlockChecksum:f.call(0x1a58);break;
case S::ClearBlockReserved:r[1].er=h.sub32(r[1].er,r[1].er);break;
case S::SendBlockReserved:f.call(0x1a58);break;
case S::BindBlockEnd:f.move(r[0].rh,0xfffffff7);break;
case S::SendBlockEnd:f.call(0x119c);break;
case S::ReturnBlock:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native DiagnosticTransfer clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
