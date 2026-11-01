#pragma once
#include <array>
#include <cstdint>
namespace ks {class StateDspFactoryUploadTask final {public:enum class Stage {ResetBegin,SetInitialPayload,SetInitialAddress,WriteInitial,SetCoefficient,SetNumerator,DivideFirst,CaptureQuotient,CopyRemainder,ClearRemainderHigh,DivideSecond,CaptureFraction,WriteCoefficient,AdvanceCoefficient,CompareCoefficient,AdmitCoefficient,WaitInitial,SetDescriptors,SetSource,SetDescriptorCount,RetainDescriptorCount,ReadDescriptor,ReadAddress,CompareEmpty,AdmitEmpty,CompareLarge,AdmitLarge,CaptureSegments,ShiftSegmentFirst,ShiftSegmentSecond,ShiftSegmentThird,ShiftSegmentFourth,ShiftSegmentFifth,SetSegmentCount,RetainSegments,RetainDescriptors,RetainAddress,UploadSegment,RestoreAddress,RetainCommittedAddress,RetainSource,SetSegmentAddress,WriteSegment,WaitSegment,RestoreSource,RestoreCommittedAddress,RestoreDescriptors,RestoreSegments,AdvanceAddress,DecrementSegments,AdmitSegments,SkipDirect,RetainDirectDescriptors,RetainDirectAddress,UploadDirect,RestoreDirectAddress,RetainDirectSource,SetDirectAddress,WriteDirect,WaitDirect,RestoreDirectSource,RestoreDirectDescriptors,RestoreDescriptorCount,DecrementDescriptorCount,AdmitDescriptors,ResetEnd,SetFinalPayload,SetFinalAddress,WriteFinal,SetDoneAddress,ReadDone,TestDone,AdmitDone,Return,AwaitChild,Complete};enum class Read {Descriptor,Address};enum class Child {Reset,Initial,Coefficient,Wait,Upload,Commit,Final,Done};Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingChild()const{return phase_==Stage::AwaitChild;}Child child()const{return child_;}unsigned descriptorIndex()const{return descriptor_;}unsigned coefficientIndex()const{return coefficient_;}uint32_t source()const{return source_;}uint32_t descriptorAddress()const{return address_;}uint16_t sampleCount()const{return sampleCount_;}uint32_t payload()const{return child_==Child::Initial?0x7fffff00u:child_==Child::Coefficient?uint32_t(quotient_)<<16|fraction_:child_==Child::Final?0xffffff00u:address_;}bool resumeChild(uint32_t result=0){if(!awaitingChild())return false;if(child_==Child::Upload)source_=result;if(child_==Child::Done)done_=uint8_t(result);phase_=resume_;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,74> costs{{20,9,6,20,6,9,23,3,3,3,23,3,20,3,6,3,20,9,9,3,11,14,14,3,8,6,3,3,3,3,3,3,3,6,20,20,20,20,20,20,20,6,20,15,20,20,20,20,9,3,3,3,20,20,15,20,20,6,20,15,20,20,11,3,8,20,9,6,20,6,20,3,3,17}};return unsigned(phase_)<costs.size()?costs[unsigned(phase_)]:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto c=nextClocks()){if(c>available-used)break;switch(phase_){case Stage::ResetBegin:await(Child::Reset,Stage::SetInitialPayload);break;
case Stage::WriteInitial:await(Child::Initial,Stage::SetCoefficient);break;
case Stage::SetCoefficient:coefficient_=2;next();break;
case Stage::DivideFirst:quotient_=uint16_t(0x8000u/coefficient_);remainder_=uint16_t(0x8000u%coefficient_);next();break;
case Stage::DivideSecond:fraction_=uint16_t(remainder_/coefficient_);next();break;
case Stage::WriteCoefficient:await(Child::Coefficient,Stage::AdvanceCoefficient);break;
case Stage::AdvanceCoefficient:++coefficient_;next();break;
case Stage::AdmitCoefficient:phase_=coefficient_<=1024?Stage::SetNumerator:Stage::WaitInitial;break;
case Stage::WaitInitial:await(Child::Wait,Stage::SetDescriptors);break;
case Stage::ReadDescriptor:count_=uint16_t(uint32_t(read(Read::Descriptor))>>16);sampleCount_=count_;next();break;
case Stage::ReadAddress:address_=uint32_t(read(Read::Address));next();break;
case Stage::AdmitEmpty:phase_=count_?Stage::CompareLarge:Stage::RestoreDescriptorCount;break;
case Stage::AdmitLarge:phase_=count_<=2048?Stage::RetainDirectDescriptors:Stage::CaptureSegments;break;
case Stage::CaptureSegments:segments_=count_;next();break;
case Stage::ShiftSegmentFirst:segments_>>=2;next();break;
case Stage::ShiftSegmentSecond:segments_>>=2;next();break;
case Stage::ShiftSegmentThird:segments_>>=2;next();break;
case Stage::ShiftSegmentFourth:segments_>>=2;next();break;
case Stage::ShiftSegmentFifth:segments_>>=2;next();break;
case Stage::SetSegmentCount:sampleCount_=1024;next();break;
case Stage::UploadSegment:await(Child::Upload,Stage::RestoreAddress);break;
case Stage::WriteSegment:await(Child::Commit,Stage::WaitSegment);break;
case Stage::WaitSegment:await(Child::Wait,Stage::RestoreSource);break;
case Stage::AdvanceAddress:address_+=0x40000;next();break;
case Stage::DecrementSegments:--segments_;next();break;
case Stage::AdmitSegments:phase_=segments_?Stage::RetainSegments:Stage::SkipDirect;break;
case Stage::SkipDirect:phase_=Stage::RestoreDescriptorCount;break;
case Stage::UploadDirect:await(Child::Upload,Stage::RestoreDirectAddress);break;
case Stage::WriteDirect:await(Child::Commit,Stage::WaitDirect);break;
case Stage::WaitDirect:await(Child::Wait,Stage::RestoreDirectSource);break;
case Stage::DecrementDescriptorCount:++descriptor_;next();break;
case Stage::AdmitDescriptors:phase_=descriptor_<24?Stage::RetainDescriptorCount:Stage::ResetEnd;break;
case Stage::ResetEnd:await(Child::Reset,Stage::SetFinalPayload);break;
case Stage::WriteFinal:await(Child::Final,Stage::SetDoneAddress);break;
case Stage::ReadDone:await(Child::Done,Stage::TestDone);break;
case Stage::AdmitDone:phase_=done_?Stage::Return:Stage::SetDoneAddress;break;
case Stage::Return:phase_=Stage::Complete;break; default:next();break;}used+=c;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}void await(Child c,Stage s){child_=c;resume_=s;phase_=Stage::AwaitChild;}Stage phase_=Stage::ResetBegin,resume_=Stage::Complete;Child child_=Child::Reset;uint32_t source_=0x860000,address_=0;unsigned descriptor_=0;uint16_t coefficient_=0,quotient_=0,remainder_=0,fraction_=0,count_=0,sampleCount_=0,segments_=0;uint8_t done_=0;};}
