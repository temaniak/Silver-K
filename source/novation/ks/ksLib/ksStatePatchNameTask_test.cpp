#include "ksStatePatchNameTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::StatePatchNameTask;void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;for(unsigned entry=0;entry<9;++entry)for(unsigned seed=0;seed<65536;++seed){T task{T::Entry(entry)};unsigned reads=0,writes=0,clocks=0;const bool reading=entry<3||entry==7,empty=entry==8;std::array<unsigned,17> observed{};
 auto read=[&](T::Read r)->uint32_t{++reads;if(r==T::Read::CurrentBase||r==T::Read::SelectedBase)return 0xdeadbeef;check(task.byteOffset()<16,"Name read beyond16");return (seed*13+task.byteOffset()*17+reads*23)&255;};auto write=[&](T::Write w,uint8_t value){check(task.byteOffset()==writes,"Name write offset differs");check(w==(reading||writes==16?T::Write::Scratch:T::Write::Name),"Name destination differs");if(writes==16)check(value==0,"Terminator differs");observed[writes++]=value;};
 while(!task.complete()){const unsigned storeClocks=seed&3;auto c=task.nextClocks(storeClocks);const auto before=reads,stores=writes;check(!task.advanceBefore(c-1,read,write,storeClocks)&&reads==before&&writes==stores,"Name early observation");check(task.advanceBefore(c,read,write,storeClocks)==c,"Name clock admission");clocks+=c;}
 unsigned expected=empty?17:reading?412:409;if(entry==0)expected+=30;else if(entry==1)expected+=18;else if(entry==3)expected+=30;else if(entry==4)expected+=18;else if(entry==6||entry==7)expected+=21;
 if(!empty)expected-=17*(3-(seed&3));if(clocks!=expected){std::cerr<<"entry="<<entry<<" clocks="<<clocks<<" expected="<<expected<<'\n';throw std::runtime_error("Name clocks differ");}check(writes==(empty?0:17)&&reads==(empty?0:16+unsigned(entry==0||entry==3||entry==6||entry==7)),"Name observation count differs");check(!task.advanceBefore(999,read,write),"Name restarted");++cases;}
 std::cout<<"PASS: "<<cases<<" full names jobs/all9entries,16late reads/stores+terminator,exact read/write deadline and clocks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
