#include "ksStateTickService.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace{bool tracking=false;uint64_t allocations=0;void check(bool b,const char*m){if(!b)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace{using T=ks::StateTickService;using Event=std::tuple<bool,uint32_t,uint8_t,unsigned>;
int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
struct Resolver{bool operator()(uint8_t c,uint32_t raw)const{return schema(c,raw);}bool schema(uint8_t c,uint32_t raw)const{return c<9&&raw==c;}ks::StateFieldCoordinator::Handler field(uint8_t category,uint32_t id)const{check(category<9&&id==category,"Numeric handler binding");return ks::StateFieldCoordinator::Handler(category);}ks::StatePackFieldsTask::Kind operator()(uint32_t id)const{check(id<6,"Numeric packing binding");return ks::StatePackFieldsTask::Kind(id);}};
struct Resource{std::vector<uint8_t>m=std::vector<uint8_t>(1<<24);std::array<Event,8>events{};unsigned size=0,cost=0,lastTime=0;int previous=-1;void byte(uint32_t a,uint8_t v){m[a&0xffffff]=v;}void wide(uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)byte(a+n,uint8_t(v>>(24-8*n)));}void word(uint32_t a,uint16_t v){byte(a,v>>8);byte(a+1,v);}void see(bool write,uint32_t a,uint8_t v,unsigned dt){check(a<m.size()&&dt&&dt<=cost&&(!size||dt>=lastTime)&&size<events.size(),"Pure navigation callback address/order/time");events[size++]={write,a,v,dt};lastTime=dt;}uint8_t read(uint32_t a,unsigned dt){check(a<0xfff000||a>=0xfffc00,"Pure navigation observed a CPU continuation frame");const auto v=a==0x400004||a==0x600004?uint8_t(255):m[a];see(false,a,v,dt);return v;}void write(uint32_t a,uint8_t v,unsigned dt){check(a<0xfff000||a>=0xfffc00,"Pure navigation wrote a CPU continuation frame");see(true,a,v,dt);m[a]=v;previous=history(a);}void phase(unsigned c){size=lastTime=0;cost=c;}};
struct Config{unsigned kind=0,seed=0,key=7,sceneMode=0,row=1;bool backward=false;uint16_t event=0x010b;bool pressed=true;uint8_t page=0,edit=0,screen=11,hardware=1;uint32_t source=0x30000,live=0x34000;bool sceneNested=false,evolving=false;uint8_t mode=1,multi=0,special=0,selected=1,enabled=1;};
void seed(Resource&r,const Config&c){std::fill(r.m.begin(),r.m.end(),0);r.previous=-1;r.wide(0x167bc,c.source);r.wide(0x167c0,c.live);r.wide(0x167a6,0);r.wide(0x167f0,0x40002);r.byte(0x16790,c.page);r.byte(0x16784,c.edit);r.byte(0x16785,c.screen);r.byte(0x16787,c.hardware?0:2);r.byte(0x16792,1);r.byte(0x167fb,2);r.byte(0x167ba,0);r.byte(0x167ac,1);r.byte(0x167ad,3);r.byte(0xffec14,c.hardware);r.wide(0xffec10,12345);r.byte(0x169ed,127);r.byte(0x169f7,0);r.byte(0x169f9,0);r.byte(0xfffeb1,255);r.byte(0xffff51,0);r.byte(0xffff62,255);r.word(0xffed6c,0);r.word(0xffed6e,0);r.byte(0xffed6d,45);r.byte(0xffed6f,17);r.word(0x159f2,0x1234);
 for(unsigned n=0;n<256;++n){r.byte(c.source+n,uint8_t(c.seed+n));r.byte(c.live+n,uint8_t(c.seed+n+1));r.byte(71586+n,127);r.byte(68974+n,0);r.word(0x15200+2*n,uint16_t(n*127));r.byte(0x816000+n,uint8_t(c.seed+n));}for(unsigned n:{0u,139u,222u,223u,224u,225u,226u})r.byte(c.source+n,0);
 for(unsigned n=0;n<78;++n){const auto d=0x40000+n*0x1000;r.wide(0x101ae + 4*n,d);r.byte(d,1);r.byte(d+1,0);for(unsigned item=0;item<256;++item){const auto at=d+2+20*item;for(unsigned k=0;k<16;++k)r.byte(at+k,uint8_t(65+k));r.byte(at+16,10);r.byte(at+18,0);r.byte(at+19,127);}}
 for(unsigned n=0;n<47;++n){r.byte(0xfd66+8*n,0);r.byte(0xfd68+8*n,255);r.byte(0xfd69+8*n,3);r.byte(0xfd6c+8*n,0);r.byte(0xfd6d+8*n,1);}for(unsigned n=0;n<9;++n)r.wide(0x1013e + 4*n,n);for(unsigned n=0;n<6;++n)r.wide(71306+4*n,n);for(const auto&schema:ks::StateReloadPanelService::schema)for(unsigned i=0;i<8;++i)r.byte(0xfd66+8*schema.key+i,schema.bytes[i]);for(unsigned n=0;n<48;++n){r.byte(0x1c0+5*n,uint8_t(n%4));r.byte(0x1c1+5*n,1);r.byte(0x1c2+5*n,255);r.byte(0x1c3+5*n,7);r.byte(0x1c4+5*n,uint8_t(n%4));}
 for(unsigned n=0;n<16;++n)r.byte(0x1733c+n,uint8_t(65+n));r.byte(0x1734c,0);
 if(true){
  for(unsigned slot=0;slot<256;++slot){const auto at=0x17560+20*slot;r.wide(at,c.source+256*(slot%4));r.byte(at+4,uint8_t(slot%4));r.word(at+8,uint16_t(slot<4?1:0));r.byte(at+6,uint8_t(c.seed));r.byte(at+12,uint8_t(c.seed&7));}
  for(unsigned part=0;part<5;++part){r.wide(0x174ae +20*part,c.source+256*part);for(unsigned n=4;n<20;++n)r.byte(0x174ae +20*part+n,uint8_t(c.seed+n));for(unsigned n=0;n<256;++n)r.byte(c.source+256*part+n,uint8_t((c.seed*19+n*3)%128));for(unsigned n:{0u,139u,222u,223u,224u,225u,226u})r.byte(c.source+256*part+n,0);r.byte(c.source+256*part+73,uint8_t(c.seed%3));r.byte(c.source+256*part+76,uint8_t(c.seed%4));}
  r.byte(0x17526,uint8_t(c.seed%4));r.byte(0x16798,3);r.byte(0x167ba,0);r.byte(0x174a6,uint8_t(c.seed%8));r.byte(0x1755e,uint8_t(c.seed&1?255:0));r.word(0x15138,0x4470);r.word(0x1512e,1);r.word(0x159f2,0xffff);r.wide(0x1748e,0x36000);r.byte(0x17492,1);r.byte(0x17493,0);r.byte(0x1679d,64);r.byte(0x1679e,0);r.byte(0x1679f,0);r.byte(0x167a0,0);r.byte(0xfd66+8*28,8);r.byte(0xffec22,0);r.byte(0xffff62,0);
  for(unsigned n=0;n<64;++n)r.byte(0x36000+n,uint8_t(c.seed+n));for(unsigned n=0;n<16;++n){r.byte(0x36000+16+2*n,uint8_t(16+n));r.byte(0x36000+17+2*n,uint8_t(n&1?1:0));r.byte(0x17495+n,uint8_t(c.seed+n));}
  for(unsigned page=0;page<16;++page)for(unsigned n=0;n<256;++n){const auto pos=n%64;r.byte(0x85ec00+page*256+n,pos>=16&&pos<48?uint8_t(pos&1?1:16+(pos-16)/2):uint8_t(n+c.seed));}
 }
 r.byte(0x167ba,c.mode);r.byte(0x167bb,uint8_t(c.seed&1));r.byte(0x167d5,c.multi);r.byte(0x1679e,c.special);r.byte(0x16799,c.selected);r.byte(0x167b8,1);r.byte(0x167b9,1);r.byte(0x167d0,1);r.byte(0x167d1,1);r.byte(0x167d4,1);r.byte(0x16798,1);r.byte(0x16792,0);r.byte(0x167fb,uint8_t(c.seed%3));r.byte(0x16782,uint8_t(c.kind==7));r.byte(0x167a1,uint8_t(c.seed&1));r.byte(0x167f8,49);r.byte(0x167f9,50);r.byte(0x167fa,51);r.byte(0x167f6,uint8_t(c.seed&1?1:255));r.byte(0x167da,uint8_t(c.seed%25));r.byte(0x167d7,uint8_t(c.seed%16));r.byte(0x167d8,uint8_t(c.seed&1));r.byte(0x167d9,uint8_t(c.seed%59));
 for(unsigned n=0;n<256;++n){r.byte(0x101a4+2*n,uint8_t(1u<<(n%4)));r.byte(0x101a5+2*n,c.enabled);}for(unsigned part=0;part<5;++part)for(unsigned n=0;n<256;++n){r.byte(0x168fc+part*256+n,0);r.byte(0x16e12+part*256+n,0);}

 r.byte(0x16790,c.kind==2?2:c.kind==3?3:0);r.byte(0x1679e,c.kind==4?1:0);r.byte(0x1679f,c.sceneMode);r.byte(0x17492,c.row);r.byte(0x16792,c.kind==5);r.byte(0x16793,c.key);r.byte(0x16784,c.kind==6||c.kind==0);r.byte(0x16785,c.kind==0?9:11);r.byte(0x16787,c.kind==0?2:0);r.byte(0xffec14,c.kind==0?0:1);r.wide(0x167f0,0x40002);r.word(0xfffff6,48);r.byte(0xffee72,c.kind==1?12:c.backward?11:13);r.byte(0xffee73,24);r.byte(0xffee74,c.seed%14);r.byte(0xffee75,c.backward?255:1);r.byte(0x167a1,0);r.byte(0x167fb,0);r.byte(0x169f6,1);r.byte(0x169f8,0);r.byte(0x174a6,0);r.byte(0x167ba,c.mode);r.byte(0x16799,c.selected);r.byte(0x16798,c.selected);r.wide(0x167bc,c.source);r.wide(0x167c0,c.live);

}
}


