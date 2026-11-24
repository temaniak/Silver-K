#pragma once

#include "../musicalClock.h"
#include "controllerEvent.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace novation::controller::command {

// MIDI clock uses 24 pulses per quarter. Program divisions are indexed in
// the K/KS order (triplet, straight or dotted as defined by the patch).
inline constexpr std::array<uint8_t,16> ArpDivisionPulses{
    2,3,4,6,8,9,12,16,18,24,32,36,48,64,72,96};

enum class ClockAuthority : uint8_t { Internal, Host, Midi };
enum class ClockPolicy : uint8_t { Auto, Internal, ExternalMidi };

struct MusicalClockSignal {
    NativeFrame at{};
    uint64_t pulse=0;
    uint64_t epoch=0;
    ClockAuthority source=ClockAuthority::Internal;
    double framesPerPulse=0;
    bool tick=false;
    bool running=false;
    bool reset=false;
    bool freshMidiPeriod=false;
};

// One worker owns this native timeline. An input at the same frame as a
// generated tick is applied first; ticks strictly earlier must be drained.
// Stop suppresses arp ticks (running=false) but not the effects clock.
class NativeMusicalClock final {
    using Position=novation::controller::MusicalClock::Position;
    static constexpr uint64_t Max=std::numeric_limits<uint64_t>::max();
    struct Period {
        uint64_t numerator=0,denominator=1;
        double frames() const noexcept {return double(numerator)/double(denominator);}
    };
    struct Deadline { uint64_t whole=0,remainder=0; };
public:
    NativeMusicalClock(ClockRate rate,NativeFrame at={},uint32_t tempo=120000000) noexcept
        : rate_(rate),frontier_(at),tempo_(validTempo(tempo)?tempo:120000000) {
        period_=interval(rate_,tempo_);
        if(period_.numerator)next_=Deadline{at.ticks,0};
    }
    void setHostSampleRate(double rate) noexcept {
        hostSampleRate_=std::isfinite(rate) && rate>0?rate:0;
    }
    ClockAuthority source() const noexcept {return source_;}
    bool running() const noexcept {return running_;}
    uint64_t epoch() const noexcept {return epoch_;}
    double framesPerPulse() const noexcept {return period_.frames();}
    bool measuredMidiPeriod() const noexcept {return midiPeriod_.has_value();}
    uint32_t tempo() const noexcept {return tempo_;}

    std::optional<NativeFrame> nextDeadline() const noexcept {
        auto result=loss_;
        if(next_) {
            const auto due=rounded(*next_);
            if(!result || due<*result)result=due;
        }
        return result;
    }

    std::optional<MusicalClockSignal> takeDue(NativeFrame horizon) noexcept {
        const auto next=nextDeadline();
        if(!next || horizon<*next)return {};
        frontier_=*next;
        if(loss_ && *loss_==*next) {
            loss_.reset();lastMidi_.reset();midiPeriod_.reset();externalPlaying_=false;
            if(policy_==ClockPolicy::Auto) {
                source_=ClockAuthority::Internal;running_=true;
                period_=interval(rate_,tempo_);
                next_=Deadline{next->ticks,0};
            } else {
                // No false musical advances from an absent external sender.
                running_=false;next_.reset();
            }
            return resetSignal(*next);
        }
        const auto result=emitTick(*next);
        advanceDeadline();
        return result;
    }

    std::optional<MusicalClockSignal> setPolicy(NativeFrame at,ClockPolicy policy) noexcept {
        if(!canApply(at) || unsigned(policy)>unsigned(ClockPolicy::ExternalMidi))return {};
        if(policy_==policy)return {};
        frontier_=at;policy_=policy;host_.reset();
        lastMidi_.reset();midiPeriod_.reset();loss_.reset();externalPlaying_=true;
        return setSource(at,policy==ClockPolicy::ExternalMidi?ClockAuthority::Midi:ClockAuthority::Internal);
    }

    bool setTempo(NativeFrame at,uint32_t microBpm) noexcept {
        const auto wanted=interval(rate_,microBpm);
        if(!canApply(at) || !wanted.numerator)return false;
        frontier_=at;tempo_=microBpm;
        if(source_==ClockAuthority::Internal)retime(at,wanted);
        return true;
    }

    bool rebase(NativeFrame at,ClockRate rate) noexcept {
        if(!canApply(at) || !interval(rate,tempo_).numerator)return false;
        const double ratio=(double(rate.numerator)*rate_.denominator)/(double(rate_.numerator)*rate.denominator);
        const double remaining=loss_ && at<*loss_?double(loss_->ticks-at.ticks)*ratio:0;
        const uint32_t effective=host_ && source_==ClockAuthority::Host?host_->tempo:tempo_;
        const auto wanted=interval(rate,effective);
        if(!wanted.numerator)return false;
        frontier_=at;
        if(source_==ClockAuthority::Midi) {
            if(midiPeriod_)midiPeriod_=scaledMidiPeriod(midiPeriod_->numerator,ratio);
            period_=midiPeriod_.value_or(wanted);
            lastMidi_.reset();
        } else retime(at,wanted);
        rate_=rate;
        if(loss_)loss_=add(at,safeCeil(remaining));
        if(host_)host_->hasPpq=false; // next host snapshot supplies a new PPQ anchor
        return true;
    }

