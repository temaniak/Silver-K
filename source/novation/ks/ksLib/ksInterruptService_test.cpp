#include "ksInterruptService.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
}
int main(){try{
 uint64_t cases=0,phases=0,rejections=0;
 for(unsigned kind=0;kind<3;++kind)for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer){
  ks::InterruptService task{ks::InterruptService::Kind(kind)};
  std::array<uint8_t,256> bytes{};for(unsigned i=0;i<256;++i)bytes[i]=uint8_t(i*149+producer);
  uint16_t readIndex=uint16_t(consumer);uint8_t status=uint8_t(producer*13+consumer),error=0,control=0xf0,transmitted=0;unsigned sends=0,reads=0,writes=0;
  auto read=[&](uint32_t a,unsigned when)->uint8_t{
   check(when<=task.nextClocks(),"Read crossed its phase");++reads;
   if(a==0xffff8c)return status;if(a==0xffed6c)return 0;if(a==0xffed6d)return uint8_t(producer);
   if(a==0xffed6e)return uint8_t(readIndex>>8);if(a==0xffed6f)return uint8_t(readIndex);
   check(a>=0xffed70&&a<0xffee70,"Unexpected resource read");return bytes[a-0xffed70];
  };
  auto write=[&](uint32_t a,uint8_t value,unsigned when){
   check(when<=task.nextClocks(),"Write crossed its phase");++writes;
   if(a==0xffff8c)status=value;else if(a==0xffed68)error=value;else if(a==0xffff8a)control=value;
   else if(a==0xffed6e)readIndex=uint16_t((readIndex&255)|(unsigned(value)<<8));
   else if(a==0xffed6f)readIndex=uint16_t((readIndex&0xff00)|value);
   else {check(a==0xffff8b,"Unexpected resource write");transmitted=value;++sends;}
  };
  while(auto cost=task.nextClocks()){
   auto beforeReads=reads,beforeWrites=writes;
   check(task.advanceBefore(cost-1,read,write)==0&&reads==beforeReads&&writes==beforeWrites&&task.nextClocks()==cost,"Rejected phase mutated its state/resources");++rejections;
   check(task.advanceBefore(cost,read,write)==cost,"Admitted phase did not finish");++phases;
  }
  check(task.complete()&&!task.advanceBefore(1000,read,write),"Completed interrupt restarted");
  const uint8_t initial=uint8_t(producer*13+consumer);
  if(kind==0)check(status==(initial&0xc7)&&error==2&&!sends&&readIndex==consumer,"UART error result differs");
  else if(kind==1){
   if(producer==consumer)check(control==0x70&&!sends&&readIndex==consumer&&status==initial,"Empty transmit result differs");
   else check(sends==1&&transmitted==bytes[consumer]&&readIndex==((consumer+1)&255)&&status==(initial&0x7f)&&control==0xf0,"Transmit/ring rollover result differs");
  }else check(!reads&&!writes,"Unused interrupt touched resources");
  ++cases;
 }
 std::cout<<"PASS: "<<cases<<" CPU-independent interrupt cases; "<<phases<<" phases; "<<rejections<<" insufficient-budget rejections, queue rollover/exactly-once/completion\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
