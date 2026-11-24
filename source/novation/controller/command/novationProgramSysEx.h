#pragma once

#include "controllerEvent.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace novation::controller::command {

struct NovationProgramSysEx {
    static constexpr size_t HeaderSize = 13;
    uint8_t type, control, bank, program;
    const uint8_t* data;
    size_t size;

    static constexpr size_t programBytes(Model model) noexcept { return model == Model::K ? 128 : 256; }
    static constexpr size_t payloadBytes(Model model, uint8_t type) noexcept {
        switch (type) {
        case 0: case 1: return programBytes(model);
        case 2: case 3: return model == Model::KS ? 1024 : 256;
        case 4: return model == Model::KS ? 256 : 0;
        default: return 0;
        }
    }

    static bool addressed(Model model, uint8_t channel, SysExBytes frame) noexcept {
        constexpr uint8_t prefix[]{0xf0, 0, 0x20, 0x29, 1};
        if (!frame.data || frame.size < HeaderSize + 1 || frame.size > MaxSysExBytes)
            return false;
        const auto* bytes = frame.data;
        return std::equal(std::begin(prefix), std::end(prefix), bytes) &&
            bytes[5] == (model == Model::K ? 0x41 : 0x42) && (bytes[6] == 0x7f || bytes[6] == channel) &&
            bytes[frame.size - 1] == 0xf7 &&
            std::all_of(bytes + 1, bytes + frame.size - 1, [](uint8_t value) { return value < 128; });
    }

    static bool identityRequest(uint8_t channel, SysExBytes frame) noexcept {
        const auto* bytes = frame.data;
        return bytes && frame.size == 6 && bytes[0] == 0xf0 && bytes[1] == 0x7e &&
            (bytes[2] == 0x7f || bytes[2] == channel) && bytes[3] == 6 && bytes[4] == 1 && bytes[5] == 0xf7;
    }

    static bool knownType(Model model, uint8_t type) noexcept {
        return payloadBytes(model,type) || (type >= 0x40 && type <= (model == Model::K ? 0x43 : 0x44));
    }

    static bool firmwareType(uint8_t type) noexcept { return type >= 0x7d && type <= 0x7f; }

    static std::optional<NovationProgramSysEx> decode(Model model, uint8_t channel, SysExBytes frame) noexcept {
        if (!addressed(model, channel, frame))
            return std::nullopt;
        const auto* bytes = frame.data;
        const auto type = bytes[7];
        const auto payload = frame.size - HeaderSize - 1;
        const auto expected = payloadBytes(model, type);
        const bool request = type >= 0x40 && type <= (model == Model::K ? 0x43 : 0x44);
        if (request ? payload != 0 : !expected || payload != expected)
            return std::nullopt;
        const bool bankControl = type == 1 || type == 0x41 ||
            (model == Model::K ? type == 2 || type == 0x42 : type == 3);
        if (bankControl && bytes[8] > 1)
            return std::nullopt;
        return NovationProgramSysEx{type, bytes[8], bytes[11], bytes[12], bytes + HeaderSize, payload};
    }

    static std::vector<uint8_t> encode(Model model, uint8_t type, uint8_t control, uint8_t bank, uint8_t program,
                                       const uint8_t* data = nullptr, size_t size = 0) {
        std::vector<uint8_t> bytes{0xf0, 0, 0x20, 0x29, 1, uint8_t(model == Model::K ? 0x41 : 0x42),
            0x7f, type, control, 0x11, uint8_t(model == Model::K ? 1 : 2), bank, program};
        bytes.reserve(HeaderSize + size + 1);
        if (size) bytes.insert(bytes.end(), data, data + size);
        bytes.push_back(0xf7);
        return bytes;
    }
};

}
