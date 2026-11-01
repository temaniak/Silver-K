#include "ksStateParameterExpandCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks {namespace {using T=StateParameterExpandTask;using S=T::Stage;StateParameterExpandPartialTest partialTest=nullptr;struct Fields{unsigned read,shift,second,copy,mask,write;};constexpr std::array<Fields,19> fields{{{0x673a,0x0,0x0,0x6742,0x6744,0x6746},{0x0,0x674e,0x0,0x6750,0x6752,0x6754},{0x0,0x675c,0x0,0x675e,0x6760,0x6762},{0x0,0x676a,0x0,0x676c,0x676e,0x6770},{0x0,0x6778,0x0,0x677a,0x677c,0x677e},{0x6786,0x0,0x0,0x678e,0x6790,0x6792},{0x0,0x679a,0x679c,0x679e,0x67a0,0x67a2},{0x0,0x67aa,0x0,0x67ac,0x67ae,0x67b0},{0x67b8,0x0,0x0,0x67c0,0x67c2,0x67c4},{0x0,0x67cc,0x67ce,0x67d0,0x67d2,0x67d4},{0x67dc,0x0,0x0,0x67e4,0x67e6,0x67e8},{0x0,0x67f0,0x0,0x67f2,0x67f4,0x67f6},{0x0,0x67fe,0x0,0x6800,0x6802,0x6804},{0x680c,0x0,0x0,0x6814,0x6816,0x6818},{0x0,0x6820,0x0,0x6822,0x6824,0x6826},{0x0,0x682e,0x0,0x6830,0x6832,0x6834},{0x0,0x683c,0x0,0x0,0x683e,0x6840},{0x6848,0x0,0x0,0x6850,0x6852,0x6854},{0x0,0x0,0x0,0x0,0x0,0x6862}}};struct Point{unsigned location,length;};Point point(const T& t){const auto f=fields[t.output()];switch(t.stage()){case S::ReadCurrentBase:return {0x6726,8};case S::CaptureCurrent:return {0x672e,2};case S::DispatchFirst:return {0x6730,2};case S::ReadLiveBase:return {0x6732,8};case S::ReadPacked:return {f.read,8};case S::ShiftPacked:return {f.shift,2};case S::ShiftPackedAgain:return {f.second,2};case S::CopyPiece:return {f.copy,2};case S::MaskPiece:return {f.mask,2};case S::WritePiece:return {f.write,8};case S::ShiftSigned:return {0x685c,2};case S::ArithmeticSignedFirst:return {0x685e,2};case S::ArithmeticSignedSecond:return {0x6860,2};case S::WriteSigned:return {0x6862,8};case S::RetainRequest:return {0x686a,2};case S::ReadMode:return {0x686c,6};case S::AdmitMode:return {0x6872,2};case S::CaptureSigned:return {0x6874,2};case S::DispatchSigned:return {0x6876,4};case S::RestoreRequest:case S::AwaitSigned:return {0x687a,2};case S::ReturnExpansion:return {0x687c,2};default:return {0,0};}}}
void setStateParameterExpandPartialForTests(StateParameterExpandPartialTest f){partialTest=f;}
bool StateParameterExpandCalls::step(ControllerCpu& h){using namespace controllerDetail;const unsigned pc=unsigned(h.getPC());std::optional<Call>* selected=nullptr;for(auto& c:calls)if(c&&c->token==h.getSP()){selected=&c;break;}if((pc==0x6726||pc==0x673a)&&(!selected||point((*selected)->task).location!=pc)){if(!selected)for(auto& c:calls)if(!c){selected=&c;break;}if(!selected)throw std::runtime_error("Native field-expansion capacity exceeded");selected->emplace(pc==0x6726?T::Entry::Refresh:T::Entry::Fields,h.getSP(),uint32_t(h.regs[5].er),uint32_t(h.regs[4].er));++started;}
 if(!selected||point((*selected)->task).location!=pc){if(selected)selected->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}auto& call=**selected;auto& task=call.task;auto& r=h.regs;if(task.awaitingSigned())task.resumeSigned();const auto stage=task.stage();const unsigned address=(call.target+task.targetOffset())&0xffffff;const unsigned storeClocks=address<0x400000?((h.lastwrite&1)||int(address)!=(h.lastwrite|1)?3:0):3;const auto cost=task.nextClocks(storeClocks);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);uint32_t observed=0;
 auto read=[&](T::Read what)->uint32_t{if(what==T::Read::CurrentBase)return observed=call.source=uint32_t(h.read32(0x167bc));if(what==T::Read::LiveBase)return observed=call.target=uint32_t(h.read32(0x167c0));if(what==T::Read::Mode)return observed=uint8_t(h.read8(0x167ba));return observed=uint8_t(h.read8(int32_t(call.source+task.sourceOffset())));};auto write=[&](unsigned offset,uint8_t value){f.store8(int32_t(call.target+offset),value);};
 const auto output=task.output();if(task.advanceBefore(cost,read,write,storeClocks)!=cost)throw std::runtime_error("Native expansion admission differs");switch(stage){case S::ReadCurrentBase:{f.move(r[5].er,observed);break;}
case S::CaptureCurrent:{f.move(r[4].er,r[5].er);call.target=call.source;break;}
case S::DispatchFirst:{f.call(0x673a);break;}
case S::ReadLiveBase:{f.move(r[4].er,observed);break;}
case S::ReadPacked:{f.move(r[0].rl,observed);break;}
case S::ShiftPacked:{constexpr unsigned shifts[]{0,2,1,1,1,0,2,2,0,2,0,2,2,0,2,2,2,0,0};f.shift(r[0].rl,shifts[output],false);break;}
case S::ShiftPackedAgain:{f.shift(r[0].rl,1,false);break;}
case S::CopyPiece:{f.move(r[0].rh,r[0].rl);break;}
case S::MaskPiece:{constexpr unsigned masks[]{3,1,1,1,3,7,3,3,7,3,3,3,3,3,3,3,1,7,255};if(output==16){r[0].rl&=masks[output];value(h,r[0].rl);}else{r[0].rh&=masks[output];value(h,r[0].rh);};break;}
case S::WritePiece:{;break;}
case S::ShiftSigned:{f.shift(r[0].rl,1,true);break;}
case S::ArithmeticSignedFirst:{f.arithmeticRight(r[0].rl,2);break;}
case S::ArithmeticSignedSecond:{f.arithmeticRight(r[0].rl,2);break;}
case S::WriteSigned:{;break;}
case S::RetainRequest:{f.push(r[6].r);break;}
case S::ReadMode:{f.move(r[6].rl,observed);break;}
case S::AdmitMode:{;break;}
case S::CaptureSigned:{f.move(r[6].rh,r[0].rl);break;}
case S::DispatchSigned:{f.call(0x4c1e);break;}
case S::RestoreRequest:{f.pop(r[6].r);break;}
case S::ReturnExpansion:{f.ret();break;} default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native expansion clocks differ");if(task.complete()){h.pc=h.makepc(f.next);selected->reset();++completed;return true;}if(!task.awaitingSigned())call.token=h.getSP();h.pc=h.makepc(task.awaitingSigned()?f.next:point(task).location);return true;}}
