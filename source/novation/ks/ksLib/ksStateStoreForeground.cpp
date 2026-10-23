#include "ksStateStoreForeground.h"
#include "ksControllerState.h"
namespace ks {namespace {
uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}
uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int recipeIndex(uint32_t address){constexpr uint32_t roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((address&0xffffff)==roots[n])return int(n);return-1;}
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
}
bool StateStoreForeground::step(ControllerCpu&h){
 using T=StateStoreService;using K=T::Kind;using C=T::Component;
 if(!job_){
  if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17500||!h.interruptMode2||h.indent<=0)return false;
  K kind;switch(h.getPC()){
  case 0x71ba:kind=K::MultiSerialize;break;case 0x687e:kind=K::PackAll;break;case 0x688e:kind=K::PackOne;break;
  case 0x68b0:kind=K::Oscillators;break;case 0x68de:kind=K::Unison;break;case 0x6922:kind=K::Lfo;break;case 0x6944:kind=K::Filter;break;case 0x6970:kind=K::Envelope;break;case 0x69a6:kind=K::ProgramMode;break;
  case 0x6e42:kind=K::ReadPatchName;break;case 0x6e52:kind=K::ReadMultiName;break;case 0x6e5e:kind=K::ReadProvidedName;break;case 0x6e7a:kind=K::WritePatchName;break;case 0x6e8a:kind=K::WriteMultiName;break;case 0x6e96:kind=K::WriteProvidedName;break;case 0x6eb0:kind=K::WriteSelectedName;break;case 0x6eba:kind=K::ReadSelectedName;break;default:return false;
  }
  const auto stack=h.getSP();if((stack&1)||stack<0xfff400||stack>0xfffbfc)return false;
  const auto longword=[&](unsigned a){return(uint32_t(h.nativeLowBase[a])<<24)|(uint32_t(h.nativeLowBase[a+1])<<16)|(unsigned(h.nativeLowBase[a+2])<<8)|h.nativeLowBase[a+3];};
  const uint8_t group=uint8_t(h.regs[2].rl);uint32_t source=kind>=K::ReadPatchName?uint32_t(h.regs[0].er):uint32_t(h.regs[4].er);
  if(kind==K::PackOne&&(group<1||group>6))return false;
  if(kind==K::MultiSerialize||kind==K::PackAll||kind==K::PackOne)for(unsigned n=0;n<6;++n)if(recipeIndex(longword(71306+4*n))<0)return false;
  if(kind==K::PackAll||kind==K::PackOne){if(!T::dataSpan(longword(0x167bc),228))return false;}
  else if(kind>=K::Oscillators&&kind<=K::ProgramMode){if(!T::dataSpan(source,228))return false;}
  else if(kind==K::ReadPatchName||kind==K::WritePatchName){if(!T::dataSpan(longword(0x167bc)+151,16))return false;}
  else if(kind==K::ReadSelectedName||kind==K::WriteSelectedName){if(!T::dataSpan(longword(0x1748e),16))return false;}
  else if(kind==K::ReadProvidedName||kind==K::WriteProvidedName){if(!T::dataSpan(source,16))return false;}
  job_.emplace(kind,T::Input{source,group},stack,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&job=*job_;if(job.depth!=h.indent)return false;auto&service=job.service;const auto cost=service.nextClocks(h.lastwrite);const auto start=h.cycles;
 bool retained=false,released=false;
 if(const auto*t=service.multiTask()){using S=StateMultiSerializeTask::Stage;switch(t->stage()){
 case S::RetainOriginal:case S::RetainMulti:case S::RetainProgram:case S::RetainPart:case S::RetainNameTarget:case S::DispatchPart:case S::DispatchPacking:retained=true;break;
 case S::RestoreNameTarget:case S::RestorePart:case S::RestoreProgram:case S::RestoreMulti:case S::RestoreOriginal:case S::ReturnMulti:released=true;break;default:break;}}
 if(const auto*t=service.packTask()){using S=StatePackRefreshTask::Stage;switch(t->stage()){case S::DispatchEntry:case S::RetainGroup:case S::DispatchRecipe:retained=true;break;case S::RestoreGroup:case S::ReturnEntry:case S::ReturnAll:released=true;break;default:break;}}
 if(const auto*t=service.recipeTask())released=t->stage()==StatePackFieldsTask::Stage::ReturnPacked;
 if(const auto*t=service.partTask())released=t->stage()==PartLookupTask::Stage::Complete;
 if(const auto*t=service.nameTask())released=t->stage()==StatePatchNameTask::Stage::ReturnRead||t->stage()==StatePatchNameTask::Stage::ReturnWrite;
 const auto previous=h.lastwrite;const auto used=service.advanceBefore(cost,[&](uint32_t a,unsigned offset){h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned offset){h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,v);},[&](uint32_t descriptor){const auto index=recipeIndex(descriptor);if(index<0)throw std::runtime_error("Unsupported native store packing descriptor");job.descriptor=descriptor;return StatePackFieldsTask::Kind(index);},previous);
 if(!cost||cost!=used)throw std::logic_error("Native store phase admission differs");
 if(retained)h.lastwrite=-1;if(released)h.lastread=-1;++phases;components|=service.components();
 if(service.complete()){
  const auto kind=service.kind();uint8_t flags=h.ccr&0xd0;
  if(kind==K::MultiSerialize){h.regs[0].er=int32_t(service.original());h.regs[1].er=0x167b8;h.regs[2].er=int32_t((job.descriptor&0xffff0000)|5);h.regs[3].er=int32_t(0x10000u|service.lastProgram());h.regs[4].er=int32_t(service.base());flags|=nz32(service.original());}
  else if(kind==K::PackAll||kind==K::PackOne){
   const auto oldGroup=uint16_t(h.regs[2].r);h.regs[0].r=int16_t((unsigned(service.contributor())<<8)|service.value());h.regs[1].er=int32_t(kind==K::PackAll?service.base()+service.field():service.field());h.regs[2].er=int32_t((job.descriptor&0xffff0000)|(kind==K::PackAll?(oldGroup&0xff00)|7:oldGroup));h.regs[4].er=int32_t(service.base());
   if(kind==K::PackOne)flags=uint8_t((h.ccr&0xf0)|unsigned(service.carry())|nz16(oldGroup));
  }else if(kind<=K::ProgramMode){h.regs[0].r=int16_t((unsigned(service.contributor())<<8)|service.value());h.regs[1].er=int32_t(service.field());flags=uint8_t((h.ccr&0xf0)|unsigned(service.carry())|nz32(service.field()));}
  else {
   const bool reading=kind==K::ReadPatchName||kind==K::ReadMultiName||kind==K::ReadProvidedName||kind==K::ReadSelectedName;
   const auto stored=service.storage();uint8_t hc=h.ccr&0x21;
   if(kind==K::ReadPatchName||kind==K::WritePatchName)hc=controlArithmetic::add(int32_t(stored-151),int32_t(151),h.ccr).flags&0x21;
   else if(kind==K::ReadMultiName||kind==K::WriteMultiName)hc=0;
   h.regs[0].er=int32_t(stored+16);h.regs[1].er=0x1734c;h.regs[2].er=uint16_t(h.regs[2].r)&0xff00;if(reading)h.regs[6].er=0x1733c;flags|=uint8_t(hc|4);
  }
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(job.caller+n);}
  h.setSP(job.caller+4);h.pc=h.makepc(target&0xffffff);h.ccr=flags;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
