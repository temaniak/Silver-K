#pragma once

#include "nativeMusicalClock.h"
#include <cstddef>

namespace novation::controller::command {
enum class ArpPattern : uint8_t { Up,Down,UpDown,UpDownRepeat,AsPlayed,Random,Chord };

struct ArpRhythmStep {
    uint16_t offset=0,sounding=0,tail=0;
    uint8_t velocity=0;
};

struct ArpRhythm {
    std::array<ArpRhythmStep,64> steps{};
    uint16_t units=0;
    uint8_t size=0;
};

struct ArpSettings {
    ArpPattern pattern=ArpPattern::Up;
    uint8_t division=3;
    uint8_t octaves=1;
    uint16_t gateQ16=32768;
    uint8_t fixedVelocity=0;
    uint8_t channel=0;
    bool enabled=false,keySync=false,latch=false;
    bool internal=true,midiOut=false;
    const ArpRhythm* rhythm=nullptr;
    uint16_t soundingGain=256,tailGain=0;
    bool rhythmVelocity=false;
};

struct ArpNote {
    NativeFrame at{};
    uint64_t generation=0;
    uint8_t note=0,velocity=0,channel=0;
    bool on=false,internal=false,midiOut=false;
};

// Only held pitches, musical phase and a gate deadline. The consumer owns
// actual DSP allocations; an off event is tied to the on's generation, never
// a search by MIDI pitch in the physical voice list.
class NativeArpeggiator final {
    static constexpr uint64_t Max=std::numeric_limits<uint64_t>::max();
    struct Key {uint8_t note,velocity;};
public:
    static constexpr size_t OutputCapacity=256;

    explicit NativeArpeggiator(uint32_t randomSeed=0x9e3779b9u) noexcept
        : random_(randomSeed?randomSeed:1) {}
    const ArpSettings& settings() const noexcept {return settings_;}
    bool enabled() const noexcept {return settings_.enabled;}
    size_t keyCount() const noexcept {return keyCount_;}
    std::optional<NativeFrame> nextDeadline() const noexcept {
        return stepAt_ && (!gate_ || *stepAt_<*gate_)?stepAt_:gate_;
    }
    const ArpNote* front() const noexcept {return count_?&output_[head_]:nullptr;}
    void pop() noexcept {if(count_){--count_;head_=(head_+1)%OutputCapacity;}}

    bool configure(NativeFrame at,ArpSettings next) noexcept {
        if(count_ || !canApply(at) || next.division>=ArpDivisionPulses.size() || !next.octaves || next.octaves>4 ||
           next.channel>=16 || next.fixedVelocity>=128 ||
           unsigned(next.pattern)>unsigned(ArpPattern::Chord) ||
           (next.rhythm && (!next.rhythm->units || !next.rhythm->size || next.rhythm->size>next.rhythm->steps.size())))
            return false;
        if(next.enabled!=settings_.enabled || next.pattern!=settings_.pattern || next.division!=settings_.division ||
           next.octaves!=settings_.octaves || next.rhythm!=settings_.rhythm ||
           next.internal!=settings_.internal || next.midiOut!=settings_.midiOut || next.channel!=settings_.channel) {
            close(at);
            resetStep();
        }
        if(settings_.enabled && !next.enabled) {
            keyCount_=physicalCount_=0;
            physical_.fill(false);
            velocities_.fill(0);
        }
        settings_=next;
        if(!next.latch && !pedal_)clearReleased();
        if(!keyCount_)resetStep();
        frontier_=at;
        return true;
    }

    // Flush output before each next input. A half-drained note cohort cannot
    // be mutated by a later edit or a program barrier.
    bool press(NativeFrame at,uint8_t note,uint8_t velocity) noexcept {
        if(count_ || !canApply(at) || note>=128 || !velocity || velocity>=128)return false;
        const bool wasSilent=physicalCount_==0;
        if(wasSilent && settings_.latch && !pedal_)clearReleased();
        const bool isNew=velocities_[note]==0;
        if(!physical_[note])++physicalCount_;
        physical_[note]=true;
        if(isNew)keys_[keyCount_++]=note;
        velocities_[note]=velocity;
        if(wasSilent && settings_.keySync && !gate_)resetStep();
        if(isNew && gate_ && at<*gate_ && settings_.pattern==ArpPattern::Chord)
            chordNote(at,{note,velocity},lastOctave_,lastVelocity_);
        frontier_=at;
        return true;
    }

    bool release(NativeFrame at,uint8_t note) noexcept {
        if(count_ || !canApply(at) || note>=128)return false;
        if(physical_[note])--physicalCount_;
        physical_[note]=false;
        if(!settings_.latch && !pedal_)remove(note);
        if(!keyCount_) {close(at);resetStep();}
        frontier_=at;
        return true;
    }

