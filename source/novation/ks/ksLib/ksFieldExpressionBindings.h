#pragma once
#include "ksStateFieldService.h"
#include "ksNativeParameterEditService.h"
namespace ks {
// A single data-map cursor transfer. Unknown bits retain their caller value;
// no register/CCR bank, CPU stack, or completion ABI is created or consumed.
struct ExpressionSourceTransfer final {
 uint32_t mask=0,value=0;
 void merge(uint32_t bits,uint32_t next){value=(value&~bits)|(next&bits);mask|=bits;}
 void then(ExpressionSourceTransfer next){merge(next.mask,next.value);}
 uint32_t apply(uint32_t prior)const{return(prior&~mask)|(value&mask);}
};
namespace fieldExpressionBindings {
using K=StateFieldService::Kind;using F=StateFieldActionsTask;
inline ExpressionSourceTransfer full(uint32_t n){return{0xffffffffu,n};}inline ExpressionSourceTransfer word(uint16_t n){return{0xffffu,n};}inline ExpressionSourceTransfer low(uint8_t n){return{0xffu,n};}inline ExpressionSourceTransfer high(uint8_t n){return{0xff00u,uint32_t(n)<<8};}
template<class V>ExpressionSourceTransfer ui(const V&,unsigned=0);
template<class V>ExpressionSourceTransfer fieldRoot(const V&,unsigned=0);
template<class V>ExpressionSourceTransfer label(const V&v){if(!v.summary().label)throw std::logic_error("Missing expression label outcome");const auto&o=*v.summary().label;return full(o.facts.envelopeCopied?0x167e8u:o.task.patch());}
inline ExpressionSourceTransfer applied(const StateApplyService*p){return p&&p->signalled()?full(0x400000):ExpressionSourceTransfer{};}
inline ExpressionSourceTransfer packet(const StateFieldPacketService*p){return p?full(p->address()==0x18?0x400000:0x600000):ExpressionSourceTransfer{};}
template<class V>ExpressionSourceTransfer sceneNode(const V&v,const StateSceneCoordinator::Summary&s,unsigned n,unsigned depth){if(depth>64||n>=s.count)throw std::logic_error("Expression scene outcome depth");using S=StateSceneCoordinator;using C=S::Kind;const auto&o=s.calls[n];const auto&f=o.facts;ExpressionSourceTransfer out;const auto child=[&](C k){for(unsigned j=0;j<o.childCount;++j)if(s.calls[o.children[j]].kind==k){out.then(sceneNode(v,s,o.children[j],depth+1));return;}};const auto notify=[&]{if(f.voice){const auto*p=v.lastVoice();if(!p||!p->global())throw std::logic_error("Missing expression scene voice outcome");if(const auto*d=p->global()->lastDspService())out.then(full(d->port()==DspPort::A?0x400000:0x600000));}};
 if(o.kind==C::Bank||o.kind==C::Map||o.kind==C::Load||o.kind==C::Backup||o.kind==C::Copy||o.kind==C::Restore)return out;
 if(o.kind==C::Activate){if(f.bank){child(C::Load);out.then(low(1));child(C::Apply);}return out;}
 if(o.kind==C::Apply){if(f.parameterUsed)out.then(high(f.lastOld));child(C::Notify);return out;}
 if(o.kind==C::Notify){notify();return out;}
 const bool knob=o.kind==C::Knob;if(knob){if(f.knobMagnitude<4||f.lock)return out;if(f.knobMode){if(!f.bank&&f.ui)out.then(ui(v,depth+1));return out;}}
 if(f.bank==f.oldBank)return out;if(!f.bank){child(C::Restore);child(C::Notify);if(f.ui)out.then(ui(v,depth+1));return out;}if(!f.oldBank)child(C::Map);if(!f.mode){child(C::Restore);child(C::Activate);}if(f.ui)out.then(ui(v,depth+1));return out;
}
template<class V>ExpressionSourceTransfer scene(const V&view,unsigned depth){const auto v=view.sceneView();const auto*s=v.sceneRootSummary();if(!s)throw std::logic_error("Missing expression scene root");return sceneNode(v,*s,0,depth+1);}
template<class V>ExpressionSourceTransfer field(F::Kind kind,const V&v,unsigned depth){if(depth>64)throw std::logic_error("Expression field outcome depth");const auto&entry=v.summary().fields[unsigned(kind)];if(!entry)throw std::logic_error("Missing expression field outcome");const auto&o=*entry;ExpressionSourceTransfer out;const auto event=o.input.event;
 if(kind==F::Kind::Return||kind==F::Kind::Control||kind==F::Kind::Cycle)return out;
 if(kind==F::Kind::Current){if(uint8_t(event>>8))out.then(applied(v.lastApply()));return out;}
 if(kind==F::Kind::DirectStep||kind==F::Kind::IndexedStep){out.then(field(F::Kind::Current,v,depth+1));if(kind==F::Kind::DirectStep&&uint8_t(o.input.pointer-o.task.patch())==225)out.then(packet(v.lastPacket()));return out;}
 if(kind==F::Kind::Bit||kind==F::Kind::Pair){if(uint8_t(event>>8))out.then(applied(v.lastApply()));return out;}
 if(kind==F::Kind::Clear)return ui(v,depth+1);
 if(kind==F::Kind::Global||kind==F::Kind::Selector||kind==F::Kind::Indexed){const bool zero=(o.input.mode>>8)&&!uint8_t(v.summary().bounds->task.value());if(zero&&(uint8_t(o.input.mode)==1||uint8_t(o.input.mode)==14))return out;out.then(applied(v.lastApply()));out.then(word(event));if(!o.task.enabled())return out;out.then(label(v));if(v.summary().timer)out.then(full(0x5006));return out;}
 throw std::logic_error("Unsupported expression field kind");
}
template<class V>ExpressionSourceTransfer fieldRoot(const V&v,unsigned depth){if(depth>64)throw std::logic_error("Expression root outcome depth");const auto kind=v.kind();const auto&s=v.summary();if(kind>=K::IndexedAddress&&kind<=K::GlobalAddress||kind==K::Bounds||kind==K::EditActive||kind==K::Return||kind==K::Publish)return{};if(kind==K::Label)return label(v);if(kind==K::Velocity||kind==K::ApplyVelocity)return packet(v.lastPacket());if(kind>=K::Global&&kind<=K::Control)return field(F::Kind(unsigned(kind)-unsigned(K::Global)),v,depth+1);if(kind==K::DirectEdit&&!uint8_t(v.input().event>>8))return{};
 const auto*w=s.editWorkflow?&*s.editWorkflow:s.workflow?&*s.workflow:nullptr;if(!w)throw std::logic_error("Missing expression workflow");ExpressionSourceTransfer out=word(uint16_t((w->result.automatic?0x100u:0u)|w->result.key));const auto c=w->result.category;if(c==8)out.then(scene(v,depth+1));else if(c==1)out.then(packet(v.lastPacket()));else out.then(field(c==0?F::Kind::Indexed:c==2?F::Kind::DirectStep:c==3?F::Kind::IndexedStep:c==4?F::Kind::Pair:c==5?F::Kind::Selector:c==6?F::Kind::Bit:F::Kind::Global,v,depth+1));return out;
}
template<class V>ExpressionSourceTransfer ui(const V&v,unsigned depth){if(depth>64)throw std::logic_error("Expression UI outcome depth");const auto*r=v.lastRefresh();if(!r)throw std::logic_error("Missing expression UI refresh");if(r->automaticIndex!=~0u)return fieldRoot(v.completedView(r->automaticIndex),depth+1);if(r->rendered){if(!v.lastScreen())throw std::logic_error("Missing expression UI render");return low(0);}return{};}
}
inline ExpressionSourceTransfer fieldUiExpressionTransfer(const StateFieldService&s){if(!s.complete())throw std::logic_error("Incomplete expression UI source");return fieldExpressionBindings::ui(s.projectionView());}
inline ExpressionSourceTransfer parameterEditExpressionTransfer(const NativeParameterEditService&s){if(!s.complete())throw std::logic_error("Incomplete parameter expression source");ExpressionSourceTransfer out;if(s.summary().expand)out=fieldExpressionBindings::full(s.summary().expand->summary().source);if(s.summary().displayed)out.then(fieldUiExpressionTransfer(*s.displayOutcome()));return out;}
struct PanelActivationTransfer final{bool known=false;uint8_t value=0;void then(PanelActivationTransfer v){if(v.known)*this=v;}uint8_t apply(uint8_t prior)const{return known?value:prior;}};
namespace fieldActivationBindings {
using K=StateFieldService::Kind;using F=StateFieldActionsTask;
inline PanelActivationTransfer assigned(uint8_t v){return{true,v};}
inline PanelActivationTransfer displayed(const StateScreenService::DisplayResult&r){const auto&in=r.input;return r.observed&&r.presentation==StateDisplayService::Presentation::Table&&(in.type==38||in.type==39&&in.value)?assigned(in.value/12<2?45:32):PanelActivationTransfer{};}
inline PanelActivationTransfer value(const StateScreenService::ValueResult&r){PanelActivationTransfer out;if(!r.observed||!r.type)return out;out.then(displayed(r.currentDisplay));out.then(displayed(r.current==r.reference?r.matchDisplay:r.referenceDisplay));return out;}
template<class V>PanelActivationTransfer ui(const V&,unsigned=0);
template<class V>PanelActivationTransfer fieldRoot(const V&,unsigned=0);
inline PanelActivationTransfer applied(const StateApplyService*p){return p?assigned(p->cached()):PanelActivationTransfer{};}
template<class V>PanelActivationTransfer sceneNode(const V&v,const StateSceneCoordinator::Summary&s,unsigned n,unsigned depth){if(depth>64||n>=s.count)throw std::logic_error("Activation scene outcome depth");using C=StateSceneCoordinator::Kind;const auto&o=s.calls[n];const auto&f=o.facts;PanelActivationTransfer out;const auto child=[&](C k){for(unsigned j=0;j<o.childCount;++j)if(s.calls[o.children[j]].kind==k){out.then(sceneNode(v,s,o.children[j],depth+1));return;}};
 if(o.kind==C::Bank||o.kind==C::Map||o.kind==C::Load||o.kind==C::Backup||o.kind==C::Copy)return out;if(o.kind==C::Restore){if(f.previous)out=assigned(uint8_t((f.lastEnabled?f.patch+f.lastParameter:f.lastParameter)>>8));return out;}if(o.kind==C::Activate){if(f.bank){child(C::Load);child(C::Apply);}return out;}if(o.kind==C::Apply){out=assigned(uint8_t((f.lastEnabled?f.patch+f.lastParameter:f.lastParameter)>>8));child(C::Notify);return out;}if(o.kind==C::Notify){if(f.voice){const auto*p=v.lastVoice();if(!p||!p->global())throw std::logic_error("Missing activation scene voice outcome");const auto&r=p->global()->summary();const auto&t=*r.time[2];out=assigned(uint8_t((t.mode()?0x159f4u+t.index():r.pair->pair())>>8));if(r.effect)out=assigned(uint8_t(r.effect->second()>>8));}return out;}
 if(o.kind==C::Knob){if(f.knobMagnitude<4||f.lock)return out;if(f.knobMode){if(!f.bank&&f.ui)out.then(ui(v,depth+1));return out;}}if(f.bank==f.oldBank)return out;if(!f.bank){child(C::Restore);child(C::Notify);if(f.ui)out.then(ui(v,depth+1));return out;}if(!f.oldBank)child(C::Map);if(!f.mode){child(C::Restore);child(C::Activate);}if(f.ui)out.then(ui(v,depth+1));return out;
}
template<class V>PanelActivationTransfer scene(const V&view,unsigned depth){const auto v=view.sceneView();const auto*s=v.sceneRootSummary();if(!s)throw std::logic_error("Missing activation scene root");return sceneNode(v,*s,0,depth+1);}
template<class V>PanelActivationTransfer field(F::Kind kind,const V&v,unsigned depth){if(depth>64)throw std::logic_error("Activation field outcome depth");const auto&entry=v.summary().fields[unsigned(kind)];if(!entry)throw std::logic_error("Missing activation field outcome");const auto&o=*entry;PanelActivationTransfer out;const auto event=o.input.event;
 if(kind==F::Kind::Return||kind==F::Kind::Control||kind==F::Kind::Cycle)return out;
 if(kind==F::Kind::Current){out=assigned(uint8_t(event>>8));if(uint8_t(event>>8))out.then(applied(v.lastApply()));return out;}
 if(kind==F::Kind::DirectStep||kind==F::Kind::IndexedStep)return field(F::Kind::Current,v,depth+1);
 if(kind==F::Kind::Bit||kind==F::Kind::Pair){out=assigned(uint8_t(event>>8));if(kind==F::Kind::Pair&&uint8_t(event>>8))out.then(applied(v.lastApply()));return out;}
 if(kind==F::Kind::Clear)return ui(v,depth+1);
 if(kind==F::Kind::Global||kind==F::Kind::Selector||kind==F::Kind::Indexed){out=assigned(uint8_t(o.task.type()));if(v.summary().bounds)out=assigned(uint8_t(v.summary().bounds->task.bounds()>>8));const bool zero=(o.input.mode>>8)&&!uint8_t(v.summary().bounds->task.value());if(zero&&(uint8_t(o.input.mode)==1||uint8_t(o.input.mode)==14))return out;out.then(applied(v.lastApply()));if(o.task.enabled())if(const auto*p=v.lastScreen())out.then(value(p->value));return out;}throw std::logic_error("Unsupported activation field kind");
}
template<class V>PanelActivationTransfer fieldRoot(const V&v,unsigned depth){if(depth>64)throw std::logic_error("Activation field root depth");const auto kind=v.kind();const auto&s=v.summary();if(kind>=K::IndexedAddress&&kind<=K::GlobalAddress){if(s.address->kind!=StateEditAddressTask::Kind::Global)return assigned(uint8_t(s.address->task.base()>>8));return{};}if(kind==K::Bounds)return assigned(uint8_t(s.bounds->task.bounds()>>8));if(kind==K::Label||kind==K::EditActive||kind==K::Return||kind==K::Publish||kind==K::Velocity||kind==K::ApplyVelocity)return{};if(kind>=K::Global&&kind<=K::Control)return field(F::Kind(unsigned(kind)-unsigned(K::Global)),v,depth+1);if(kind==K::DirectEdit&&!uint8_t(v.input().event>>8))return{};
 const auto*w=s.editWorkflow?&*s.editWorkflow:s.workflow?&*s.workflow:nullptr;if(!w)throw std::logic_error("Missing activation workflow");const auto c=w->result.category;if(c==8)return scene(v,depth+1);if(c==1)return{};return field(c==0?F::Kind::Indexed:c==2?F::Kind::DirectStep:c==3?F::Kind::IndexedStep:c==4?F::Kind::Pair:c==5?F::Kind::Selector:c==6?F::Kind::Bit:F::Kind::Global,v,depth+1);
}
template<class V>PanelActivationTransfer ui(const V&v,unsigned depth){if(depth>64)throw std::logic_error("Activation UI outcome depth");const auto*r=v.lastRefresh();if(!r)throw std::logic_error("Missing activation UI refresh");PanelActivationTransfer out;if(r->automaticIndex!=~0u)out=fieldRoot(v.completedView(r->automaticIndex),depth+1);else if(r->rendered){const auto*p=v.lastScreen();if(!p)throw std::logic_error("Missing activation UI render");out=assigned(p->format);out.then(value(p->value));}if(r->partsShown)out=assigned(r->mask);if(r->specialShown&&r->specialMessage)out=assigned(uint8_t((r->table+8)>>8));return out;}
}
inline PanelActivationTransfer fieldUiActivationTransfer(const StateFieldService&s){if(!s.complete())throw std::logic_error("Incomplete activation UI outcome");return fieldActivationBindings::ui(s.projectionView());}
inline PanelActivationTransfer parameterEditActivationTransfer(const NativeParameterEditService&s){if(!s.complete())throw std::logic_error("Incomplete parameter activation outcome");PanelActivationTransfer out;if(s.summary().map&&s.summary().map->summary().map->outcome()!=StateFieldMapTask::Outcome::Unmapped)out={true,0};if(s.summary().displayed)out.then(fieldUiActivationTransfer(*s.displayOutcome()));return out;}

}
