#pragma once
#include <array>
#include <cstdint>

namespace ks::nativeControl {
using KeyBitmap=std::array<uint8_t,16>;
struct KeySearch {
    int note=-1;
    unsigned clocks=0,boundaries=0; // Body only; caller/return transport separate.
};
// Strictly adjacent physical/held key, independent of processor state. The
// bitmap has exactly the 128 MIDI keys; out-of-range inputs are rejected before
// touching it. Durations include the admitted SRAM observations at 25 MHz.
inline KeySearch nextKey(const KeyBitmap& keys,unsigned after){
    if(after>=128)return {};
    KeySearch result{-1,33,11};
    unsigned group=after/8;int bit=int(after%8);
    for(;group<keys.size();++group,bit=-1){
        const unsigned byte=keys[group];result.clocks+=9;result.boundaries+=2;
        if(byte){
            unsigned found=unsigned(bit+1);
            while(found<8 && !(byte&(1u<<found)))++found;
            if(found<8){
                const unsigned attempts=found-unsigned(bit+1)+1;
                result.clocks+=15*attempts+6;result.boundaries+=5*attempts+2;
                result.note=int(group*8+found);return result;
            }
            const unsigned absent=unsigned(7-bit);
            result.clocks+=15*absent+9;result.boundaries+=5*absent+3;
        }
        result.clocks+=12;result.boundaries+=4;
    }
    return result;
}
inline KeySearch previousKey(const KeyBitmap& keys,unsigned before){
    if(before>=128)return {};
    KeySearch result{-1,33,11};
    int group=int(before/8),bit=int(before%8);
    for(;group>=0;--group,bit=8){
        const unsigned byte=keys[unsigned(group)];result.clocks+=9;result.boundaries+=2;
        if(byte){
            int found=bit-1;
            while(found>=0 && !(byte&(1u<<unsigned(found))))--found;
            if(found>=0){
                const unsigned attempts=unsigned(bit-found);
                result.clocks+=12*attempts+6;result.boundaries+=4*attempts+2;
                result.note=group*8+found;return result;
            }
            result.clocks+=12*unsigned(bit)+6;result.boundaries+=4*unsigned(bit)+2;
        }
        result.clocks+=12;result.boundaries+=4;
    }
    return result;
}

// Live, preemptible search. Each byte is observed at its original data-read
// boundary, not frozen at task construction. Inspecting an observed byte uses
// native arithmetic; the intervening controller time is an interruptible wait.
class KeySearchTask final {
public:
    enum class Direction {Next,Previous};
    enum class Phase {Admission,ReadByte,InspectByte,Complete};
    // Semantic progress for scheduling/debug adapters. No source-code address,
    // CPU condition byte or general-purpose operand storage drives the task.
    struct Cursor {Phase phase;unsigned offset,group,storageIndex;int afterBit;unsigned observedByte;int candidate;bool forward;};
    KeySearchTask(Direction direction,unsigned key)
        : forward_(direction==Direction::Next),group_(int(key/8)),storageIndex_(key/8),bit_(int(key%8)),done_(key>=256) {}
    bool complete()const{return done_;}
    unsigned nextBoundaryClocks()const{return done_?0:interval_?3:6;}
    KeySearch result()const{return {done_ && found_ && note_<128?note_:-1,elapsed_,boundaries_};}
    uint8_t terminalKey()const{return uint8_t(note_);}
    Cursor cursor()const{return {done_?Phase::Complete:phase_,intervalSize_-interval_,unsigned(group_),storageIndex_,bit_,observed_,note_,forward_};}
    // readByte(index) observes live keyboard storage, without exposing its
    // address or a CPU register/flag/stack ABI to the task. No allocation/wait.
    template<class Read>unsigned advanceBefore(unsigned available,Read&& readByte){
        const unsigned start=elapsed_;
        while(!done_){
            const unsigned cost=nextBoundaryClocks();if(cost>available)break;
            available-=cost;elapsed_+=cost;++boundaries_;
            if(interval_){
                if(--interval_)continue;
                if(after_==After::Finish){done_=true;continue;}
                if(after_==After::Move){group_=int(nextGroup_);storageIndex_+=forward_?1u:unsigned(-1);bit_=forward_?-1:8;}
                phase_=Phase::ReadByte;continue;
            }
            const unsigned byte=readByte(storageIndex_);observed_=byte;phase_=Phase::InspectByte;
            // One common admission/empty-byte decision boundary follows the
            // read. Candidate traversal and byte advancement are timed below.
            interval_=1;
            int found=bit_+(forward_?1:-1);
            if(byte){
                if(forward_){
                    while(found<8 && !(byte&(1u<<unsigned(found))))++found;
                    if(found<8)interval_+=5*unsigned(found-bit_)+2;
                    else interval_+=5*unsigned(7-bit_)+3;
                }else{
                    while(found>=0 && !(byte&(1u<<unsigned(found))))--found;
                    if(found>=0)interval_+=4*unsigned(bit_-found)+2;
                    else interval_+=4*unsigned(bit_)+2;
                }
            }else found=forward_?8:-1;
            if(found>=0 && found<8){note_=group_*8+found;found_=true;after_=After::Finish;}
            else {
                interval_+=4;
                const unsigned next=unsigned(group_*8+(forward_?8:-8))&255u;
                nextGroup_=next/8;
                // Internal byte-key sentinels retain the original wrapping
                // traversal too; ordinary MIDI keys remain in 0..127.
                after_=(next&128u)?After::Finish:After::Move;note_=int(next);
            }
            intervalSize_=interval_;
        }
        return elapsed_-start;
    }
private:
    enum class After {Read,Move,Finish};
    bool forward_;
    int group_;unsigned storageIndex_;int bit_,note_=-1;
    bool done_,found_=false;
    unsigned interval_=11,intervalSize_=11,elapsed_=0,boundaries_=0,observed_=0,nextGroup_=0;
    Phase phase_=Phase::Admission;
    After after_=After::Read;
};
}
