#include "ksMidiPacketService.h"
#include "ksMidiPacketAbi.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 using T=ks::MidiPacketService;using K=T::Kind;std::vector<uint8_t>memory(1<<24);uint64_t cases=0,phases=0,rejects=0,polls=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<4096,"Pedal fixed storage grew unexpectedly");
 for(unsigned mode=0;mode<256;++mode)for(unsigned event:{0u,0x7f80u,0x80ffu,0xffffu})for(auto kind:{K::NoteOff,K::NoteOn,K::Pedal,K::Expression,K::PitchBend,K::Modulation,K::Pressure,K::Byte,K::QueueOff,K::QueueOn,K::TransmitOff,K::TransmitOn,K::Panic,K::Control,K::RawControl,K::Program}){
  const auto word=[&](uint32_t a,unsigned v){memory[a]=uint8_t(v>>8);memory[a+1]=uint8_t(v);};std::fill(memory.begin()+0x15000,memory.begin()+0x30000,uint8_t(0));std::fill(memory.begin()+0xffed00,memory.begin()+0xffef00,uint8_t(0));
  memory[0x169ed]=uint8_t(mode);memory[0x17526]=uint8_t(mode);memory[0x169b1]=uint8_t(mode^7);memory[0x16798]=uint8_t(mode);memory[0x174ae +20*mode+11]=uint8_t(mode^7);word(0x15b04,255);word(0x15b06,0);word(0xffed6c,254);word(0xffed6e,255);
  unsigned count=0,waits=0;int previous=-1;tracking=true;
  {T service(kind,{uint16_t(event),uint16_t(event),uint8_t(mode)});while(!service.complete()){
   const auto cost=service.nextClocks(previous);check(cost>0,"Pure Pedal stalled");std::array<unsigned char,sizeof(T)>snapshot{};std::memcpy(snapshot.data(),&service,sizeof(T));unsigned callbacks=0;
   const auto noRead=[&](uint32_t,unsigned){++callbacks;return uint8_t(0);};const auto noWrite=[&](uint32_t,uint8_t,unsigned){++callbacks;};check(service.advanceBefore(cost-1,noRead,noWrite,previous)==0&&!callbacks&&!std::memcmp(snapshot.data(),&service,sizeof(T)),"Pure insufficient budget changed graph");++rejects;
   const auto*serial=service.task<ks::SerialForegroundService>();if(serial&&serial->atTransmit(ks::SerialTransmitTask::Stage::ReadConsumer)&&++waits==5)word(0xffed6e,17);
   const auto read=[&](uint32_t a,unsigned dt){check(a<1u<<24&&dt<=cost,"Pure read span/time");++reads;return memory[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(a<1u<<24&&dt<=cost&&!(a>=0xfff000&&a<0xfffc00),"Pure write span/time/scratch");memory[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;++writes;};
   check(service.advanceBefore(cost,read,write,previous)==cost,"Pure Pedal budget");if(service.effect().write)previous=-1;check(++count<10000,"Pure Pedal did not finish");++phases;
  }
  const auto&s=service.summary();const unsigned expectedBytes=kind==K::Byte?1:kind==K::Pressure||kind==K::Program?2:3;check(s.bytes==expectedBytes,"Pure actual completed packet bytes");
  if(kind==K::QueueOn||kind==K::QueueOff){for(const auto&v:s.output)check(v.queued&&!v.serial,"Queue packet must not transmit UART");check(uint8_t(s.output[0].event)==uint8_t((mode^7)|(kind==K::QueueOn?0x90:0x80)),"Queue status");check(uint8_t(s.output[1].event)==uint8_t(event)&&uint8_t(s.output[2].event)==uint8_t(event>>8),"Queue payload");}
  else {for(unsigned j=0;j<s.bytes;++j)check(s.output[j].serial,"Serial packet completion");if(kind==K::Pedal||kind==K::Expression||kind==K::Modulation){check(s.output[0].queued==(mode>63),"Per-byte mirrored mode");check(uint8_t(s.output[1].event)==(kind==K::Pedal?64:kind==K::Expression?11:1),"Controller identity");check(uint8_t(s.output[2].event)==uint8_t(event>>8),"Controller value");}}
  struct A{std::array<uint32_t,7>values{};uint8_t flags=0;};A a;projectMidiPacket(a,service);check(uint16_t(a.values[6])==s.event,"Pure final projection");components|=service.components();polls+=waits;}
  tracking=false;++cases;
 }
 // Evolving invalid producer/consumer is rejected at the actual word read,
 // before the derived address or publication is used.
 unsigned guards=0;
 for(unsigned which=0;which<4;++which){T service(K::Byte,{0x7f01});memory[0x169ed]=255;for(unsigned a:{0x15b04u,0x15b06u,0xffed6cu,0xffed6eu})memory[a]=memory[a+1]=0;bool armed=false,threw=false;unsigned count=0;while(!service.complete()){
  const auto*q=service.task<ks::InternalMidiQueueTask>();const auto*s=service.task<ks::SerialForegroundService>();const bool target=which==0?(q&&q->stage()==ks::InternalMidiQueueTask::Stage::ReadPushProducer):which==1?(q&&q->stage()==ks::InternalMidiQueueTask::Stage::ReadPushConsumer):which==2?(s&&s->atTransmit(ks::SerialTransmitTask::Stage::ReadProducer)):(s&&s->atTransmit(ks::SerialTransmitTask::Stage::ReadConsumer));
  if(target){memory[which==0?0x15b04:which==1?0x15b06:which==2?0xffed6c:0xffed6e]=1;armed=true;}
  const auto cost=service.nextClocks();unsigned emitted=0;try{service.advanceBefore(cost,[&](uint32_t a,unsigned){return memory[a];},[&](uint32_t a,uint8_t v,unsigned){memory[a]=v;++emitted;});}catch(const std::logic_error&){threw=true;check(!emitted,"Live malformed index wrote at rejecting phase");break;}check(++count<1000,"Live guard stalled");
 }check(armed&&threw,"Live queue guard missed");++guards;}
 check(components==63&&!allocations,"Pure Pedal components/allocations");std::cout<<"PASS Pure PedalOutput graphs="<<cases<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" ready_reads="<<polls<<" guards="<<guards<<" reads="<<reads<<" writes="<<writes<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