    std::optional<MusicalClockSignal> host(NativeFrame at,Position pos) noexcept {
        if(policy_!=ClockPolicy::Auto || !canApply(at))return {};
        const bool available=pos.available && std::isfinite(pos.bpm) && pos.bpm>=1 && pos.bpm<=1000;
        if(!available) {
            if(source_!=ClockAuthority::Host)return {};
            frontier_=at;
            host_.reset();
            return setSource(at,ClockAuthority::Internal);
        }
        const uint32_t tempo=uint32_t(std::llround(pos.bpm*1000000));
        const auto intervalNow=interval(rate_,tempo);
        if(!intervalNow.numerator)return {};
        frontier_=at;
        const bool hasPpq=pos.hasPpq && std::isfinite(pos.ppq) && std::abs(pos.ppq)<1e12;
        const bool entering=source_!=ClockAuthority::Host;
        const bool starting=pos.playing && !running_;
        const bool stopping=!pos.playing && running_;
        bool seek=false;
        if(!entering && host_ && running_ && pos.playing && hasPpq && host_->hasPpq) {
            const double step=1.0/(24*period_.frames());
            const double expected=host_->ppq+double(at.ticks-host_->at.ticks)*step;
            seek=std::abs(pos.ppq-expected)>std::max(1e-5,2*step);
        }
        if(entering || starting || seek) {
            const double ticks=hasPpq?pos.ppq*24:0;
            const double ordinal=std::ceil(ticks-1e-9-hostRoundingMargin()/intervalNow.frames());
            pulse_=ordinal>0?uint64_t(ordinal):0;
            period_=intervalNow;
            setNext(at,std::max(0.0,ordinal-ticks));
        } else retime(at,intervalNow);
        source_=ClockAuthority::Host;running_=pos.playing;
        host_=HostAnchor{at,pos.ppq,tempo,hasPpq};
        loss_.reset();lastMidi_.reset();midiPeriod_.reset();
        if(entering || starting || stopping || seek)return resetSignal(at);
        return {};
    }

