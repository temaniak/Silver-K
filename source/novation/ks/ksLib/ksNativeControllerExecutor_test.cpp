#include "ksNativeControllerExecutor.h"
#include "ksRom.h"
#include "ksNativeOraclePartition_test.h"
#include <fstream>
#include <string>
#include "h8s/h8s.hpp"
#include <vector>
#include <tuple>
#include <array>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <new>
#include <iostream>
#include <algorithm>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
template<class T>struct OwnedStorage{std::unique_ptr<T>storage{new T};T*value=storage.get();T*operator->(){return value;}};
using Access=ks::nativeOracleTest::Access;using Raw=ks::nativeOracleTest::Raw;
struct Original:H8SEmulator{using H8SEmulator::indent;using H8SEmulator::lastread;using H8SEmulator::lastwrite;};
struct OriginalBus:H8SDevice{std::vector<Raw>events;uint8_t read(uint32_t a)override{auto v=a==0x400004||a==0x600004?uint8_t(255):a==0xffff8c?uint8_t(state->memory[a]|128):state->memory[a];events.emplace_back(false,a,v,state->cycles,uint32_t(state->getPC()),uint32_t(state->getSP()));return v;}void write(uint32_t a,uint8_t v)override{state->memory[a]=v;events.emplace_back(true,a,v,state->cycles,uint32_t(state->getPC()),uint32_t(state->getSP()));}};
struct NativeBus:ks::ControllerResourceBus{std::vector<Access>events;uint64_t statusReads=0;uint8_t read(uint32_t a)override{++statusReads;auto v=a==0x400004||a==0x600004?uint8_t(255):a==0xffff8c?uint8_t(resources->memory[a]|128):resources->memory[a];events.emplace_back(false,a,v,resources->cycles);return v;}void write(uint32_t a,uint8_t v)override{resources->memory[a]=v;events.emplace_back(true,a,v,resources->cycles);}};
// Optional bounded binary tapes retain every observed transfer, including
// retired opcode/vector/frame bytes. Native records have no synthetic PC/SP.
struct Tapes {
 std::ofstream original,native,partition,ram;bool enabled=false;uint64_t bytes=0;
 explicit Tapes(const char*prefix){if(!prefix)return;const std::string p(prefix);original.open(p+"-h8.raw",std::ios::binary);native.open(p+"-native.raw",std::ios::binary);partition.open(p+"-partition.raw",std::ios::binary);ram.open(p+"-ram-diffs.raw",std::ios::binary);check(bool(original)&&bool(native)&&bool(partition)&&bool(ram),"Cannot open proof tapes");original.write("KSORAW02",8);native.write("KSNRAW02",8);partition.write("KSPART02",8);ram.write("KSRAMD02",8);enabled=true;}
 void put(std::ostream&o,uint64_t v,unsigned n){check(bytes+n<=512ull*1024*1024,"Bounded proof tape capacity exceeded");for(unsigned i=0;i<n;++i)o.put(char(v>>(8*i)));bytes+=n;}
 void phase(uint64_t id,const ks::nativeOracleTest::Before&b,uint32_t afterSp,uint64_t endClock,const std::vector<Raw>&old,const std::vector<Access>&now,const ks::nativeOracleTest::Partition&p,int oldRead,int oldWrite,int nativeRead,int nativeWrite){if(!enabled)return;
  for(unsigned i=0;i<old.size();++i){const auto&e=old[i];put(original,id,8);put(original,i,4);put(original,e.write,1);put(original,e.address,4);put(original,e.value,1);put(original,e.clock,8);put(original,e.pc,4);put(original,e.sp,4);}
  for(unsigned i=0;i<now.size();++i){const auto&e=now[i];put(native,id,8);put(native,i,4);put(native,std::get<0>(e),1);put(native,std::get<1>(e),4);put(native,std::get<2>(e),1);put(native,std::get<3>(e),8);}
  put(partition,id,8);put(partition,b.pc,4);put(partition,b.entry,4);put(partition,b.sp,4);put(partition,afterSp,4);put(partition,p.frameLow,4);put(partition,b.rootSp,4);put(partition,uint32_t(b.selected),4);put(partition,p.fetchFirst,4);put(partition,p.fetchBytes,4);put(partition,b.clock,8);put(partition,endClock,8);put(partition,uint32_t(oldRead),4);put(partition,uint32_t(oldWrite),4);put(partition,uint32_t(nativeRead),4);put(partition,uint32_t(nativeWrite),4);put(partition,p.meaning.size(),4);for(auto m:p.meaning)put(partition,unsigned(m),1);
  check(bool(original)&&bool(native)&&bool(partition),"Incomplete raw proof tape");
 }
 void difference(uint64_t phase,uint32_t start,unsigned size,bool frame,const uint8_t*a,const uint8_t*b){if(!enabled)return;put(ram,phase,8);put(ram,start,4);put(ram,size,4);put(ram,frame,1);for(unsigned n=0;n<size;++n){put(ram,a[n],1);put(ram,b[n],1);}check(bool(ram),"Incomplete RAM difference tape");}
 void flush(){if(enabled){original.flush();native.flush();partition.flush();ram.flush();check(bool(original)&&bool(native)&&bool(partition)&&bool(ram),"Incomplete final proof tapes");}}
};
ks::nativeOracleTest::Before before(const Original&a,H8SDisassembler&decoder){
 ks::nativeOracleTest::Before b;b.pc=uint32_t(a.getPC());b.sp=a.getSP();b.rootSp=0xfff800;b.pending=a.pending_irqs;b.pendingHigh=a.pending_irqs_high;b.clock=a.cycles;b.mask=a.exr;b.mode2=a.interruptMode2;std::copy(std::begin(a.interruptPriority),std::end(a.interruptPriority),b.priorities.begin());b.selected=ks::nativeOracleTest::selected(b);
 b.entry=b.pc&~1u;if(b.selected>=0){for(unsigned n=0;n<4;++n)b.vectorWord=(b.vectorWord<<8)|a.memory[4*unsigned(b.selected)+n];b.entry=(b.vectorWord&0xffffff)&~1u;}
 check(b.entry<=0x1000000-b.opcode.size(),"Opcode image range");std::copy(a.memory+b.entry,a.memory+b.entry+b.opcode.size(),b.opcode.begin());decoder.loadmem(b.opcode.data(),b.opcode.size(),b.entry);decoder.pc=decoder.makepc(b.entry);decoder.step();b.opcodeBytes=uint32_t(decoder.getPC())-b.entry;return b;
}
bool ramEqual(const uint8_t*a,const uint8_t*b,const std::vector<uint8_t>&written,Tapes&t,uint64_t phase){bool same=true;for(uint32_t at=0;at<0x1000000;){if(a[at]==b[at]){++at;continue;}const auto first=at;const bool frame=(written[at/8]>>(at&7))&1;do{++at;}while(at<0x1000000&&a[at]!=b[at]&&bool((written[at/8]>>(at&7))&1)==frame);t.difference(phase,first,at-first,frame,a+first,b+first);if(!frame)same=false;}return same;}

