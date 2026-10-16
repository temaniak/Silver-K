#include "ksNoteProgramCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks {
namespace {NoteProgramPartialTest partialTest=nullptr;using T=NoteProgramTask;using S=T::Stage;using K=T::Kind;
struct Point{unsigned location,length;};
Point point(const T& t){switch(t.stage()){
 case S::ClearIndex:return {0x3642,2};case S::ReadPart:return {0x3644,6};case S::DoubleIndex:return {0x364a,2};case S::ReadProgram:return {0x364c,8};case S::CompareBank:return {0x3654,2};case S::AdmitBank:return {0x3656,2};case S::CompareLow:return {0x3658,2};case S::AdmitLow:return {0x365a,2};case S::CompareHigh:return {0x365c,2};case S::AdmitHigh:return {0x365e,2};case S::CaptureNote:return {0x3660,2};case S::SampleIndex:return {0x3662,2};case S::CompareSample:return {0x3664,2};case S::AdmitSample:return {0x3666,2};
 case S::ClearSampleWord:return {0x3668,2};case S::CopySample:return {0x366a,2};case S::SetSampleStride:return {0x366c,4};case S::SampleProduct:return {0x3670,2};case S::CopyBank:return {0x3672,2};case S::BankIndex:return {0x3674,2};case S::NormalizeBank:return {0x3676,2};case S::SetBankStride:return {0x3678,4};case S::BankProduct:return {0x367c,2};case S::AddProducts:return {0x367e,2};case S::AddStorage:return {0x3680,6};case S::DispatchControls:return {0x3686,4};case S::AwaitControls:case S::ReadOriginal:return {0x368a,10};case S::PublishTemporary:return {0x3694,10};case S::RetainOriginal:return {0x369e,4};case S::RetainControls:return {0x36a2,4};case S::DispatchPlay:return {0x36a6,2};case S::AwaitPlay:case S::RestoreControls:return {0x36a8,4};case S::RestoreOriginal:return {0x36ac,4};case S::PublishOriginal:return {0x36b0,10};case S::FinishTemporary:return {0x36ba,2};case S::DispatchPart:return {0x36bc,4};case S::AwaitPart:case S::PublishLast:return {0x36c0,8};case S::DispatchUnison:return {0x36c8,4};case S::AwaitUnison:case S::DispatchFeedback:return {0x36cc,4};case S::AwaitFeedback:case S::Finish:return {0x36d0,2};default:throw std::runtime_error("Invalid native note-program stage");}}
uint8_t bind(uint32_t a,unsigned base,unsigned stride){if(a<base||a>=base+256*stride||(a-base)%stride)throw std::runtime_error("Invalid native note-program binding");return uint8_t((a-base)/stride);}
}
void setNoteProgramPartialForTests(NoteProgramPartialTest fn){partialTest=fn;}
bool NoteProgramCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());auto& v=h.regs;
 std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token-c->depth==h.getSP()){selected=&c;break;}
 if(pc==0x3642||(pc==0x36bc&&!selected)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native note-program capacity exceeded");selected->emplace(pc==0x3642?K::Select:K::Play,h.getSP());++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**selected;auto& task=call.task;if(task.awaitingControls())task.resumeControls(bind(uint32_t(v[0].er),0x174ae,20));if(task.awaitingPart())task.resumePart(bind(uint32_t(v[0].er),0x160d2,334));if(task.awaitingChild())task.resumeChild();
 const auto stage=task.stage();const auto begin=h.cycles;Boundary clocks(h,pc,point(task).length);const unsigned controls=0x174ae + unsigned(task.controlPart())*20,part=0x160d2 + unsigned(task.part())*334;
 auto timing=[&](unsigned offset){const unsigned address=part+offset;return (h.lastwrite&1)||address!=(unsigned(h.lastwrite)|1u)?3u:0u;};const auto cost=task.nextClocks(task.publication()?timing(314):3);
 auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Part:return uint8_t(h.read8(0x17526));case T::Read::Program:return uint16_t(h.read16(92078+task.tableOffset()));case T::Read::OriginalReference:return uint32_t(h.read32(controls));default:return uint8_t(v[5].rl);}};
 auto write=[&](T::Write field,uint32_t data){if(field==T::Write::LastNote)h.write8(int8_t(data),part+314);else h.write32(int32_t(data),controls);};
 if(task.advanceBefore(cost,read,write,timing)!=cost)throw std::runtime_error("Native note-program deadline mismatch");
 switch(stage){
 case S::ClearIndex:v[0].er=h.sub32(v[0].er,v[0].er);break;case S::ReadPart:v[0].rl=int8_t(task.selectedPart());value(h,v[0].rl);break;case S::DoubleIndex:clocks.shift(v[0].rl,1,true);break;case S::ReadProgram:v[0].r=int16_t(task.program());value(h,v[0].r);break;
 case S::CompareBank:h.sub8(v[0].rh,4);break;case S::CompareLow:h.sub8(v[5].rl,36);break;case S::CompareHigh:h.sub8(v[5].rl,84);break;case S::CaptureNote:v[3].rl=int8_t(task.note());value(h,v[3].rl);break;case S::SampleIndex:v[3].rl=h.add8(v[3].rl,-36);break;case S::CompareSample:h.sub8(v[3].rl,v[0].rl);break;
 case S::ClearSampleWord:v[1].r=h.sub16(v[1].r,v[1].r);break;case S::CopySample:v[1].rl=int8_t(task.sampleIndex());value(h,v[1].rl);break;case S::SetSampleStride:v[1].e=0x100;value(h,v[1].e);break;case S::SampleProduct:v[1].er=int32_t(task.sampleOffset());h.clockI(20);break;
 case S::CopyBank:v[0].rl=int8_t(task.bankIndex());value(h,v[0].rl);break;case S::BankIndex:v[0].rl=h.add8(v[0].rl,-5);break;case S::NormalizeBank:v[0].r&=255;value(h,v[0].r);break;case S::SetBankStride:v[0].e=0x3200;value(h,v[0].e);break;case S::BankProduct:v[0].er=int32_t(task.bankOffset());h.clockI(20);break;
 case S::AddProducts:v[1].er=h.add32(v[1].er,v[0].er);break;case S::AddStorage:v[1].er=h.add32(v[1].er,0x839000);break;
 case S::DispatchControls:case S::DispatchPlay:case S::DispatchPart:case S::DispatchUnison:case S::DispatchFeedback:clocks.call(stage==S::DispatchControls?0xa66a:stage==S::DispatchPlay?0x36bc:stage==S::DispatchPart?0x4000:stage==S::DispatchUnison?0x8fcc:0xa00e);h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native note-program dispatch clocks differ");return true;
 case S::ReadOriginal:v[2].er=int32_t(task.originalReference());value(h,v[2].er);break;case S::PublishTemporary:value(h,int32_t(task.patchReference()));break;
 case S::RetainOriginal:clocks.push(v[2].er);call.depth+=4;break;case S::RetainControls:clocks.push(v[0].er);call.depth+=4;break;case S::RestoreControls:clocks.pop(v[0].er);call.depth-=4;break;case S::RestoreOriginal:clocks.pop(v[2].er);call.depth-=4;break;case S::PublishOriginal:value(h,int32_t(task.originalReference()));break;
 case S::PublishLast:value(h,int8_t(task.lastNote()));break;
 case S::Finish:case S::FinishTemporary:clocks.ret();h.pc=h.makepc(clocks.next);if(h.cycles-begin!=cost)throw std::runtime_error("Native note-program return clocks differ");selected->reset();++completed;return true;default:break;
 }
 if(h.cycles-begin!=cost)throw std::runtime_error("Native note-program bus clocks differ");h.pc=h.makepc(point(task).location);return true;
}
}
