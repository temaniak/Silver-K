#include "ksPanelScanService.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>
namespace {
bool measuring=false;uint64_t allocations=0,cases=0,phases=0,rejections=0,rows=0,events=0;unsigned kinds=0,components=0;
void check(bool value,const char*why){if(!value)throw std::runtime_error(why);}
using K=ks::PanelScanService::Kind;
struct Config{K kind=K::Scan;uint16_t producer=126,consumer=0,event=0x142;unsigned mappingBias=0,overrideKey=256,overrideMapping=0;bool live=false;
 std::array<uint8_t,6>previous{{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa}},samples{{0x55,0x55,0x55,0x55,0x55,0x55}};};
struct Resources{
 std::array<uint8_t,512>mapping{},ram{},ports{};std::array<uint8_t,6>samples{};
 uint8_t&at(uint32_t a){if(a>=0xb800&&a<0xba00)return mapping[a-0xb800];if(a>=0x15300&&a<0x15500)return ram[a-0x15300];check(a>=0xfffe00&&a<=0xffffff,"Unexpected panel resource");return ports[a-0xfffe00];}
 uint8_t read(uint32_t a){if(a==0xffff51){const auto mux=at(0xffff62);check(mux>=0x20&&mux<0x26,"Panel sample escaped selected row");return samples[mux-0x20];}return at(a);}
 uint16_t word(uint32_t a){return uint16_t((unsigned(at(a))<<8)|at(a+1));}void word(uint32_t a,uint16_t v){at(a)=uint8_t(v>>8);at(a+1)=uint8_t(v);}
 bool operator==(const Resources&b)const{return mapping==b.mapping&&ram==b.ram&&ports==b.ports&&samples==b.samples;}
};
struct Transfer{uint32_t address;uint8_t value;unsigned offset;bool writing;bool operator==(const Transfer&b)const{return address==b.address&&value==b.value&&offset==b.offset&&writing==b.writing;}};
struct Event{uint16_t index,value;bool operator==(const Event&b)const{return index==b.index&&value==b.value;}};
struct Harness{
 ks::PanelScanService task;Resources resource;std::array<Transfer,8>transfers{};std::array<Event,64>published{};unsigned count=0,eventCount=0,muxCount=0,gateCount=0,reads=0,steps=0,cost=0;
 uint64_t clocks=0,digest=0xcbf29ce484222325ull;
 explicit Harness(const Config&c):task(c.kind,c.event){resource.samples=c.samples;resource.word(0x1537c,c.producer);resource.word(0x1537e,c.consumer);
  for(unsigned n=0;n<128;++n)resource.at(0x15380+n)=uint8_t(n*149);for(unsigned n=0;n<256;++n)resource.at(0xb8e8+n)=uint8_t(n*37+c.mappingBias);
  if(c.overrideKey<256)resource.at(0xb8e8+c.overrideKey)=uint8_t(c.overrideMapping);for(unsigned n=0;n<6;++n)resource.at(0x15400+n)=c.previous[n];resource.at(0xffff62)=0x30;resource.at(0xfffeb1)=255;
 }
 void event(uint32_t a,uint8_t v,unsigned offset,bool writing){check(offset&&offset<=cost,"Panel callback crossed phase budget");if(count)check(offset>=transfers[count-1].offset,"Panel callback clocks reversed");check(count<transfers.size(),"Panel transfer fanout exceeded bound");transfers[count++]={a,v,offset,writing};digest^=uint64_t(a)|(uint64_t(v)<<24)|(uint64_t(writing)<<32);digest*=1099511628211ull;digest^=clocks+offset;digest*=1099511628211ull;}
 unsigned step(unsigned available){count=0;cost=task.nextClocks();const bool publishing=task.atPush(ks::PanelInputTask::Stage::WriteEvent);
  const auto used=task.advanceBefore(available,[&](uint32_t a,unsigned offset){++reads;const auto v=resource.read(a);event(a,v,offset,false);return v;},
   [&](uint32_t a,uint8_t v,unsigned offset){event(a,v,offset,true);if(a==0xffff62){check(v==(muxCount<6?uint8_t(0x20+muxCount):0x30)&&muxCount<7,"Six-row mux order differs");++muxCount;}if(a==0xfffeb1){check(v==(gateCount?255:0)&&gateCount<2,"Gate disable/restore order differs");++gateCount;}resource.at(a)=v;});
  check(!used||used==cost,"Panel service crossed phase boundary");if(used){clocks+=used;++steps;if(publishing){check(count==2&&transfers[1].address==transfers[0].address+1&&eventCount<published.size(),"Panel event word transfer shape differs");published[eventCount++]={uint16_t(transfers[0].address-0x15380),uint16_t((unsigned(transfers[0].value)<<8)|transfers[1].value)};}}return used;
 }
};
void advance(Harness&a,Harness&b){
 const auto cost=a.task.nextClocks();check(cost&&a.steps<8192,"Panel graph did not make bounded progress");
 const auto before=std::make_tuple(a.task.component(),a.task.components(),a.task.event(),a.task.producer(),a.task.eventIndex(),a.task.key(),a.task.mapping(),a.task.lastRowUnchanged());const auto digest=a.digest,clock=a.clocks;const auto reads=a.reads;
 for(auto budget:{0u,cost-1}){check(!a.step(budget)&&!a.count&&a.task.nextClocks()==cost&&a.digest==digest&&a.clocks==clock&&a.reads==reads&&before==std::make_tuple(a.task.component(),a.task.components(),a.task.event(),a.task.producer(),a.task.eventIndex(),a.task.key(),a.task.mapping(),a.task.lastRowUnchanged()),"Rejected panel budget changed resources/continuation");++rejections;}
 check(a.step(cost)==cost&&b.step(std::numeric_limits<unsigned>::max())==cost,"Panel admitted phase was not exact");
 check(a.task.component()==b.task.component()&&a.task.components()==b.task.components()&&a.task.nextClocks()==b.task.nextClocks()&&a.task.complete()==b.task.complete()&&a.digest==b.digest&&a.clocks==b.clocks&&a.count==b.count,"Panel budget changed continuation/resources/timing");for(unsigned n=0;n<a.count;++n)check(a.transfers[n]==b.transfers[n],"Panel callback depends on phase budget");++phases;
}
void run(Config c){
 Harness a(c),b(c);Resources wanted=a.resource;unsigned liveMask=0;
 while(!a.task.complete()){
  if(c.live){
   const auto once=[&](unsigned bit,bool when,auto&&change){if(!(liveMask&(1u<<bit))&&when){change();liveMask|=1u<<bit;}};
   once(0,a.task.atMatrix(ks::PanelMatrixTask::Stage::ReadPrevious),[&]{a.resource.at(0x15400)=b.resource.at(0x15400)=0x11;});
   once(1,a.task.atMatrix(ks::PanelMatrixTask::Stage::ReadSample),[&]{a.resource.samples[0]=b.resource.samples[0]=0xe7;});
   once(2,a.task.atMatrix(ks::PanelMatrixTask::Stage::CompareRow),[&]{a.resource.at(0x15400)=b.resource.at(0x15400)=0x88;a.resource.samples[0]=b.resource.samples[0]=0x3c;});
   once(3,a.task.atPush(ks::PanelInputTask::Stage::ReadMapping),[&]{a.resource.at(0xb8e9)=b.resource.at(0xb8e9)=0xd6;});
   once(4,a.task.atPush(ks::PanelInputTask::Stage::ReadPushProducer),[&]{a.resource.word(0x1537c,126);b.resource.word(0x1537c,126);a.resource.at(0xb8e9)=b.resource.at(0xb8e9)=0x99;});
  }
  advance(a,b);
 }
 check(!a.task.nextClocks()&&!a.step(10000)&&!a.count,"Completed panel graph restarted");check(a.resource==b.resource,"Panel resources depend on phase budget");
 std::array<Event,64>expected{};unsigned eventCount=0;uint16_t producer=c.producer;
 if(c.live){check(liveMask==31,"Panel live fixture missed a resource boundary");c.previous[0]=0x11;c.samples[0]=0xe7;producer=126;wanted.at(0xb8e9)=0x99;wanted.samples[0]=0x3c;}
 const auto push=[&](uint16_t source){const auto key=uint8_t(source);const auto mapped=uint8_t(c.live&&eventCount==0?0xd6:wanted.at(0xb8e8+key));const auto value=uint16_t((source&0xff00)|mapped);wanted.word(0x15380+producer,value);expected[eventCount++]={producer,value};producer=uint16_t((producer+2)&126);};
 if(c.kind==K::ButtonPush)push(c.event);
 else{for(unsigned row=0;row<6;++row){if(c.previous[row]!=c.samples[row])wanted.at(0x15400+row)=c.samples[row];for(unsigned bit=0;bit<8;++bit)if((c.previous[row]^c.samples[row])&(1u<<bit))push(uint16_t((c.samples[row]&(1u<<bit)?0:0x100)|unsigned(row*8+bit)));}wanted.at(0xffff62)=0x30;wanted.at(0xfffeb1)=255;check(a.muxCount==7&&a.gateCount==2,"Panel did not scan six rows exactly once");rows+=6;}
 wanted.word(0x1537c,c.kind==K::Initialize?0:producer);if(c.kind==K::Initialize)wanted.word(0x1537e,0);
 check(a.resource==wanted,"Panel resource-level oracle differs");check(a.eventCount==eventCount,"Panel event count differs");for(unsigned n=0;n<eventCount;++n)check(a.published[n]==expected[n],"Panel physical event order, polarity, mapping or ring wrap differs");
 if(c.kind==K::ButtonPush)check(a.task.event()==expected[0].value&&a.task.eventIndex()==c.producer&&a.task.producer()==producer&&a.task.key()==uint8_t(c.event),"Standalone button push result differs");
 else if(c.kind==K::Scan)check(a.task.lastRowUnchanged()==(c.previous[5]==c.samples[5]),"Last-row branch result differs");
 events+=eventCount;kinds|=1u<<unsigned(c.kind);components|=a.task.components();++cases;
}
}
void*operator new(std::size_t n){if(measuring)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){try{
 measuring=true;Config c;
 for(auto kind:{K::Initialize,K::Scan})for(unsigned row=0;row<6;++row)for(unsigned pattern=0;pattern<256;++pattern)for(bool reverse:{false,true}){c=Config{};c.kind=kind;c.previous.fill(0x55);c.samples=c.previous;c.previous[row]=reverse?uint8_t(pattern):0;c.samples[row]=reverse?0:uint8_t(pattern);run(c);}
 for(unsigned producer=0;producer<128;producer+=2)for(unsigned consumer=0;consumer<128;consumer+=2)for(auto kind:{K::Initialize,K::Scan,K::ButtonPush}){c=Config{};c.kind=kind;c.producer=uint16_t(producer);c.consumer=uint16_t(consumer);c.previous=c.samples;run(c);}
 for(unsigned event=0;event<65536;++event){c=Config{};c.kind=K::ButtonPush;c.event=uint16_t(event);c.producer=uint16_t(event&126);c.mappingBias=event>>8;run(c);}
 for(unsigned key=0;key<256;++key)for(unsigned mapping=0;mapping<256;++mapping){c=Config{};c.kind=K::ButtonPush;c.event=uint16_t(0xa500|key);c.overrideKey=key;c.overrideMapping=mapping;run(c);}
 for(unsigned producer=0;producer<128;++producer){c=Config{};c.kind=K::ButtonPush;c.producer=uint16_t(producer);run(c);}
 for(auto kind:{K::Initialize,K::Scan})for(unsigned producer=0;producer<128;producer+=2){c=Config{};c.kind=kind;c.producer=uint16_t(producer);run(c);}
 c=Config{};c.live=true;c.producer=40;run(c);
 measuring=false;check(kinds==7&&components==7&&!allocations,"Panel graph/allocation coverage incomplete");
 std::printf("PASS: %llu CPU-independent panel graphs; %llu phases; %llu rejected budgets; %llu ordered rows / %llu mapped events; kinds=0x%x components=0x%x allocations=%llu; six-row polarity/order, ring wrap/odd transfer, all event/mapping bytes, initialize reset, live resources and exact phase budgets verified\n",static_cast<unsigned long long>(cases),static_cast<unsigned long long>(phases),static_cast<unsigned long long>(rejections),static_cast<unsigned long long>(rows),static_cast<unsigned long long>(events),kinds,components,static_cast<unsigned long long>(allocations));return 0;
 }catch(const std::exception&e){measuring=false;std::fprintf(stderr,"%s\n",e.what());return 1;}}
