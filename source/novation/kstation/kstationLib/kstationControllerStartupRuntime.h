#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationStartupTask.h"
#include "kstationResetImageTask.h"
#include <optional>
namespace kstation {
// Reset resource binding. Early image installation deliberately accepts an
// unbound interrupt runtime only while its mask is disabled. The real image
// copy and generated trampoline publish the board overlay before IRQ/DSP
// bindings may verify and protect the physical image.
class ControllerStartupRuntime final {
 using F=StartupTask;using T=ResetImageTask;using S=T::Stage;
public:
 using ImageChild=T::Child;
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,UnsupportedImage,AwaitService};
 struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 struct Range{uint32_t begin,end;};
 inline static constexpr std::array<Range,5>CodeRanges{{{0x100,0x104},{0x142,0x15c},{0x1bc,0x240},{0x280,0x42c},{0x3a58,0x3a6c}}};
 bool bindBootResources(ControllerDeviceState&d,const uint8_t*image,size_t size){
  if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(CodeRanges.size()))return false;
  for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a])return false;
  for(auto r:CodeRanges)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin))return false;
  bound_=&d;expected_=image;return true;
 }
 bool bindMirroredFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_!=&d||mirrored_||!image||size!=49728||!d.hasImmutableCapacity(CodeRanges.size()))return false;for(auto r:CodeRanges)for(auto a=r.begin;a<r.end;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;for(auto r:CodeRanges)if(!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;mirrored_=true;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}
 bool active()const{return leaf_.has_value()||image_.has_value();}void reset(){leaf_.reset();image_.reset();copied_=0;imageMatches_=true;}
 bool startLeaf(F::Kind kind){if(!bound_||active())return false;leaf_.emplace(kind);return true;}
 bool startImage(T::Entry entry=T::Entry::Verify){if(!bound_||active()||entry==T::Entry::Relocated)return false;copied_=0;imageMatches_=true;image_.emplace(entry);return true;}
 const T*imageTask()const{return image_?&*image_:nullptr;}const F*leafTask()const{return leaf_?&*leaf_:nullptr;}
 ImageChild child()const{return image_?image_->child():ImageChild::None;}
 bool resumeChild(ImageChild expected){return image_&&expected!=ImageChild::None&&image_->child()==expected&&image_->resumeChild();}
 bool held()const{return image_&&image_->held();}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{
  if(bound_!=&d||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(!irq.boundTo(d)&&irq.mask()!=ControllerInterruptRuntime::Mask::Disabled)||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;
  if(image_&&image_->stage()==S::Mask&&!supportedImage())return 0;
  const auto n=leaf_?leaf_->nextClocks(writeCredits(d),7):image_->nextClocks(credits(d));return n?n+(d.pendingIrqs?1u:0u):0;
 }
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned available){
  if(bound_!=&d)return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};
  if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};
  if(irq.active()||(!irq.boundTo(d)&&irq.mask()!=ControllerInterruptRuntime::Mask::Disabled)||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};
  if(child()!=ImageChild::None)return {0,false,Block::AwaitService};
  if(image_&&image_->stage()==S::Mask&&!supportedImage())return {0,false,Block::UnsupportedImage};
  auto clocks=nextClocks(d,irq),arb=d.pendingIrqs?1u:0u;if(!clocks||available<clocks)return {};if(arb)(void)d.read8(0xfffff2);clocks-=arb;const auto begin=d.cycles;bool stackWrite=false,done=false;
  if(leaf_){auto&task=*leaf_;auto st=task.stage();const auto wc=writeCredits(d);auto used=task.advanceBefore(clocks,wc,7,[&](F::Publication p){d.cycles=begin+3;write(d,address(p),p.value,p.width);});if(used!=clocks)throw std::logic_error("K startup leaf phase");done=task.complete();(void)st;
  }else{auto&task=*image_;const auto stage=task.stage();auto cr=credits(d);if(stage==S::BindImage||stage==S::BindRom){copied_=0;imageMatches_=true;}
   stackWrite=stage==S::PublishError||stage==S::PublishUpdate||stage==S::PublishDone||stage==S::UpdateFirmware||stage==S::RetainDelay||stage==S::RetainInner;
   if(stage==S::Mask)irq.setMask(ControllerInterruptRuntime::Mask::Disabled);
   const auto used=task.advanceBefore(clocks,cr,[&](T::Read field,unsigned index){unsigned a=field==T::Read::Signature?0x110+index:field==T::Read::StoredSignature?0x210110+index:field==T::Read::Version?0x138:field==T::Read::StoredVersion?0x210138:field==T::Read::ImageSize?task.sourceBase()+0x104:field==T::Read::ImageWord||field==T::Read::ExpectedChecksum?task.sourceBase()+index:field==T::Read::Trampoline?0x420+index:0xfffe3d;unsigned width=field==T::Read::Version||field==T::Read::StoredVersion||field==T::Read::ImageWord?2:field==T::Read::ImageSize||field==T::Read::ExpectedChecksum?4:1;unsigned trailing=field==T::Read::Signature||field==T::Read::StoredSignature||field==T::Read::ImageWord||field==T::Read::ExpectedChecksum||field==T::Read::Trampoline?2:0;d.cycles=begin+clocks-cr.read-trailing;return read(d,a,width);},[&](T::Write field,unsigned index,uint32_t value){unsigned a=field==T::Write::Trampoline?0xfffd10+index:field==T::Write::Overlay?0xffffcf:field==T::Write::AccessState?0xffffed:task.targetBase()+index;unsigned width=field==T::Write::Checksum?4:field==T::Write::ImageWord||field==T::Write::Tail?2:1;if(field==T::Write::ImageWord&&task.targetBase()==0x400000){if(index+1>=49728||uint16_t(value)!=uint16_t((unsigned(expected_[index])<<8)|expected_[index+1]))imageMatches_=false;copied_+=2;}d.cycles=begin+clocks-cr.write;write(d,a,value,width);});
   if(used!=clocks)throw std::logic_error("K reset image phase");done=task.complete();
  }
  d.cycles=begin+clocks;d.lastRead=-1;if(stackWrite)d.lastWrite=-1;
  if(done){leaf_.reset();image_.reset();}return {clocks+arb,done,Block::None};
 }
