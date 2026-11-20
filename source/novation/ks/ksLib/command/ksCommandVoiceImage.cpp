#include "ksCommandVoiceImage.h"
#include "../ksParameterDependencies.h"
#include "../ksParameterScaleMath.h"

namespace novation::controller::command
{
    namespace
    {
        using ks::parameter::calculate;
        using ks::parameter::Formula;
        using ks::parameter::dependencySign8;
        using ks::parameter::dependencySign16;

        constexpr uint32_t word24(uint32_t value) noexcept { return value & 0xffffff; }
        constexpr uint32_t highByte(uint8_t value) noexcept { return uint32_t(value) << 16; }
        constexpr uint32_t highWord(uint16_t value) noexcept { return uint32_t(value) << 8; }
        constexpr uint32_t wave(uint8_t value) noexcept { return uint32_t(value) * 9 + 0x400; }
        constexpr uint32_t quarter(uint8_t value) noexcept { return uint32_t(value) << 14; }
        constexpr int centered(uint8_t value) noexcept { return dependencySign8(uint8_t(value - 64)); }

        constexpr uint32_t bipolar(uint8_t value) noexcept
        {
            return word24(uint32_t(centered(value) * 43690));
        }

        constexpr uint32_t bipolarHalf(uint8_t value) noexcept
        {
            return highWord(uint16_t(centered(value) * 128));
        }

        constexpr uint32_t endpoint(uint8_t value, bool doubled = false) noexcept
        {
            auto scaled = uint16_t(centered(value) * 256);
            if (value == 127)
                scaled |= 255;
            if (doubled)
            {
                scaled = uint16_t(scaled * 2u);
                return highWord(scaled) | uint8_t((scaled >> 8) - uint8_t(scaled));
            }
            return highWord(scaled) | uint8_t(scaled);
        }

        constexpr uint32_t square(uint8_t value) noexcept
        {
            const uint32_t scaled = (uint16_t(value) << 8) | uint8_t(value * 2);
            return (scaled * scaled * 2u) >> 8;
        }

        constexpr uint32_t signedSquare(uint8_t value) noexcept
        {
            const auto offset = centered(value);
            const auto magnitude = offset * offset * 8;
            return highWord(uint16_t(offset < 0 ? -magnitude : magnitude));
        }

        constexpr uint32_t velocityWord(uint8_t velocity, uint8_t depth) noexcept
        {
            const auto weight = centered(depth);
            if (weight < 0)
                velocity ^= 127;
            const auto offset = dependencySign8(uint8_t(velocity - 127));
            const auto product = offset * (weight < 0 ? -weight : weight) * 4;
            return product == 0 ? 0x7fffff : highWord(uint16_t(0x7f00 + product));
        }

        uint32_t coefficient(uint32_t input) noexcept
        {
            return uint32_t(ks::decodeParameterCoefficient(int16_t(dependencySign16(uint16_t(input)))));
        }

        uint32_t timeWord(uint8_t value, uint16_t base, uint8_t multiplier, uint8_t threshold,
                          bool half = false) noexcept
        {
            const auto input = calculate(Formula::TimeInput, {value, multiplier, threshold, base}).first;
            return coefficient(input) >> (half ? 9 : 8);
        }

        uint32_t curveWord(const KSVoiceImageTables& tables, uint8_t field) noexcept
        {
            const auto offset = centered(field);
            const auto index = unsigned(offset < 0 ? -offset : offset);
            return calculate(Formula::CurveProduct, {offset < 0, tables.modulationCurve[index]}).first;
        }

        uint32_t modulationWord(const KSVoiceImagePatch& patch, const KSVoiceImageTables& tables,
                                const KSVoiceImageContext& voice, unsigned bias, unsigned depth) noexcept
        {
            const auto index = calculate(Formula::ModulationIndex,
                {patch[bias], patch[depth], voice.modulation[0], patch[depth + 1], voice.modulation[1],
                 patch[depth + 2], voice.modulation[2]});
            return calculate(Formula::CurveProduct,
                {index.flags & 1u, tables.modulationCurve[index.first / 2]}).first;
        }

        uint32_t pitchWord(const KSVoiceImagePatch& patch, const KSVoiceImageTables& tables,
                           const KSVoiceImageContext& voice, unsigned lane) noexcept
        {
            const auto field = 7 + 8 * lane;
            const auto octave = calculate(Formula::PitchOctave, {tables.pitch, patch[6], lane}).first;
            const auto coarse = calculate(Formula::PitchCoarse, {octave, patch[field]}).first;
            const auto fine = uint16_t(uint8_t(voice.globalFine + patch[field + 1]) + voice.wheelFine - 192);
            const auto pitch = calculate(Formula::PitchFine, {coarse, fine}).first;
            return calculate(Formula::PitchWheel, {pitch, patch[field + 2], voice.wheelPitch}).first;
        }

