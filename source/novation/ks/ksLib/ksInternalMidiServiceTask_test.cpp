#include "ksInternalMidiServiceTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::InternalMidiServiceTask;using S=T::Stage;
void check(bool b){if(!b)throw std::runtime_error("Internal MIDI service property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned input=0;input<256;++input)for(unsigned remaining=0;remaining<256;++remaining)for(unsigned flags=0;flags<4;++flags){
  T task(T::Kind::Byte);unsigned count=remaining,cursor=0x30001;unsigned data=999,channel=999,expected=999,status=0x90,handler=0;bool dispatched=false;
  auto read=[&](T::Read field)->uint32_t{switch(field){case T::Read::Input:return input;case T::Read::Length:return 2;case T::Read::Flags:return flags;case T::Read::Remaining:return count;case T::Read::Cursor:return cursor;case T::Read::RunningStatus:return status;case T::Read::Handler:return 0x1234;case T::Read::Channel:return 7;}return 0;};
  auto write=[&](T::Write field,uint32_t value){switch(field){case T::Write::Remaining:count=value;break;case T::Write::Cursor:cursor=value;break;case T::Write::Data:data=value;break;case T::Write::Expected:expected=value;break;case T::Write::Channel:channel=value;break;case T::Write::RunningStatus:status=value;break;default:break;}};
  unsigned steps=0;while(!task.complete()){
   const auto before=task.stage();const auto cost=task.nextClocks();check(cost&&++steps<100);check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before);check(task.advanceBefore(cost,read,write)==cost);++intervals;
   if(before==S::DispatchImmediate||before==S::DispatchMessage){handler=task.handler();dispatched=true;}
   if(task.awaitingChild())check(task.resumeChild(0));
  }
  if(input>=128){check(expected==2);if(flags&2)check(dispatched&&handler==0x1234&&count==remaining);else check(!dispatched&&count==2&&cursor==0x15c13&&status==input&&channel==(input&15));}
  else if(!remaining)check(!dispatched&&data==999&&cursor==0x30001);
  else if(remaining>1)check(!dispatched&&data==input&&count==remaining-1&&cursor==0x30002);
  else check(dispatched&&data==input&&handler==0x1234&&count==((flags&1)?2u:0u)&&cursor==((flags&1)?0x15c13u:0x30001u));
  ++cases;
 }
 std::cout<<"PASS: "<<cases<<" internal MIDI parser policies / "<<intervals<<" intervals; byte/remaining/descriptor flags, immediate and running status, ordered publication and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