int main(int argc,char**argv){try{if(argc!=2&&argc!=3){std::cout<<"SKIP: authenticated controller ROM input required\n";return 77;}g_dasm=false;ks::Rom rom(argv[1]);check(rom.isValid(),"Authenticated controller ROM required");const auto&romData=rom.data();Tapes tapes(argc==3?argv[2]:nullptr);std::vector<uint8_t>frameWritten(0x1000000/8);std::array<uint64_t,4>meanings{};
 auto a=std::make_unique<Original>();OwnedStorage<ks::ControllerDeviceState>b;OwnedStorage<ks::NativeControllerExecutor>owner;auto decoder=std::make_unique<H8SDisassembler>();OriginalBus x;NativeBus y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->attachResourceBus(&y);for(unsigned at:{0u,0xa00000u}){a->loadmem(romData.data(),romData.size(),at);b->loadmem(romData.data(),romData.size(),at);}decoder->loadmem(romData.data(),romData.size(),0);
 enum class K{Main};
 struct Config{K kind=K::Main;unsigned alias=0,key=0xa0,pattern=3,part=2,flags=0xa5,count=8,pointer=0,value=99,mode=0,hardware=4,inhibit=0,notify=0,edit=0,previous=16,patchFlags=0,expression=1,held=0,bank=1;unsigned probePhase=2,busy=0;bool live=false,caller=false,nested=false,reentry=false,reset=false;};Config cfg;
std::vector<uint8_t>ownerSnapshot(sizeof(ks::NativeControllerExecutor)),deviceSnapshot(sizeof(ks::ControllerDeviceState));uint64_t snapshots=0,freshPlanGuards=0,queryBeforeIrq=0,invalidForegroundMaskGuards=0;std::array<bool,2>maskGuardContexts{};std::array<uint64_t,8>emptyMainPriorities{};uint64_t emptyMainGuards=0,activeEmptyIrqGuards=0;bool oneShot=false;
 // Unbound metadata must precede invalid mode, map and foreground masks.
 b->interruptMode2=false;b->nativeLowBase=nullptr;b->nativeLowSize=0;
 std::memcpy(ownerSnapshot.data(),owner.value,ownerSnapshot.size());std::memcpy(deviceSnapshot.data(),b.value,deviceSnapshot.size());
 const auto unboundEvents=y.events.size();const auto unboundReads=y.statusReads;
 const auto unboundUnchanged=[&]{return !std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size())&&unboundEvents==y.events.size()&&unboundReads==y.statusReads;};
 check(owner->playbackBlocked(*b.value)==ks::NativeControllerExecutor::Blocked::Metadata&&!owner->nextClocks(*b.value)&&!owner->advanceBefore(*b.value,~0u)&&!owner->advanceNextPhase(*b.value)&&unboundUnchanged(),"Unbound playback metadata precedence/state");
 for(const uint8_t mask:{uint8_t(0),uint8_t(8),uint8_t(255)})check(owner->interruptBlocked(*b.value,mask)==ks::NativeControllerExecutor::Blocked::Metadata&&!owner->interruptNextClocks(*b.value,mask)&&!owner->advanceInterruptBefore(*b.value,~0u,mask)&&unboundUnchanged(),"Unbound interrupt metadata precedence/state");
 check(owner->bindVerifiedRom(romData.data()),"Metadata binding");
 auto checkInvalidForegroundMasks=[&]{
  const auto context=owner->activeInterrupt()?1u:0u;check(!maskGuardContexts[context],"Invalid foreground mask context repeated");
  for(const uint8_t mask:{uint8_t(8),uint8_t(255)}){
   std::memcpy(ownerSnapshot.data(),owner.value,ownerSnapshot.size());std::memcpy(deviceSnapshot.data(),b.value,deviceSnapshot.size());const auto events=y.events.size();const auto reads=y.statusReads;
   const auto unchanged=[&]{return !std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size())&&events==y.events.size()&&reads==y.statusReads;};
   tracking=true;const auto blocked=owner->interruptBlocked(*b.value,mask);tracking=false;check(blocked==ks::NativeControllerExecutor::Blocked::Mask&&unchanged(),"Invalid foreground mask query changed owner/device/resources");
   tracking=true;const auto clocks=owner->interruptNextClocks(*b.value,mask);tracking=false;check(!clocks&&unchanged(),"Invalid foreground mask clocks changed owner/device/resources");
   tracking=true;const auto used=owner->advanceInterruptBefore(*b.value,~0u,mask);tracking=false;check(!used&&unchanged(),"Invalid foreground mask advance changed owner/device/resources");++invalidForegroundMaskGuards;
  }
  maskGuardContexts[context]=true;
 };
 uint64_t phases=0,cyclesDone=0,rejects=0,irqExpected=0,irqActual=0,nestedCases=0,criticalHeld=0;
 auto run=[&](unsigned scenario,unsigned vector=0,int child=-1,unsigned nestedVector=0,bool critical=false){
  std::fill(frameWritten.begin(),frameWritten.end(),uint8_t(0));
  for(unsigned n=0;n<8;++n)a->regs[n].er=int32_t((std::array<uint32_t,5>{0u,0x80000000u,0xffffffffu,0x7fffffffu,0x12345678u})[(n+cfg.flags)%5]);a->regs[6].r=int16_t((cfg.value<<8)|cfg.key);a->regs[2].rl=int8_t(cfg.part);a->regs[0].er=0x18055;a->regs[1].er=0;a->regs[4].er=0x15c1f;a->regs[5].er=0x18000;
  a->pc=a->makepc(0x4338);a->setSP(0xfff800);a->cycles=1000;a->indent=0;a->ccr=uint8_t(cfg.flags);a->exr=0;a->interruptMode2=true;a->lastread=a->lastwrite=-1;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;a->interruptPriority[32]=6;
  b->cycles=1000;b->pending_irqs=b->pending_irqs_high=0;b->interruptMode2=true;b->lastread=b->lastwrite=-1;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[32]=6;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->nativeLowWritable=true;
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0x15000,m+0x30000,uint8_t(0));std::fill(m+0xffec00,m+(1<<24),uint8_t(0));const auto byte=[&](unsigned at,unsigned v){m[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};
   wide(0xfff800,0x4338);std::fill(m+0xfff000,m+0xfff800,uint8_t(0x5a));byte(0x17526,cfg.part);
   wide(0x167bc,0x18000);wide(0x167c0,0x19000);byte(0x15c1f,cfg.key);byte(0x15c20,cfg.value);byte(0x174a6,4);for(unsigned i=0;i<14;++i){wide(0x17560+20*i,0x18000);byte(0x17560+20*i+4,cfg.part);byte(0x17560+20*i+5,60);word(0x17560+20*i+8,0x4012);byte(0x17560+20*i+17,1);}wide(0x174aeu+20*cfg.part,0x18000);byte(0x160d2u+334*cfg.part+309,cfg.previous);byte(0x160d2u+334*cfg.part+331,cfg.bank);byte(0x1603a,0);byte(0x160d2u+334*cfg.part+332,cfg.value);byte(0x167ba,cfg.mode);byte(0x167bb,cfg.mode);byte(0x15c23,cfg.inhibit);byte(0x16792,cfg.notify);byte(0x16794,0);byte(0x16784,cfg.edit);byte(0x16790,0);byte(0x16793,1);byte(0x1679e,0);byte(0x1679f,1);byte(0x169ed,127);word(0x167ac,0x0103);word(0x167ae,0x0103);byte(0x16798,cfg.part);byte(0x167d4,cfg.part);wide(0x167a6,0);wide(0x1748e,0x18000);byte(0xffff51,0);byte(0x1679a,cfg.value);byte(0x158f2,cfg.hardware);byte(0x1678d,cfg.value);for(unsigned i=0;i<512;++i){byte(0x18000+i,(cfg.key+i)%128);byte(0x19000+i,255);byte(0x19200+i,cfg.key-i);}
   for(unsigned j=0;j<256;++j)byte(0x18000+j,j+1);byte(0x18000,16);byte(0x18001,17);byte(0x180ad,8);byte(0x180b1,cfg.expression);byte(0x174aeu+20*cfg.part+17,cfg.held);for(unsigned j=0;j<66;++j)word(0x15200+2*j,uint16_t(j*511));word(0x15130,0x3c00);word(0x17516,256);byte(0x17518,10);byte(0x17519,20);byte(0x1751a,30);byte(0x169f0,64);byte(0x17521,64);byte(0x18058,cfg.patchFlags);byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,255);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);word(0xffec64,0);word(0xffec66,0);byte(0xffed68,0);byte(0xffff8d,0x42);byte(0xffffd5,0xc0);byte(0xffff55,255);byte(0xffff54,12);byte(0xffff59,0xe0);byte(0xffff50,31);byte(0xffff98,0x73);
   for(unsigned n=0;n<36;++n){byte(0xb918+n,n);byte(0xb93c+n,n&1);}for(unsigned n=0;n<4;++n){byte(0xffec16+n,1u<<n);byte(0xffec24+n,128u>>n);word(0xffff90+2*n,0x8000);word(0x15652+2*n,0x8000);word(0x1560a+2*n,0);word(0x1569a+2*n,0);}byte(0x156e4,255);
  }
  for(auto*m:{a->memory,b->memory}){std::copy(rom.data().begin()+0xf250,rom.data().begin()+0xf2da,m+0xf250);m[0xf250+6*2]=uint8_t(cfg.alias);const auto word=[&](unsigned at,unsigned v){m[at]=uint8_t(v>>8);m[at+1]=uint8_t(v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};for(unsigned p=0;p<5;++p){wide(0x174aeu+20*p,0x18000);m[0x174aeu+20*p+11]=0;m[0x160d2u+334*p+208]=0;m[0x160d2u+334*p+209]=1;m[0x160d2u+334*p+327]=4;}m[0x167d3]=15;m[0x15c1b]=m[0x15c0f]=m[0x15c2b]=0xa0;m[0x15c1d]=m[0x15c11]=m[0x15c2c]=cfg.pattern?1:0;m[0x15c1a]=m[0x15c0e]=m[0x15c2a]=0;wide(0x15c16,0x15c20);wide(0x15c0a,0x15c14);wide(0x15c24,0x15c29);m[0x15c1f]=m[0x15c13]=m[0x15c28]=60;m[0x15c20]=m[0x15c14]=m[0x15c29]=99;if(cfg.pattern==2){word(0xffec64,3);m[0xffec68]=0xa0;m[0xffec69]=60;m[0xffec6a]=99;word(0x15b04,3);m[0x15b08]=0xa0;m[0x15b09]=61;m[0x15b0a]=99;}}
  for(auto*m:{a->memory,b->memory}){for(unsigned n=0;n<14;++n){m[0x17560+20*n+8]=0;m[0x17560+20*n+9]=0;}m[0x1679e]=0;m[0x167d3]=15;for(unsigned p=0;p<4;++p){m[0x160d2+334*p+248]=0;m[0x160d2+334*p+327]=0;}m[0xffec64]=m[0xffec65]=m[0xffec66]=m[0xffec67]=0;m[0x15b04]=m[0x15b05]=m[0x15b06]=m[0x15b07]=0;}
  if(cfg.pattern==3)for(auto*m:{a->memory,b->memory}){m[0xffec64]=0;m[0xffec65]=3;m[0xffec68]=0xb0;m[0xffec69]=6;m[0xffec6a]=44;m[0x15b04]=0;m[0x15b05]=3;m[0x15b08]=0xb0;m[0x15b09]=6;m[0x15b0a]=44;for(unsigned p=0;p<5;++p)m[0x160d2+334*p+332]=104;}
  if(cfg.pattern>=5)for(auto*m:{a->memory,b->memory}){m[0x1537c]=0;m[0x1537d]=2;m[0x1537e]=m[0x1537f]=0;m[0x15380]=1;m[0x15381]=0x63;m[0x16784]=1;m[0x16785]=11;m[0x16787]=0;m[0xffec14]=1;m[0x1678f]=cfg.pattern==5?6:0x38;m[0x16790]=m[0x1679e]=m[0x1679f]=m[0x16792]=0;m[0x81a00c]=0;m[0x81a00d]=96;m[0x81a016]=0;m[0x81a017]=255;m[0x81a018]=47;m[0x81a019]=0;for(unsigned n=0;n<21;++n)m[0x81fe00+n]=0;for(unsigned p=0;p<5;++p){auto at=0x174aeu+20*p;const auto source=0x169fcu+256*p;for(unsigned n=0;n<4;++n)m[at+n]=uint8_t(source>>(24-8*n));for(unsigned n=0;n<256;++n)m[source+n]=uint8_t((n*3)%128);for(unsigned n:{0u,139u,222u,223u,224u,225u,226u})m[source+n]=0;m[source+149]=0;m[source+85]=120;m[source+86]=64;m[source+87]=0;m[source+88]=0;}if(cfg.pattern==6)std::copy(rom.data().begin(),rom.data().end(),m+0x804000);m[0x81fe07]=0;m[0x167aa]=1;m[0x167ab]=3;m[0x16795]=1;}

  if(scenario==1)for(auto*m:{a->memory,b->memory}){const uint32_t tick=1007;for(unsigned n=0;n<4;++n)m[0xffec10+n]=uint8_t(tick>>(24-8*n));m[0x1537a]=64;}
  // Initial operands are explicit fixture lifecycle values. Later inputs
  // come only from the pure result, never from the H8 oracle.
  check(owner->beginPlayback({uint16_t((cfg.value<<8)|cfg.key),0xff,0x18000}),"Explicit lifecycle input");
  // Empty playback queries and rejected budgets must be pure, including when
  // mode and mapping errors are combined or changed after a query.
  check(!owner->activeMain()&&!owner->activeInterrupt()&&!b->pending_irqs&&!b->pending_irqs_high,"Empty Main fixture preconditions");
  const auto emptyCost=owner->nextClocks(*b.value);check(emptyCost==ks::ServiceCycleTask{}.nextClocks(),"Empty Main initial cost");
  std::memcpy(ownerSnapshot.data(),owner.value,ownerSnapshot.size());std::memcpy(deviceSnapshot.data(),b.value,deviceSnapshot.size());
  const auto emptyEvents=y.events.size();const auto emptyReads=y.statusReads;
  check(owner->nextClocks(*b.value)==emptyCost&&!owner->advanceBefore(*b.value,0)&&!owner->advanceBefore(*b.value,emptyCost-1),"Empty Main query/budget");
  check(!std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size())&&emptyEvents==y.events.size()&&emptyReads==y.statusReads,"Empty Main rejection changed state/observer");
  for(uint8_t mask=0;mask<8;++mask){
   check(owner->interruptBlocked(*b.value,mask)==ks::NativeControllerExecutor::Blocked::None&&!owner->interruptNextClocks(*b.value,mask)&&!owner->advanceInterruptBefore(*b.value,~0u,mask),"Empty IRQ foreground mask selection");
   check(!std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size())&&emptyEvents==y.events.size()&&emptyReads==y.statusReads,"Empty IRQ foreground query changed state/observer");
  }
  auto* const lowBase=b->nativeLowBase;const auto lowSize=b->nativeLowSize;
  b->nativeLowBase=nullptr;b->nativeLowSize=0x1fffff;b->interruptMode2=false;
  check(owner->playbackBlocked(*b.value)==ks::NativeControllerExecutor::Blocked::InterruptMode&&!owner->nextClocks(*b.value),"Mode must precede invalid map");
  b->interruptMode2=true;check(owner->playbackBlocked(*b.value)==ks::NativeControllerExecutor::Blocked::ResourceMap&&!owner->nextClocks(*b.value),"Null map must block playback");
  b->nativeLowBase=lowBase;check(owner->playbackBlocked(*b.value)==ks::NativeControllerExecutor::Blocked::ResourceMap&&!owner->nextClocks(*b.value),"Short map must block playback");
  b->nativeLowSize=lowSize;check(owner->playbackBlocked(*b.value)==ks::NativeControllerExecutor::Blocked::None&&owner->nextClocks(*b.value)==emptyCost,"Restored map must replan playback");++emptyMainGuards;
  // An external cost query retains no plan: changing interrupt mode before the
  // one-shot call must block without touching controller or observed resources.
  const auto queriedCost=owner->nextClocks(*b.value);check(queriedCost,"Fresh-plan fixture stalled");b->interruptMode2=false;
  std::memcpy(ownerSnapshot.data(),owner.value,ownerSnapshot.size());std::memcpy(deviceSnapshot.data(),b.value,deviceSnapshot.size());const auto guardEvents=y.events.size();const auto guardReads=y.statusReads;
  tracking=true;const auto guarded=owner->advanceNextPhase(*b.value);tracking=false;
  check(!guarded&&!std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size())&&guardEvents==y.events.size()&&guardReads==y.statusReads,"One-shot reused an external query after resource mutation");b->interruptMode2=true;++freshPlanGuards;
  if(!maskGuardContexts[0]){check(!owner->activeInterrupt(),"Inactive foreground mask fixture already owns an IRQ");checkInvalidForegroundMasks();}
