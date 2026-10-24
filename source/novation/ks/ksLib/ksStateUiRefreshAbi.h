#pragma once
#include "ksStateUiRefreshService.h"
#include "ksStateDisplayService.h"
#include "ksStateScreenForeground.h"
#include "ksControlArithmetic.h"
namespace ks {
// Combinational projection at the external return only. The UI graph retains
// presentation and resource results, never this machine boundary view.
template<class CallerAbi>bool projectStateUiRefreshOrdinary(CallerAbi&a,const StateUiRefreshService::Summary&s,const StateDisplayService::LcdResult&lcd){
 if(s.kind!=StateUiRefreshService::Kind::Refresh||(!s.programShown&&!s.bankShown&&!s.partsShown))return false;
 auto&v=a.values;const auto low=[](uint32_t&x,uint8_t b){x=(x&0xffffff00u)|b;};const auto word=[](uint32_t&x,uint16_t w){x=(x&0xffff0000u)|w;};
 if(s.categoryShown){v[0]=s.categoryEnd;word(v[1],0);}
 if(s.nameRead){v[0]=s.nameSourceEnd;v[1]=0x1734c;v[2]&=0xff00u;}
 if(s.headingObserved)v[1]=s.heading;
 if(s.headingModeObserved)v[0]=(v[0]&0xffff00ffu)|(unsigned(s.headingModeValue)<<8);
 if(s.programNumberObserved)word(v[0],s.programWord);
 low(v[0],s.bankShown&&!s.partsShown?s.performancePage:0);
 if(s.partsShown){v[1]=0x167b8;word(v[2],uint16_t(unsigned(s.mask)<<8));if(s.partNumberObserved)low(v[0],s.lastPartNumber);}
 if(s.sourceObserved)v[6]=(s.source&0xffff0000u)|s.character;else word(v[6],s.character);
 using A=PanelTextTask::Action;uint8_t hc=a.flags&0x21;if(lcd.observed)hc=lcd.action==A::Write?controlArithmetic::subtract(int8_t(lcd.reply),int8_t(20),0).flags&0x21:lcd.action==A::Command?controlArithmetic::subtract(int8_t(lcd.input),int8_t(251),0).flags&0x21:0;
 const auto w=uint16_t(v[0]);a.flags=uint8_t((a.flags&0xd0)|hc|(s.partsShown?uint8_t((w&0x8000?8:0)|(!w?4:0)):uint8_t((uint8_t(v[0])&128?8:0)|(!uint8_t(v[0])?4:0))));return true;
}
template<class Abi>void projectStateUiRowDisplay(Abi&a,const StateUiRefreshService::Presentation&r){
 if(!r.observed)return;auto&v=a.values;using P=StateDisplayService::Presentation;const auto low=[](uint32_t&x,uint8_t b){x=(x&0xffffff00u)|b;};const auto high=[](uint32_t&x,uint8_t b){x=(x&0xffff00ffu)|(unsigned(b)<<8);};const auto word=[](uint32_t&x,uint16_t w){x=(x&0xffff0000u)|w;};
 if(r.form==P::Table){v[0]=r.cursor;v[1]=r.tableAfterHeader;low(v[2],0);if(r.input.type==38||(r.input.type==39&&r.input.value)){high(v[2],r.input.value/12<2?45:32);v[3]=r.record;}word(v[6],r.output);}
 else if(r.form==P::Number){low(v[0],r.number);if(r.input.type==47)v[1]=(r.patch&0xffffff00u)|r.mode;}
 else if(r.form==P::Text||r.form==P::Disabled){if(r.kind==StateDisplayService::Kind::Format&&r.input.type==47)v[1]=(r.patch&0xffffff00u)|r.mode;v[6]=(r.textSource&0xffff0000u)|r.output;}
 else if(r.form==P::Character)word(v[6],r.output);
}
template<class CallerAbi>bool projectStateUiRefreshSpecial(CallerAbi&a,const StateUiRefreshService::Summary&s){
 if(!s.specialShown)return false;auto&v=a.values;const auto low=[](uint32_t&x,uint8_t b){x=(x&0xffffff00u)|b;};const auto word=[](uint32_t&x,uint16_t w){x=(x&0xffff0000u)|w;};
 const auto hc=[](const StateDisplayService::LcdResult&lcd,uint8_t fallback){using A=PanelTextTask::Action;if(!lcd.observed)return uint8_t(fallback&0x21);return uint8_t(lcd.action==A::Write?controlArithmetic::subtract(int8_t(lcd.reply),int8_t(20),0).flags&0x21:lcd.action==A::Command?controlArithmetic::subtract(int8_t(lcd.input),int8_t(251),0).flags&0x21:0);};
 if(s.specialMessage){
  for(unsigned row=0;row<4;++row){const auto&r=s.row[row];v[1]=r.program;v[2]=s.table+row*2+2;if(r.bank){low(v[0],uint8_t(r.bank-1));v[1]=r.lookup;projectStateUiRowDisplay(a,r.display);}}
  v[0]=0x1141f;v[2]=s.table+8;a.flags&=0xd0;
 }else{
  v[0]=s.selected;low(v[1],s.lateKind);
  if(s.nameRead){v[0]=s.nameSourceEnd;v[1]=0x1734c;v[2]&=0xff00u;}
  if(s.lateKind<2){const auto arithmetic=hc(s.lastDisplay.lcd,s.lateKind?0:0x21);a.flags=uint8_t((a.flags&0xd0)|arithmetic|(v[5]&0x80000000u?8:0)|(!v[5]?4:0));}
  else {const auto w=uint16_t(v[0]);a.flags=uint8_t((a.flags&0xd0)|hc(s.lastDisplay.lcd,0)|(w&0x8000?8:0)|(!w?4:0));}
 }
 if(s.sourceObserved)v[6]=(s.source&0xffff0000u)|s.character;else word(v[6],s.character);return true;
}

// Dispatch only completed semantic outcomes at the one outer return edge.
// A render tail is supplied explicitly by the owning graph; direct automatic
// edit recursion has no projection in this admitted Clear -> Refresh scope.
template<class CallerAbi>bool projectStateUiRefresh(CallerAbi&a,const StateUiRefreshService::Summary&s,const StateScreenService::Summary*render=nullptr){
 if(render){if(s.kind!=StateUiRefreshService::Kind::Refresh||s.specialShown||s.programShown||s.bankShown||s.partsShown)return false;StateScreenForeground::CallerAbi final;final.values=a.values;final.flags=a.flags;StateScreenForeground::projectRender(final,*render);a.values=final.values;a.flags=final.flags;return true;}
 if(s.specialShown)return projectStateUiRefreshSpecial(a,s);
 return projectStateUiRefreshOrdinary(a,s,s.lcd);
}

}
