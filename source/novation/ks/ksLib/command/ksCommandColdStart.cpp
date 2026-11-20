#include "ksCommandColdStart.h"

#include "ksCommandFeedback.h"
#include "ksParameterIngress.h"
#include "ksRefreshPacket.h"
#include "../ksPhysicalDspPair.h"
#include "../../../controller/command/dspBoot.h"
#include "../../../controller/command/dspMusicalClock.h"
#include "../../../controller/command/novationProgramSysEx.h"

#include <stdexcept>

namespace novation::controller::command {
namespace {
constexpr size_t WaveBytes = 129024;
constexpr uint16_t SampleStatus = 0x803e, SampleBuffer = 0x8c00;

struct WaveSegment {
    uint32_t descriptor;
    size_t offset, samples;
};

uint32_t big32(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i)
        value = (value << 8) | bytes.at(offset + i);
    return value;
}

bool byteSample(uint32_t descriptor) {
    return (descriptor & 0x08000000) != 0;
}

std::vector<WaveSegment> describeWaves(const std::vector<uint8_t>& firmware, size_t bytes) {
    if (bytes != WaveBytes)
        throw std::runtime_error("KS Wave ROM size differs from firmware descriptors");
    std::vector<WaveSegment> segments;
    size_t offset = 0;
    for (size_t i = 0; i < 24; ++i) {
        const size_t count = big32(firmware, 0x15140 + 8 * i) >> 16;
        auto descriptor = big32(firmware, 0x15144 + 8 * i);
        if (!count) continue;
        const size_t size = count <= 2048 ? count : 1024;
        for (size_t segment = 0; segment < count / size; ++segment) {
            const auto length = size * (byteSample(descriptor) ? 1 : 2);
            if (length > bytes - offset)
                throw std::runtime_error("KS Wave ROM descriptor exceeds input");
            segments.push_back({descriptor, offset, size});
            offset += length;
            descriptor += 0x40000;
        }
    }
    if (offset != bytes)
        throw std::runtime_error("KS Wave ROM descriptor extent differs");
    return segments;
}

void uploadWaves(DspBootstrap& boot, const std::vector<uint8_t>& wave, const std::vector<WaveSegment>& segments) {
    constexpr uint64_t ResponseDeadline = 250000;
    const auto awaitBuffer = [&](uint64_t maxCycles) { boot.waitResponse(SampleStatus, 0xff0000, 0x800000, maxCycles); };
    std::array<uint32_t, 1024> harmonics{0x7fffff};
    for (unsigned i = 1; i < harmonics.size(); ++i)
        harmonics[i] = (0x8000u / (i + 1)) << 8;
    boot.write(SampleBuffer, harmonics.data(), harmonics.size());
    // The first response includes synthesis of the resident analog waves.
    awaitBuffer(2500000);

    std::array<uint32_t, 2048> words;
    for (const auto segment : segments) {
        auto offset = segment.offset;
        const auto suffix = uint8_t(segment.descriptor);
        for (size_t i = 0; i < segment.samples; ++i) {
            const auto high = wave[offset++];
            if (byteSample(segment.descriptor)) {
                words[i] = (uint32_t(uint8_t(high + 128)) << 16) | suffix;
            } else {
                words[i] = (uint32_t(high) << 16) | (uint32_t(wave[offset++]) << 8) | suffix;
            }
        }
        boot.write(SampleBuffer, words.data(), segment.samples);
        const auto commit = segment.descriptor >> 8;
        boot.write(SampleStatus, &commit, 1);
        awaitBuffer(ResponseDeadline);
    }
    constexpr uint32_t Finish = 0xffffff;
    boot.write(SampleStatus, &Finish, 1);
    boot.waitResponse(0x803d, 0xff, 1, ResponseDeadline);
    constexpr uint32_t VoiceConfiguration = 14;
    boot.write(2, &VoiceConfiguration, 1);
}

struct BootPort {
    ks::PhysicalDspPair& pair;
    unsigned index;
    ks::DSP& dsp() { return pair.dsp(index); }

    DspBootPort bind() {
        return {this, DspEndpoint::bind(dsp()),
            [](void* context, BusCycle at) {
                auto& pair = static_cast<BootPort*>(context)->pair;
                pair.advanceTo(at.ticks);
                pair.discardAudio();
            },
            [](void* context) { return BusCycle{static_cast<BootPort*>(context)->pair.controllerCycle()}; },
            [](void* context) { return NativeFrame{static_cast<BootPort*>(context)->pair.dsp(1).audioFramesProduced()}; },
            [](void* context, const uint32_t* words, size_t size, uint32_t entry) {
                auto& dsp = static_cast<BootPort*>(context)->dsp();
                if (!dsp.bootFinished()) return false;
                for (size_t i = 0; i < size; ++i)
                    if (dsp.memory().get(dsp56k::MemArea_P, entry + uint32_t(i)) != words[i]) return false;
                return true;
            }};
    }
};

}