private:
 static unsigned address(F::Publication p){switch(p.field){case F::Field::P6Data:return 0xffffcb;case F::Field::P6Direction:return 0xffffc9;case F::Field::P8Data:return 0xffffcf;case F::Field::P8Direction:return 0xffffcd;case F::Field::P9Data:return 0xffffd2;case F::Field::P9Direction:return 0xffffd0;case F::Field::PAData:return 0xffffd3;case F::Field::PADirection:return 0xffffd1;case F::Field::PBData:return 0xffffd6;case F::Field::PBDirection:return 0xffffd4;case F::Field::P4Direction:return 0xffffc5;case F::Field::AccessState:return 0xffffed;case F::Field::WaitControl:return 0xffffee;case F::Field::InternalClear:return 0xfffd10+p.index*2;case F::Field::MainScratch:return 0xcd6c+p.index;}return 0;}
 static unsigned bus(unsigned a,unsigned width,int last){unsigned clocks=0;for(unsigned i=0;i<width;++i){auto at=(a+i)&0xffffff;if(at<0x400000||(at>=0xfffd10&&at<0xffffa0)){if((last&1)||at!=unsigned(last|1))clocks+=at<0x400000?3:1;last=int(at);}else{clocks+=at>=0xffffa0?1:3;last=-1;}}return clocks;}
 static uint32_t read(ControllerDeviceState&d,unsigned a,unsigned width){uint32_t v=0;for(unsigned i=0;i<width;++i)v=(v<<8)|d.read8(a+i);return v;}
 static void write(ControllerDeviceState&d,unsigned a,uint32_t v,unsigned width){for(unsigned i=0;i<width;++i)d.write8(a+i,uint8_t(v>>(8*(width-i-1))));}
 unsigned writeCredits(const ControllerDeviceState&d)const{auto p=leaf_->publication();return bus(address(p),p.width,d.lastWrite);}
 T::Credits credits(const ControllerDeviceState&d)const{T::Credits cr;cr.call=10;cr.shortCall=5;cr.ret=7;cr.push=6;cr.pop=6;auto&t=*image_;switch(t.stage()){
 case S::ReadSignature:cr.read=bus(0x110+t.index(),1,-1);break;case S::ReadStoredSignature:cr.read=bus(0x210110+t.targetIndex(),1,-1);break;case S::ReadVersion:cr.read=bus(0x138,2,-1);break;case S::ReadStoredVersion:cr.read=bus(0x210138,2,-1);break;case S::ReadSize:case S::ReadWarmSize:cr.read=bus(t.sourceBase()+0x104,4,-1);break;case S::ReadWord:cr.read=bus(t.sourceBase()+t.index(),2,-1);break;case S::ReadChecksum:cr.read=bus(t.sourceBase()+t.index(),4,-1);break;case S::ReadMarker:case S::ReadWarmMarker:cr.read=1;break;case S::ReadTrampoline:cr.read=3;break;
 case S::WriteWord:cr.write=bus(t.targetBase()+t.index()-2,2,d.lastWrite);break;case S::WriteChecksum:cr.write=bus(t.targetBase()+t.index()-4,4,d.lastWrite);break;case S::WriteTail:cr.write=bus(t.targetBase()+t.index(),2,d.lastWrite);break;case S::WriteTrampoline:cr.write=bus(0xfffd10+t.targetIndex(),1,d.lastWrite);break;case S::PublishOverlay:case S::PublishAccessState:cr.write=1;break;default:break;}return cr;}
 bool supportedImage()const{return copied_==49728&&imageMatches_;}
 ControllerDeviceState*bound_=nullptr;bool mirrored_=false;const uint8_t*expected_=nullptr;unsigned copied_=0;bool imageMatches_=true;std::optional<F>leaf_;std::optional<T>image_;
};
}
