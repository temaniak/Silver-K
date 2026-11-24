#include "ksCommandGlobals.h"

#include "../ksParameterScaleMath.h"
#include "ksRefreshPacket.h"

#include <algorithm>

namespace novation::controller::command
{
    KSGlobalPart KSGlobalPart::fromPatch(const std::array<uint8_t, 256>& patch) noexcept
    {
        KSGlobalPart part{};
        part.centeredControls = {patch[81], patch[82], patch[83]};
        part.externalFlags = patch[123];
        part.distortionLevel = patch[97];
        part.distortionWheel = patch[98];
        part.distortionCompensation = patch[99];
        part.eq = {patch[94], patch[95]};
        part.eqDepth = patch[96];
        part.eqFrequency = patch[93];
        part.eqShape = patch[92];
        part.chorus = {patch[112], patch[113]};
        part.chorusMode = patch[218];
        part.chorusDepth = patch[115];
        part.chorusCentre = patch[116];
        part.chorusFeedback = patch[114];
        part.chorusSend = patch[110];
        part.chorusWheel = patch[111];
        part.reverbSend = patch[107];
        part.reverbWheel = patch[108];
        part.reverbDecay = patch[109];
        part.reverbType = patch[217];
        part.partLevel = patch[179];
        part.partOutput = patch[180];
        part.delay = {patch[102], patch[103]};
        part.delayRatio = patch[106];
        part.delaySend = patch[100];
        part.delayWheel = patch[101];
        part.delayFeedback = patch[104];
        part.delayStereoWidth = patch[105];
        part.pan = {patch[118], patch[119]};
        part.panDepth = patch[120];
        part.panPosition = patch[117];
        part.triggerToggle = patch[246];
        part.triggerTime = patch[247];
        part.triggerAmount = patch[248];
        return part;
    }

    KSGlobalMaster KSGlobalMaster::fromPatch(const std::array<uint8_t, 256>& patch,
                                             uint8_t previousEffectStatus) noexcept
    {
        return {patch[89], patch[90], patch[91], patch[124], previousEffectStatus, patch[208]};
    }

    namespace
    {
        constexpr uint32_t highByte(uint8_t value) noexcept
        {
            return uint32_t(value) << 16;
        }

        constexpr uint16_t centeredTwice(uint8_t value) noexcept
        {
            return uint16_t(uint32_t(uint8_t(value - 64)) << 9);
        }

        constexpr uint32_t centeredWord(uint8_t value) noexcept
        {
            return uint32_t(centeredTwice(value)) << 8;
        }

        constexpr uint32_t gainWord(uint8_t value) noexcept
        {
            return uint32_t(uint8_t(value * 2u)) * 0x8102u;
        }

        constexpr uint32_t inverseWord(uint8_t value) noexcept
        {
            const auto inverted = uint8_t(value ^ 127u);
            return highByte(inverted) | (uint32_t(uint8_t(inverted * 2u)) << 8);
        }

        constexpr uint32_t balanceWord(uint8_t value) noexcept
        {
            const auto centered = uint8_t(value - 64);
            const auto shaped = uint16_t(((uint32_t(centered) << 8) | (centered == 63 ? 255u : 0u)) * 2u);
            return (uint32_t(shaped) << 8) | uint8_t((shaped >> 8) - uint8_t(shaped));
        }

        constexpr uint32_t compensationWord(uint8_t value) noexcept
        {
            const auto shaped = uint16_t(uint8_t(value - 128) >> 1) << 8;
            return uint32_t(shaped - (shaped >> 4)) << 8;
        }

        constexpr uint32_t frequencyWord(uint8_t value, uint16_t base) noexcept
        {
            const auto product = (uint32_t(value) << 8) * 0xcccdu;
            const auto shaped = uint16_t((product >> 16) + base - 0x3f50u);
            const auto bounded = (shaped & 0x8000u) ? 0x7fffu : shaped;
            return (bounded << 8) | uint8_t(product >> 8);
        }