        uint32_t lfoRateWord(const KSVoiceImageTables& tables, uint8_t rate) noexcept
        {
            const auto input = calculate(Formula::LfoRateInput, {rate, tables.lfoRate}).first;
            return calculate(Formula::LfoWeighted, {rate, coefficient(input)}).first >> 8;
        }

        std::array<uint32_t, 2> lfoRatePair(const KSVoiceImageTables& tables, uint8_t selector,
                                            uint8_t rate) noexcept
        {
            if (selector)
                return {0, tables.reciprocalTiming[selector - 1] >> 8};
            return {lfoRateWord(tables, rate), 0};
        }

        constexpr uint16_t read16(const uint8_t* bytes) noexcept
        {
            return uint16_t((uint16_t(bytes[0]) << 8) | bytes[1]);
        }
    }

    std::optional<KSVoiceImageTables> decodeKsVoiceImageTables(const uint8_t* image, size_t size) noexcept
    {
        constexpr size_t curveAddress = 0x15200;
        constexpr size_t timingAddress = 0xf20c;
        KSVoiceImageTables tables{};
        if (!image || size < curveAddress + tables.modulationCurve.size() * 2)
            return std::nullopt;

        const auto timingRate = read16(image + 0x1512e);
        if (!timingRate)
            return std::nullopt;
        tables.pitch = read16(image + 0x15130);
        tables.filter = read16(image + 0x15132);
        tables.attack = read16(image + 0x15134);
        tables.decay = read16(image + 0x15136);
        tables.lfoRate = read16(image + 0x15138);
        tables.lfoDelay = read16(image + 0x1513a);
        tables.glide = read16(image + 0x1513c);
        for (size_t i = 0; i < tables.modulationCurve.size(); ++i)
            tables.modulationCurve[i] = read16(image + curveAddress + 2 * i);

        // Startup's two 16-bit divisions compute a fixed-point quotient.
        const auto seed = uint16_t(0x3e800000u / timingRate);
        for (size_t i = 0; i < tables.reciprocalTiming.size(); ++i)
        {
            const auto divisor = uint16_t(read16(image + timingAddress + 2 * i) << 4);
            if (!divisor)
                return std::nullopt;
            tables.reciprocalTiming[i] = uint32_t((uint64_t(seed) << 16) / divisor) * 2;
        }
        return tables;
    }

