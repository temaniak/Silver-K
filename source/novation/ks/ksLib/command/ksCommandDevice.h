#pragma once

#include "../../../controller/command/commandAudioDevice.h"
#include "ksUserDrumPattern.h"
#include "ksDesktopDrumMap.h"

namespace ks {
class Rom;

class KSCommandDevice final : public novation::controller::command::CommandAudioDevice {
public:
    KSCommandDevice(Format, Rom, std::vector<uint8_t> wave, std::vector<uint8_t> flashOrState,
                    novation::controller::command::KSUserDrumPatterns userDrums = {});
    bool loadDrumMap(const novation::controller::command::KSDesktopDrumMap&,
                     const std::function<bool(uint8_t)>& enqueue,
                     std::shared_ptr<novation::controller::command::KSDesktopPatchExtras> = {});
    bool loadPreset(const novation::controller::command::KSDesktopPreset&,
                    const std::function<bool(uint8_t)>& enqueue,
                    std::shared_ptr<novation::controller::command::KSDesktopPatchExtras> = {});
    bool switchPart(novation::controller::command::KSDesktopPartSwitch,
                    const std::function<bool(uint8_t)>& enqueue);
    novation::controller::command::KSDesktopSoundLoad& soundLoad(uint8_t index) noexcept {
        return drums_->loads[index];
    }
    std::optional<novation::controller::command::KSDrumBank> drumBank(uint8_t bank);
    novation::controller::command::KSDesktopLoadStatus drumLoadStatus() const noexcept;
    novation::controller::command::KSDesktopLoadStatus presetLoadStatus() const noexcept;
    bool partSwitchPending(uint64_t snapshotRevision) const noexcept;

private:
    KSCommandDevice(Format, Rom, std::vector<uint8_t>, std::vector<uint8_t>,
                    novation::controller::command::KSUserDrumPatterns,
                    std::shared_ptr<novation::controller::command::KSDesktopDrums>);
    std::shared_ptr<novation::controller::command::KSDesktopDrums> drums_;
    using SoundLoad = novation::controller::command::KSDesktopSoundLoad;
    SoundLoad* lastDrumLoad_ = nullptr;
    SoundLoad* lastPresetLoad_ = nullptr;
    SoundLoad* lastPartSwitch_ = nullptr;
    bool loadSound(const novation::controller::command::KSDesktopSound&, const std::function<bool(uint8_t)>&,
                   SoundLoad*&, std::shared_ptr<novation::controller::command::KSDesktopPatchExtras> = {});
    novation::controller::command::KSDesktopLoadStatus loadStatus(const SoundLoad*) const noexcept;
};
}