        constexpr std::array<uint32_t, 2> rateWords(KSEffectTime control, const KSGlobalTables& tables) noexcept
        {
            if (control.sync)
                return {0, tables.reciprocalTimes[control.sync - 1] >> 8};
            const auto quotient = tables.timingRate ? uint16_t(0x28a1ea0u / tables.timingRate) : 0u;
            return {(quotient * control.time * control.time) >> 8, 0};
        }

        constexpr std::array<uint32_t, 2> delayWords(KSEffectTime control, const KSGlobalTables& tables) noexcept
        {
            if (control.sync)
                return {0, tables.delayTimes[control.sync - 1] >> 8};
            const auto scaled = (uint32_t(uint8_t(control.time * 2u)) * 175u * tables.timingRate) >> 16;
            return {std::clamp(scaled, 4u, 0x7ff8u), 0};
        }

        constexpr std::array<uint32_t, 2> chorusRange(const KSGlobalPart& part, uint16_t rate) noexcept
        {
            const bool wide = part.chorusMode != 0;
            const auto firstScale = uint16_t(((0x1feu * rate) >> 16) * 2u);
            const auto secondScale = uint16_t(((0x200u * rate) >> 16) * 4u);
            const auto first = wide ? ((uint32_t(part.chorusDepth) * 255u) >> 1) << 16
                                    : uint32_t(firstScale) * part.chorusDepth;
            const auto second = wide ? (uint32_t(part.chorusCentre) * 255u) << 16
                                     : uint32_t(secondScale) * part.chorusCentre;
            const auto lower = first + (wide ? 0x1000000u : 0x800u);
            const auto upper = (wide ? 0x7fff0000u : 0x30000u) - lower;
            // Two ordered bounds also define the result if the bounds cross.
            return {first >> 8, std::min(std::max(second, lower), upper) >> 8};
        }

        constexpr uint32_t feedbackWord(uint8_t value) noexcept
        {
            const auto bits = centeredTwice(value);
            const auto signedValue = bits < 0x8000u ? int32_t(bits) : int32_t(bits) - 0x10000;
            return (uint32_t(signedValue * 0x7333) * 2u) >> 8;
        }

        constexpr unsigned integerRoot(uint16_t value) noexcept
        {
            unsigned root = 0;
            for (unsigned bit = 128; bit; bit >>= 1)
            {
                const auto candidate = root + bit;
                if (candidate * candidate <= value)
                    root = candidate;
            }
            return root;
        }

        constexpr uint32_t panPositionWord(uint8_t value) noexcept
        {
            const auto centered = uint8_t(value - 64);
            const auto low = centered < 128 ? uint8_t(centered * 4u) : 0u;
            const auto word = uint16_t(((uint32_t(centered) << 8) | (low ? low | 3u : 0u)) * 2u);
            return (uint32_t(word) << 8) | uint8_t(word);
        }

        constexpr uint32_t triggerCoefficient(uint8_t time) noexcept
        {
            const auto bits = uint16_t(uint8_t(time + 104u) * 170u);
            const auto encoded = bits < 0x8000u ? int32_t(bits) : int32_t(bits) - 0x10000;
            return uint32_t(ks::decodeParameterCoefficient(int16_t(encoded))) >> 8;
        }

        constexpr uint32_t matrixWord(uint16_t coefficient, uint16_t rate) noexcept
        {
            return (uint32_t(uint16_t(coefficient * 2u)) * rate) >> 16;
        }

    }

