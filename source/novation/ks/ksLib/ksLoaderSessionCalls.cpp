#include "ksLoaderSessionCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
LoaderSessionPartialTest partialTest=nullptr;using T=LoaderSessionTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,75> points{{{0xd0c,2},{0xd0e,6},{0xd14,2},{0xd16,2},{0xd18,6},{0xd1e,4},{0xd22,2},{0xd24,2},{0xd26,2},{0xd28,6},{0xd2e,4},{0xd32,4},{0xd36,2},{0xd38,2},{0xd3a,4},{0xd3e,2},{0xd40,4},{0xd44,2},{0xd46,4},{0xd4a,2},{0xd4c,2},{0xd4e,2},{0xd50,2},{0xd52,2},{0xd54,2},{0xd56,4},{0xd5a,2},{0xd5c,6},{0xd62,4},{0xd66,2},{0xd68,6},{0xd6e,4},{0xd72,6},{0xd78,4},{0xd7c,2},{0xd7e,6},{0xd84,6},{0xd8a,4},{0xd8e,2},{0xd90,2},{0xd92,2},{0xd94,4},{0xd98,2},{0xd9a,2},{0xd9c,6},{0xda2,2},{0xda4,4},{0xda8,2},{0xdaa,2},{0xdac,4},{0xdb0,2},{0xdb2,4},{0xdb6,6},{0xdbc,8},{0xdc4,2},{0xdc6,2},{0xdc8,2},{0xdca,2},{0xdcc,2},{0xdce,2},{0xdd0,2},{0xdd2,2},{0xdd4,2},{0xdd6,4},{0xdda,2},{0xddc,4},{0xde0,2},{0xde2,6},{0xde8,2},{0xdea,2},{0xdec,2},{0xdee,2},{0xdf0,2},{0xdf2,6},{0xdf8,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setLoaderSessionPartialForTests(LoaderSessionPartialTest value){partialTest=value;}
bool LoaderSessionCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xd0c:kind=T::Kind::Update;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native LoaderSession capacity exceeded");slot->emplace(*kind,h.getSP());++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint32_t(r[1].er),uint32_t(r[3].er),uint32_t(r[6].er),bool(h.ccr&4));const auto stage=task.stage();
 const unsigned ref=task.cursor()&0xffffff;
 auto accessCost=[&](unsigned width,int previous){unsigned result=0;for(unsigned i=0;i<width;++i){unsigned at=(ref+i)&0xffffff;result+=byteClocks(at,previous);previous=(at<0x400000||(at>=0xfffd10&&at<0xffffa0))?int(at):-1;}return result;};
 const unsigned store=stage==S::StoreImageChecksum?accessCost(4,h.lastwrite):byteClocks(stage==S::WriteReceiveMode?0xffff8a:stage==S::StoreReceiveStatus?0xffed69:0xffec33,h.lastwrite);
 const unsigned word=accessCost(2,stage==S::StoreGuardWord?h.lastwrite:-1);const auto cost=task.nextClocks(store,word);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read field,uint32_t ref)->uint32_t{switch(field){case T::Read::BlockCount:return uint16_t(h.read16(0xffec2c));case T::Read::ImageWords:return uint32_t(h.read32(0xffec2e));case T::Read::ImageWord:return uint16_t(h.read16(ref));}return 0;};auto write=[&](T::Write field,uint32_t ref,uint32_t value){switch(field){case T::Write::ReceiveMode:f.store8(0xffff8a,int8_t(value));break;case T::Write::Status:f.store8(stage==S::StoreReceiveStatus?0xffed69:0xffec33,int8_t(value));break;case T::Write::Checksum:f.store32(ref,int32_t(value));break;case T::Write::Guard:f.store16(ref,int16_t(value));break;}};
 if(task.advanceBefore(cost,read,write,store,word)!=cost)throw std::runtime_error("Native LoaderSession admission differs");
 switch(stage){case S::ReadEntryInputs:f.call(0xccc);break;
case S::CompareEntryInputs:h.sub32(r[1].er,0xffbfbfbf);break;
case S::ReturnIgnored:f.ret();break;
case S::BindUpdateNotice:f.move(r[6].er,0x267);break;
case S::ShowUpdateNotice:f.call(0x9b8);break;
case S::ConfirmUpdate:f.call(0xd00);break;
case S::ReturnCancelled:f.ret();break;
case S::BindReceiveNotice:f.move(r[6].er,0x287);break;
case S::ShowReceiveNotice:f.call(0x9b8);break;
case S::ConfigureSerial:f.call(0xfbc);break;
case S::BindReceiveMode:f.move(r[0].rl,0x50);break;
case S::LowerInterruptMask:h.exr&=uint8_t(0xfe);break;
case S::SaveSettleWord:f.push(r[0].r);break;
case S::BindSettleOuter:f.move(r[0].r,0xa);break;
case S::SaveSettleOuter:f.push(r[0].r);break;
case S::BindSettleInner:f.move(r[0].r,0xa67);break;
case S::TickSettleInner:f.inc(r[0].r,-1);break;
case S::RestoreSettleOuter:f.pop(r[0].r);break;
case S::TickSettleOuter:f.inc(r[0].r,-1);break;
case S::RestoreSettleWord:f.pop(r[0].r);break;
case S::ResetReceiveQueue:f.call(0xfe4);break;
case S::ClearReceiveStatus:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::ReceiveHeader:f.call(0xe82);break;
case S::BindImageNotice:f.move(r[6].er,0x2be);break;
case S::ShowImageNotice:f.call(0x9b8);break;
case S::BindImageIdentity:f.move(r[6].er,0xffec2a);break;
case S::ShowImageIdentity:f.call(0xbda);break;
case S::ClearBlockIndex:r[4].r=h.sub16(r[4].r,r[4].r);break;
case S::ReadBlockCount:f.move(r[4].e,task.remaining());break;
case S::BindReceiveDestination:f.move(r[6].er,0xa04000);break;
case S::ReceiveBlock:f.call(0xefc);break;
case S::CompareBlockChecksum:h.sub32(r[3].er,r[1].er);break;
case S::ShowBlockError:f.call(0xb52);break;
case S::NextBlockIndex:f.inc(r[4].r,1);break;
case S::ReadProgressTotal:f.move(r[0].e,task.total());break;
case S::BindProgressIndex:f.move(r[0].r,r[4].r);break;
case S::ShowBlockProgress:f.call(0xb2c);break;
case S::TickBlockCount:f.inc(r[4].e,-1);break;
case S::ReceiveFooter:f.call(0xec4);break;
case S::MaskInterrupts:h.exr=uint8_t(0x7);break;
case S::BindChecksumSource:f.move(r[6].er,0xa04000);break;
case S::ReadImageWords:f.move(r[4].er,task.words());break;
case S::DoubleImageWords:f.shift(r[4].er,1,false);break;
case S::ClearImageSum:r[5].er=h.sub32(r[5].er,r[5].er);break;
case S::ClearSampleHigh:r[0].e=h.sub16(r[0].e,r[0].e);break;
case S::ReadImageWord:f.move(r[0].r,task.sample());r[6].er+=2;h.clockI(2);break;
case S::AddImageWord:r[5].er=h.add32(r[5].er,r[0].er);break;
case S::TickImageWords:f.inc(r[4].er,-1);break;
case S::CompareImageChecksum:h.sub32(r[5].er,r[1].er);break;
case S::ShowChecksumError:f.call(0xb5e);break;
case S::AdvanceChecksumDestination:r[6].er=int32_t(uint32_t(r[6].er)+0x4);break;
case S::BindGuardCount:f.move(r[0].er,0x80);break;
case S::AdvanceGuardDestination:r[6].er=int32_t(uint32_t(r[6].er)+0x2);break;
case S::TickGuardCount:f.inc(r[0].r,-1);break;
case S::BindSuccessStatus:f.move(r[0].rl,0x55);break;
case S::ReturnUpdate:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LoaderSession clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
