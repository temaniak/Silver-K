#pragma once
#include "ksMidiFileTransportService.h"
#include "ksMidiFileTransportCompatibility.h"
#include "ksMidiFileTransportResolver.h"
namespace ks {class ControllerCpu;
class MidiFileTransportForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const MidiFileTransportService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;returnedRequests.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,23>returnedRequests{};
private:
 using Image=MidiFileTransportCompatibility::Image;
 struct Job{Job(MidiFileTransportService::Input i,const Image&a,const uint8_t*verified,uint32_t sp,int d,uint8_t p):service(i),accounting(a),caller(sp),depth(d),priority(p){metadata.assign(verified);}MidiFileTransportResolver::Metadata metadata;MidiFileTransportService service;MidiFileTransportCompatibility accounting;std::array<uint64_t,23>returns{};uint32_t caller=0;int depth=0;uint8_t priority=0;};
 struct Observer{Job&j;template<class...A>void operator()(A&&...a){j.accounting(std::forward<A>(a)...);}void operator()(MidiFileTransportService::Notice n,const MidiFileTransportService&s){j.accounting(n,s);if(n==MidiFileTransportService::Notice::ChildComplete)++j.returns[unsigned(s.child())];}};
 std::optional<Job>job_;
};}