const auto returnsBefore=owner->metrics().mainReturns;const auto irqBefore=owner->metrics().interruptStarts,irqReturnedBefore=owner->metrics().interruptReturns;unsigned expectedHere=0;
  for(unsigned cycle=0;cycle<4;++cycle){unsigned count=0;bool injected=false,nestedInjected=false;do{
   const bool mainTarget=owner->main()&&owner->main()->coordinator().awaitingChild()&&int(owner->main()->childIndex())==child;
   const bool target=critical?(cycle==0&&owner->mainMask()==7):child<0?true:mainTarget;
   if(vector&&!injected&&target){
    check(owner->nextClocks(*b.value),"Pre-IRQ query stalled");++queryBeforeIrq;
    for(auto*m:{a->memory,b->memory})if(vector==90){m[0xffed6c]=0;m[0xffed6d]=1;m[0xffed6e]=m[0xffed6f]=0;m[0xffed70]=0xf8;}
    const auto level=uint8_t(vector==32?6:1);a->interruptPriority[vector]=b->interruptPriority[vector]=level;a->interrupt(vector);b->interrupt(vector);injected=true;++expectedHere;
    if(critical){check(a->exr==7&&owner->semanticMask(owner->mainMask())==7,"Critical injection missing actual mask7");const auto prior=owner->metrics().interruptStarts;check(owner->nextClocks(*b.value)==owner->main()->nextClocks(b->lastwrite,b->lastread),"Masked IRQ replaced Main phase");check(owner->metrics().interruptStarts==prior,"Masked IRQ admitted early");++criticalHeld;}
   }
   if(nestedVector&&injected&&!nestedInjected&&owner->interrupts().depth()==1&&owner->semanticMask(owner->mainMask())<7){check(owner->nextClocks(*b.value),"Pre-nested-IRQ query stalled");++queryBeforeIrq;const auto level=uint8_t(nestedVector==32?6:7);a->interruptPriority[nestedVector]=b->interruptPriority[nestedVector]=level;a->interrupt(nestedVector);b->interrupt(nestedVector);nestedInjected=true;++expectedHere;++nestedCases;}

   if(!maskGuardContexts[1]&&owner->activeInterrupt())checkInvalidForegroundMasks();
   const auto cost=owner->nextClocks(*b.value);check(cost,"Owner stalled");const auto eventSize=y.events.size();const auto reads=y.statusReads;
   if(owner->activeInterrupt()&&!b->pending_irqs&&!b->pending_irqs_high){check(cost==owner->interruptNextClocks(*b.value,owner->mainMask()),"Active empty-pending IRQ must continue");++activeEmptyIrqGuards;}
   if(owner->main()&&!owner->activeInterrupt()&&!b->pending_irqs&&!b->pending_irqs_high){const auto priority=owner->mainMask();check(priority<8,"Main priority outside legal range");++emptyMainPriorities[priority];}
   // Every phase rejects an insufficient budget. Whole object snapshots are
   // sampled here and checked byte-for-byte across the rejected call.
   const bool snapshot=phases%257==0||b->pending_irqs||b->pending_irqs_high;
   if(snapshot){std::memcpy(ownerSnapshot.data(),owner.value,ownerSnapshot.size());std::memcpy(deviceSnapshot.data(),b.value,deviceSnapshot.size());++snapshots;}
   tracking=true;const auto rejected=owner->advanceBefore(*b.value,cost-1);tracking=false;
   if(snapshot)check(!std::memcmp(ownerSnapshot.data(),owner.value,ownerSnapshot.size())&&!std::memcmp(deviceSnapshot.data(),b.value,deviceSnapshot.size()),"Rejected phase changed whole native owner/device");
   check(!rejected&&eventSize==y.events.size()&&reads==y.statusReads,"Underbudget mutated external resource observer");++rejects;
   x.events.clear();y.events.clear();const auto reference=before(*a,*decoder);const auto pc=uint32_t(a->getPC());a->step();tracking=true;const auto used=oneShot?owner->advanceNextPhase(*b.value):owner->advanceBefore(*b.value,cost);tracking=false;
   const auto part=ks::nativeOracleTest::partition(x.events,y.events,reference,a->getSP());
   for(unsigned i=0;i<x.events.size();++i){const auto m=unsigned(part.meaning[i]);++meanings[m];if(m==unsigned(ks::nativeOracleTest::Meaning::RetiredFrame)&&x.events[i].write){const auto at=x.events[i].address;frameWritten[at/8]|=uint8_t(1u<<(at&7));}}
   tapes.phase(phases,reference,a->getSP(),a->cycles,x.events,y.events,part,a->lastread,a->lastwrite,b->lastread,b->lastwrite);
   if(used!=cost||a->cycles!=b->cycles||part.device!=y.events){std::cerr<<"First divergence scenario="<<scenario<<" cycle="<<cycle<<" phase="<<count<<" H8pc="<<std::hex<<pc<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';ramEqual(a->memory,b->memory,frameWritten,tapes,phases);tapes.flush();throw std::runtime_error("Native owner/H8 resources/time differ");}check(a->exr==owner->semanticMask(owner->mainMask()),"Semantic priority differs");++phases;check(++count<4000000,"Phase bound");
  }while(owner->metrics().mainReturns<returnsBefore+cycle+1);
   check(uint32_t(a->getPC())==0x4338&&!a->indent,"Original H8 did not wrap with native owner");check(!vector||injected||(critical&&cycle>0),"Requested real IRQ site not reached");
   check(owner->metrics().mainReturns==returnsBefore+cycle+1&&!owner->activeMain(),"Cyclic owner return");check(ramEqual(a->memory,b->memory,frameWritten,tapes,phases),"Full RAM differs outside bytes actually written as retired frames");const auto&out=owner->result();check(out.expressionSource==uint32_t(a->regs[5].er)&&out.activation==uint8_t(a->regs[2].rh),"Genuine domain result differs");check(owner->nextInput().presentation==out.presentation&&owner->nextInput().activation==out.activation&&owner->nextInput().expressionSource==out.expressionSource,"Next input not pure result");std::cout<<"PASS owner H8 api="<<(oneShot?"one-shot":"checked")<<" scenario="<<scenario<<" cycle="<<cycle<<" phases="<<count<<" irqVector="<<vector<<" child="<<child<<" nested="<<nestedVector<<" lastreadNative/H8="<<b->lastread<<'/'<<a->lastread<<std::endl;++cyclesDone;
  }
  check(owner->metrics().interruptStarts==irqBefore+expectedHere&&owner->metrics().interruptReturns==irqReturnedBefore+expectedHere&&!owner->activeInterrupt(),"Actual IRQ admission/return count differs");irqExpected+=expectedHere;irqActual+=owner->metrics().interruptReturns-irqReturnedBefore;check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"Pending IRQ masks differ");
  owner->requestBoundaryStop();check(!owner->playbackEnabled(),"Boundary stop did not publish");
 };
 for(bool useOneShot:{false,true}){oneShot=useOneShot;
 for(unsigned scenario:{0u,3u,1u,5u}){cfg=Config{};cfg.key=0;cfg.pattern=scenario==1?0:scenario;run(scenario);}check(!allocations,"Native owner allocated");for(unsigned vector:{28u,32u,42u,88u,89u,90u,91u}){cfg=Config{};cfg.key=0;cfg.pattern=0;run(0,vector);}
 for(int child=0;child<17;++child){cfg=Config{};cfg.key=0;cfg.pattern=0;const unsigned vectors[]{28,32,42,88,89,90,91};run(0,vectors[child%7],child);}
 for(auto pair:std::array<std::array<unsigned,2>,2>{{{{28,32}},{{32,89}}}}){cfg=Config{};cfg.key=0;cfg.pattern=0;run(0,pair[0],-1,pair[1]);}
 cfg=Config{};cfg.key=0;cfg.pattern=0;run(1,32,-1,0,true);
 }
 check(maskGuardContexts[0]&&maskGuardContexts[1]&&invalidForegroundMaskGuards==4,"Invalid foreground mask active/inactive coverage missing");
 check(emptyMainGuards&&activeEmptyIrqGuards&&emptyMainPriorities[0]&&emptyMainPriorities[7],"Empty playback/active IRQ priority coverage missing");
 check(!allocations&&irqExpected==irqActual,"Final IRQ/alloc accounting");for(auto n:owner->metrics().children)check(n==cyclesDone,"Actual child return count");std::cout<<"PASS actualH8/newDevice/nativeowner cycles="<<cyclesDone<<" phases="<<phases<<" portableCostMinus1Rejects="<<rejects<<" wholeObjectSnapshotRejects="<<snapshots<<" freshPlanMutationGuards="<<freshPlanGuards<<" queryBeforeIrq="<<queryBeforeIrq<<" invalidForegroundMaskGuards="<<invalidForegroundMaskGuards<<" emptyMainGuards="<<emptyMainGuards<<" activeEmptyIrqGuards="<<activeEmptyIrqGuards<<" emptyMainPriority0/6/7="<<emptyMainPriorities[0]<<'/'<<emptyMainPriorities[6]<<'/'<<emptyMainPriorities[7]<<" allocations="<<allocations<<" IRQactual="<<irqActual<<" nested="<<nestedCases<<" heldCritical="<<criticalHeld<<" comparedDevice="<<meanings[0]<<" verifiedFetch="<<meanings[1]<<" selectedVectorBytes="<<meanings[2]<<" actualFrameTransfers="<<meanings[3]<<" noControllerCpu/Compatibility/ksLib\n";tapes.flush();return 0;
 }catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
