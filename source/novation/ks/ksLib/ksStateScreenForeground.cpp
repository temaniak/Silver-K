#include "ksStateScreenForeground.h"
#include "ksControllerState.h"
#include <stdexcept>
namespace ks {namespace {
using T=StateScreenService;using K=T::Kind;using Abi=StateScreenForeground::CallerAbi;
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}void word(uint32_t&v,uint16_t w){v=(v&0xffff0000u)|w;}
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
uint8_t lcdArithmetic(const T::DisplayResult&r,uint8_t fallback){if(!r.lcd.observed)return fallback&0x21;using A=PanelTextTask::Action;return r.lcd.action==A::Write?controlArithmetic::subtract(int8_t(r.lcd.reply),int8_t(20),0).flags&0x21:r.lcd.action==A::Command?controlArithmetic::subtract(int8_t(r.lcd.input),int8_t(251),0).flags&0x21:0;}
void projectDisplay(Abi&a,const T::DisplayResult&r){if(!r.observed)return;using P=StateDisplayService::Presentation;auto&v=a.values;const auto entry=a.flags;uint8_t flags=entry&0xd0;const auto&in=r.input;
 if(r.presentation==P::Unknown)flags=controlArithmetic::subtract(int8_t(in.type),int8_t(49),entry).flags;
 else if(r.presentation==P::Table){v[0]=r.cursor;v[1]=r.tableAfterHeader;low(v[2],0);if(in.type==38||(in.type==39&&in.value)){high(v[2],in.value/12<2?45:32);v[3]=r.record;}word(v[6],r.output);flags|=uint8_t(lcdArithmetic(r,0)|4);}
 else if(r.presentation==P::Number){low(v[0],r.number);if(in.type==47)v[1]=(r.patch&0xffffff00u)|r.mode;flags|=uint8_t(lcdArithmetic(r,0)|(r.signedNumber?nz32(v[0]):nz16(uint16_t(v[0]))));}
 else if(r.presentation==P::Disabled||r.presentation==P::Text){uint8_t fallback=r.kind==StateDisplayService::Kind::Off||r.kind==StateDisplayService::Kind::Text?entry&0x21:0;if(r.kind==StateDisplayService::Kind::Format&&in.type==47){v[1]=(r.patch&0xffffff00u)|r.mode;fallback=controlArithmetic::subtract(int8_t(r.mode),int8_t(7),0).flags&0x21;}v[6]=(r.textSource&0xffff0000u)|r.output;flags|=uint8_t(lcdArithmetic(r,fallback)|nz32(v[5]));}
 else if(r.presentation==P::Character){word(v[6],r.output);flags|=uint8_t(lcdArithmetic(r,entry)|nz16(uint16_t(v[0])));}else flags|=uint8_t((entry&0x21)|nz16(uint16_t(v[0])));a.flags=flags;
}
void projectGate(Abi&a,const T::GateResult&g){auto&v=a.values;const auto entry=a.flags;low(v[0],uint8_t(g.accepted));uint8_t hc=entry&0x21;
 if(g.hardware)low(v[1],g.hardware);else {word(v[1],uint16_t((unsigned(g.screen)<<8)|g.page));hc=controlArithmetic::subtract(int16_t(uint16_t(v[1])),int16_t(0x902),0).flags&0x21;if(g.editObserved)low(v[1],g.edit);}a.flags=uint8_t((entry&0xd0)|hc|nz8(uint8_t(v[0])));
}
void projectReset(Abi&a,uint8_t last){a.values[0]=0x816000;a.values[1]=0x159ec;a.values[2]=(a.values[2]&0xff00u)|last;a.flags=uint8_t((a.flags&0xf1)|4);}
void projectLabel(Abi&a,const T::Summary&s){auto&v=a.values;v[0]=s.record+17;low(v[1],s.parameter);high(v[2],s.format);low(v[5],0);word(v[6],s.labelWord);StateScreenForeground::projectValue(a,s.value);}
bool endpoint(uint32_t a){return T::dataSpan(a,1)||(a>=0x800000&&a<0x900000)||(a>=0x400000&&a<0x400010)||a==0xffff51||a==0xffff61||a==0xffff62||a==0xfffeb1||a==0xffffa4||a==0xffffa5||a==0xffffa6||a==0xffff8a;}
int recipe(uint32_t raw){constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return int(n);return-1;}
}
void StateScreenForeground::projectValue(CallerAbi&a,const T::ValueResult&r){if(!r.observed)return;auto&v=a.values;const auto index=uint8_t(r.index);v[1]=index;v[3]=index;const auto pointer=r.base+index;const auto added=controlArithmetic::add(int32_t(r.base),int32_t(index),a.flags);v[0]=(pointer&0xffffff00u)|r.current;v[1]=r.type;a.flags=uint8_t((added.flags&0xf1)|nz8(r.type));if(!r.type)return;
 high(v[1],r.current);low(v[6],145);projectDisplay(a,r.currentDisplay);v[1]=index;
 if(index>=222&&index<234)low(v[0],0);else v[0]=(r.storedPointer&0xffffff00u)|r.reference;
 word(v[1],uint16_t((unsigned(r.current)<<8)|r.type));a.flags=controlArithmetic::subtract(int8_t(r.current),int8_t(r.reference),a.flags).flags;
 if(r.current==r.reference)projectDisplay(a,r.matchDisplay);else {low(v[6],209);projectDisplay(a,r.referenceDisplay);}
}
void StateScreenForeground::projectRender(CallerAbi&a,const T::Summary&s){projectDisplay(a,s.heading);a.values[1]=s.parameter;projectLabel(a,s);a.flags=uint8_t((a.flags&0xd0)|nz32(a.values[0]));}
bool StateScreenForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17530||!h.interruptMode2||h.indent<=0)return false;K kind;switch(h.getPC()){case 0x582a:kind=K::Descriptor;break;case 0x58ae:kind=K::Set;break;case 0x58ce:kind=K::Next;break;case 0x58e4:kind=K::Previous;break;case 0x5900:kind=K::Refresh;break;case 0x590a:kind=K::Gate;break;case 0x5932:kind=K::Render;break;case 0x5976:kind=K::Label;break;case 0x59a6:kind=K::Edit;break;case 0x5a90:kind=K::Value;break;default:return false;}const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  T::Input in;in.index=uint8_t(kind==K::Set?h.regs[6].rl:h.regs[0].rl);in.format=uint8_t(h.regs[2].rh);in.character=uint16_t(h.regs[6].r);in.valueIndex=uint32_t(h.regs[1].er);const auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};if(!T::admitted(kind,in,peek))return false;if(kind==K::Edit){const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek(a+n));return v;};if(!T::dataSpan(wide(0x167c0),256))return false;for(unsigned n=0;n<6;++n)if(recipe(wide(71306+4*n))<0)return false;}job_.emplace(kind,in,sp,h.indent,h.exr);++started;++kinds[unsigned(kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned offset){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving screen read resource");h.cycles=start+offset;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned offset){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving screen write resource");h.cycles=start+offset;h.lastwrite=history(a);h.writeResource(a,v);},[](uint32_t raw){const auto index=recipe(raw);if(index<0)throw std::logic_error("Unsupported evolving screen packing descriptor");return StatePackFieldsTask::Kind(index);},previous);
 if(!cost||used!=cost)throw std::logic_error("Screen phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;const auto&o=s.summary();const auto kind=s.kind();
  if(kind==K::Descriptor){const auto&d=o.descriptor;a.values[0]=d.pointer;if(d.sentinel)a.values[1]=d.patch;word(a.values[1],uint16_t((unsigned(d.count)<<8)|d.page));uint8_t hc=controlArithmetic::subtract(int8_t(d.header),int8_t(255),0).flags&0x21;if(d.sentinel)hc=d.dynamic?controlArithmetic::subtract(int8_t(d.dynamicPage),int8_t(d.dynamicLimit),0).flags&0x21:controlArithmetic::subtract(int8_t(d.variant),int8_t(6),0).flags&0x21;a.flags=uint8_t((a.flags&0xd0)|hc|nz8(d.page));}
  else if(kind==K::Gate||kind==K::Refresh){projectGate(a,o.gate);if(o.reset)projectReset(a,o.resetLast);}
  else if(kind==K::Value)projectValue(a,o.value);
  else if(kind==K::Edit){if(o.apply.observed){a.values[2]=(o.editBase&0xffff0000u)|(unsigned(o.apply.cached)<<8)|o.apply.type;if(o.apply.packed){a.values[2]=(o.apply.packingDescriptor&0xffff0000u)|uint16_t(a.values[2]);a.values[4]=o.apply.packingBase;}if(o.apply.signalled){word(a.values[4],0x8041);a.values[5]=0x400000;}}projectLabel(a,o);a.values[6]=0x10000u|(unsigned(uint8_t((64u-o.level)*2+64))<<8)|o.level;a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::add(int8_t(uint8_t((64u-o.level)*2)),int8_t(64),0).flags&0xfdu));}
  else {if(kind==K::Set)a.values[2]=0x16800u|o.setReset;if(kind==K::Label)projectLabel(a,o);else projectRender(a,o);if(kind==K::Set||kind==K::Next||kind==K::Previous){projectGate(a,o.gate);if(o.reset)projectReset(a,o.resetLast);}}
  for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;
 }
 h.cycles=start+cost;return true;
}
}