    std::optional<KSVoiceImageResult> encodeKsVoiceImage(const KSVoiceImagePatch& patch,
        const KSVoiceImageTables& tables, const KSVoiceImageContext& voice,
        const KSCutoffWord& filter) noexcept
    {
        const auto& cutoff = filter.observation;
        const auto part = KSMusicalState::logicalPart(cutoff.part.part);
        if (!part || size_t(cutoff.allocation.slot) >= KSMusicalState::VoiceCount ||
            !(cutoff.part.session == cutoff.allocation.session) || cutoff.allocation.generation == 0 ||
            filter.word > 0xffffff ||
            cutoff.inputs.staticBase != tables.filter || cutoff.inputs.filter.velocityDepth != patch[150] ||
            cutoff.inputs.filter.cutoff >= 128 || cutoff.inputs.filter.velocityDepth >= 128 ||
            //128 is the native allocation's suppressed-velocity marker. It
            //is a verified later full-refresh input, not a legal MIDI byte.
            cutoff.inputs.velocity > 128 || patch[3] || patch[4] != 64 || (patch[0] & 7))
            return std::nullopt;

        for (const auto index : {11, 19, 27, 52})
            if (patch[index] >= 128)
                return std::nullopt;
        for (const auto index : {137, 138})
            if (patch[index] > tables.reciprocalTiming.size())
                return std::nullopt;
        const auto ratePairs = encodeKsVoiceTracking(patch, tables);
        if (!ratePairs)
            return std::nullopt;

        KSVoiceImageResult result{{}, *ratePairs};
        size_t position = 0;
        const auto append = [&](uint32_t word)
        {
            if (position < result.image.size())
                result.image[position] = word;
            ++position;
        };
        const auto velocity = [&](unsigned field) { return velocityWord(cutoff.inputs.velocity, patch[field]); };

        // Part header and the three equal-width oscillator lanes.
        append(0x67 + 3 * uint8_t(*part));
        append(calculate(Formula::HeaderMode, {patch[6]}).first);
        for (unsigned lane = 0; lane < 3; ++lane)
        {
            const auto field = 11 + 8 * lane;
            append(wave(patch[128 + lane]));
            append(endpoint(patch[field + 1]));
            append(endpoint(patch[field + 3], true));
            append(endpoint(patch[field + 2], true));
            append(pitchWord(patch, tables, voice, lane));
            append(curveWord(tables, patch[field]));
            append(modulationWord(patch, tables, voice, field - 1, 34));
            for (unsigned term = 31; term <= 33; ++term)
                append(bipolar(patch[term]));
            append(calculate(Formula::VoiceScale, {voice.oscillatorAmplitude[lane], patch[2]}).first);
        }

        append(quarter(patch[42]));
        append(bipolarHalf(patch[146]));
        constexpr std::array<unsigned, 7> gateFields{39, 143, 38, 142, 37, 141, 171};
        constexpr std::array<uint8_t, 7> gateModes{2, 2, 1, 1, 0, 0, 0};
        for (unsigned i = 0; i < gateFields.size(); ++i)
        {
            const auto value = patch[gateFields[i]];
            const bool level = i == 0 || i == 2 || i == 4;
            append(voice.panelGate && patch[222] != gateModes[i] ? 0 :
                level ? quarter(value) : bipolarHalf(value));
        }
        append(quarter(patch[40]));
        append(bipolarHalf(patch[144]));
        append(quarter(patch[41]));
        append(bipolarHalf(patch[145]));
        append(highByte(patch[43]));
        const auto volume = calculate(Formula::Volume, {patch[45], patch[44]});
        append(volume.first);
        append(volume.second);
        append(filter.word);
        append(curveWord(tables, patch[52]));
        append(modulationWord(patch, tables, voice, 51, 53));
        for (unsigned field = 48; field <= 50; ++field)
            append(bipolar(patch[field]));
        append(calculate(Formula::RateWord, {patch[47]}).first >> 8);
        append(patch[140]);
        append(patch[0] & 32 ? patch[140] : 3);

        // Envelope and auxiliary coefficients.
        append(square(patch[56]));
        append(signedSquare(patch[57]));
        append(signedSquare(patch[172]));
        append(velocity(58));
        append(timeWord(patch[59], tables.attack, 107, 117));
        append(timeWord(patch[60], tables.decay, 107, 127));
        append(patch[173] ? timeWord(patch[173], tables.lfoDelay, 80, 115, true) : 0);
        append(velocity(61));
        append(timeWord(patch[62], tables.attack, 107, 117));
        append(timeWord(patch[63], tables.decay, 107, 127));
        append(timeWord(patch[65], tables.decay, 107, 127));
        append(square(patch[64]));
        append(coefficient(calculate(Formula::ClockInput, {patch[125], patch[0]}).first) >> 8);
        append(uint8_t(*part));
        append(patch[148]);
        append(velocity(66));
        append(timeWord(patch[67], tables.attack, 107, 117));
        append(timeWord(patch[68], tables.decay, 107, 127));
        append(timeWord(patch[70], tables.decay, 107, 127));
        append(calculate(Formula::RateWord, {patch[69]}).first >> 8);

        for (unsigned lane = 0; lane < 2; ++lane)
        {
            const auto mode = patch[131 + lane];
            const auto shape = uint8_t(patch[135 + lane] * 2);
            const auto field = 72 + 3 * lane;
            const auto delaySelector = patch[137 + lane];
            const auto delay = delaySelector ? tables.reciprocalTiming[delaySelector - 1] >> 9 : 0;

            append(0x520u + uint8_t(mode * 4));
            append(highByte(shape) | shape);
            append((*ratePairs)[2 * lane]);
            append((*ratePairs)[2 * lane + 1]);
            append(calculate(Formula::LfoFlags, {patch[133 + lane], mode}).first >> 8);
            append(delaySelector ? 0 : timeWord(patch[field + 2], tables.lfoDelay, 80, 115, true));
            append(delay);
            append(velocity(169 + lane));
        }

        // Neutral detune leaves the last note intact. Zero glide emits a zero
        // rate and a unity companion before the previous-note word.
        append(highByte(uint8_t((patch[168] ? patch[168] : voice.transposedNote) - 60)));
        append(0);
        append(0x7fffff);
        append(highByte(uint8_t(voice.previousNote - 60)));
        if (position != result.image.size())
            return std::nullopt;
        return result;
    }

    std::optional<KSVoiceTrackingWords> encodeKsVoiceTracking(const KSVoiceImagePatch& patch,
        const KSVoiceImageTables& tables) noexcept
    {
        if (patch[73] > tables.reciprocalTiming.size() || patch[76] > tables.reciprocalTiming.size())
            return std::nullopt;
        const auto first = lfoRatePair(tables, patch[73], patch[72]);
        const auto second = lfoRatePair(tables, patch[76], patch[75]);
        return KSVoiceTrackingWords{first[0], first[1], second[0], second[1]};
    }
}
