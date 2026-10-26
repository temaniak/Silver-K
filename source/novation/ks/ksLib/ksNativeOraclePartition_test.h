#pragma once
#include <array>
#include <vector>
#include <tuple>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
namespace ks::nativeOracleTest {
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Raw {
 bool write;uint32_t address;uint8_t value;uint64_t clock;uint32_t pc,sp;
 Raw(bool w,uint32_t a,uint8_t v,uint64_t c,uint32_t p,uint32_t s):write(w),address(a),value(v),clock(c),pc(p),sp(s){}
};
enum class Meaning:uint8_t {Device=0,OpcodeFetch=1,AdmittedVector=2,RetiredFrame=3};
struct Before {
 uint32_t pc=0,sp=0,rootSp=0,entry=0,vectorWord=0;
 uint64_t pending=0,pendingHigh=0,clock=0;uint8_t mask=0;bool mode2=false;
 std::array<uint8_t,128>priorities{};std::array<uint8_t,10>opcode{};
 int selected=-1;unsigned opcodeBytes=0;
};
struct Partition {
 std::vector<Access>device;std::vector<Meaning>meaning;
 uint32_t frameLow=0;unsigned fetchFirst=0,fetchBytes=0;
};
inline void require(bool b,const char*why){if(!b)throw std::logic_error(why);}
// Independently predict the oracle's mode-2 admission from its own state.
inline int selected(const Before&b){
 int vector=-1;unsigned level=b.mask&7;if(!b.mode2)return vector;
 for(unsigned v=0;v<128;++v)if(((v<64?b.pending:b.pendingHigh)>>(v&63))&1)
  if(b.priorities[v]>level){level=b.priorities[v];vector=int(v);}
 return vector;
}
inline Partition partition(const std::vector<Raw>&raw,const std::vector<Access>&native,const Before&b,uint32_t afterSp){
 require(b.selected==selected(b),"Oracle IRQ prediction differs from its pending/priority/mask");
 require(b.rootSp<=0x1000000&&b.rootSp>=0xffdc00,"Invalid explicit root stack bound");
 require(b.sp>=0xffdc00&&b.sp<=b.rootSp&&afterSp>=0xffdc00&&afterSp<=b.rootSp,"Oracle stack leaves its admitted live RAM");
 Partition p;p.frameLow=std::min(b.sp,afterSp);p.meaning.assign(raw.size(),Meaning::Device);
 for(const auto&e:raw){require(e.sp>=0xffdc00&&e.sp<=b.rootSp,"Observed oracle stack outside live RAM");p.frameLow=std::min(p.frameLow,e.sp);}
 const auto frame=[&](uint32_t a){return a>=p.frameLow&&a<b.rootSp;};
 // Native code may never read or write an actual live machine frame.
 for(const auto&e:native)require(!frame(std::get<1>(e)),"Native resource touched an actual retired machine frame");
 if(b.selected>=0){
  // Exact original mode-2 entry: PC/CCR word then EXR word, then the four
  // bytes of this selected vector. No other vector-table address is excluded.
  require(raw.size()>=10,"Truncated actual IRQ entry");
  const std::array<uint32_t,6>stack{{b.sp-4,b.sp-3,b.sp-2,b.sp-1,b.sp-6,b.sp-5}};
  for(unsigned i=0;i<6;++i){require(raw[i].write&&raw[i].address==stack[i]&&frame(raw[i].address),"Actual IRQ save prefix differs");p.meaning[i]=Meaning::RetiredFrame;}
  uint32_t word=0;for(unsigned i=0;i<4;++i){const auto&e=raw[6+i];require(!e.write&&e.address==unsigned(b.selected)*4+i,"Selected IRQ vector reads are not the actual contiguous prefix");word=(word<<8)|e.value;p.meaning[6+i]=Meaning::AdmittedVector;}
  require(word==b.vectorWord&&((word&0xffffff)&~1u)==b.entry,"Observed IRQ vector did not enter the predicted handler");p.fetchFirst=10;
 }else{require(b.entry==(b.pc&~1u),"Ordinary opcode entry differs");}
 require(b.opcodeBytes>=2&&b.opcodeBytes<=10&&!(b.opcodeBytes&1),"Invalid decoded opcode length");
 require(raw.size()>=p.fetchFirst+b.opcodeBytes,"Truncated opcode fetch prefix");
 require(raw[p.fetchFirst].pc==b.entry,"Actual opcode entry was not observed");
 for(unsigned n=0;n<b.opcodeBytes;++n){const auto&e=raw[p.fetchFirst+n];require(!e.write&&e.address==b.entry+n&&e.value==b.opcode[n],"Opcode fetch prefix is not the exact contiguous decoded bytes");require(!frame(e.address),"Opcode fetched from an actual machine frame");p.meaning[p.fetchFirst+n]=Meaning::OpcodeFetch;}
 p.fetchBytes=b.opcodeBytes;
 for(unsigned i=0;i<raw.size();++i){const auto&e=raw[i];
  if(p.meaning[i]==Meaning::Device&&frame(e.address))p.meaning[i]=Meaning::RetiredFrame;
  if(p.meaning[i]==Meaning::Device)p.device.emplace_back(e.write,e.address,e.value,e.clock);
 }
 return p;
}
}
