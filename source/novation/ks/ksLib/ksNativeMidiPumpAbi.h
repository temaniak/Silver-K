#pragma once
#include "ksNativeMidiPumpService.h"
#include "ksNativeMidiDecodeAbi.h"
#include "ksNativeQueuedMidiDecodeAbi.h"
#include "ksNativeMidiQueueAbi.h"
#include "ksNativeMidiInitializationAbi.h"
#include "ksControlArithmetic.h"
namespace ks {
// Temporary Stage3 compatibility accounting. This object belongs only to the
// OUTER migration host. It never belongs to any native service, supplies no
// answers to one, and never changes ControllerCpu before outer completion.
template<class A>class NativeMidiPumpAbiObserver final {
public:
 explicit NativeMidiPumpAbiObserver(A&a):external_(a){}
 void operator()(NativeMidiPumpService::Notice notice,InternalMidiServiceTask::Stage stage,const NativeMidiPumpService&s){using N=NativeMidiPumpService::Notice;using P=InternalMidiServiceTask::Stage;
 switch(notice){case N::Policy:if(stage==P::CompareReceiveByte||stage==P::CompareQueueByte||stage==P::CompareReceiveEmpty||stage==P::CompareQueueEmpty){const auto limit=stage==P::CompareReceiveByte||stage==P::CompareQueueByte?1:0;external_.flags=controlArithmetic::subtract(int8_t(external_.values[6]>>8),int8_t(limit),external_.flags).flags;}break;
 case N::Serial:if(const auto*x=s.serialOutcome();x&&x->complete()){const bool pop=x->kind()==SerialForegroundService::Kind::PopRx;if(pop)external_.values[6]=(external_.values[6]&0xffff0000u)|x->result();const auto v=pop?external_.values[0]:uint32_t(uint16_t(external_.values[0]));external_.flags=uint8_t((external_.flags&0xd0)|(v&(pop?0x80000000u:0x8000u)?8:!v?4:0));}break;
 case N::Queue:if(const auto*x=s.queueOutcome();x&&x->complete())projectNativeMidiQueue(external_,*x);break;
 case N::Decode:if(const auto*x=s.decodeOutcome();x&&x->complete())projectNativeMidiDecode(external_,*x);break;
 case N::QueuedDecode:if(const auto*x=s.queuedDecodeOutcome();x&&x->complete())projectNativeQueuedMidiDecode(external_,*x);break;
 case N::ParserReset:if(const auto*x=s.parserOutcome();x&&x->complete())projectNativeMidiInitialization(external_,*x);break;
 case N::SystemReset:case N::SysExReset:throw std::logic_error("Unexpected Pump observer scope");
 }}
private:A&external_;
};
}
