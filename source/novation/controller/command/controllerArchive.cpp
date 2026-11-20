#include "controllerArchive.h"

#include "midiDecoder.h"
#include "novationProgramSysEx.h"
#include "baseLib/sha256.h"

#include <stdexcept>

namespace novation::controller::command {
namespace {
constexpr uint8_t Magic[]{'S', 'K', 'C', 'M', 'D'};
constexpr uint8_t LegacyK[]{'K', 'S', 'T', 'S', 'T', 1, 0, 0};
constexpr size_t HashLength = 64;
constexpr size_t HeaderSize = 8 + HashLength + 3 * 4;
constexpr size_t MaxPending = 65536;

void appendSize(std::vector<uint8_t>& bytes, size_t size) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(uint8_t(size >> shift));
}

uint32_t sizeAt(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

bool valid(const ControllerArchive& saved) {
    if ((saved.model != Model::K && saved.model != Model::KS) ||
        saved.userFlash.size() != (saved.model == Model::K ? KUserFlashBytes : KSUserFlashBytes) ||
        saved.resourceHash.size() != HashLength || saved.pendingControl.size() > MaxPending ||
        saved.ksCheckpoint.size() > MaxPending ||
        !std::all_of(saved.resourceHash.begin(), saved.resourceHash.end(), [](char value) {
            return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
        }))
        return false;
    const auto program = NovationProgramSysEx::decode(saved.model, 0x7f, {saved.activeProgram.data(), saved.activeProgram.size()});
    if (!program || (program->type != 0 && !(saved.model == Model::KS && program->type == 2)))
        return false;
    if (!saved.ksCheckpoint.empty())
        return saved.model == Model::KS && saved.pendingControl.empty();
    MidiDecoder decoder;
    for (const auto byte : saved.pendingControl) {
        const auto result = decoder.push(byte, [&](const EventPayload& event) {
            if (const auto* sysex = std::get_if<SysExBytes>(&event)) {
                if (sysex->size <= 6 || (sysex->data[6] >= 16 && sysex->data[6] != 0x7f)) return false;
                const auto program = NovationProgramSysEx::decode(saved.model, sysex->data[6], *sysex);
                return program && program->type == 0;
            }
            return std::holds_alternative<ControlChange>(event) || std::holds_alternative<ProgramChange>(event);
        });
        if (result != MidiDecoder::Result::Consumed) return false;
    }
    return decoder.atMessageBoundary();
}
}

std::vector<uint8_t> encodeControllerArchive(const ControllerArchive& saved) {
    if (!valid(saved)) throw std::invalid_argument("Invalid native controller archive");
    const bool checkpoint = !saved.ksCheckpoint.empty();
    const auto& pending = checkpoint ? saved.ksCheckpoint : saved.pendingControl;
    std::vector<uint8_t> bytes;
    bytes.reserve(HeaderSize + saved.userFlash.size() + saved.activeProgram.size() + pending.size() + HashLength);
    bytes.insert(bytes.end(), std::begin(Magic), std::end(Magic));
    bytes.push_back(checkpoint ? 2 : 1);
    bytes.push_back(saved.model == Model::K ? 1 : 2);
    bytes.push_back(0);
    bytes.insert(bytes.end(), saved.resourceHash.begin(), saved.resourceHash.end());
    appendSize(bytes, saved.userFlash.size());
    appendSize(bytes, saved.activeProgram.size());
    appendSize(bytes, pending.size());
    bytes.insert(bytes.end(), saved.userFlash.begin(), saved.userFlash.end());
    bytes.insert(bytes.end(), saved.activeProgram.begin(), saved.activeProgram.end());
    bytes.insert(bytes.end(), pending.begin(), pending.end());
    const auto hash = baseLib::SHA256(bytes).toString();
    bytes.insert(bytes.end(), hash.begin(), hash.end());
    return bytes;
}

std::optional<ControllerArchive> decodeControllerArchive(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < HeaderSize + HashLength || bytes.size() > HeaderSize + KSUserFlashBytes + 1038 + MaxPending + HashLength ||
        !std::equal(std::begin(Magic), std::end(Magic), bytes.begin()) ||
        (bytes[5] != 1 && bytes[5] != 2) || (bytes[6] != 1 && bytes[6] != 2) || bytes[7] != 0)
        return std::nullopt;
    const auto end = bytes.size() - HashLength;
    const auto flash = sizeAt(bytes.data() + 8 + HashLength);
    const auto program = sizeAt(bytes.data() + 12 + HashLength);
    const auto pending = sizeAt(bytes.data() + 16 + HashLength);
    if (uint64_t(flash) + program + pending != end - HeaderSize ||
        (bytes[5] == 2 && (bytes[6] != 2 || !pending)))
        return std::nullopt;
    const auto hash = baseLib::SHA256(bytes.data(), end).toString();
    if (!std::equal(hash.begin(), hash.end(), bytes.begin() + end)) return std::nullopt;
    const auto start = bytes.begin() + HeaderSize;
    ControllerArchive saved{bytes[6] == 1 ? Model::K : Model::KS,
        std::string(bytes.begin() + 8, bytes.begin() + 8 + HashLength),
        {start, start + flash}, {start + flash, start + flash + program}, {}, {}};
    auto& payload = bytes[5] == 2 ? saved.ksCheckpoint : saved.pendingControl;
    payload.assign(start + flash + program, bytes.begin() + end);
    return valid(saved) ? std::optional{std::move(saved)} : std::nullopt;
}

std::optional<ControllerArchive> decodeLegacyKArchive(const std::vector<uint8_t>& bytes, std::string resourceHash) {
    constexpr size_t Patch = sizeof(LegacyK) + KUserFlashBytes, Controllers = Patch + 128;
    if (bytes.size() != Controllers + 16 * 128 || !std::equal(std::begin(LegacyK), std::end(LegacyK), bytes.begin()))
        return std::nullopt;
    if (!std::all_of(bytes.begin() + Patch, bytes.begin() + Controllers, [](uint8_t value) { return value < 128; }))
        return std::nullopt;
    ControllerArchive saved{Model::K, std::move(resourceHash),
        {bytes.begin() + sizeof(LegacyK), bytes.begin() + Patch},
        NovationProgramSysEx::encode(Model::K, 0, 0, 0, 0, bytes.data() + Patch, 128), {}};
    if (saved.activeProgram[NovationProgramSysEx::HeaderSize + 106] > 12)
        saved.activeProgram[NovationProgramSysEx::HeaderSize + 106] = 0;
    for (size_t channel = 0; channel < 16; ++channel)
        for (const uint8_t controller : {1, 2, 7, 64}) {
            const auto value = bytes[Controllers + channel * 128 + controller];
            if (value < 128)
                saved.pendingControl.insert(saved.pendingControl.end(), {uint8_t(0xb0 | channel), controller, value});
        }
    return valid(saved) ? std::optional{std::move(saved)} : std::nullopt;
}

}
