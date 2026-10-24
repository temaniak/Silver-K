#include "ksStateFieldForeground.h"
#include "ksControllerState.h"
#include "ksStateUiRefreshAbi.h"
#include "ksStateSceneForeground.h"
#include "ksStateSceneEditForeground.h"
#include "ksStateVoiceRefreshForeground.h"
#include <stdexcept>
namespace ks {namespace {
// These combinational helpers run once at the external return. The active
// graph retains only domain observations; interrupt children never borrow
// the caller's registers, stack or flags.
using A=StateFieldForeground::CallerAbi;using C=StateFieldCoordinator;using F=StateFieldActionsTask;using K=StateFieldService::Kind;
uint8_t nz8(uint8_t v){return uint8_t((v&128?8:0)|(!v?4:0));}uint8_t nz16(uint16_t v){return uint8_t((v&0x8000?8:0)|(!v?4:0));}uint8_t nz32(uint32_t v){return uint8_t((v&0x80000000?8:0)|(!v?4:0));}
void low(uint32_t&v,uint8_t b){v=(v&0xffffff00u)|b;}void high(uint32_t&v,uint8_t b){v=(v&0xffff00ffu)|(unsigned(b)<<8);}void word(uint32_t&v,uint16_t b){v=(v&0xffff0000u)|b;}
uint8_t hcSub8(uint8_t a,uint8_t b){return controlArithmetic::subtract(int8_t(a),int8_t(b),0).flags&0x21;}
uint8_t lcdHc(const StateDisplayService&s,uint8_t entry){const auto&r=s.lcdResult();if(!r.observed)return entry&0x21;using P=PanelTextTask::Action;return r.action==P::Write?hcSub8(r.reply,20):r.action==P::Command?hcSub8(r.input,251):0;}
void address(A&a,const C::AddressOutcome&o){a.values[1]=o.task.target();if(o.kind!=StateEditAddressTask::Kind::Global)a.values[2]=o.task.base();a.flags=controlArithmetic::add(int32_t(o.task.target()-o.task.offset()),int32_t(o.task.offset()),a.flags).flags;}
void bounds(A&a,const C::BoundsOutcome&o){const auto type=o.input.request;auto&v=a.values;v[1]=o.task.pointer();word(v[2],uint16_t(o.task.bounds()));word(v[3],uint16_t(o.task.bounds()));uint8_t hc=a.flags&0x21;
 if(type==1||type==2){hc=0;if(!(o.input.mode>>8)){int16_t n=int16_t(int8_t(uint8_t((o.input.event>>8)-(type==1?61:63))))*(type==1?49:101);if(type==2)n=int16_t(uint16_t(n)<<1);hc=controlArithmetic::add(int8_t(uint16_t(n)>>8),int8_t(64),0).flags&0x21;high(v[6],uint8_t(o.task.value()));}}
 else if(type==3||type==19){v[2]=uint16_t(o.task.bounds());v[3]=(o.task.patch()&0xffff0000u)|uint16_t(o.task.bounds());hc=controlArithmetic::add(int32_t(o.input.pointer),int32_t(type==3?o.task.index():o.task.index()*104),0).flags&0x21;}
 else if(type==10||type==21||type==22){hc=0;if(!(o.input.mode>>8))word(v[6],uint16_t(uint8_t(o.input.event>>8)*(type==10?40:type==21?48:64)));}
 else if(type)hc=hcSub8(type,22);a.flags=uint8_t((a.flags&0xd0)|hc|nz16(uint16_t(o.task.bounds())));
}
void apply(A&a,const StateApplyService&s,uint32_t recipe){auto&v=a.values;word(v[2],uint16_t((unsigned(s.cached())<<8)|s.type()));if(s.packed()){v[2]=(recipe&0xffff0000u)|uint16_t(v[2]);v[4]=s.packingBase();}v[3]=s.pickupObserved()?(0x16800u|s.pickup()):s.reference();uint8_t hc=hcSub8(uint8_t(s.field()),218);if(s.signalled()){v[0]=(s.dspRetried()?uint32_t(s.dspRemaining())<<16:s.postBase()&0xffff0000u)|s.signalMask();word(v[4],0x8041);v[5]=0x400000;hc=s.dspRetried()?0:controlArithmetic::subtract(int16_t(0x8040),int16_t(s.dspCached()),0).flags&0x21;}else v[0]=s.enabledObserved()?(s.postBase()&0xffffff00u)|s.postEnabled():s.postBase();a.flags=uint8_t((a.flags&0xd0)|hc|nz32(s.reference()));}
void indicator(A&a,const C::IndicatorOutcome&o){const auto&t=o.task;const auto input=uint8_t(o.input.event>>8);uint8_t hc=0;if(t.mode()==3)hc=uint8_t((input>>4)&1);else if(t.mode()!=1&&t.mode()!=2){hc=hcSub8(input,t.limit());const auto n=std::min(input,t.limit());if(int8_t(n)>0)hc=uint8_t((hc&0x20)|(n>=1&&n<=8&&((uint32_t(o.originalSet)<<(n-1))&128)?1:0));}a.flags=uint8_t((a.flags&0xd0)|hc|nz32(a.values[0]));}
void packet(A&a,const StateFieldPacketService&s){word(a.values[0],uint16_t(s.wire()>>8));if(s.retried())a.values[0]=(uint32_t(s.remaining())<<16)|uint16_t(a.values[0]);word(a.values[4],uint16_t(s.address()+1));a.values[5]=s.address()==0x18?0x400000:0x600000;const auto hc=s.retried()?0:controlArithmetic::subtract(int16_t(s.address()),int16_t(s.cached()),0).flags&0x21;a.flags=uint8_t((a.flags&0xd0)|hc|nz16(uint16_t(s.address()+1)));}
template<class View>void projectField(A&,const View&,StateFieldService::Input,uint32_t,uint32_t);
template<class View>void projectSceneView(A&,const View&);
template<class View>void projectSceneEditView(A&,const View&);
template<class View>void projectUiView(A&a,const View&s){
 const auto*ui=s.lastRefresh();if(!ui)throw std::logic_error("Missing completed UI outcome");
 // Refresh enables the LCD and clears the delta before either edit tail.
 if(ui->automaticIndex!=~0u||ui->rendered){low(a.values[0],ui->editDecision);word(a.values[6],12);a.flags=uint8_t((a.flags&0xd0)|nz8(ui->editDecision));}
 if(ui->automaticIndex!=~0u){const auto child=s.completedView(ui->automaticIndex);projectField(a,child,child.input(),child.handlerBinding(),child.recipeBinding());return;}
 if(ui->rendered){if(!s.lastScreen())throw std::logic_error("Missing completed UI render");StateScreenForeground::projectRender(a,*s.lastScreen());return;}
 if(!projectStateUiRefresh(a,*ui))throw std::logic_error("Unsupported Refresh ABI projection");
}
template<class View>void projectSceneView(A&a,const View&s){
 const auto owned=s.sceneView();const auto*scene=owned.sceneRootSummary();if(!scene)throw std::logic_error("Missing completed Scene outcome");
 StateSceneForeground::project(a,*scene,[&](A&abi,const auto&){const auto*voice=owned.lastVoice();if(!voice)throw std::logic_error("Missing completed Scene voice refresh");StateVoiceRefreshForeground::CallerAbi final;final.values=abi.values;final.flags=abi.flags;StateVoiceRefreshForeground::project(final,*voice);abi.values=final.values;abi.flags=final.flags;},[&](A&abi,const auto&){projectUiView(abi,owned);});
}
template<class View>void projectSceneEditView(A&a,const View&s){
 const auto owned=s.sceneEditView();const auto*edit=owned.sceneEditRootSummary();if(!edit)throw std::logic_error("Missing completed SceneEdit outcome");
 StateSceneEditForeground::project(a,*edit,[&](A&abi,const auto&,auto){projectSceneView(abi,owned);},[&](A&abi,const auto&){projectUiView(abi,owned);},[&](A&abi,const auto&){const auto*d=owned.lastCharacter();if(!d||d->kind()!=StateDisplayService::Kind::Character)throw std::logic_error("Missing completed SceneEdit character");word(abi.values[6],d->output());abi.flags=uint8_t((abi.flags&0xd0)|lcdHc(*d,abi.flags)|nz16(uint16_t(abi.values[0])));});
}
template<class View>void label(A&a,const View&s){const auto&o=*s.summary().label;const auto&f=o.facts;auto&v=a.values;v[0]=0x4100|uint8_t(f.envelopeKind-65);v[5]=o.task.patch();if(f.modulationWord)v[1]=f.modulationWord;if(f.sourceCopied){v[1]=uint16_t(v[1]);low(v[1],f.sourceLastByte);}if(f.envelopeCopied){v[0]=o.result.pointer;v[1]=uint16_t(v[1]);low(v[1],uint8_t(o.task.character()));v[5]=0x167e8;}if(const auto*d=s.lastDisplay()){v[6]=(d->textSource()&0xffff0000u)|d->output();a.flags=uint8_t((a.flags&0xd0)|lcdHc(*d,f.envelopeCopied?0:int8_t(uint8_t(f.envelopeKind-65))<0?hcSub8(f.envelopeKind,65):hcSub8(uint8_t(f.envelopeKind-65),3))|nz32(v[5]));}}
template<class View>const C::FieldOutcome*fieldResult(const View&s,F::Kind k){const auto&o=s.summary().fields[unsigned(k)];return o?&*o:nullptr;}
template<class View>void cycle(A&a,const C::FieldOutcome&o,const View&s){auto&v=a.values;const auto request=o.input.request;if(!request){low(v[6],uint8_t(o.task.control()));if(s.summary().indicatorResult)indicator(a,*s.summary().indicatorResult);return;}low(v[2],uint8_t(o.task.limit()));high(v[6],uint8_t(o.task.value()));const auto incremented=uint8_t(uint8_t(o.input.event>>8)+1);const auto compared=controlArithmetic::subtract(int8_t(incremented),int8_t(o.task.limit()),a.flags).flags;a.flags=incremented<=o.task.limit()?compared:uint8_t((compared&0xf1)|4);}
template<class View>void field(A&a,F::Kind kind,const View&s,uint32_t recipe){const auto*op=fieldResult(s,kind);if(!op)throw std::logic_error("Missing semantic field outcome");const auto&o=*op;auto&v=a.values;const auto savedEvent=o.input.event;const auto pointer=o.input.pointer;
 if(kind==F::Kind::Return)return;
 if(kind==F::Kind::Control){low(v[6],uint8_t(o.task.control()));if(s.summary().indicatorResult)indicator(a,*s.summary().indicatorResult);return;}
 if(kind==F::Kind::Cycle){cycle(a,o,s);return;}
 if(kind==F::Kind::Current){high(v[2],uint8_t(savedEvent>>8));high(v[6],uint8_t(fieldResult(s,F::Kind::Cycle)->input.event>>8));cycle(a,*fieldResult(s,F::Kind::Cycle),s);a.flags=uint8_t((a.flags&0xf1)|nz8(uint8_t(savedEvent>>8)));if(uint8_t(savedEvent>>8)&&s.lastApply())apply(a,*s.lastApply(),recipe);return;}
 if(kind==F::Kind::DirectStep||kind==F::Kind::IndexedStep){if(s.summary().address)address(a,*s.summary().address);field(a,F::Kind::Current,s,recipe);if(kind==F::Kind::DirectStep){const auto patch=o.task.patch();v[0]=patch;v[1]=pointer-patch;a.flags=controlArithmetic::subtract(int8_t(v[1]),int8_t(225),a.flags).flags;if(uint8_t(v[1])==225&&s.lastPacket()){v[1]=(unsigned(fieldResult(s,F::Kind::Current)->task.value())<<8)|uint8_t(v[1]);packet(a,*s.lastPacket());}}return;}
 if(kind==F::Kind::Bit||kind==F::Kind::Pair){if(s.summary().address)address(a,*s.summary().address);v[3]=o.result.selectorReference;v[2]=(o.input.descriptor+o.task.index())&0xffff0000u;word(v[2],uint16_t((unsigned(uint8_t(savedEvent>>8))<<8)|o.result.bit));
 if(kind==F::Kind::Bit){const auto control=uint8_t(o.task.control());const auto*aux=s.lastApply();if(uint8_t(savedEvent>>8)&&aux){high(v[6],uint8_t(s.lastApplyEvent()>>8));low(v[6],control);const auto saved=uint16_t(v[2]);apply(a,*aux,recipe);word(v[2],saved);}low(v[2],0);high(v[6],uint8_t(o.task.value()));low(v[6],control);if(s.summary().indicatorResult)indicator(a,*s.summary().indicatorResult);return;}
 const auto raw=o.result.pairComponent;const auto upper=uint8_t(o.task.value());word(v[3],uint16_t((unsigned(raw)<<8)|o.result.pairUpper));high(v[6],uint8_t(o.task.value()));low(v[6],uint8_t(savedEvent>>8)?o.result.pairUpper:uint8_t(fieldResult(s,F::Kind::Cycle)->task.control()));low(v[2],0);if(uint8_t(savedEvent>>8)&&s.lastApply())apply(a,*s.lastApply(),recipe);else {if(s.summary().indicatorResult)indicator(a,*s.summary().indicatorResult);a.flags=uint8_t((a.flags&0xf1)|4);}(void)upper;return;}
 if(kind==F::Kind::Clear){projectUiView(a,s);return;}
 if(kind==F::Kind::Global)v[1]=0x168fc;
 if(kind==F::Kind::Global||kind==F::Kind::Selector||kind==F::Kind::Indexed){if(s.summary().address)address(a,*s.summary().address);high(v[2],uint8_t(o.task.type()));if(s.summary().bounds)bounds(a,*s.summary().bounds);const bool zero=(o.input.mode>>8)&&!uint8_t(s.summary().bounds->task.value());if(zero&&(uint8_t(o.input.mode)==1||uint8_t(o.input.mode)==14)){low(v[6],uint8_t(fieldResult(s,F::Kind::Control)->task.control()));if(s.summary().indicatorResult)indicator(a,*s.summary().indicatorResult);word(v[6],savedEvent);a.flags=uint8_t((a.flags&0xf1)|nz16(savedEvent));return;}
 high(v[6],uint8_t(o.task.value()));if(o.input.mode>>8)low(v[6],uint8_t(s.summary().bounds->task.value()));if(s.lastApply())apply(a,*s.lastApply(),recipe);word(v[5],savedEvent);v[6]=0x16800|uint8_t(o.task.enabled());a.flags=uint8_t((a.flags&0xf1)|nz8(uint8_t(o.task.enabled())));if(!o.task.enabled())return;
 v[0]=o.task.base();v[1]=pointer-o.task.base();label(a,s);word(v[1],uint16_t(pointer-o.task.base()));if(const auto*screen=s.lastScreen())StateScreenForeground::projectValue(a,screen->value);low(v[0],1);word(v[6],uint16_t(0x6800|o.task.enabled()));low(v[1],s.summary().editActive);if(!uint8_t(v[1]))v[0]=0x16800|uint8_t(o.task.guard());if(s.summary().timer){v[5]=0x5006;v[6]=s.summary().tick;a.flags=uint8_t((a.flags&0xd0)|(controlArithmetic::add(int16_t(uint8_t(o.task.enabled())*8),int16_t(200),0).flags&0x21)|nz16(uint16_t(v[6])));}else a.flags=uint8_t((a.flags&0xf1)|nz8(s.summary().editActive?s.summary().editActive:uint8_t(o.task.guard())));return;}
}
template<class View>void velocity(A&a,const View&s,uint16_t event){const auto curve=uint16_t(unsigned(uint8_t(event>>8))*uint8_t(event>>8)*2);a.values[1]=(uint32_t(curve)<<16)|uint8_t(curve);if(s.lastPacket())packet(a,*s.lastPacket());word(a.values[6],event);a.flags=uint8_t((a.flags&0xf1)|nz16(event));}
template<class View>void projectField(A&a,const View&s,StateFieldService::Input in,uint32_t handler,uint32_t recipe){const auto kind=s.kind();const auto&summary=s.summary();auto&v=a.values;
 if(kind>=K::IndexedAddress&&kind<=K::GlobalAddress){address(a,*summary.address);return;}
 if(kind==K::Bounds){bounds(a,*summary.bounds);return;}
 if(kind==K::Label){label(a,s);return;}
 if(kind==K::EditActive){low(v[1],summary.workflow->task.edit());a.flags=uint8_t((a.flags&0xf1)|nz8(uint8_t(v[1])));return;}
 if(kind==K::Return)return;
 if(kind==K::Velocity||kind==K::ApplyVelocity){velocity(a,s,in.event);return;}
 if(kind==K::Publish){if(summary.published){v[0]=(summary.publishedBase+209)&0xffffff00u;low(v[0],255);v[1]=(v[1]&0xff00)|255;a.flags=uint8_t((a.flags&0xf1)|8);}else{v[0]=0x16800;low(v[0],uint8_t(summary.workflow->task.publish()));a.flags=uint8_t((a.flags&0xf1)|nz8(uint8_t(v[0])));}return;}
 if(kind>=K::Global&&kind<=K::Control){field(a,F::Kind(unsigned(kind)-unsigned(K::Global)),s,recipe);return;}
 if(kind==K::DirectEdit&&!uint8_t(in.event>>8)){word(v[6],in.event);a.flags=uint8_t((a.flags&0xf1)|nz16(in.event));return;}
 const auto&w=summary.editWorkflow?*summary.editWorkflow:*summary.workflow;const auto event=w.input.event;const auto category=w.result.category;v[0]=w.result.descriptor;v[1]=handler;word(v[5],uint16_t((w.result.automatic?0x100:0)|w.result.key));word(v[6],event);
 if(category==8)projectSceneView(a,s);else if(category==1)velocity(a,s,event);else field(a,category==0?F::Kind::Indexed:category==2?F::Kind::DirectStep:category==3?F::Kind::IndexedStep:category==4?F::Kind::Pair:category==5?F::Kind::Selector:category==6?F::Kind::Bit:F::Kind::Global,s,recipe);
 v[0]=0;word(v[6],kind==K::DirectEdit?in.event:event);a.flags=uint8_t((a.flags&0xd0)|nz16(uint16_t(v[6])));
}
}
void StateFieldForeground::project(CallerAbi&a,const StateFieldService&s,StateFieldService::Input in,uint32_t handler,uint32_t recipe){projectField(a,s.projectionView(),in,handler,recipe);}
void StateFieldForeground::projectScene(CallerAbi&a,const StateFieldService&s){projectSceneView(a,s.projectionView());}
void StateFieldForeground::projectSceneEdit(CallerAbi&a,const StateFieldService&s){projectSceneEditView(a,s.projectionView());}
}
namespace ks {namespace {
int resourceHistory(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
int recipeIndex(uint32_t raw){constexpr unsigned roots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)if((raw&0xffffff)==roots[n])return int(n);return -1;}
struct FieldResolver{uint32_t&handler;uint32_t&recipe;StateFieldCoordinator::Handler field(uint8_t category,uint32_t raw){constexpr unsigned targets[]{0x4f4a,0x4f06,0x51d8,0x51f4,0x5252,0x4f22,0x51fe,0x4f0c,0xb554};if(category>=9||raw!=targets[category])throw std::logic_error("Unsupported evolving field handler category");handler=raw;return StateFieldCoordinator::Handler(category);}StatePackFieldsTask::Kind operator()(uint32_t raw){const auto n=recipeIndex(raw);if(n<0)throw std::logic_error("Unsupported evolving field packing descriptor");recipe=raw;return StatePackFieldsTask::Kind(n);}};
bool endpoint(uint32_t a){return StateStoreService::dataSpan(a,1)||(a>=0x800000&&a<0x900000)||(a>=0x1c0&&a<0x300)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffffa5||a==0xfffeb1||a==0xffff51||a==0xffff61||a==0xffff62||(a>=0xffff80&&a<0xffffa0);}
}
bool StateFieldForeground::step(ControllerCpu&h){
 if(!job_){if(!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x17600||!h.interruptMode2||h.indent<=0)return false;constexpr unsigned roots[]{0x4ea4,0x4eaa,0x4eb0,0x4e7c,0x4e8a,0x4f0c,0x4f22,0x4f4a,0x5006,0x51d8,0x51f4,0x51fe,0x5252,0x52e4,0x530e,0x531e,0x5334,0x5012,0x5112,0x52ac,0x52e6,0x52fc,0x4efe,0x4f06,0x4df2};unsigned selected=25;for(unsigned n=0;n<25;++n)if(uint32_t(h.getPC())==roots[n]){selected=n;break;}if(selected==25)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  StateFieldService::Input in;in.descriptor=uint32_t(h.regs[0].er);in.pointer=uint32_t(h.regs[1].er);in.event=uint16_t(h.regs[6].r);in.mode=uint16_t(h.regs[5].r);in.limits=uint16_t(h.regs[2].r);in.request=uint8_t(h.regs[2].rh);in.action.value=0x5006;
  const auto peek=[&](uint32_t raw){const auto a=raw&0xffffff;return a<h.nativeLowSize?h.nativeLowBase[a]:h.memory[a];};if(!StateFieldService::admitted(StateFieldService::Kind(selected),in,peek))return false;
  for(unsigned n=0;n<6;++n){uint32_t raw=0;for(unsigned i=0;i<4;++i)raw=(raw<<8)|peek(71306+4*n+i);if(recipeIndex(raw)<0)return false;}
  job_.emplace(StateFieldService::Kind(selected),in,sp,h.indent,h.exr);++started;++kinds[selected];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto cost=s.nextClocks(h.lastwrite);const auto start=h.cycles;const auto previous=h.lastwrite;FieldResolver resolver{j.handler,j.recipe};
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving field read endpoint");h.cycles=start+dt;h.lastread=resourceHistory(a);return h.readResource(a);},[&](uint32_t raw,uint8_t value,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported evolving field write endpoint");h.cycles=start+dt;h.lastwrite=resourceHistory(a);h.writeResource(a,value);},resolver,previous);
 if(!cost||used!=cost)throw std::logic_error("Native field phase admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s,j.input,j.handler,j.recipe);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;h.exr=j.priority;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