    std::optional<KSGlobalTables> decodeKsGlobalTables(const uint8_t* decodedOs, size_t size) noexcept
    {
        constexpr size_t ImageSize = 87360;
        constexpr size_t TimingRate = 0x1512e;
        constexpr size_t ShapeBase = 0x15132;
        constexpr size_t MatrixRows = 0x15282;
        constexpr size_t DelayRatios = 0x152fa;
        constexpr size_t TimingFactors = 0xf20c;
        if (!decodedOs || size != ImageSize)
            return std::nullopt;

        const auto readWord = [decodedOs](size_t offset) -> uint16_t
        {
            return uint16_t((uint16_t(decodedOs[offset]) << 8) | decodedOs[offset + 1]);
        };
        KSGlobalTables tables{};
        tables.timingRate = readWord(TimingRate);
        tables.shapeBase = readWord(ShapeBase);
        if (!tables.timingRate)
            return std::nullopt;

        for (size_t i = 0; i < tables.reverb.size(); ++i)
        {
            auto& row = tables.reverb[i];
            const auto offset = MatrixRows + i * 20;
            row.bias = decodedOs[offset];
            row.depth = decodedOs[offset + 1];
            row.seed = readWord(offset + 2);
            row.factor = readWord(offset + 4);
            row.recurrenceBias = readWord(offset + 6);
            for (size_t j = 0; j < row.tail.size(); ++j)
                row.tail[j] = readWord(offset + 8 + j * 2);
        }
        for (size_t i = 0; i < tables.delayRatios.size(); ++i)
        {
            const auto offset = DelayRatios + i * 4;
            tables.delayRatios[i] = {readWord(offset), readWord(offset + 2)};
        }

        const auto reciprocalSeed = uint16_t(0x3e800000u / tables.timingRate);
        const auto delayScale = (0x8312u * tables.timingRate * 2u) >> 16;
        for (size_t i = 0; i < tables.reciprocalTimes.size(); ++i)
        {
            const auto factor = uint16_t(readWord(TimingFactors + i * 2) << 4);
            if (!factor)
                return std::nullopt;
            tables.reciprocalTimes[i] = ((uint32_t(reciprocalSeed) << 16) / factor) * 2u;
            tables.delayTimes[i] = uint16_t(factor * 2u) * delayScale;
        }
        return tables;
    }

    bool appendKsAuxiliary(KSRefreshPacket& packet, uint8_t liveAuxiliary) noexcept
    {
        return packet.appendWord(Endpoint::B, 0x8045, liveAuxiliary);
    }

