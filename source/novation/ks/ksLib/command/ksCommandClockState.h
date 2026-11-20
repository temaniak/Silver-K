#pragma once

#include "../../../controller/command/controllerEvent.h"
#include <array>
#include <limits>
#include <optional>

namespace novation::controller::command {
enum class KSClockRequestKind:uint8_t { Pulse,Start,Continue,Stop,SongPosition,TempoSetting,SourceSetting };
enum class KSClockOrigin:uint8_t { Midi,Host,Internal,Parameter };
enum class KSClockAdmission:uint8_t { Accepted,Unhandled,Invalid,StaleSession,Backwards,Full,Exhausted };
struct KSClockRequest {
 Session session{};uint64_t sequence=0;NativeFrame at{};
 KSClockRequestKind kind=KSClockRequestKind::Pulse;KSClockOrigin origin=KSClockOrigin::Midi;
 uint16_t value=0;
 friend constexpr bool operator==(KSClockRequest a,KSClockRequest b)noexcept {
  return a.session==b.session && a.sequence==b.sequence && a.at==b.at && a.kind==b.kind && a.origin==b.origin && a.value==b.value;
 }
};

// P06's complete native clock/transport ingress boundary for P08. This owns
// accepted pending requests, not an H8 clock servo, a guessed arp phase or
// elapsed instruction work. Origins stay explicit for the P08 clock policy.
// A consumer acknowledges only after applying the exact typed request.
class KSCommandClockState final {
public:
 static constexpr size_t Capacity=64;
 struct Projection {
  Session session{};uint64_t accepted=0,acknowledged=0,pulses=0;size_t pending=0,highWater=0;
  std::optional<KSClockRequest> lastApplied;
 };
 explicit KSCommandClockState(Session session)noexcept:session_(session){}
 KSCommandClockState(const KSCommandClockState&)=delete;
 KSCommandClockState& operator=(const KSCommandClockState&)=delete;

 KSClockAdmission admit(Session session,NativeFrame at,const EventPayload& event,KSClockOrigin origin=KSClockOrigin::Midi)noexcept {
  if(session!=session_ || !session.owner || !session.generation)return KSClockAdmission::StaleSession;
  if(unsigned(origin)>unsigned(KSClockOrigin::Parameter))return KSClockAdmission::Invalid;
  if(const auto* realtime=std::get_if<MidiRealtime>(&event)) {
   switch(realtime->status) {
    case 0xf8:return push(at,KSClockRequestKind::Pulse,0,origin);
    case 0xfa:return push(at,KSClockRequestKind::Start,0,origin);
    case 0xfb:return push(at,KSClockRequestKind::Continue,0,origin);
    case 0xfc:return push(at,KSClockRequestKind::Stop,0,origin);
    default:return KSClockAdmission::Unhandled;
   }
  }
  if(const auto* common=std::get_if<MidiCommon>(&event)) {
   if(common->status!=0xf2)return KSClockAdmission::Unhandled;
   if(common->data1>=128 || common->data2>=128)return KSClockAdmission::Invalid;
   return push(at,KSClockRequestKind::SongPosition,uint16_t(common->data1|(uint16_t(common->data2)<<7)),origin);
  }
  return KSClockAdmission::Unhandled;
 }
 KSClockAdmission parameter(Session session,NativeFrame at,uint8_t field,uint8_t value)noexcept {
  if(session!=session_ || !session.owner || !session.generation)return KSClockAdmission::StaleSession;
  if(field!=84 && field!=243)return KSClockAdmission::Unhandled;
  if(value>=128)return KSClockAdmission::Invalid;
  return push(at,field==84?KSClockRequestKind::TempoSetting:KSClockRequestKind::SourceSetting,value,KSClockOrigin::Parameter);
 }
 std::optional<KSClockRequest> front()const noexcept{return count_?std::optional{requests_[head_]}:std::nullopt;}
 bool acknowledge(const KSClockRequest& request)noexcept {
  if(!count_ || !(request==requests_[head_]))return false;
  lastApplied_=request;head_=(head_+1)%Capacity;--count_;++acknowledged_;return true;
 }
 Projection projection()const noexcept{return {session_,accepted_,acknowledged_,pulses_,count_,highWater_,lastApplied_};}
 std::optional<NativeFrame> lastAdmission()const noexcept{return lastAccepted_;}
 size_t available()const noexcept{return Capacity-count_;}
private:
 KSClockAdmission push(NativeFrame at,KSClockRequestKind kind,uint16_t value,KSClockOrigin origin)noexcept {
  if(lastAccepted_ && at<*lastAccepted_)return KSClockAdmission::Backwards;
  if(accepted_==std::numeric_limits<uint64_t>::max())return KSClockAdmission::Exhausted;
  if(count_==Capacity)return KSClockAdmission::Full;
  const KSClockRequest request{session_,accepted_+1,at,kind,origin,value};
  requests_[(head_+count_)%Capacity]=request;++count_;++accepted_;
  if(kind==KSClockRequestKind::Pulse)++pulses_;
  if(highWater_<count_)highWater_=count_;lastAccepted_=at;return KSClockAdmission::Accepted;
 }
 Session session_;std::array<KSClockRequest,Capacity> requests_{};
 size_t head_=0,count_=0,highWater_=0;uint64_t accepted_=0,acknowledged_=0,pulses_=0;
 std::optional<NativeFrame> lastAccepted_;std::optional<KSClockRequest> lastApplied_;
};
}
