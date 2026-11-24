#pragma once

#include "ksCommandGlobals.h"
#include "ksProgramFlash.h"
#include "../../../controller/command/commandScheduler.h"
#include "../../../controller/command/midiDecoder.h"

#include <type_traits>

namespace novation::controller::command {

enum class KSReloadKind : uint8_t { Single = 1, Scene, Parts };

struct KSReloadIntent {
    KSReloadKind kind{};
    KSLayout layout{};
    ProgramSelection single{};
    uint8_t performance = 0, activeMask = 0, partMask = 0;
    KSPerformanceRecord patches{};
    KSGlobalMaster master{};
};

struct KSQueuedMidi {
    uint64_t delay = 0;
    std::vector<uint8_t> message;
};

struct KSControllerCheckpoint {
    uint64_t nextByteDelay = 0;
    MidiDecoder::State decoder;
    std::array<uint8_t,16> banks{};
    std::array<uint8_t,16> desktopSelectors{};
    std::array<uint8_t,5> partSelectors{};
    std::array<uint8_t,4> performanceChannels{16,16,16,16};
    uint8_t globalSelector = 255, editorMask = 0, editorPart = 0;
    std::optional<KSVoiceImagePatch> storedSingle;
    std::optional<KSReloadIntent> reload;
    std::vector<KSQueuedMidi> midi;
};

// One complete message per queue record. The byte decoder's incomplete frame
// is stored separately, at the frontier of accepted transport bytes.
inline std::vector<uint8_t> encodeKSQueuedMidi(const EventPayload& payload) {
    return std::visit([](const auto& event) -> std::vector<uint8_t> {
        using T = std::decay_t<decltype(event)>;
        if constexpr (std::is_same_v<T,NoteOn>) return {uint8_t(0x90|event.channel),event.note,event.velocity};
        else if constexpr (std::is_same_v<T,NoteOff>) return {uint8_t(0x80|event.channel),event.note,event.velocity};
        else if constexpr (std::is_same_v<T,ControlChange>) return {uint8_t(0xb0|event.channel),event.controller,event.value};
        else if constexpr (std::is_same_v<T,ProgramChange>) return {uint8_t(0xc0|event.channel),event.program};
        else if constexpr (std::is_same_v<T,PolyPressure>) return {uint8_t(0xa0|event.channel),event.note,event.pressure};
        else if constexpr (std::is_same_v<T,ChannelPressure>) return {uint8_t(0xd0|event.channel),event.pressure};
        else if constexpr (std::is_same_v<T,PitchBend>) return {uint8_t(0xe0|event.channel),uint8_t(event.value&127),uint8_t(event.value>>7)};
        else if constexpr (std::is_same_v<T,MidiRealtime>) return {event.status};
        else if constexpr (std::is_same_v<T,MidiCommon>) {
            if (event.status == 0xf2) return {event.status,event.data1,event.data2};
            if (event.status == 0xf6) return {event.status};
            return {event.status,event.data1};
        } else if constexpr (std::is_same_v<T,SysExBytes>) return {event.data,event.data+event.size};
        else return {};
    }, payload);
}

inline std::optional<EventPayload> decodeKSQueuedMidi(const std::vector<uint8_t>& message) {
    if (message.empty() || message.size() > MaxSysExBytes || message.front() < 0x80)
        return std::nullopt;
    if (message.front() == 0xf0) {
        if (message.size() < 2 || message.back() != 0xf7 ||
            !std::all_of(message.begin()+1,message.end()-1,[](uint8_t b) { return b < 128; }))
            return std::nullopt;
        return SysExBytes{message.data(),message.size()};
    }
    if (message.size() > 3 || !std::all_of(message.begin()+1,message.end(),[](uint8_t b) { return b < 128; }))
        return std::nullopt;
    MidiDecoder decoder;
    std::optional<EventPayload> payload;
    for (const auto byte : message)
        if (decoder.push(byte,[&](EventPayload event) {
                if (payload) return false;
                payload = event;
                return true;
            }) != MidiDecoder::Result::Consumed)
            return std::nullopt;
    return decoder.atMessageBoundary() ? payload : std::nullopt;
}

namespace ksCheckpointDetail {
    constexpr size_t MaxBytes = 65536;
    constexpr uint8_t Version = 1;

    inline void number(std::vector<uint8_t>& out,uint64_t value,unsigned size) {
        for (unsigned i=0;i<size;++i)
            out.push_back(uint8_t(value>>(8*i)));
    }