    bool appendKsGlobals(KSRefreshPacket& packet, const KSGlobalPart& part, const KSGlobalMaster& master,
                         const KSGlobalTables& tables, std::optional<KSPart> performancePart) noexcept
    {
        const auto effectStatus = performancePart ? master.performanceEffectStatus : uint8_t(0);
        const bool statusChanged = master.previousEffectStatus != effectStatus;
        const auto wordCount = (performancePart ? 57u : 62u) + unsigned(statusChanged);
        if ((performancePart && size_t(*performancePart) >= KSMusicalState::PartCount) ||
            part.eq.sync > tables.reciprocalTimes.size() || part.chorus.sync > tables.reciprocalTimes.size() ||
            part.pan.sync > tables.reciprocalTimes.size() || part.delay.sync > tables.delayTimes.size() ||
            part.delayRatio >= tables.delayRatios.size() || !packet.canAppend(wordCount, wordCount))
            return false;

        const auto writeA = [&](uint16_t address, uint32_t word) { packet.appendWord(Endpoint::A, address, word); };
        const auto partOffset = performancePart ? uint16_t(KSGlobalPartStride * uint8_t(*performancePart)) : uint16_t(0);
        const auto writePart = [&](uint16_t address, uint32_t word) { writeA(uint16_t(address + partOffset), word); };
        const auto eq = rateWords(part.eq, tables);
        const auto chorus = rateWords(part.chorus, tables);
        const auto range = chorusRange(part, tables.timingRate);

        writePart(0x080b, (part.externalFlags & 0x40u) ? 0x7fff00u : 0u);
        writePart(0x080c, gainWord(part.distortionLevel));
        writePart(0x080d, balanceWord(part.distortionWheel));
        writePart(0x080e, compensationWord(part.distortionCompensation));
        for (unsigned i = 0; i < part.centeredControls.size(); ++i)
            writePart(uint16_t(0x080f + i), centeredWord(part.centeredControls[i]));
        writePart(0x0812, eq[0]);
        writePart(0x0813, eq[1]);
        writePart(0x0814, uint32_t(part.eqDepth) << 15);
        writePart(0x0815, frequencyWord(part.eqFrequency, tables.shapeBase));
        writePart(0x0816, centeredWord(part.eqShape));
        writePart(0x0817, chorus[0]);
        writePart(0x0818, chorus[1]);
        writePart(0x0819, part.chorusMode);
        writePart(0x081a, range[0]);
        writePart(0x081b, range[1]);
        writePart(0x081c, feedbackWord(part.chorusFeedback));
        writePart(0x081d, highByte(part.chorusSend));
        writePart(0x081e, centeredWord(part.chorusWheel));
        writePart(0x081f, part.chorusMode);
        writePart(0x0820, highByte(part.reverbSend));
        writePart(0x0821, centeredWord(part.reverbWheel));

        const auto& matrix = tables.reverb[std::min<size_t>(part.reverbType, tables.reverb.size() - 1)];
        const auto combined = uint16_t((uint16_t(matrix.bias) << 8) + part.reverbDecay * matrix.depth);
        const auto gain = integerRoot(uint16_t(combined * 2u)) * 128u + 0x7fffu;
        writePart(0x0822, uint32_t(combined) << 8);
        writePart(0x0823, 0x599a00u);
        writePart(0x0824, (0x599au * gain) >> 8);
        auto seed = matrix.seed;
        for (unsigned i = 0; i < 6; ++i)
        {
            writePart(uint16_t(0x0825 + i), matrixWord(seed, tables.timingRate));
            seed = uint16_t(matrixWord(seed, matrix.factor) + matrix.recurrenceBias);
        }
        for (unsigned i = 0; i < matrix.tail.size(); ++i)
            writePart(uint16_t(0x082b + i), matrixWord(matrix.tail[i], tables.timingRate));

        const auto route = encodeKsPartRoute(part.partLevel, part.partOutput);
        const auto delay = delayWords(part.delay, tables);
        const auto& ratio = tables.delayRatios[part.delayRatio];
        const auto pan = rateWords(part.pan, tables);
        writePart(KSPartOutputAddress, route[0]);
        writePart(KSPartOutputAddress + 1, route[1]);
        writePart(KSPartOutputAddress + 2, route[2]);
        writePart(0x0834, delay[0]);
        writePart(0x0835, delay[1]);
        writePart(0x0836, uint32_t(ratio.left) << 7);
        writePart(0x0837, uint32_t(ratio.right) << 7);
        writePart(0x0838, highByte(part.delaySend));
        writePart(0x0839, centeredWord(part.delayWheel));
        writePart(0x083a, highByte(part.delayFeedback));
        writePart(0x083b, inverseWord(part.delayStereoWidth));
        writePart(0x083c, pan[0]);
        writePart(0x083d, pan[1]);
        writePart(0x083e, gainWord(part.panDepth));
        writePart(0x083f, panPositionWord(part.panPosition));

        const auto level = highByte(master.sibilanceLevel);
        writeA(0x804d, gainWord(master.vocoderBalance));
        writeA(0x8049, master.sibilanceType ? 0 : level);
        writeA(0x804a, master.sibilanceType ? level : 0);
        writeA(0x804c, inverseWord(master.vocoderWidth));
        if (statusChanged)
            writeA(0x803f, 0x800000u | effectStatus);
        if (performancePart)
            return true;

        const auto triggerAmount = (uint32_t(part.triggerAmount) * 230u + 0x666u) << 8;
        const auto coefficient = triggerCoefficient(part.triggerTime);
        const auto first = part.triggerToggle ? 0u : coefficient;
        const auto second = part.triggerToggle ? coefficient : 0u;
        writeA(0x8058, (part.externalFlags & 0x20u) ? triggerAmount : 0u);
        writeA(0x0005, first);
        writeA(0x0006, second);
        packet.appendWord(Endpoint::B, 0x0005, first);
        packet.appendWord(Endpoint::B, 0x0006, second);
        return true;
    }
}
