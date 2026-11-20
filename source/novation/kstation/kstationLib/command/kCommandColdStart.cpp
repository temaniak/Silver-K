#include "kCommandColdStart.h"

#include "../kPhysicalTimeline.h"
#include "../../../controller/command/dspBoot.h"
#include "../../../controller/command/dspMusicalClock.h"

#include <stdexcept>

namespace novation::controller::command {
namespace {
uint16_t word(const std::vector<uint8_t>& firmware, size_t offset) {
    return uint16_t(firmware.at(offset) * 256u + firmware.at(offset + 1));
}

int signedByte(uint8_t value) {
    return value < 128 ? value : int(value) - 256;
}

struct BootPort {
    kstation::command::PhysicalTimeline& physical;
    uint8_t hostRead(uint32_t offset) { return physical.read(offset); }
    bool hostWrite(uint32_t offset, uint8_t value) { return physical.write(offset, value); }
};
}

KColdProgram readKColdProgram(const std::vector<uint8_t>& firmware, const KProgramTables& tables) {
    KColdProgram result{};
    for (size_t i = 0; i < result.record.size(); ++i)
        result.record[i] = firmware.at(0xba5c + i);
    result.channel = firmware.at(0xbb16);

    auto& expression = result.expression;
    for (size_t i = KPackedProgramBytes; i < expression.patch.size(); ++i)
        expression.patch[i] = firmware.at(0xba5c + i);
    expression.controls = {0, 0, 0};
    for (size_t i = 0; i < expression.modulationCurve.size(); ++i) {
        const auto bits = word(firmware, 0xbfca + 2 * i);
        expression.modulationCurve[i] = bits < 0x8000 ? int16_t(bits) : int16_t(int32_t(bits) - 0x10000);
    }
    expression = decodeKProgram(result.record.data(), expression);

    auto& tuning = result.tunings;
    tuning.oscillatorPitch = word(firmware, 0xbf9a);
    tuning.filterPitch = word(firmware, 0xbf9c);
    tuning.attack = word(firmware, 0xbf9e);
    tuning.envelope = word(firmware, 0xbfa0);
    tuning.lfo = word(firmware, 0xbfa4);
    tuning.portamento = word(firmware, 0xbfa6);
    tuning.pitchBend = word(firmware, 0xbfae);
    tuning.partTranspose = expression.patch[0xad];
    tuning.finePitch = firmware.at(0xbfb5);
    tuning.velocityCurve = firmware.at(0xbfb7);
    tuning.retainedGlideNote = 60;
    tuning.lfoRates = tables.syncRates;
    for (size_t i = 0; i < tuning.slotScales.size(); ++i) {
        const auto offset = firmware.at(0xc04c + i);
        const auto signedOffset = signedByte(offset);
        const auto upper = signedOffset < 0 ? 0x0f : (signedOffset <= 7 ? 0x27 : 0x37);
        const auto lower = signedOffset >= 0 ? 0xef : (signedOffset >= -8 ? 0xd7 : 0xc7);
        tuning.slotScales[i] = {offset, uint8_t(upper - signedOffset), uint8_t(lower - signedOffset)};
    }
    return result;
}

void bootKCommandDsp(kstation::command::PhysicalTimeline& physical, const std::vector<uint8_t>& firmware) {
    const auto program = readDspBootProgram(KBootImage, firmware);
    const auto table = readDspBootTable(KBootImage, firmware);
    if (physical.cycle() || physical.bootFinished())
        throw std::logic_error("K cold boot requires a fresh DSP instance");
    physical.reset(false);
    BootPort port{physical};
    const DspBootPort endpoint{&port, DspEndpoint::bind(port),
        [](void* context, BusCycle at) {
            auto& dsp = static_cast<BootPort*>(context)->physical;
            dsp.advanceTo(at.ticks);
            dsp.discardAudio();
        },
        [](void* context) { return BusCycle{static_cast<BootPort*>(context)->physical.cycle()}; },
        [](void* context) { return NativeFrame{static_cast<BootPort*>(context)->physical.frames()}; },
        [](void* context, const uint32_t* words, size_t size, uint32_t entry) {
            auto& dsp = static_cast<BootPort*>(context)->physical;
            if (!dsp.bootFinished()) return false;
            for (size_t i = 0; i < size; ++i)
                if (dsp.dsp().memory().get(dsp56k::MemArea_P, entry + uint32_t(i)) != words[i]) return false;
            return true;
        }};
    DspBootstrap boot(endpoint, KBootImage, 3);
    boot.start(program, table);
    std::array<uint32_t, 1024> harmonics{0x7fffff};
    for (unsigned i = 1; i < harmonics.size(); ++i)
        harmonics[i] = (0x8000u / (i + 1)) << 8;
    boot.write(0x8400, harmonics.data(), harmonics.size());
    constexpr uint32_t selectedVoice = 0;
    boot.write(0x802d, &selectedVoice, 1);
    constexpr uint32_t voices = 8;
    boot.write(2, &voices, 1);
    // The standalone panel starts at a fixed, nonzero master volume.
    constexpr uint32_t MasterLevel = 63;
    constexpr uint32_t MasterGain = (2 * MasterLevel * MasterLevel) << 8;
    boot.write(0x13, &MasterGain, 1);
    constexpr ClockRate rate{112000000, 2304};
    const double bpm = 64 + firmware.at(0xba5c + 0x54);
    const auto clock = encodeDspClock(rate, 60 * double(rate.numerator) / (24 * rate.denominator * bpm));
    boot.write(4, &clock->delay, 1);
    boot.write(3, &clock->phase, 1);
    // The audio ISR runs while the firmware builds its waveform tables.
    // The smoothed DAC gain is applied by the foreground only after that work.
    boot.waitResponse(0x8043, 0xffffff, MasterGain, 48000000, 8192);
}

}
