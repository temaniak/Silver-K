#include "ksResetImageTask.h"
#include <vector>
#include <stdexcept>
#include <iostream>
using T=ks::ResetImageTask;using S=T::Stage;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(){try{unsigned cases=0;for(unsigned entry=0;entry<3;++entry)for(unsigned scenario=0;scenario<6;++scenario)for(unsigned seed=0;seed<256;++seed){
 T t(static_cast<T::Entry>(entry));unsigned clear=0,copy=0,words=0,tail=0,overlay=0,markers=0,controls=0,children=0,boundaries=0;uint32_t sum=0;const auto word=[&](unsigned i){return uint16_t(seed*193+i*149);};for(unsigned i=0;i<32;++i)sum+=word(i);const bool marker=scenario==0||scenario==5;const auto stop=[&]{return t.playback()||t.stage()==S::HoldError||t.stage()==S::HoldUpdate;};
 while(!stop()){
  if(t.awaiting()){++children;check(t.resumeChild(),"resume");continue;}
  auto read=[&](S s,unsigned i)->uint32_t{switch(s){case S::ReadProbeMarker:case S::ReadWarmMarker:return marker?0x55:0;case S::ReadBootByte:return uint8_t(seed+i*7);case S::ReadSignature:return uint8_t(seed+i);case S::ReadStoredSignature:return uint8_t(seed+i+(scenario==1&&i==17));case S::ReadImageSize:return scenario==2?0:64;case S::ReadImageWord:return word(i/2);case S::ReadExpectedChecksum:return sum+(scenario==3);case S::ReadOverlayByte:return uint8_t(seed+i*13);case S::SwitchOverlay:return uint8_t(seed);default:throw std::runtime_error("read kind");}};
  auto write=[&](S s,unsigned i,uint32_t v){switch(s){case S::ClearWord:check(i==clear++&&!v,"clear");break;case S::PublishMarker:check(!v&&!markers++,"marker");break;case S::WriteBootByte:check(i==copy++&&v==uint8_t(seed+i*7),"copy");break;case S::WriteImageWord:check(i==words*2&&v==word(words),"words");++words;break;case S::WriteChecksum:check(i==64&&v==sum,"checksum");break;case S::WriteTailWord:check(i==68+tail*2&&!v,"tail");++tail;break;case S::WriteOverlayByte:check(i==overlay++&&v==uint8_t(seed+i*13),"overlay");break;case S::SwitchOverlay:check(v==(seed&254),"overlay bits");++controls;break;case S::PublishOverlayMode:check(v==0xd2,"overlay mode");++controls;break;default:throw std::runtime_error("write kind");}};
  const auto cost=t.nextClocks();check(cost,"cost");const auto stage=t.stage();check(!t.advanceBefore(cost-1,read,write)&&t.stage()==stage,"early admission");check(t.advanceBefore(cost,read,write)==cost,"admission");check(++boundaries<400000,"stalled");
 }
 if(entry==1){check(!clear&&!copy&&!words&&!overlay&&!controls,"warm writes");check(t.stage()==(marker?S::HoldUpdate:S::Playback),"warm handoff");}
 else if(entry==2){check(controls==2&&!clear&&!copy&&!words&&!overlay,"relocated writes");check(t.stage()==(marker?S::HoldUpdate:S::Playback),"relocated handoff");}
 else {check(clear==2048&&markers==1,"cold clear");if(marker){check(copy==16384&&!words&&!tail&&overlay==14&&controls==2&&t.stage()==S::HoldUpdate,"bootcopy");}else if(scenario==1||scenario==2){check(!words&&!tail&&!overlay&&!controls&&t.stage()==S::HoldError,"reject image");}else {check(words==32&&tail==128,"word copy");if(scenario==3)check(!overlay&&!controls&&t.stage()==S::HoldError,"reject checksum");else check(overlay==14&&controls==2&&t.playback(),"verified playback");}}
 check(children>0||entry==1&&!marker||entry==2&&!marker,"children");++cases;
 }
 std::cout<<"PASS: "<<cases<<" independent native reset/image/overlay scenarios, all marker/signature/size/checksum/copy/handoff branches, ordered bytes and early deadlines\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