    bool pedal(NativeFrame at,bool down) noexcept {
        if(count_ || !canApply(at))return false;
        pedal_=down;
        if(!down && !settings_.latch)clearReleased();
        if(!keyCount_) {close(at);resetStep();}
        frontier_=at;
        return true;
    }

    bool clear(NativeFrame at) noexcept {
        if(count_ || !canApply(at))return false;
        close(at);
        keyCount_=physicalCount_=0;
        physical_.fill(false);velocities_.fill(0);
        resetStep();frontier_=at;
        return true;
    }

    bool gateDue(NativeFrame horizon) noexcept {
        if(count_ || !gate_ || horizon<*gate_ || (stepAt_ && *stepAt_<*gate_))return false;
        const auto at=*gate_;
        close(at);frontier_=at;
        return true;
    }

    bool takeDue(NativeFrame horizon) noexcept {
        if(gateDue(horizon))return true;
        if(count_ || !stepAt_ || horizon<*stepAt_)return false;
        const auto at=*stepAt_;
        frontier_=at;
        const auto& rhythm=*settings_.rhythm;
        const auto& step=rhythm.steps[stepIndex_];
        const auto divisor=ArpDivisionPulses[settings_.division];
        const double framesPerUnit=framesPerPulse_*divisor/6;
        const double gate=(step.sounding*settings_.soundingGain+step.tail*settings_.tailGain)*framesPerUnit/256;
        if(!startStep(at,gate,settings_.rhythmVelocity?step.velocity:0))return false;
        const uint16_t from=step.offset;
        const bool wraps=++stepIndex_==rhythm.size;
        if(wraps)stepIndex_=0;
        const uint64_t distance=(rhythm.steps[stepIndex_].offset+unsigned(wraps?rhythm.units:0)-from)*divisor;
        if(!distance || *stepPhase_>Max-distance)resetStep();
        else {
            *stepPhase_+=distance;
            planRhythmStep();
        }
        return true;
    }

    bool clock(MusicalClockSignal event) noexcept {
        if(count_ || !canApply(event.at))return false;
        frontier_=event.at;
        if(event.reset || epoch_!=event.epoch || !event.running) {
            close(event.at);
            resetStep();
        }
        epoch_=event.epoch;
        if(!event.tick || !event.running || !settings_.enabled || !keyCount_)return true;
        const auto divisor=ArpDivisionPulses[settings_.division];
        if(!pulseAnchor_ || event.pulse<*pulseAnchor_)pulseAnchor_=settings_.keySync?event.pulse:0;
        const auto elapsed=event.pulse-*pulseAnchor_;
        if(settings_.rhythm)return rhythmTick(event,elapsed,divisor);
        if(elapsed%divisor)return true;
        return startStep(event.at,event.framesPerPulse*divisor*settings_.gateQ16/65536.0,0);
    }

private:
    bool startStep(NativeFrame at,double gateFrames,uint8_t velocity) noexcept {
        if(generation_==Max)return false;
        close(at);
        ++generation_;
        lastVelocity_=settings_.fixedVelocity?settings_.fixedVelocity:velocity;
        if(settings_.pattern==ArpPattern::Chord) {
            lastOctave_=uint8_t(cursor_++%settings_.octaves);
            for(size_t i=0;i<keyCount_;++i)chordNote(at,{keys_[i],velocities_[keys_[i]]},lastOctave_,lastVelocity_);
        } else if(const auto key=nextKey())emitOn(at,*key,lastVelocity_);
        if(activeCount_) {
            const auto wanted=std::ceil(gateFrames);
            const auto frames=wanted>0?uint64_t(std::min(wanted,double(UINT32_MAX))):1u;
            if(at.ticks<=Max-frames)gate_=NativeFrame{at.ticks+frames};
        }
        return true;
    }

