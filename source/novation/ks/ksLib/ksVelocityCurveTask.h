#pragma once
#include <cstdint>
namespace ks {
// Original integer velocity shaping: repeat the admitted quantized product,
// choose inversion/square/mixed or a live threshold, and preserve wrap/clamp.
// Threshold input is observed only at its scheduled read. No CPU state.
class VelocityCurveTask final {
public:
 enum class Stage {CaptureBase,PowerTick,PowerGate,PowerSeed,PowerProduct,PowerScale,PowerRepeat,
  InverseSeed,Invert,TestCurve,CurveGate,TickOne,GateOne,UseInverse,TickTwo,GateTwo,
  SquareSeed,SquareProduct,SquareScale,TickThree,GateThree,MixedProduct,MixedScale,
  TickFour,NormalizeCurve,NormalizeWide,ResolveThreshold,ReadThreshold,TestParity,ParityGate,
  EvenCompare,EvenGate,OddCompare,OddGate,Suppress,Complete};
 enum class End {Normal,Inverted,Square,Mixed,EvenKept,OddKept,Suppressed,Empty};
 struct Advance {unsigned clocks=0,boundaries=0;};
 VelocityCurveTask(uint8_t velocity,uint8_t curve,uint8_t power):base_(velocity),current_(velocity),curve_(curve),power_(power){}
 static VelocityCurveTask empty(){VelocityCurveTask task(0,0,0);task.stage_=Stage::Complete;task.end_=End::Empty;return task;}
 Stage stage()const{return stage_;}End ending()const{return end_;}
 uint8_t base()const{return base_;}uint8_t velocity()const{return current_;}uint8_t inverse()const{return inverse_;}
 uint8_t curve()const{return curve_;}uint8_t power()const{return power_;}uint8_t threshold()const{return threshold_;}
 uint16_t product()const{return product_;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{switch(stage_){case Stage::PowerProduct:case Stage::SquareProduct:case Stage::MixedProduct:return 15;
  case Stage::ResolveThreshold:return 9;case Stage::ReadThreshold:return 6;case Stage::Complete:return 0;default:return 3;}}
 template<class Threshold>Advance advanceBefore(unsigned available,Threshold&& threshold){
  Advance p;while(const auto cost=nextClocks()){
   if(cost>available)break;
   switch(stage_){
   case Stage::CaptureBase:stage_=Stage::PowerTick;break;
   case Stage::PowerTick:--power_;stage_=Stage::PowerGate;break;
   case Stage::PowerGate:stage_=power_?Stage::PowerSeed:Stage::InverseSeed;break;
   case Stage::PowerSeed:stage_=Stage::PowerProduct;break;
   case Stage::PowerProduct:product_=uint16_t(base_)*current_;stage_=Stage::PowerScale;break;
   case Stage::PowerScale:scale();stage_=Stage::PowerRepeat;break;
   case Stage::PowerRepeat:stage_=Stage::PowerTick;break;
   case Stage::InverseSeed:stage_=Stage::Invert;break;
   case Stage::Invert:inverse_=current_^0x7f;stage_=Stage::TestCurve;break;
   case Stage::TestCurve:stage_=Stage::CurveGate;break;
   case Stage::CurveGate:if(curve_)stage_=Stage::TickOne;else finish(End::Normal);break;
   case Stage::TickOne:--curve_;stage_=Stage::GateOne;break;
   case Stage::GateOne:stage_=curve_?Stage::TickTwo:Stage::UseInverse;break;
   case Stage::UseInverse:current_=inverse_;finish(End::Inverted);break;
   case Stage::TickTwo:--curve_;stage_=Stage::GateTwo;break;
   case Stage::GateTwo:stage_=curve_?Stage::TickThree:Stage::SquareSeed;break;
   case Stage::SquareSeed:stage_=Stage::SquareProduct;break;
   case Stage::SquareProduct:product_=uint16_t(current_)*current_;stage_=Stage::SquareScale;break;
   case Stage::SquareScale:scale();finish(End::Square);break;
   case Stage::TickThree:--curve_;stage_=Stage::GateThree;break;
   case Stage::GateThree:stage_=curve_?Stage::TickFour:Stage::MixedProduct;break;
   case Stage::MixedProduct:product_=uint16_t(current_)*inverse_;stage_=Stage::MixedScale;break;
   case Stage::MixedScale:scale();finish(End::Mixed);break;
   case Stage::TickFour:--curve_;stage_=Stage::NormalizeCurve;break;
   case Stage::NormalizeCurve:stage_=Stage::NormalizeWide;break;
   case Stage::NormalizeWide:stage_=Stage::ResolveThreshold;break;
   case Stage::ResolveThreshold:stage_=Stage::ReadThreshold;break;
   case Stage::ReadThreshold:threshold_=uint8_t(threshold(curve_));stage_=Stage::TestParity;break;
   case Stage::TestParity:stage_=Stage::ParityGate;break;
   case Stage::ParityGate:stage_=threshold_&1?Stage::OddCompare:Stage::EvenCompare;break;
   case Stage::EvenCompare:stage_=Stage::EvenGate;break;
   case Stage::EvenGate:if(base_>threshold_)stage_=Stage::Suppress;else finish(End::EvenKept);break;
   case Stage::OddCompare:stage_=Stage::OddGate;break;
   case Stage::OddGate:if(base_<threshold_)stage_=Stage::Suppress;else finish(End::OddKept);break;
   case Stage::Suppress:current_=0x80;finish(End::Suppressed);break;case Stage::Complete:break;
   }
   available-=cost;p.clocks+=cost;++p.boundaries;
  }return p;
 }
private:
 void scale(){product_=uint16_t(product_<<1);current_=uint8_t(product_>>8);}
 void finish(End end){end_=end;stage_=Stage::Complete;}
 uint8_t base_=0,current_=0,inverse_=0,curve_=0,power_=0,threshold_=0;uint16_t product_=0;
 Stage stage_=Stage::CaptureBase;End end_=End::Normal;
};
}