    struct Reader {
        const std::vector<uint8_t>& bytes;
        size_t offset = 0;
        bool valid = true;

        const uint8_t* take(size_t size) {
            if (!valid || size > bytes.size()-offset) {
                valid=false;
                return nullptr;
            }
            const auto* begin=bytes.data()+offset;
            offset+=size;
            return begin;
        }
        uint64_t number(unsigned size) {
            const auto* data=take(size);
            uint64_t value=0;
            if (data)
                for (unsigned i=0;i<size;++i)
                    value|=uint64_t(data[i])<<(8*i);
            return value;
        }
        uint8_t byte() { return uint8_t(number(1)); }
        template<size_t N> void copy(std::array<uint8_t,N>& target) {
            if (const auto* data=take(N))std::copy_n(data,N,target.begin());
        }
        std::vector<uint8_t> vector(size_t size) {
            if (!size)return {};
            if (const auto* data=take(size))return {data,data+size};
            return {};
        }
    };

    inline bool validReload(const KSReloadIntent& reload) noexcept {
        const auto bank=reload.single.bank;
        if (bank<1 || bank>8 || reload.single.program>=(bank<=4?100:50) ||
            !reload.partMask || reload.partMask>15 || reload.activeMask>15 ||
            (reload.layout!=KSLayout::Single && reload.layout!=KSLayout::Performance))
            return false;
        const bool multi=reload.layout==KSLayout::Performance;
        if ((multi && reload.performance>=100) ||
            (!multi && (reload.activeMask!=1 || reload.partMask!=1)))
            return false;
        const auto& master=reload.master;
        if (master.vocoderBalance>=128 || master.vocoderWidth>=128 || master.sibilanceLevel>=128 ||
            master.sibilanceType>1 || master.performanceEffectStatus>=128)
            return false;
        switch (reload.kind) {
        case KSReloadKind::Single: return !multi;
        case KSReloadKind::Scene: return !multi || reload.partMask==15;
        case KSReloadKind::Parts: return multi && !(reload.partMask & ~reload.activeMask);
        }
        return false;
    }

    inline bool validPatch(const KSVoiceImagePatch& patch) noexcept {
        return std::all_of(patch.begin(),patch.begin()+209,[](uint8_t b) { return b<128; });
    }