    bool rhythmTick(MusicalClockSignal event,uint64_t pulses,uint8_t divisor) noexcept {
        if(pulses>Max/6)return false;
        const auto& rhythm=*settings_.rhythm;
        pulsePhase_=pulses*6;timeOfPulse_=event.at;framesPerPulse_=event.framesPerPulse;
        if(!stepPhase_) {
            const auto cycle=uint64_t(rhythm.units)*divisor;
            const auto phase=pulsePhase_%cycle;
            stepIndex_=0;
            while(stepIndex_<rhythm.size && uint64_t(rhythm.steps[stepIndex_].offset)*divisor<phase)
                ++stepIndex_;
            const bool wrap=stepIndex_==rhythm.size;
            if(wrap)stepIndex_=0;
            const auto delta=(uint64_t(rhythm.steps[stepIndex_].offset)*divisor+unsigned(wrap)*cycle)-phase;
            if(pulsePhase_>Max-delta)return false;
            stepPhase_=pulsePhase_+delta;
        }
        planRhythmStep();
        return true;
    }
    void planRhythmStep() noexcept {
        stepAt_.reset();
        if(!stepPhase_)return;
        // A KS rhythm unit is one sixth of the selected division.
        // Interpolate within the current pulse and re-anchor at the next.
        const auto offset=*stepPhase_>pulsePhase_?*stepPhase_-pulsePhase_:0;
        if(offset>=6)return;
        const auto frames=uint64_t(std::ceil(double(offset)*framesPerPulse_/6));
        if(timeOfPulse_.ticks<=Max-frames)stepAt_=NativeFrame{timeOfPulse_.ticks+frames};
    }
    bool canApply(NativeFrame at) const noexcept {
        const auto deadline=nextDeadline();
        return !(at<frontier_) && (!deadline || !(*deadline<at));
    }
    void resetStep() noexcept {cursor_=0;pulseAnchor_.reset();stepPhase_.reset();stepAt_.reset();}
    void remove(uint8_t note) noexcept {
        if(!velocities_[note])return;
        size_t i=0;
        while(i<keyCount_ && keys_[i]!=note)++i;
        for(;i+1<keyCount_;++i)keys_[i]=keys_[i+1];
        velocities_[note]=0;--keyCount_;
    }
    void clearReleased() noexcept {
        size_t count=0;
        for(size_t i=0;i<keyCount_;++i) {
            const auto key=keys_[i];
            if(physical_[key])keys_[count++]=key;
            else velocities_[key]=0;
        }
        keyCount_=count;
    }
    void append(ArpNote note) noexcept {
        // A transition emits at most one whole 128-note off cohort and one
        // whole 128-note on cohort; an input cannot arrive with pending output.
        output_[(head_+count_)%OutputCapacity]=note;
        ++count_;
    }
    void close(NativeFrame at) noexcept {
        for(size_t i=0;i<activeCount_;++i) {
            auto note=active_[i];
            note.at=at;note.on=false;note.velocity=64;
            append(note);
        }
        activeCount_=0;gate_.reset();
    }
    void emitOn(NativeFrame at,Key key,uint8_t velocity) noexcept {
        ArpNote note{at,generation_,key.note,velocity?velocity:key.velocity,
                     settings_.channel,true,settings_.internal,settings_.midiOut};
        active_[activeCount_++]=note;
        append(note);
    }
    void chordNote(NativeFrame at,Key key,uint8_t octave,uint8_t velocity) noexcept {
        const unsigned pitch=key.note+12u*octave;
        for(size_t i=0;i<activeCount_;++i)if(active_[i].note==pitch)return;
        if(pitch<128 && activeCount_<active_.size())emitOn(at,{uint8_t(pitch),key.velocity},velocity);
    }
    std::optional<Key> nextKey() noexcept {
        std::array<uint8_t,128> ordered;
        size_t count=0;
        if(settings_.pattern==ArpPattern::AsPlayed) {
            std::copy_n(keys_.data(),keyCount_,ordered.data());
            count=keyCount_;
        } else {
            for(unsigned pitch=0;pitch<128;++pitch)
                if(velocities_[pitch])ordered[count++]=uint8_t(pitch);
        }
        if(!count)return {};
        const auto total=count*settings_.octaves;
        size_t index=cursor_++;
        switch(settings_.pattern) {
        case ArpPattern::Down:index=total-1-index%total;break;
        case ArpPattern::UpDown:
            if(total==1)index=0;
            else {index%=2*(total-1);if(index>=total)index=2*(total-1)-index;}
            break;
        case ArpPattern::UpDownRepeat:
            index%=2*total;if(index>=total)index=2*total-1-index;
            break;
        case ArpPattern::Random:
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
            index=random_%total;break;
        default:index%=total;break;
        }
        const auto raw=ordered[index%count];
        const auto pitch=raw+12u*(index/count);
        return pitch<128?std::optional{Key{uint8_t(pitch),velocities_[raw]}}:std::nullopt;
    }

    ArpSettings settings_{};
    NativeFrame frontier_{};
    uint64_t epoch_=0,generation_=0;
    size_t keyCount_=0,physicalCount_=0,cursor_=0,activeCount_=0,head_=0,count_=0;
    std::optional<uint64_t> pulseAnchor_;
    std::optional<NativeFrame> gate_;
    std::optional<NativeFrame> stepAt_;
    std::optional<uint64_t> stepPhase_;
    uint64_t pulsePhase_=0;
    NativeFrame timeOfPulse_{};
    double framesPerPulse_=0;
    bool pedal_=false;
    uint32_t random_;
    uint8_t lastOctave_=0,lastVelocity_=0,stepIndex_=0;
    std::array<bool,128> physical_{};
    std::array<uint8_t,128> keys_{},velocities_{};
    std::array<ArpNote,128> active_{};
    std::array<ArpNote,OutputCapacity> output_{};
};
}
