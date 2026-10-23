#include "ksLoaderCodecCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
LoaderCodecPartialTest partialTest=nullptr;using T=LoaderCodecTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,57> points{{{0xf38,4},{0xf3c,2},{0xf3e,2},{0xf40,6},{0xf46,2},{0xf48,4},{0xf4c,2},{0xf4e,2},{0xf50,2},{0xf52,2},{0xf54,2},{0xf56,2},{0xf58,4},{0xf5c,2},{0xf5e,4},{0xf62,2},{0xf64,2},{0xf66,2},{0xf68,2},{0xf6a,2},{0xf6c,2},{0xf6e,2},{0xf70,2},{0xf72,2},{0xf74,2},{0xf76,2},{0xf78,2},{0xf7a,2},{0xf7c,4},{0xf80,2},{0xf82,2},{0xf84,2},{0xf86,2},{0xf88,2},{0xf8a,2},{0xf8c,2},{0xf8e,2},{0xf90,2},{0xf92,2},{0xf94,2},{0xf96,2},{0xf98,2},{0xf9a,2},{0xf9c,4},{0xfa0,2},{0xfa2,2},{0xfa4,2},{0xfa6,2},{0xfa8,2},{0xfaa,2},{0xfac,2},{0xfae,2},{0xfb0,2},{0xfb2,2},{0xfb4,4},{0xfb8,2},{0xfba,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){at&=0xffffff;if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setLoaderCodecPartialForTests(LoaderCodecPartialTest value){partialTest=value;}
bool LoaderCodecCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0xf38:kind=T::Kind::Prefix;break;case 0xf58:kind=T::Kind::Word;break;case 0xf6a:kind=T::Kind::Long;break;case 0xf8c:kind=T::Kind::Block;break;case 0xf98:kind=T::Kind::Group;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native LoaderCodec capacity exceeded");slot->emplace(*kind,h.getSP(),uint16_t(h.regs[1].r),uint32_t(h.regs[6].er),uint32_t(h.regs[3].er));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild(uint8_t(r[0].rl),uint32_t(r[1].er),uint32_t(r[3].er),uint32_t(r[6].er));const auto stage=task.stage();
 const unsigned ref=task.destination()&0xffffff;const unsigned store=byteClocks(ref,-1)+byteClocks((ref+1)&0xffffff,int(ref));const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
auto read=[&](T::Read,uint32_t offset){return uint8_t(h.read8(0x1b0 + offset));};auto write=[&](T::Write,uint32_t ref,uint16_t value){f.store16(ref,int16_t(value));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native LoaderCodec admission differs");
 switch(stage){case S::ReadStart:f.call(0x10d0);break;
case S::CompareStart:h.sub8(r[0].rl,0xfffffff0);break;
case S::BindPrefix:f.move(r[1].er,0x1b0);break;
case S::BindPrefixCount:f.move(r[2].rl,0x6);break;
case S::ReadPrefixData:f.call(0x10d0);break;
case S::ReadPrefixExpected:f.move(r[0].rh,task.expected());r[1].er+=1;h.clockI(2);break;
case S::ComparePrefix:h.sub8(r[0].rl,r[0].rh);break;
case S::TickPrefixCount:f.inc(r[2].rl,-1);break;
case S::ReturnPrefix:f.ret();break;
case S::ReadWordLow:f.call(0x10d0);break;
case S::BindWordLow:f.move(r[1].rl,r[0].rl);break;
case S::ReadWordHigh:f.call(0x10d0);break;
case S::BindWordHigh:f.move(r[1].rh,r[0].rl);break;
case S::ClearWordHighBit:f.shift(r[1].rl,1,true);break;
case S::JoinWord:f.shift(r[1].r,1,false);break;
case S::ReturnWord:f.ret();break;
case S::ReadLongLow:f.call(0xf58);break;
case S::SaveLongLow:f.push(r[1].r);break;
case S::ReadLongHigh:f.call(0xf58);break;
case S::BindLongHigh:f.move(r[1].e,r[1].r);break;
case S::RestoreLongLow:f.pop(r[1].r);break;
case S::ShiftLongLowOne:f.shift(r[1].r,1,true);break;
case S::ShiftLongLowTwo:f.shift(r[1].r,1,true);break;
case S::JoinLongOne:f.shift(r[1].er,1,false);break;
case S::JoinLongTwo:f.shift(r[1].er,1,false);break;
case S::ReadLongTop:f.call(0x10d0);break;
case S::BindTopWeight:f.move(r[0].rh,0x10);break;
case S::MultiplyLongTop:r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);break;
case S::RetainLongTop:f.move(r[0].rh,r[0].rl);break;
case S::ClearLongTopLow:r[0].rl=h.sub8(r[0].rl,r[0].rl);break;
case S::MergeLongTop:r[1].e|=r[0].r;value(h,r[1].e);break;
case S::ReturnLong:f.ret();break;
case S::ClearBlockSum:r[3].er=h.sub32(r[3].er,r[3].er);break;
case S::BindBlockCount:f.move(r[0].e,r[1].r);break;
case S::ReadBlockGroup:f.call(0xf98);break;
case S::TickBlockCount:f.inc(r[0].e,-1);break;
case S::ReturnBlock:f.ret();break;
case S::ReadGroupLead:f.call(0xf58);break;
case S::BindGroupCarry:f.move(r[2].e,r[1].r);break;
case S::BindGroupWeight:f.move(r[2].r,0x4000);break;
case S::ReadGroupWord:f.call(0xf58);break;
case S::MultiplyGroupWord:r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[2].r));h.clockI(20);break;
case S::MergeGroupCarry:r[1].r|=r[2].e;value(h,r[1].r);break;
case S::NextGroupDestination:f.inc(r[6].er,2);break;
case S::RetainGroupCarry:f.move(r[2].e,r[1].e);break;
case S::ClearSampleHigh:r[1].e=h.sub16(r[1].e,r[1].e);break;
case S::AddSampleSum:r[3].er=h.add32(r[3].er,r[1].er);break;
case S::ShiftGroupWeightOne:f.shift(r[2].r,1,false);break;
case S::ShiftGroupWeightTwo:f.shift(r[2].r,1,false);break;
case S::CompareGroupWeight:h.sub16(r[2].r,0x1);break;
case S::ReturnGroup:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native LoaderCodec clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