KSColdPrograms readKsColdPrograms(const KSProgramFlash& flash) {
    const auto mode = flash.global(4);
    KSColdPrograms result{};
    // Early version-one command archives also encoded Performance as zero
    // in the Performance bank field. Accept that layout on import.
    result.layout = mode == 1 || flash.global(0) == 0 ? KSLayout::Performance : KSLayout::Single;
    result.single = {flash.global(2), flash.global(3)};
    result.performance = flash.global(1);
    result.globalChannel = flash.global(8);
    result.activeMask = 1;
    const auto single = flash.single(result.single);
    if (!single || mode > 1 || result.globalChannel >= 16)
        throw std::runtime_error("KS cold selection or global channel is invalid");
    result.patches[0] = *single;
    if (result.layout == KSLayout::Performance) {
        const auto performance = flash.performance(result.performance);
        if (!performance)
            throw std::runtime_error("KS cold Performance is unavailable");
        std::copy(performance->begin(), performance->end(), result.patches.begin() + 1);
        result.activeMask = uint8_t((*performance)[0][190] & 15);
    }
    for (auto& patch : result.patches) {
        expandKsParameterFields(patch);
        patch[229] = flash.global(14); // Centre of the live pitch control.
        for (size_t i = 0; i < 21; ++i)
            patch[234 + i] = flash.global(i);
    }
    return result;
}

bool restoreKsColdProgram(KSColdPrograms& programs, SysExBytes saved) noexcept {
    const auto frame = NovationProgramSysEx::decode(Model::KS, programs.globalChannel, saved);
    if (!frame || (frame->type != 0 && frame->type != 2))
        return false;
    const bool single = frame->type == 0;
    programs.layout = single ? KSLayout::Single : KSLayout::Performance;
    programs.activeMask = single ? 1 : uint8_t(frame->data[190] & 15);
    for (size_t i = 0; i < (single ? 1u : 4u); ++i) {
        auto& patch = programs.patches[i + !single];
        std::copy_n(frame->data + 256 * i, 209, patch.begin());
        expandKsParameterFields(patch);
    }
    // The first part carries the shared tempo in a saved Performance dump.
    if (!single)
        programs.patches[0][84] = programs.patches[1][84];
    return true;
}

std::array<uint8_t, 3> ksColdVoiceScale(const std::vector<uint8_t>& firmware, size_t slot) {
    if (slot >= KSMusicalState::VoiceCount)
        throw std::out_of_range("KS cold voice scale slot");
    const auto offset = firmware.at(0x1535a + slot);
    const auto signedOffset = offset < 128 ? int(offset) : int(offset) - 256;
    const auto lower = signedOffset < 0 ? 15 : (signedOffset <= 7 ? 39 : 55);
    const auto upper = signedOffset >= 0 ? 239 : (signedOffset >= -8 ? 215 : 199);
    return {offset, uint8_t(lower - signedOffset), uint8_t(upper - signedOffset)};
}

void bootKsCommandDsps(ks::PhysicalDspPair& pair, const std::vector<uint8_t>& firmware,
                      const std::vector<uint8_t>& wave, const KSColdPrograms& programs, const KSGlobalTables& tables) {
    const auto imageA = readDspBootProgram(KsABootImage, firmware);
    const auto imageB = readDspBootProgram(KsBBootImage, firmware);
    const auto table = readDspBootTable(KsABootImage, firmware);
    const auto segments = describeWaves(firmware, wave.size());
    if (pair.controllerCycle() || pair.dsp(0).bootFinished() || pair.dsp(1).bootFinished())
        throw std::logic_error("KS cold boot requires a fresh DSP pair");
    BootPort portA{pair, 0}, portB{pair, 1};
    DspBootstrap a(portA.bind(), KsABootImage, 3), b(portB.bind(), KsBBootImage, 3);
    a.start(imageA, table);
    b.start(imageB, table);
    uploadWaves(b, wave, segments);
    if (!pair.startedBoth() || !pair.serialRunning())
        throw std::runtime_error("KS DSP serial link did not start");

    constexpr uint32_t MasterLevel = 63;
    constexpr uint32_t MasterGain = (2 * MasterLevel * MasterLevel) << 8;
    a.write(0x18, &MasterGain, 1);

    auto master = programs.master();
    const auto send = [&](const KSRefreshPacket& packet) {
        for (size_t i = 0; i < packet.size(); ++i) {
            const auto write = *packet.at(i);
            auto& boot = write.endpoint == Endpoint::A ? a : b;
            boot.write(write.address, write.words, write.count);
        }
    };
    KSRefreshPacket auxiliary;
    if (!appendKsAuxiliary(auxiliary, programs.patches[0][225]))
        throw std::runtime_error("KS cold auxiliary source is invalid");
    send(auxiliary);
    const bool single = programs.layout == KSLayout::Single;
    for (size_t part = 0; part < (single ? 1u : 4u); ++part) {
        KSRefreshPacket packet;
        const auto& patch = programs.patches[part + !single];
        const auto logical = single ? std::nullopt : std::optional{KSPart(part)};
        if (!appendKsGlobals(packet, KSGlobalPart::fromPatch(patch), master, tables, logical))
            throw std::runtime_error("KS cold patch globals are invalid");
        send(packet);
        if (!single) master.previousEffectStatus = master.performanceEffectStatus;
    }
    const uint32_t activeMask = programs.activeMask;
    constexpr ClockRate rate{ks::DSP::ClockHz, 2 * ks::DSP::SerialSlotCycles};
    const double bpm = 64 + programs.patches[0][84];
    const auto clock = encodeDspClock(rate, 60 * double(rate.numerator) / (24 * rate.denominator * bpm));
    a.write(4, &clock->delay, 1);
    b.write(3, &clock->phase, 1);
    a.write(3, &clock->phase, 1);
    a.write(0x8056, &activeMask, 1);
    a.waitResponse(KSCommandFeedback::partStatusAddress(KSPart::One), 0, 0, 25000);
}

}