    inline bool valid(const KSControllerCheckpoint& saved) {
        const auto selector=[](uint8_t b) { return b<128 || b==255; };
        if (!MidiDecoder::valid(saved.decoder) || !selector(saved.globalSelector) || saved.editorMask>15 || saved.editorPart>4 ||
            !std::all_of(saved.banks.begin(),saved.banks.end(),selector) ||
            !std::all_of(saved.partSelectors.begin(),saved.partSelectors.end(),selector) ||
            !std::all_of(saved.desktopSelectors.begin(),saved.desktopSelectors.end(),[](uint8_t b) { return b<128; }) ||
            !std::all_of(saved.performanceChannels.begin(),saved.performanceChannels.end(),[](uint8_t b) { return b<=16; }) ||
            (saved.storedSingle && !validPatch(*saved.storedSingle)) ||
            (saved.reload && !validReload(*saved.reload)) || saved.midi.size()>CommandScheduler::Capacity)
            return false;
        if (saved.reload) {
            const auto& reload=*saved.reload;
            for (size_t i=0;i<reload.patches.size();++i)if (reload.partMask&(1u<<i)) {
                const auto& patch=reload.patches[i];
                if (!validPatch(patch))return false;
                if (reload.kind==KSReloadKind::Parts &&
                    (patch[187]<1 || patch[187]>8 || patch[188]>=(patch[187]<=4?100:50)))
                    return false;
            }
        }
        uint64_t delay=0;
        size_t sysex=0;
        for (const auto& queued : saved.midi) {
            const auto event=decodeKSQueuedMidi(queued.message);
            if (queued.delay<delay || queued.delay>saved.nextByteDelay || !event)
                return false;
            if (std::holds_alternative<SysExBytes>(*event) && ++sysex>CommandScheduler::SysExSlots)
                return false;
            delay=queued.delay;
        }
        return true;
    }
}

inline std::vector<uint8_t> encodeKSControllerCheckpoint(const KSControllerCheckpoint& saved) {
    using namespace ksCheckpointDetail;
    if (!valid(saved))return {};
    std::vector<uint8_t> out{Version,uint8_t(bool(saved.storedSingle)),uint8_t(bool(saved.reload))};
    number(out,saved.nextByteDelay,8);
    out.insert(out.end(),saved.banks.begin(),saved.banks.end());
    out.insert(out.end(),saved.desktopSelectors.begin(),saved.desktopSelectors.end());
    out.insert(out.end(),saved.partSelectors.begin(),saved.partSelectors.end());
    out.insert(out.end(),saved.performanceChannels.begin(),saved.performanceChannels.end());
    out.insert(out.end(),{saved.globalSelector,saved.editorMask,saved.editorPart,saved.decoder.runningStatus,saved.decoder.pendingStatus,
                         saved.decoder.firstData,uint8_t(saved.decoder.discardingSysEx)});
    number(out,saved.decoder.sysExPrefix.size(),2);
    number(out,saved.midi.size(),2);
    out.insert(out.end(),saved.decoder.sysExPrefix.begin(),saved.decoder.sysExPrefix.end());
    if (saved.storedSingle)out.insert(out.end(),saved.storedSingle->begin(),saved.storedSingle->end());
    if (saved.reload) {
        const auto& reload=*saved.reload;
        const auto& master=reload.master;
        out.insert(out.end(),{uint8_t(reload.kind),uint8_t(reload.layout),reload.single.bank,reload.single.program,
                             reload.performance,reload.activeMask,reload.partMask,master.vocoderBalance,
                             master.vocoderWidth,master.sibilanceLevel,master.sibilanceType,master.performanceEffectStatus});
        for (size_t i=0;i<reload.patches.size();++i)
            if (reload.partMask&(1u<<i))out.insert(out.end(),reload.patches[i].begin(),reload.patches[i].end());
    }
    for (const auto& queued : saved.midi) {
        number(out,queued.delay,8);
        number(out,queued.message.size(),2);
        out.insert(out.end(),queued.message.begin(),queued.message.end());
    }
    if (out.size()>MaxBytes)return {};
    return out;
}

inline std::optional<KSControllerCheckpoint> decodeKSControllerCheckpoint(const std::vector<uint8_t>& data) {
    using namespace ksCheckpointDetail;
    if (data.empty() || data.size()>MaxBytes)return std::nullopt;
    Reader in{data};
    if (in.byte()!=Version)return std::nullopt;
    const auto single=in.byte(), pending=in.byte();
    if (single>1 || pending>1)return std::nullopt;
    KSControllerCheckpoint saved;
    saved.nextByteDelay=in.number(8);
    in.copy(saved.banks);
    in.copy(saved.desktopSelectors);
    in.copy(saved.partSelectors);
    in.copy(saved.performanceChannels);
    saved.globalSelector=in.byte();
    saved.editorMask=in.byte();
    saved.editorPart=in.byte();
    saved.decoder.runningStatus=in.byte();
    saved.decoder.pendingStatus=in.byte();
    saved.decoder.firstData=in.byte();
    const auto discard=in.byte();
    saved.decoder.discardingSysEx=discard!=0;
    const auto prefix=in.number(2), count=in.number(2);
    if (discard>1 || prefix>=MaxSysExBytes || count>CommandScheduler::Capacity)return std::nullopt;
    saved.decoder.sysExPrefix=in.vector(size_t(prefix));
    if (single)in.copy(saved.storedSingle.emplace());
    if (pending) {
        auto& reload=saved.reload.emplace();
        reload.kind=KSReloadKind(in.byte());
        reload.layout=KSLayout(in.byte());
        reload.single={in.byte(),in.byte()};
        reload.performance=in.byte();
        reload.activeMask=in.byte();
        reload.partMask=in.byte();
        reload.master={in.byte(),in.byte(),in.byte(),in.byte(),0,in.byte()};
        if (!validReload(reload))return std::nullopt;
        for (size_t i=0;i<reload.patches.size();++i)
            if (reload.partMask&(1u<<i))in.copy(reload.patches[i]);
    }
    saved.midi.reserve(size_t(count));
    for (uint64_t i=0;i<count;++i) {
        const auto delay=in.number(8), length=in.number(2);
        if (!length || length>MaxSysExBytes)return std::nullopt;
        saved.midi.push_back({delay,in.vector(size_t(length))});
    }
    if (!in.valid || in.offset!=data.size() || !valid(saved))return std::nullopt;
    return saved;
}

}