int main(){try{
 auto resource=std::make_unique<Resource>();auto storage=std::make_unique<std::aligned_storage_t<sizeof(T),alignof(T)>>();std::vector<unsigned char>snapshot(sizeof(T));Resolver resolver;
 uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0,dueGraphs=0,earlyGraphs=0,outputs=0,voiceChildren=0,liveGraphs=0;uint32_t components=0;static_assert(sizeof(T)<1600000,"Tick fixedstorage unexpectedly grew");
 auto run=[&](uint16_t elapsed,unsigned route,unsigned mode,bool evolving=false){Config c;c.kind=route;c.seed=mode;c.mode=mode&1;c.selected=mode%5;c.backward=mode&1;seed(*resource,c);auto&r=*resource;
  const uint16_t last=uint16_t(mode*1000u+0xfff0);const uint16_t counter=uint16_t(last+elapsed);r.wide(0xffec10,0xa5a50000u|counter);r.word(0x16780,last);
  r.byte(0x156e2,uint8_t(mode&1?33:32));r.byte(0xffff51,0);for(unsigned row=0;row<6;++row)r.byte(0x15400+row,uint8_t(mode&2?255:0));
  r.word(0x1537c,0);r.word(0x1537e,0);r.word(0x15b04,254);r.word(0x15b06,128);r.byte(0x169ed,uint8_t(mode&1?255:0));
  r.byte(0xffff50,uint8_t(mode%3==0?0:64));r.byte(0x1537a,uint8_t(mode%3==2?64:mode%3==0?64:0));r.byte(0x169fa,0);
  r.byte(0x1679c,uint8_t(mode&1));r.byte(0x1679b,uint8_t(mode&2?0:1));r.byte(0x1679a,uint8_t(mode&2?3:0));r.byte(0xffee98,uint8_t(mode&4));r.byte(0x169ef,1);r.byte(0x16950,64);r.word(0xffeea0,0x400);r.word(0xffeea4,0x100);r.wide(0xffee7e,0x100000);r.wide(0xffee8e,0x100000);
  // A real ParameterUpload is required in selected deferred cases. Other
  // cases retain count0 to keep the parent coverage matrix focused.
  r.byte(0x174a6,uint8_t(mode==7?1:0));r.byte(0x17526,0);r.byte(0x17560+4,0);r.word(0x17560+8,1);
  check(T::admitted([&](uint32_t a){return r.m[a&0xffffff];},resolver),"Tick pure seed rejected");
  tracking=true;auto*s=new(storage.get())T();tracking=false;unsigned count=0,mutations=0;
  while(!s->complete()){
   if(evolving)if(const auto*t=s->task<ks::StateForegroundTask>()){using S=ks::StateForegroundTask::Stage;auto once=[&](unsigned bit,auto&&change){if(!(mutations&bit)){change();mutations|=bit;}};
    if(t->stage()==S::ReadLastTick)once(1,[&]{r.word(0x16780,uint16_t(counter-25));});
    if(t->stage()==S::ReadRefreshFlags)once(2,[&]{r.byte(0x167d3,0xa5);});
    if(t->stage()==S::ReadDirty)once(4,[&]{r.byte(0x1679c,1);});
    if(t->stage()==S::RefreshParts)once(8,[&]{r.byte(0x1679b,0);r.byte(0x1679a,3);});
    if(t->stage()==S::RefreshClock)once(16,[&]{r.byte(0xffee98,1);r.byte(0x169ef,1);});
   }
   const auto previous=r.previous;const auto cost=s->nextClocks(previous);check(cost,"Tick pure stalled");r.phase(cost);std::memcpy(snapshot.data(),s,sizeof(T));tracking=true;
   const auto no=s->advanceBefore(cost-1,[&](uint32_t a,unsigned dt){return r.read(a,dt);},[&](uint32_t a,uint8_t v,unsigned dt){r.write(a,v,dt);},resolver,previous);
   tracking=false;check(!no&&!r.size&&!std::memcmp(snapshot.data(),s,sizeof(T)),"Tick cost-1 changed object or resources");++rejects;
   tracking=true;unsigned used=0;try{used=s->advanceBefore(cost,[&](uint32_t a,unsigned dt){++reads;return r.read(a,dt);},[&](uint32_t a,uint8_t v,unsigned dt){++writes;r.write(a,v,dt);},resolver,previous);}catch(...){tracking=false;std::cerr<<"Tick pure route="<<route<<" mode="<<mode<<" phase="<<count<<" owner="<<unsigned(s->component())<<'\n';throw;}tracking=false;
   check(used==cost,"Tick pure phase budget mismatch");if(s->effect().write)r.previous=-1;if(++count>=2000000){std::cerr<<"Bound route="<<route<<" mode="<<mode<<" component="<<unsigned(s->component())<<'\n';throw std::logic_error("Tick pure phase bound");}++phases;
  }
  const auto&v=s->summary();check(v.tick==counter&&v.elapsed==(evolving?25:elapsed),"Tick captured counter/elapsed oracle");const bool due=evolving||elapsed>=25;check(v.due==due,"Tick due threshold oracle");if(due){++dueGraphs;check(s->buttons()&&s->buttons()->complete()&&s->adc()&&s->adc()->complete()&&s->encoder()&&s->encoder()->complete()&&s->pedal()&&s->pedal()->complete()&&s->refresh()&&s->refresh()->complete(),"Tick incomplete closed child");check((unsigned(r.m[0x16780])<<8|r.m[0x16781])==counter,"Tick published last timestamp oracle");if(v.sent){check(s->output()&&s->output()->complete()&&s->output()->summary().bytes==3,"Tick pedal packet absent");++outputs;}else check(!s->output(),"Unchanged pedal emitted output");if(s->refresh()->deferred())voiceChildren+=s->refresh()->deferred()->summary().voiceUploads;if(route==0){const uint8_t expectedPedal=uint8_t(mode%3==2?128:mode%3==0?127:0);check(v.pedal==expectedPedal&&v.sent==(expectedPedal!=128),"Tick physical pedal transition oracle");if(mode&2)check((unsigned(r.m[0x1537c])<<8|r.m[0x1537d])==96,"Tick six rows did not publish48 actual button events");if(mode&1)check(r.m[0x156e2]==0&&r.m[0x156e3]==9,"Tick ADC start did not reset group/set settling");}}
  else{++earlyGraphs;check(!s->buttons()&&!s->encoder()&&!s->refresh()&&!s->output(),"Early tick performed panel work");check((unsigned(r.m[0x16780])<<8|r.m[0x16781])==last,"Early tick changed last timestamp");}
  if(evolving){check(mutations==31,"Tick evolving live reads missed");++liveGraphs;}components|=s->components();tracking=true;s->~T();tracking=false;++graphs;
 };
 for(uint16_t elapsed:{0u,1u,24u,25u,26u,0x7fffu,0x8000u,0xffffu})for(unsigned mode=0;mode<8;++mode)run(elapsed,0,mode);
 for(unsigned route=1;route<8;++route)run(25,route,route);
 run(0,0,0,true);
 check(!allocations,"Tick pure allocated");check(components==255&&dueGraphs&&earlyGraphs&&outputs&&voiceChildren&&liveGraphs,"Tick actual component/child coverage");
 std::cout<<"PASS Tick pure graphs="<<graphs<<" phases="<<phases<<" budgetRejects="<<rejects<<" due="<<dueGraphs<<" early="<<earlyGraphs<<" pedalPackets="<<outputs<<" actualVoiceUploads="<<voiceChildren<<" live="<<liveGraphs<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" bytes="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
