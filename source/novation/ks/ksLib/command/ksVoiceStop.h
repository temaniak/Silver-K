#pragma once

#include "ksMusicalState.h"
#include "ksRefreshPacket.h"

namespace novation::controller::command
{
    // Actual DSP operations before replacing an occupied physical voice.
    // This packet must drain before its slot is assigned to the new note.
    inline std::optional<KSRefreshPacket> encodeKsVoiceStop(VoiceSlot slot) noexcept
    {
        const auto index = size_t(slot);
        if (index >= KSMusicalState::VoiceCount)
            return std::nullopt;
        KSRefreshPacket packet;
        if (!packet.appendWord(Endpoint::B, uint16_t(0x72 + 101*index), 0x200000) ||
            !packet.appendWord(Endpoint::B, uint16_t(0x7a + 101*index), 0x200000) ||
            !packet.appendWord(Endpoint::B, uint16_t(0x8085 + 22*index), 0))
            return std::nullopt;
        return packet;
    }
}
