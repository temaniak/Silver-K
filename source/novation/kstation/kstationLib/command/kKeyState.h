#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace novation::controller::command {
class KKeyState final {
public:
    struct Key {uint8_t note,velocity;};

    void press(uint8_t note,uint8_t velocity) noexcept {
        if(note>=physical_.size() || !velocity || velocity>=128)return;
        physical_[note]=velocity;
        held_[note]=true;
    }
    void release(uint8_t note,bool sustain) noexcept {
        if(note>=physical_.size())return;
        physical_[note]=0;
        if(!sustain)held_[note]=false;
    }
    void liftPedal() noexcept {
        for(size_t i=0;i<held_.size();++i)if(!physical_[i])held_[i]=false;
    }
    // Releases for keys pressed on the previous program are still accepted.
    // Their pitches must not become new legato targets in the replacement.
    void detachProgram() noexcept {held_.fill(false);}
    void clear() noexcept {physical_.fill(0);detachProgram();}

    bool down(uint8_t note) const noexcept {return note<physical_.size() && physical_[note];}
    bool anyHeld() const noexcept {
        for(auto held:held_)if(held)return true;
        return false;
    }
    size_t physicalCount() const noexcept {
        size_t count=0;
        for(auto velocity:physical_)count+=velocity!=0;
        return count;
    }
    std::optional<Key> nearestHeld(uint8_t note) const noexcept {
        for(int distance=1;distance<128;++distance) {
            const int upper=note+distance,lower=note-distance;
            if(upper<128 && held_[upper] && physical_[upper])return Key{uint8_t(upper),physical_[upper]};
            if(lower>=0 && held_[lower] && physical_[lower])return Key{uint8_t(lower),physical_[lower]};
        }
        return {};
    }
private:
    std::array<uint8_t,128> physical_{};
    std::array<bool,128> held_{};
};
}
