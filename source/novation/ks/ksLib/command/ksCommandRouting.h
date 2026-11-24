#pragma once

#include "ksMusicalState.h"
#include "../../../controller/command/controllerEvent.h"
#include <type_traits>

namespace novation::controller::command {

struct KSPartRoute {
    static constexpr uint8_t NoChannel=16;
    uint8_t channel=NoChannel,low=0,high=127;
};
struct KSRoutingConfig {
    KSLayout layout=KSLayout::Single;
    uint8_t activeMask=1;
    std::array<KSPartRoute,KSMusicalState::StorageSlotCount> parts{};
};
enum class KSGlobalRoute { None, Program, MirroredControl };
struct KSRoutedParts {
    std::array<KSStorageSlot,KSMusicalState::PartCount> parts{};
    size_t count=0;
    KSGlobalRoute global=KSGlobalRoute::None;
    uint64_t revision=0;
};

// Channel routing is one complete musical decision. Performance can fan one
// input out to several parts and independently route global program/control.
// Storage slot zero is not a fifth simultaneous musical part.
class KSCommandRouting final {
public:
    explicit KSCommandRouting(Session session)noexcept:session_(session){}
    bool configure(KSRoutingConfig config)noexcept {
        if((config.layout!=KSLayout::Single && config.layout!=KSLayout::Performance) || config.activeMask>15 ||
           (config.layout==KSLayout::Single && config.activeMask>1) ||
           revision_==std::numeric_limits<uint64_t>::max())return false;
        for(const auto& part:config.parts)
            if(part.channel>KSPartRoute::NoChannel || part.low>=128 || part.high>=128)return false;
        config_=config;++revision_;return true;
    }
    const KSRoutingConfig& config()const noexcept{return config_;}
    uint64_t revision()const noexcept{return revision_;}
    bool current(Session session,const KSRoutedParts& batch)const noexcept
    {return session==session_ && batch.revision==revision_;}

    std::optional<KSRoutedParts> route(Session session,const ControllerEvent& event)const noexcept {
        if(session!=session_)return std::nullopt;
        const auto channel=channelOf(event.payload);
        if(!channel || *channel>=16)return std::nullopt;
        const auto count=config_.layout==KSLayout::Single?size_t(1):KSMusicalState::PartCount;
        if(event.part!=RoutedByChannel && event.part>=count)return std::nullopt;
        KSRoutedParts result;result.revision=revision_;
        // A panel control addresses the stored part, including when muted.
        // Its transport channel does not determine who receives the edit.
        if(event.part!=RoutedByChannel && std::holds_alternative<ControlChange>(event.payload)) {
            result.parts[result.count++]=*KSMusicalState::storageSlot(config_.layout,KSPart(event.part));
            return result;
        }
        for(size_t i=0;i<count;++i) {
            const auto slot=KSStorageSlot(config_.layout==KSLayout::Single?0:i+1);
            if((config_.activeMask&(1u<<i)) && (event.part==RoutedByChannel || event.part==i) &&
               config_.parts[size_t(slot)].channel==*channel)result.parts[result.count++]=slot;
        }
        if(config_.layout==KSLayout::Performance && event.part==RoutedByChannel && config_.parts[0].channel==*channel) {
            if(std::holds_alternative<ProgramChange>(event.payload))result.global=KSGlobalRoute::Program;
            else if(std::holds_alternative<ControlChange>(event.payload))result.global=KSGlobalRoute::MirroredControl;
        }
        return result;
    }
    bool noteInRange(KSStorageSlot part,uint8_t note)const noexcept {
        const auto index=size_t(part);
        return index<config_.parts.size() && note<128 &&
            (config_.layout==KSLayout::Single?index==0:index>0) &&
            note>=config_.parts[index].low && note<=config_.parts[index].high;
    }
    static std::optional<uint8_t> channelOf(const EventPayload& event)noexcept {
        return std::visit([](const auto& value)->std::optional<uint8_t>{
            using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,NoteOn> || std::is_same_v<T,NoteOff> || std::is_same_v<T,ControlChange> ||
                         std::is_same_v<T,ProgramChange> || std::is_same_v<T,PolyPressure> ||
                         std::is_same_v<T,ChannelPressure> || std::is_same_v<T,PitchBend>)return value.channel;
            return std::nullopt;
        },event);
    }
private:
    Session session_;
    KSRoutingConfig config_{};
    uint64_t revision_=0;
};
}
