#include "ksStateSaveForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
using T=StateSaveService;using K=T::Kind;
uint8_t nz8(uint8_t v){return uint8_t((v&0x80?8:0)|(!v?4:0));}uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
int recipe(uint32_t address){constexpr uint32_t roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((address&0xffffff)==roots[n])return int(n);return-1;}
bool handler(uint8_t category,uint32_t raw){switch(category){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}}
struct Resolver {uint32_t&descriptor;StatePackFieldsTask::Kind operator()(uint32_t raw)const{const auto n=recipe(raw);if(n<0)throw std::logic_error("Unsupported live save packing descriptor");descriptor=raw;return StatePackFieldsTask::Kind(n);}bool operator()(uint8_t category,uint32_t raw)const{return handler(category,raw);}};
uint8_t arithmetic(const T::Summary&s,uint8_t entry){switch(s.decision){
 case T::LastDecision::Entry:return entry&0x21;
 case T::LastDecision::ProgramBank:return controlArithmetic::subtract(int8_t(s.presetProgram>>8),int8_t(4),0).flags&0x21;
 case T::LastDecision::ProgramAlias:return controlArithmetic::subtract(int8_t(s.presetProgram),int8_t(49),0).flags&0x21;
 case T::LastDecision::FormatIndex:return controlArithmetic::subtract(int16_t(s.formatIndex),int16_t(599),0).flags&0x21;
 case T::LastDecision::SourceAdvance:return controlArithmetic::add(int32_t(s.multiSourceBeforeAdvance),int32_t(256),0).flags&0x21;
 case T::LastDecision::LcdReply:{using A=PanelTextTask::Action;return s.lcd.action==A::Write?controlArithmetic::subtract(int8_t(s.lcd.reply),int8_t(20),0).flags&0x21:s.lcd.action==A::Command?controlArithmetic::subtract(int8_t(s.lcd.input),int8_t(251),0).flags&0x21:0;}
 case T::LastDecision::Page:switch(s.pageDecision){case StateSavePageService::Decision::SkipProtected:return controlArithmetic::add(int32_t(s.pageLeft),int32_t(s.pageRight),0).flags&0x21;case StateSavePageService::Decision::FlashRange:case StateSavePageService::Decision::Protection:return controlArithmetic::subtract(int32_t(s.pageLeft),int32_t(s.pageRight),0).flags&0x21;default:return controlArithmetic::subtract(int8_t(s.pageLeft),int8_t(s.pageRight),0).flags&0x21;}}
 return 0;
}
bool endpoint(uint32_t a){return T::dataSpan(a,1)||(a>=0x800000&&a<0x900000)||(a>=0x1c0&&a<0x300)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffff51||a==0xffff61||a==0xffff62||a==0xfffeb1;}
}
bool StateSaveForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17530||!h.interruptMode2||h.indent<=0)return false;K kind;switch(h.getPC()){
 case 0x47b8:kind=K::CommitMulti;break;case 0x47d6:kind=K::CommitSingle;break;case 0x6494:kind=K::PresetSave;break;case 0x64e4:kind=K::FormatDirect;break;case 0x6576:kind=K::FormatPart;break;case 0x668c:kind=K::Offset;break;case 0x66bc:kind=K::Check;break;case 0x66d2:kind=K::Message;break;case 0x6662:kind=K::FlashCommit;break;case 0x69e2:kind=K::GlobalSave;break;case 0x71a6:kind=K::MultiCurrent;break;case 0x72e0:kind=K::MultiProvided;break;case 0x11ae:kind=K::FlashProgram;break;case 0x1272:kind=K::FlashCompare;break;default:return false;}
  const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|peek(a+n);return v;};
  T::Input in;in.program=uint32_t(h.regs[0].er);in.source=uint32_t(h.regs[1].er);in.target=uint32_t(h.regs[6].er);in.noticeAction.value=0x55d4;in.saveAction.value=0x55e2;
  if(kind==K::FlashProgram||kind==K::FlashCompare||kind==K::FlashCommit||kind==K::Offset)in.source=uint32_t(h.regs[5].er);
  const bool packed=kind==K::CommitSingle||kind==K::CommitMulti||kind==K::PresetSave||kind==K::MultiCurrent;
  if(packed){for(unsigned n=0;n<6;++n)if(recipe(wide(71306+4*n))<0)return false;if(!T::dataSpan(wide(0x167bc),228))return false;}
  if((kind==K::FormatDirect||kind==K::FormatPart)&&(!T::dataSpan(in.source,209)||!T::dataSpan(in.program,2)))return false;
  if(kind==K::MultiProvided&&(!T::dataSpan(in.source,1024)||!T::dataSpan(in.program,2)))return false;
  if((kind==K::FlashProgram||kind==K::FlashCompare||kind==K::FlashCommit||kind==K::Offset)&&!T::dataSpan(in.source,256))return false;
  if(kind==K::FlashCompare){const auto a=in.target&0xffff00;if(!(a>=0x800000&&a<0x900000)&&!T::dataSpan(a,256))return false;}
  if(kind==K::FlashProgram||kind==K::FlashCommit||kind==K::Offset){const bool fails=(in.target-0x800000u)&0x80000000u;const auto a=in.target&0xffffff;if(!fails&&(a<0x800000||a>=0x900000))return false;}
  if(kind==K::CommitSingle&&!StateReloadService::admittedSchema(peek,handler))return false;
  job_.emplace(kind,in,sp,h.indent);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;const auto priority=s.writePriority();Resolver resolver{j.descriptor};
 const auto used=s.advanceBefore(cost,[&](uint32_t a,unsigned offset){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving save read resource");h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t a,uint8_t v,unsigned offset){a&=0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving save write resource");h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,v);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native full save phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;if(!priority&&s.writePriority()){j.savedPriority=h.exr;h.exr|=6;}else if(priority&&!s.writePriority())h.exr=j.savedPriority;++phases;components|=s.components();
 if(s.complete()){
  const auto kind=s.kind();const auto&o=s.summary();const auto in=j.input;const auto entry=h.ccr;uint8_t flags=uint8_t((entry&0xd0)|arithmetic(o,entry));
  const auto page=[&]{h.regs[5].er=int32_t(o.pageSource);h.regs[6].er=int32_t(o.pageTarget);};
  const auto notice=[&]{h.regs[5].er=0x55d4;h.regs[6].er=int32_t(s.tick());};
  const auto format=[&]{
   h.regs[2].r=int16_t(o.formatProgram);
   if(!o.formatSourceAfter){h.regs[0].er=int32_t(((in.program+1)&0xffff0000u)|s.index());flags=uint8_t((entry&0xd0)|controlArithmetic::subtract(int16_t(s.index()),int16_t(599),0).flags);return;}
   h.regs[0].er=int32_t(o.writeFlag?0x10000u|o.commitPending:0x16800u);h.regs[1].er=int32_t(o.formatSourceAfter);h.regs[3].e=0;h.regs[4].rl=int8_t(o.formatLastByte);page();flags|=nz32(uint32_t(h.regs[5].er));
  };
  const auto preset=[&]{
   h.regs[0].er=int32_t(o.alias?(o.formatSourceAfter?(o.writeFlag?0x10000u|o.commitPending:0x16800u):0x10000u|o.formatIndex):0x167acu);h.regs[1].er=int32_t(o.alias&&o.formatSourceAfter?o.formatSourceAfter:s.current());h.regs[2].er=int32_t((j.descriptor&0xffff0000u)|o.presetProgram);if(o.formatCopied)h.regs[3].e=0;h.regs[4].er=int32_t(o.formatCopied?(o.packingBase&0xffffff00u)|o.formatLastByte:o.packingBase);h.regs[5].er=0x55e2;h.regs[6].er=int32_t(s.tick());flags|=nz16(uint16_t(s.tick()));
  };
  const auto multi=[&](bool current){
   if(current){h.regs[0].er=0x167aa;h.regs[1].er=0x169fc;h.regs[3].er=int32_t(0x10000u|o.packingProgram);h.regs[4].er=int32_t(o.packingBase);}
   h.regs[2].er=int32_t(0x16800u|o.writeFlag);
   if(!o.writeFlag){notice();flags|=nz16(uint16_t(s.tick()));return;}
   h.regs[0].r=int16_t(o.multiProgram);
   if(!o.multiAccepted){flags=uint8_t((entry&0xd0)|(uint8_t(o.multiProgram>>8)==1?controlArithmetic::subtract(int8_t(o.multiProgram),int8_t(99),0).flags:controlArithmetic::subtract(int8_t(o.multiProgram>>8),int8_t(1),0).flags));return;}
   h.regs[0].er=int32_t(0x10000u|(o.multiTarget&0xff00u));h.regs[1].er=int32_t(o.multiSource+1024);h.regs[3].e=0;h.regs[4].rl=0;page();flags|=4;
  };
  switch(kind){
   case K::FlashProgram:case K::FlashCompare:page();flags|=nz32(uint32_t(h.regs[0].er));break;
   case K::FlashCommit:h.regs[0].r=int16_t(o.commitPending);page();flags|=nz32(uint32_t(h.regs[5].er));break;
   case K::Offset:h.regs[0].er=int32_t(o.writeFlag?0x10000u|o.commitPending:0x16800u);if(o.writeFlag)page();else {h.regs[5].er=int32_t(in.source+256);h.regs[6].er=int32_t(in.target+256);}flags|=nz32(uint32_t(h.regs[5].er));break;
   case K::Check:h.regs[0].er=int32_t(0x16800u|o.checkFlag);if(o.checkFlag)flags|=nz8(o.checkFlag);else{h.regs[0].rl=0;notice();flags|=4;}break;
   case K::Message:notice();flags|=nz16(uint16_t(s.tick()));break;
   case K::GlobalSave:h.regs[0].er=int32_t(0x10000u|o.commitPending);h.regs[1].er=0x167fc;h.regs[2].er=uint16_t(h.regs[2].r)&0xff00;page();flags|=nz32(uint32_t(h.regs[5].er));break;
   case K::FormatDirect:case K::FormatPart:format();break;
   case K::PresetSave:preset();break;
   case K::MultiCurrent:multi(true);break;case K::MultiProvided:multi(false);break;
   case K::CommitMulti:multi(true);h.regs[0].rl=0;notice();flags=uint8_t((flags&0xf1)|4);break;
   case K::CommitSingle:
    if(o.reloaded){h.regs[0].er=int32_t((o.reloadCurrent&0xffffff00u)|o.marker);h.regs[1].er=0;h.regs[2].er=int32_t((o.reloadResolvedBase&0xffff0000u)|o.reloadBounds);h.regs[3].er=int32_t((o.reloadSelectorReference&0xffff0000u)|o.reloadBounds);h.regs[4].er=int32_t((o.reloadExpandedBase&0xffff0000u)|0x8046u);}
    else {preset();h.regs[0].rl=0;}
    notice();flags=uint8_t((entry&0xd0)|arithmetic(o,entry)|nz16(uint16_t(s.tick())));break;
  }
  uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;h.ccr=flags;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
