#pragma once

#include "ksCommandVoiceImage.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace novation::controller::command {

using KSPerformanceRecord = std::array<KSVoiceImagePatch, 4>;
using KSDrumBank = std::array<KSVoiceImagePatch, 50>;
static_assert(sizeof(KSDrumBank)==50*256);

// The worker owns one stable user image. Preset writes never target the
// external OS or Wave ROM, and publish only after the entire dump is checked.
class KSProgramFlash final {
public:
    static constexpr size_t UserOffset = 0x1fc00;
    static constexpr size_t FullBytes = 0x80000, UserBytes = 0x40400;
    static constexpr size_t Globals = 0x1ff00 - UserOffset;

    explicit KSProgramFlash(std::vector<uint8_t>& flash) : user_(flash.data()) {
        if (flash.size() == FullBytes)
            user_ += UserOffset;
        else if (flash.size() != UserBytes)
            throw std::runtime_error("KS Flash must contain a full image or the saved user region");
    }

    const uint8_t* userData() const noexcept { return user_; }
    const uint8_t* globalPage() const noexcept { return user_ + Globals; }
    uint8_t global(size_t index) const noexcept { return globalPage()[index]; }
    bool writable() const noexcept { return global(5) == 1; }

    const uint8_t* drumBankData(uint8_t bank) const noexcept {
        return bank>=5 && bank<=8 ? user_+*singleOffset({bank,0}) : nullptr;
    }

    uint64_t drumBankRevision(size_t index) const noexcept { return drumRevisions_[index]; }

    bool canStoreDrumBank(uint8_t bank,const KSDrumBank& records) const noexcept {
        return drumBankData(bank) && writable() &&
            std::all_of(records.begin(),records.end(),[](const auto& record){
                return midiData(record.data(),record.size());
            });
    }

    void storeDrumBank(uint8_t bank,const KSDrumBank& records) noexcept {
        std::memcpy(user_+*singleOffset({bank,0}),records.data(),sizeof(records));
        ++drumRevisions_[bank-5];
    }

    bool storeGlobals(const uint8_t* page) noexcept {
        if (!writable() || !midiData(page, 256) || page[0] != 1 || page[1] >= 100 ||
            !singleOffset({page[2],page[3]}) || page[4] > 1 || page[5] > 1 || page[8] >= 16)
            return false;
        std::copy_n(page,256,user_+Globals);
        return true;
    }

    std::optional<KSVoiceImagePatch> single(ProgramSelection selection) const noexcept {
        const auto offset = singleOffset(selection);
        if (!offset || !midiData(user_ + *offset, KSVoiceImagePatch{}.size()))
            return std::nullopt;
        KSVoiceImagePatch patch;
        std::copy_n(user_ + *offset, patch.size(), patch.begin());
        return patch;
    }

    std::optional<KSPerformanceRecord> performance(uint8_t program) const noexcept {
        if (program >= 100)
            return std::nullopt;
        const auto* bytes = user_ + Performances + size_t(program) * PerformanceBytes;
        if (!midiData(bytes, PerformanceBytes))
            return std::nullopt;
        KSPerformanceRecord parts;
        for (auto& patch : parts) {
            std::copy_n(bytes, patch.size(), patch.begin());
            bytes += patch.size();
        }
        return parts;
    }

    bool storeSingle(ProgramSelection selection, const uint8_t* bytes) noexcept {
        const auto offset = singleOffset(selection);
        if (!writable() || !offset || !midiData(bytes, KSVoiceImagePatch{}.size()))
            return false;
        std::copy_n(bytes,KSVoiceImagePatch{}.size(),user_ + *offset);
        if(selection.bank>=5)++drumRevisions_[selection.bank-5];
        return true;
    }

    // Desktop Drum Edit changes the project-owned working record. It does
    // not issue a hardware store command or touch the external factory data.
    bool editDrumVoice(ProgramSelection selection, uint8_t field, uint8_t value) noexcept {
        if (selection.bank < 5 || selection.bank > 8 || selection.program >= 49 ||
            !KSDrumVoiceEdit::voiceField(field) || value >= 128)
            return false;
        user_[*singleOffset(selection) + field] = value;
        ++drumRevisions_[selection.bank - 5];
        return true;
    }

    bool storePerformance(uint8_t program, const uint8_t* bytes) noexcept {
        if (!writable() || program >= 100 || !midiData(bytes, PerformanceBytes))
            return false;
        std::copy_n(bytes,PerformanceBytes,user_ + Performances + size_t(program) * PerformanceBytes);
        return true;
    }

private:
    static constexpr size_t Singles = 0x20000 - UserOffset;
    static constexpr size_t Performances = 0x45800 - UserOffset;
    static constexpr size_t PerformanceBytes = 1024;

    static bool midiData(const uint8_t* bytes, size_t size) noexcept {
        return bytes && std::all_of(bytes, bytes + size, [](uint8_t byte) { return byte < 128; });
    }

    static std::optional<size_t> singleOffset(ProgramSelection selection) noexcept {
        const auto bank = selection.bank, program = selection.program;
        if (bank < 1 || bank > 8 || program >= (bank <= 4 ? 100 : 50))
            return std::nullopt;
        const auto index = bank <= 4 ? (bank - 1) * 100 + program : 400 + (bank - 5) * 50 + program;
        return Singles + size_t(index) * KSVoiceImagePatch{}.size();
    }

    uint8_t* user_;
    std::array<uint64_t,4> drumRevisions_{1,1,1,1};
};

}
