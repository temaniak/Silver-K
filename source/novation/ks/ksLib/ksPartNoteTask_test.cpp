#include "ksPartNoteTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::PartNoteTask;using S=T::Stage;using R=T::Read;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;
 auto verify=[&](bool on,unsigned note,unsigned velocity,unsigned low,unsigned high,unsigned policy,unsigned count,unsigned last,unsigned replacement,unsigned preview){
  T task(on?T::Kind::On:T::Kind::Off);std::vector<unsigned> children;unsigned clocks=0,reads=0,previewCalls=0;bool zero=false;
  while(!task.complete()){
   if(task.awaitingControls()){check(task.resumeControls(13),"Control reply rejected");children.push_back(0xa66a);continue;}
   if(task.awaitingPart()){const auto s=task.stage();check(task.resumePart(s==S::OffAwaitPriorityPart?31:17),"Part reply rejected");children.push_back(0x4000);continue;}
   if(task.awaitingChild()){
    switch(task.stage()){
    case S::OnAwaitPreview:children.push_back(0x4d90);++previewCalls;break;case S::OnAwaitState:children.push_back(0x2efc);break;
    case S::OffAwaitState:children.push_back(0x2f92);break;case S::OffAwaitReplacement:children.push_back(0x3118);break;
    case S::OffAwaitStart:case S::OnAwaitStart:children.push_back(0x3642);break;case S::OffAwaitRelease:children.push_back(0x92c0);break;default:throw std::runtime_error("Unexpected child");}
    check(task.resumeChild(),"Child reply rejected");continue;
   }
   auto read=[&](R field)->unsigned{++reads;switch(field){case R::Local:return preview==1||preview==2;case R::Part:return 7;case R::ActivePart:return preview==2?8:7;case R::PreviewGate:return preview==3;
    case R::EventNote:case R::Note:return note;case R::EventVelocity:return velocity;case R::MessageWord:return velocity*256+note;
    case R::Low:check(task.controlPart()==13,"Control binding lost");return low;case R::High:return high;
    case R::Policy:check(task.part()==17,"Policy part binding lost");return policy;case R::Count:check(task.part()==31,"Priority part binding lost");return count;case R::Last:return last;case R::Replacement:return replacement;}return 0;};
   const auto cost=task.nextClocks();check(cost>0,"Part note stalled");const auto before=reads;
   check(!task.advanceBefore(cost-1,read)&&reads==before,"Early part-note observation");check(task.advanceBefore(cost,read)==cost,"Part-note deadline differs");clocks+=cost;
  }
  const bool previewExpected=on&&(preview==1||preview==3),valid=note>=low&&note<=high,releasing=!on||!velocity,allowed=!(policy&4)||(policy&0x60)==0x60;
  std::vector<unsigned> expected;if(previewExpected)expected.push_back(0x4d90);expected.push_back(0xa66a);
  unsigned duration=note<low?79:note>high?85:68;
  if(on)duration+=preview==0?30:preview==2?48:preview==1?111:96;
  if(valid){
   if(on)duration+=17;
   expected.push_back(releasing?0x2f92:0x2efc);expected.push_back(0x4000);duration+=61+(policy&4?9:0);
   if(!allowed)duration+=17;
   else if(!releasing){expected.push_back(0x3642);duration+=37;}
   else{expected.push_back(0x4000);duration+=52;bool replace=count<=1&&last==note;if(count<=1)duration+=21;
    if(replace){expected.push_back(0x3118);duration+=26;replace=replacement<128;}
    expected.push_back(replace?0x3642:0x92c0);duration+=48;
   }
  }
  check(children==expected&&previewCalls==unsigned(previewExpected),"Native part-note child decisions/order differ");
  check(clocks==duration,"Whole part-note own schedule differs");
  check(!task.resumeChild()&&!task.resumePart(0)&&!task.resumeControls(0),"Spurious part-note reply accepted");++cases;
 };
 for(bool on:{false,true})for(unsigned note=0;note<256;++note)for(unsigned low=0;low<256;++low)for(unsigned high:{0u,35u,60u,84u,127u,255u})verify(on,note,192,low,high,4,2,60,0xff,0);
 for(bool on:{false,true})for(unsigned note=0;note<256;++note)for(unsigned policy=0;policy<256;++policy)for(unsigned shape=0;shape<8;++shape)verify(on,note,shape&1?0:192,0,255,policy,shape&2?1:2,shape&4?note:uint8_t(note+1),shape&2?uint8_t(note):0xff,shape%4);
 for(unsigned velocity=0;velocity<256;++velocity)for(unsigned count=0;count<256;++count)verify(true,60,velocity,0,127,0,count,60,uint8_t(velocity+count),0);
 std::cout<<"PASS: "<<cases<<" native part NoteOn/Off range/zero-velocity/arp/mono/preview/return properties; full-byte ranges, policies, count/velocity, exact child order, independent complete own schedules and observation deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