    std::optional<MusicalClockSignal> midi(NativeFrame at,const EventPayload& payload) noexcept {
        if(!canApply(at) || policy_==ClockPolicy::Internal || source_==ClockAuthority::Host)return {};
        const auto* rt=std::get_if<MidiRealtime>(&payload);
        const auto* common=std::get_if<MidiCommon>(&payload);
        const bool song=common && common->status==0xf2 && common->data1<128 && common->data2<128;
        if(!song && (!rt || (rt->status!=0xf8 && rt->status!=0xfa && rt->status!=0xfb && rt->status!=0xfc)))return {};
        frontier_=at;
        const bool entering=source_!=ClockAuthority::Midi;
        source_=ClockAuthority::Midi;next_.reset();
        if(song) {
            pulse_=6u*(uint16_t(common->data1)|(uint16_t(common->data2)<<7));
            armLoss(at);
            return resetSignal(at);
        }
        if(rt->status!=0xf8) {
            externalPlaying_=rt->status!=0xfc;
            running_=externalPlaying_;
            if(rt->status==0xfa)pulse_=0;
            armLoss(at);
            return resetSignal(at);
        }
        bool measuredPeriod=false;
        if(lastMidi_ && *lastMidi_<at) {
            const auto measured=at.ticks-lastMidi_->ticks;
            // An instantaneous burst still counts musically, but must not
            // sweep effects through meaningless sub-frame tempo estimates.
            if(double(measured)>=interval(rate_,1000000000).frames()-1 &&
               double(measured)<=interval(rate_,1000000).frames()+1) {
                midiPeriod_=Period{measured,1};
                measuredPeriod=true;
            }
        }
        lastMidi_=at;period_=midiPeriod_.value_or(interval(rate_,tempo_));
        running_=externalPlaying_;
        armLoss(at);
        auto tick=emitTick(at);
        tick.freshMidiPeriod=measuredPeriod;
        if(entering) {bumpEpoch();tick.epoch=epoch_;tick.reset=true;}
        return tick;
    }

private:
    struct HostAnchor {NativeFrame at;double ppq;uint32_t tempo;bool hasPpq;};
    static constexpr bool validTempo(uint32_t bpm) noexcept {return bpm>=1000000 && bpm<=1000000000;}
    static Period interval(ClockRate rate,uint32_t tempo) noexcept {
        if(!rate.numerator || !rate.denominator || !validTempo(tempo) ||
           rate.denominator>(Max/2)/(24ull*tempo))return {};
        const auto numerator=uint64_t(rate.numerator)*60000000u;
        const auto denominator=uint64_t(rate.denominator)*24u*tempo;
        return numerator>=denominator?Period{numerator,denominator}:Period{};
    }
    static constexpr NativeFrame add(NativeFrame at,uint64_t n) noexcept {
        return {n>Max-at.ticks?Max:at.ticks+n};
    }
    double hostRoundingMargin() const noexcept {
        return hostSampleRate_>0?double(rate_.numerator)/(2.0*rate_.denominator*hostSampleRate_):0;
    }
    NativeFrame rounded(Deadline d) const noexcept {
        if(source_==ClockAuthority::Host && hostSampleRate_>0) {
            // Host MIDI is placed on the nearest host sample before it is
            // mapped to native frames. Leave room for that half-sample rounding.
            const auto fraction=double(d.remainder)/double(period_.denominator);
            return add({d.whole},safeCeil(fraction+hostRoundingMargin()));
        }
        return add({d.whole},d.remainder!=0);
    }
    static uint64_t safeCeil(double n) noexcept {
        return !(n>0)?0:n>=double(Max)?Max:uint64_t(std::ceil(n));
    }
    static std::optional<Period> scaledMidiPeriod(uint64_t p,double ratio) noexcept {
        return Period{std::max(uint64_t(1),safeCeil(double(p)*ratio)),1};
    }
    bool canApply(NativeFrame at) const noexcept {
        if(!period_.numerator || at<frontier_)return false;
        const auto next=nextDeadline();
        return !next || !( *next<at );
    }
    void setNext(NativeFrame from,double fraction) noexcept {
        const double frames=fraction*period_.frames();
        const uint64_t whole=frames>=double(Max)?Max:uint64_t(frames);
        const auto remainder=whole==Max?0:uint64_t((frames-double(whole))*double(period_.denominator));
        next_=Deadline{add(from,whole).ticks,remainder};
    }
    void retime(NativeFrame from,Period nextPeriod) noexcept {
        if(nextPeriod.numerator==period_.numerator && nextPeriod.denominator==period_.denominator)return;
        double fraction=0;
        if(next_ && next_->whole>=from.ticks && period_.numerator)
            fraction=(double(next_->whole-from.ticks)+double(next_->remainder)/double(period_.denominator))/period_.frames();
        period_=nextPeriod;
        if(next_)setNext(from,fraction);
    }
    void advanceDeadline() noexcept {
        const auto d=*next_;
        const auto remainder=d.remainder+period_.numerator%period_.denominator;
        const auto advance=period_.numerator/period_.denominator+remainder/period_.denominator;
        if(period_.numerator<period_.denominator || d.whole>Max-advance ||
           (d.whole+advance==Max && remainder%period_.denominator)) {
            next_.reset();return;
        }
        next_=Deadline{d.whole+advance,remainder%period_.denominator};
    }
    void armLoss(NativeFrame at) noexcept {
        const double quarterSecond=double(rate_.numerator)/(4.0*rate_.denominator);
        loss_=add(at,std::max(uint64_t(1),safeCeil(std::max(quarterSecond,4*period_.frames()))));
    }
    void bumpEpoch() noexcept { if(epoch_!=Max)++epoch_; }
    MusicalClockSignal resetSignal(NativeFrame at) noexcept {
        bumpEpoch();
        return {at,pulse_,epoch_,source_,period_.frames(),false,running_,true};
    }
    MusicalClockSignal emitTick(NativeFrame at) noexcept {
        const MusicalClockSignal signal{at,pulse_,epoch_,source_,period_.frames(),true,running_,false};
        if(running_ && pulse_!=Max)++pulse_;
        return signal;
    }
    MusicalClockSignal setSource(NativeFrame at,ClockAuthority source) noexcept {
        source_=source;running_=source!=ClockAuthority::Midi;
        period_=interval(rate_,tempo_);
        if(source==ClockAuthority::Internal)next_=Deadline{at.ticks,0};
        else next_.reset();
        return resetSignal(at);
    }

    ClockRate rate_;
    NativeFrame frontier_;
    uint32_t tempo_;
    Period period_{};
    ClockPolicy policy_=ClockPolicy::Auto;
    double hostSampleRate_=0;
    ClockAuthority source_=ClockAuthority::Internal;
    uint64_t pulse_=0,epoch_=1;
    bool running_=true,externalPlaying_=true;
    std::optional<Deadline> next_;
    std::optional<HostAnchor> host_;
    std::optional<Period> midiPeriod_;
    std::optional<NativeFrame> lastMidi_,loss_;
};
}
