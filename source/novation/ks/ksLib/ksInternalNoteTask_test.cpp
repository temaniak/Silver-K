#include "ksInternalNoteTask.h"
#include "ksArpEventTask.h"
#include <array>
#include <vector>
#include <tuple>
#include <iostream>
#include <stdexcept>
using T=ks::InternalNoteTask;using K=T::Kind;using S=T::Stage;using C=T::Child;using Write=std::pair<unsigned,unsigned>;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;auto verify=[&](K kind,unsigned note,unsigned velocity,unsigned bitmap,unsigned position,unsigned policy,unsigned latency){
 T t(kind,17,uint8_t(note),0xffffffffu);std::vector<C>children;std::vector<Write>writes;std::vector<unsigned>midi;std::array<unsigned,32>bits{};bits[position%32]=bitmap;unsigned reads=0,loops=0;while(!t.complete()&&!t.handoff()){
  check(++loops<2000,"Internal note pure stalled");if(t.awaitingChild()){children.push_back(t.child());if(t.child()==C::Part)check(t.resumePart(199),"Part reply rejected");else{if(t.child()==C::Midi){check(t.velocity()==64,"Release velocity differs");midi.push_back(t.note());}if(t.child()==C::Set||t.child()==C::Clear)check(t.note()==note,"Bitmap child note differs");check(t.resumeChild(),"Note child reply rejected");}continue;}
  auto read=[&](S s)->unsigned{++reads;switch(s){case S::OnReadNote:case S::OffReadNote:check(t.payload()==0xffffffffu,"Input note cursor differs");return note;case S::OnReadVelocity:case S::OffReadVelocity:check(t.payload()==0,"Input velocity cursor wrap differs");return velocity;case S::SetModify:case S::ClearModify:check(t.group()==note/8,"Bitmap byte group differs");return bits[t.group()];case S::ReleaseReadPolicy:return policy;case S::ReleaseReadByte:return bits[t.cursor()];default:throw std::runtime_error("Unexpected note pure read");}};auto write=[&](S,unsigned data){writes.emplace_back(t.stage()==S::EmptyTickCount?t.cursor():t.group(),data);};auto timing=[&](S){return latency;};const auto cost=t.nextClocks(t.publication()?latency:3);check(cost>0,"Internal note deadline stalled");const auto before=reads+writes.size();check(!t.advanceBefore(cost-1,read,write,timing)&&reads+writes.size()==before,"Internal note early access");check(t.advanceBefore(cost,read,write,timing)==cost,"Internal note deadline rejected");
 }
 std::vector<C>ec;std::vector<Write>ew;std::vector<unsigned>em;if(kind==K::On||kind==K::Off){ec.push_back(C::Part);ec.push_back(kind==K::On&&velocity?C::Set:C::Clear);check(t.stage()==(kind==K::On&&velocity?S::HandoffOn:S::HandoffOff),"Zero velocity handoff differs");check(t.note()==note&&t.velocity()==velocity,"Note packet differs");}
 else if(kind==K::Set||kind==K::Clear){const unsigned value=bits[note/8],mask=1u<<(note&7);ew.emplace_back(note/8,kind==K::Set?value|mask:value&~mask);}
 else if(kind==K::Empty){ec.push_back(C::Part);for(unsigned i=0;i<16;++i)ew.emplace_back(15-i,0);}
 else{ec.push_back(C::Part);if(policy&96)for(unsigned i=0;i<128;++i)if(bits[i/8]&(1u<<(i&7))){ec.push_back(C::Midi);em.push_back(i);}ec.push_back(C::Empty);}
 check(writes==ew,"Bitmap write sequence differs");check(children==ec,"Internal note child sequence differs");check(midi==em,"Release MIDI note sequence differs");++cases;
 };
 for(unsigned latency:{0u,3u}){
  for(unsigned note=0;note<256;++note)for(unsigned velocity=0;velocity<256;++velocity)for(auto kind:{K::On,K::Off})verify(kind,note,velocity,0,0,0,latency);
  for(unsigned note=0;note<256;++note)for(unsigned bitmap=0;bitmap<256;++bitmap)for(auto kind:{K::Set,K::Clear})verify(kind,note,0,bitmap,note/8,0,latency);
  for(unsigned bitmap=0;bitmap<256;++bitmap)for(unsigned position=0;position<16;++position)for(unsigned policy:{0u,32u,64u,96u})verify(K::Release,60,64,bitmap,position,policy,latency);
  verify(K::Empty,0,0,255,0,0,latency);
 }
 // Native typed continuation starts after a caller's logical retained note;
 // it neither transposes nor applies the arpeggiator's velocity override.
 for(unsigned note=0;note<256;++note)for(unsigned mask:{0u,1u})for(unsigned policy:{0u,32u,64u,96u}){using E=ks::ArpEventTask;using ES=E::Stage;using EC=E::Child;E e(E::Kind::RoutedEmit,17,uint8_t(note),64);std::vector<EC>calls;unsigned publishes=0;while(!e.complete()){if(e.awaitingChild()){calls.push_back(e.child());if(e.child()==EC::MidiOn)check(e.resumeMidi(uint8_t(policy)),"Routed MIDI reply differs");else if(e.child()==EC::Append)throw std::runtime_error("Routed note requeued");else check(e.resumeChild(),"Routed play rejected");continue;}auto read=[&](ES s)->unsigned{if(s==ES::EmitReadMask)return mask;if(s==ES::EmitReadPolicy)return policy;throw std::runtime_error("Routed note reread unrelated field");};auto write=[&](ES s,unsigned v){check(s==ES::EmitPublishWork&&v==1,"Routed work differs");++publishes;};const auto cost=e.nextClocks();check(e.advanceBefore(cost,read,write,[](ES){return 3u;})==cost,"Routed deadline differs");}std::vector<EC>expected;if(mask&1){if(policy)expected.push_back(EC::MidiOn);if(policy==0||policy==64)expected.push_back(EC::Play);}check(calls==expected&&publishes==(mask&1)&&e.playedNote()==note&&e.velocity()==64,"Routed note domain differs");++cases;}
 std::cout<<"PASS: "<<cases<<" pure internal note/bitmap/release and routed-emit tasks, full bytes/payload wrap, bitmap RMW, descending clear, ascending release notes and typed routing/deadlines\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
