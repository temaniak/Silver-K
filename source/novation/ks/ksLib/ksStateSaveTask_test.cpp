#include "ksStateSaveService.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}using T=ks::StateSavePageService;using C=T::Component;
struct Event{uint32_t address;uint8_t value;unsigned clock;};
}
int runPages(){try{
 auto memory=std::make_unique<std::array<uint8_t,1<<24>>();uint64_t graphs=0,phases=0,rejections=0,clocks=0;uint32_t mask=0;
 for(unsigned seed=0;seed<256;++seed)for(unsigned mode=0;mode<11;++mode){
  const uint32_t source=0x30000 + (seed&1),target=mode==4?0x803f00:mode==5?0x7fffff:mode==6?0x840001:0x840000;
  for(unsigned n=0;n<256;++n){(*memory)[source+n]=uint8_t(seed*71+n*83);(*memory)[(target&0xffff00)+n]=mode==0?(*memory)[source+n]:uint8_t(seed*71+n*83+1);}(*memory)[0xffed6a]=(*memory)[0xffed6b]=255;
  const auto kind=mode==7||mode==8?T::Kind::Compare:T::Kind::Program;
  if(mode==7)for(unsigned n=0;n<256;++n)(*memory)[target+n]=(*memory)[source+n];
  T service(kind,{source,target});unsigned count=0,writes=0,commands=0,ticks=mode==10?250:0;int history=-1;bool window=false;uint64_t total=0;std::array<Event,3>unlock{};
  tracking=true;
  while(!service.complete()){
   const auto cost=service.nextClocks(history);check(cost>0,"Page graph stalled");const auto component=service.component();const auto read=[&](uint32_t a,unsigned offset)->uint8_t{
    check(offset<=cost,"Read outside admitted page phase");
    if(a==0xffec13)return uint8_t(ticks++);
    if(mode==9&&component!=C::Verify&&a==target+255&&writes==256&&ticks<4)return uint8_t((*memory)[a]^1);
    if((mode==2||mode==10)&&a>=target&&a<target+256&&writes==256)return uint8_t((*memory)[a]^1);
    if(mode==3&&component==C::Verify&&a==target)return uint8_t((*memory)[a]^1);
    return (*memory)[a];};
   const auto write=[&](uint32_t a,uint8_t v,unsigned offset){check(offset<=cost,"Write outside admitted page phase");if(a>=target&&a<target+256){check(window,"Flash byte escaped priority window");++writes;}if(a==(target&0xf80000)+0x5555||a==(target&0xf80000)+0x2aaa){check(commands<3,"Unexpected Flash retry command");unlock[commands++]={a,v,offset};}(*memory)[a]=v;history=a<0x400000?int(a):-1;};
   const auto before=service.component();check(!service.advanceBefore(cost-1,read,write,history),"Page accepted insufficient budget");check(service.component()==before&&service.nextClocks(history)==cost,"Rejected page phase changed state");++rejections;
   const auto used=service.advanceBefore(cost,read,write,history);check(used==cost,"Page phase cost changed after admission");window=service.writeWindow();total+=used;++phases;check(++count<16384,"Page transaction failed to terminate");
  }
  tracking=false;check(!service.writeWindow(),"Page left critical write window active");mask|=service.components();
  if(mode==0||mode==7){check(service.source()==source+256&&service.target()==target+256,"Equal page endpoints differ");check(!writes&&!commands,"Equal page programmed Flash");}
  else if(mode==1||mode==9){check(service.source()==source+256&&service.target()==target+256,"Verified page endpoints differ");check(writes==256&&commands==3,"Program omitted Flash transaction");check(unlock[0].value==0xaa&&unlock[1].value==0x55&&unlock[2].value==0xa0,"Flash unlock sequence differs");for(unsigned n=0;n<256;++n)check((*memory)[target+n]==(*memory)[source+n],"Flash page data differs");check(!(*memory)[0xffed6a]&&!(*memory)[0xffed6b],"Pending Flash count failed modular wrap");}
  else if(mode==4){check(service.source()==source+256&&service.target()==target+256&&!writes&&!commands,"Protected Flash page was programmed");}
  else {check(service.target()&0x80000000u,"Flash failure was not reported");check(service.source()==source,"Failed Flash advanced source");if(mode==5||mode==6||mode==8)check(!writes&&!commands,"Rejected page issued Flash commands");}
  clocks+=total;++graphs;
 }
 check(!allocations,"Page transaction allocated memory");check(mask==7,"Compare/program/verify page closure not covered");
 std::cout<<"PASS: native save page graphs="<<graphs<<" phases="<<phases<<" budget_rejections="<<rejections<<" clocks="<<clocks<<" components="<<mask<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}

namespace {
struct StoreFixtureResolver {
 ks::StatePackFieldsTask::Kind operator()(uint32_t token)const{check(token>=0x5a000001&&token<=0x5a000006,"Unexpected semantic packing fixture token");return ks::StatePackFieldsTask::Kind(token-0x5a000001);}
 bool operator()(uint8_t category,uint32_t token)const{return token==(0x6b000000u|category);}
};
int runFullStores(){using S=ks::StateSaveService;using K=S::Kind;auto resource=std::make_unique<std::array<uint8_t,1<<24>>();uint64_t graphs=0,phases=0,rejected=0,flashBytes=0;uint32_t coverage=0;StoreFixtureResolver resolver;
 for(unsigned kind=0;kind<14;++kind)for(unsigned seed=0;seed<8;++seed){
  resource->fill(0);auto byte=[&](uint32_t a,uint8_t v){(*resource)[a]=v;};auto word=[&](uint32_t a,uint16_t v){byte(a,uint8_t(v>>8));byte(a+1,uint8_t(v));};auto wide=[&](uint32_t a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));};
  for(unsigned n=0;n<1024;++n){byte(0x30000 + n,uint8_t(seed*71+n*83));byte(0x169fc + n,uint8_t(seed*37+n*17));byte(0x840000 + n,uint8_t(seed*71+n*83+1));}for(unsigned n=0;n<256;++n)byte(0x168fc + n,uint8_t(seed+n*13));for(unsigned p=0;p<5;++p)for(unsigned n=0;n<20;++n)byte(0x174ae + 20*p+n,uint8_t(seed+p*3+n*5));for(unsigned n=0;n<17;++n)byte(0x1733c + n,uint8_t(65+n));
  for(unsigned p=0;p<4;++p)word(0x167b0 + 2*p,uint16_t(0x0100 + p));word(0x31000,uint16_t(seed<4?0x0103:0x0531));word(0x167ac,uint16_t(seed<4?0x0103:0x0530));word(0x167aa,0x0103);word(0x167ae,0x0103);wide(0x167bc,0x30000);wide(0x1748e,0x30000);byte(0x167d2,15);byte(0x167d3,15);byte(0x167ba,0);byte(0x16799,0);byte(0x16798,0);byte(0x167da,2);byte(0x169eb,seed&1);byte(0xffff51,0);wide(0xffec10,12345);word(0xffed6a,0);
  for(unsigned n=0;n<6;++n)wide(71306+4*n,0x5a000001 + n);
  for(const auto&schema:ks::StateReloadPanelService::schema){for(unsigned n=0;n<8;++n)byte(0xfd66 + 8*schema.key+n,schema.bytes[n]);wide(0x1013e + 4*schema.bytes[0],0x6b000000u|schema.bytes[0]);}
  for(unsigned key=0;key<64;++key){byte(0x1c0 + 5*key,uint8_t(key%4));byte(0x1c1 + 5*key,1);byte(0x1c2 + 5*key,0xfe);byte(0x1c3 + 5*key,4);byte(0x1c4 + 5*key,1);}
  for(auto address:{0x14e21u,0x14e46u,0x14e93u,0x14ea6u}){byte(address,'O');byte(address+1,'K');byte(address+2,0);}
  if(kind==1&&seed>=6){const auto base=0x845800 + 3*1024;for(unsigned p=0;p<4;++p){for(unsigned n=0;n<256;++n)byte(base+p*256+n,uint8_t(seed*17+n*31));byte(base+p*256+187,1);byte(base+p*256+188,0);}byte(base+189,15);byte(base+190,15);byte(0x167ab,3);byte(0x167ba,1);byte(0x167bb,255);byte(0x174a6,1);byte(0x167d0,15);byte(0x167d1,15);byte(0x169fc + 227,0);wide(0x167c0,0x16f12);}
  S::Input input;input.source=0x30000;input.program=0x31000;input.target=0x840000;input.noticeAction.value=1;input.saveAction.value=2;S service(K(kind),input);unsigned count=0,commands=0,bytes=0;int previous=-1;
  tracking=true;
  while(!service.complete()){
   const auto cost=service.nextClocks(previous);check(cost,"Full native save did not progress");const auto phase=service.component();const auto depth=service.depth();const auto mask=service.components();unsigned accesses=0;
   const auto read=[&](uint32_t a,unsigned offset){check(offset&&offset<=cost&&a<(1<<24),"Full save read escaped admitted interval");++accesses;return a==0x400004||a==0x600004?uint8_t(255):(*resource)[a];};
   const auto write=[&](uint32_t a,uint8_t v,unsigned offset){check(offset&&offset<=cost&&a<(1<<24),"Full save write escaped admitted interval");check(a<0xfff000||a>=0xfffc00,"Full save used guest scratch stack");++accesses;(*resource)[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;if(a>=0x800000&&a<0x900000){if((a&0x7ffff)==0x5555||(a&0x7ffff)==0x2aaa)++commands;else ++bytes;}};
   check(!service.advanceBefore(0,read,write,resolver,previous)&&!accesses,"Zero save budget changed resources");check(!service.advanceBefore(cost-1,read,write,resolver,previous)&&!accesses,"Small save budget changed resources");check(service.component()==phase&&service.depth()==depth&&service.components()==mask&&service.nextClocks(previous)==cost,"Rejected full save budget changed continuation");rejected+=2;
   check(service.advanceBefore(cost,read,write,resolver,previous)==cost,"Full save admission differs");if(service.effect().write)previous=-1;++phases;check(++count<300000,"Full save graph exceeded fixture phase bound");
  }
  tracking=false;
  const auto&o=service.summary();check(!service.writePriority()&&!service.nextClocks(previous),"Completed save retained pending write window");check(!commands||commands%3==0,"Flash unlock sequence was incomplete");check(bytes%256==0,"Save left a partially programmed page");
  const auto type=K(kind);
  if((type==K::CommitMulti||type==K::MultiCurrent||type==K::MultiProvided)&&!(seed&1))check(!commands&&!bytes,"Protected multi-save changed Flash");
  if((type==K::CommitSingle||type==K::PresetSave||type==K::FormatDirect||type==K::FormatPart||type==K::Offset)&&!(seed&1))check(!commands&&!bytes,"Protected preset save changed Flash");
  if((type==K::CommitMulti||type==K::MultiCurrent||type==K::MultiProvided)&&(seed&1)&&o.multiAccepted){
   check(bytes==1024&&commands==12,"Multi save did not commit four complete pages");const auto source=type==K::MultiProvided?0x30000:0x169fc;const auto target=0x845800 + 3*1024;for(unsigned page=0;page<4;++page){for(unsigned n=0;n<209;++n)check((*resource)[target+page*256+n]==(*resource)[source+page*256+n],"Multi save lost serialized page payload");for(unsigned n=209;n<256;++n)check(!(*resource)[target+page*256+n],"Multi Flash padding differs");}}
  if(type==K::GlobalSave)check(bytes==256&&commands==3,"Global save did not persist a complete page");
  if(type==K::CommitSingle)check(!(*resource)[0x16790]&&!(*resource)[0x16784]&&!(*resource)[0x16792],"Single save did not finish UI reset");
  if(type==K::CommitMulti)check(!(*resource)[0x16790]&&!(*resource)[0x16784]&&!(*resource)[0x16792],"Multi save did not finish UI reset");
  if(type==K::PresetSave||(type==K::CommitSingle&&!o.reloaded))check((*resource)[0x167ad]==uint8_t(seed<4?3:48),"Preset save failed to restore alias program selection");
  if(o.notice)check((*resource)[0x16791]==1,"Save notice was not armed");
  coverage|=service.components();flashBytes+=bytes;++graphs;
 }
 check(!allocations&&coverage==0x1ffff,"Full save native coverage or allocation differs");std::cout<<"PASS: full native save pure graphs="<<graphs<<" phases="<<phases<<" budget_rejections="<<rejected<<" flash_bytes="<<flashBytes<<" components="<<coverage<<" allocations="<<allocations<<'\n';return 0;
}
}
int main(){try{const auto rc=runPages();return rc?rc:runFullStores();}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
