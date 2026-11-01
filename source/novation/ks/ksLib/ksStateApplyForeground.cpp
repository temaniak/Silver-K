#include "ksStateApplyForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
int recipeIndex(uint32_t a){constexpr uint32_t roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((a&0xffffff)==roots[n])return int(n);return-1;}
bool dataSpan(uint32_t raw,unsigned count){return StateStoreService::dataSpan(raw,count);}
bool readPair(uint32_t raw){const auto a=raw&0xffffff;return dataSpan(raw,2)||(a>=0x800000&&a<0x880000&&2<=0x880000 - a);}
uint8_t classifyFlags(uint8_t field,uint8_t flags){if(field>=212||field==126||(field>=84&&field<=124))return uint8_t((flags&0xd0)|4);return controlArithmetic::subtract(int8_t(field),int8_t(field<84?84:124),flags).flags;}
}
bool StateApplyForeground::step(ControllerCpu&h){
 using T=StateApplyService;using K=T::Kind;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17600||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(h.getPC()){case 0x6ae6:kind=K::Apply;break;case 0x6bf0:kind=K::Post;break;case 0x57e2:kind=K::StoredValue;break;case 0x5816:kind=K::ClassifyStored;break;case 0x63a4:kind=K::CurrentPreset;break;case 0x63bc:kind=K::OverlayPreset;break;case 0x6198:kind=K::ProgramIndex;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  T::Input input{uint32_t(kind==K::ProgramIndex?h.regs[0].er:h.regs[1].er),uint16_t(h.regs[6].r)};
  const auto longValue=[&](unsigned a){return(uint32_t(h.nativeLowBase[a])<<24)|(uint32_t(h.nativeLowBase[a+1])<<16)|(unsigned(h.nativeLowBase[a+2])<<8)|h.nativeLowBase[a+3];};
  if(kind==K::Apply||kind==K::Post){const auto over=longValue(0x167a6),base=over?over:longValue(0x167bc);if(!dataSpan(base,256)||!dataSpan(input.reference,2)||input.reference-base>=256)return false;
   if(kind==K::Apply){if(!dataSpan(longValue(0x167c0),256)||h.memory[0xffed6c]||h.memory[0xffed6e])return false;for(unsigned n=0;n<6;++n)if(recipeIndex(longValue(71306+4*n))<0)return false;}
  }
  if((kind==K::StoredValue||kind==K::ClassifyStored)&&input.reference>255)return false;
  if(kind==K::StoredValue&&!dataSpan(longValue(0x167c0),256))return false;
  if(kind==K::ProgramIndex&&!readPair(input.reference))return false;
  job_.emplace(kind,input,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&service=job.service;const auto cost=service.nextClocks(h.lastwrite);const auto start=h.cycles;
 const bool retained=service.clearsWriteHistory(),released=service.clearsReadHistory();const auto previous=h.lastwrite;
 const auto used=service.advanceBefore(cost,[&](uint32_t a,unsigned offset){h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t value,unsigned offset){h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,value);},[&](uint32_t descriptor){const auto index=recipeIndex(descriptor);if(index<0)throw std::runtime_error("Unsupported native edit packing descriptor");job.recipeDescriptor=descriptor;return StatePackFieldsTask::Kind(index);},previous);
 if(!cost||used!=cost)throw std::logic_error("State apply phase admission differs");if(retained)h.lastwrite=-1;if(released)h.lastread=-1;h.exr=service.critical()?7:job.priority;++phases;components|=service.components();
 if(service.complete()){
  const auto kind=service.kind();uint8_t flags=h.ccr;
  if(kind==K::Apply||kind==K::Post){
   if(kind==K::Apply){h.regs[2].r=int16_t((unsigned(service.cached())<<8)|service.type());if(service.packed()){h.regs[2].e=int16_t(job.recipeDescriptor>>16);h.regs[4].er=int32_t(service.packingBase());}h.regs[3].er=int32_t(service.pickupObserved()?(0x16800u|service.pickup()):service.reference());}
   uint8_t hc=controlArithmetic::subtract(int8_t(service.field()),int8_t(218),0).flags&0x21;
   if(service.signalled()){h.regs[0].er=int32_t((service.dspRetried()?uint32_t(service.dspRemaining())<<16:service.postBase()&0xffff0000)|service.signalMask());h.regs[4].r=int16_t(0x8041);h.regs[5].er=0x400000;hc=service.dspRetried()?0:controlArithmetic::subtract(int16_t(0x8040),int16_t(service.dspCached()),0).flags&0x21;}
   else h.regs[0].er=int32_t(service.enabledObserved()?(service.postBase()&0xffffff00)|service.postEnabled():service.postBase());
   flags=uint8_t((flags&0xd0)|hc|nz32(service.reference()));
  }else if(kind==K::ClassifyStored)flags=classifyFlags(uint8_t(service.reference()),flags);
  else if(kind==K::StoredValue){const auto field=uint8_t(service.reference());if(field>=222&&field<234){h.regs[0].rl=0;flags=uint8_t((flags&0xd0)|4|(controlArithmetic::subtract(int8_t(field),int8_t(234),0).flags&0x21));}
   else {const auto pointer=service.storedPointer();const auto base=pointer-service.reference();const auto added=controlArithmetic::add(int32_t(base),int32_t(service.reference()),flags);h.regs[0].er=int32_t((pointer&0xffffff00)|service.storedValue());flags=uint8_t((added.flags&0xf1)|nz8(service.storedValue()));}
  }else if(kind==K::CurrentPreset||kind==K::OverlayPreset){if(kind==K::OverlayPreset&&!service.presetAvailable()){h.regs[0].er=0x16700;const auto difference=controlArithmetic::subtract(int8_t(service.presetBank()<=4?service.presetBank():service.presetSlot()),int8_t(service.presetBank()<=4?4:49),flags);flags=uint8_t((difference.flags&0xf1)|4);}else {h.regs[0].er=int32_t(service.presetAddress());flags=controlArithmetic::add(int32_t(uint32_t(service.programIndex())*256),int32_t(0x820000),flags).flags;}}
  else {const auto previousBank=uint8_t(service.programBank()-1);const auto bank=previousBank>4?4:previousBank;const auto extra=previousBank>4?uint8_t(previousBank-4):uint8_t(0);const auto added=controlArithmetic::add(int16_t(service.programSlot()+unsigned(bank)*100),int16_t(unsigned(extra)*50),flags);h.regs[0].er=int32_t(((service.reference()+1)&0xffff0000)|service.programIndex());flags=uint8_t((added.flags&0xf1)|nz16(uint16_t(h.regs[1].r)));}
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}h.ccr=flags;h.exr=job.priority;h.setSP(job.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
