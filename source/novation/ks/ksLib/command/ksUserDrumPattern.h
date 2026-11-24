#pragma once

#include "../../../controller/command/controllerEvent.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <optional>

namespace novation::controller::command {

struct KSUserDrumPattern {
    using Steps = std::array<uint8_t, KSDrumGridEdit::Cells>;

    static constexpr Steps uniform(uint8_t value) noexcept {
        Steps result{};
        for (auto& step : result) step = value;
        return result;
    }

    std::array<uint8_t, KSDrumGridEdit::Rows> notes{36, 38, 42, 46, 39, 37, 45, 49, 41, 51};
    Steps velocities{};
    Steps probabilities = uniform(KSDrumGridEdit::MaxProbability);
    Steps ratchets = uniform(1);
    uint8_t steps = KSDrumGridEdit::Steps;

    bool hasHits() const noexcept {
        for (size_t row = 0; row < notes.size(); ++row) {
            const auto first = velocities.begin() + row * KSDrumGridEdit::Steps;
            if (std::any_of(first, first + steps, [](uint8_t velocity) { return velocity != 0; }))
                return true;
        }
        return false;
    }
};

struct KSDrumKeyRange {
    uint8_t low = KSDrumGridEdit::Unassigned, high = KSDrumGridEdit::Unassigned;

    bool assigned() const noexcept { return low < KSDrumGridEdit::Unassigned; }
    bool contains(uint8_t note) const noexcept { return assigned() && low <= note && note <= high; }
    bool operator==(const KSDrumKeyRange& other) const noexcept {
        return low == other.low && high == other.high;
    }
};

struct KSUserDrumBank {
    std::array<KSUserDrumPattern, KSDrumGridEdit::Patterns> patterns{};
    std::array<KSDrumKeyRange, KSDrumGridEdit::Patterns> keyRanges{};
    bool selected = false, assignedKeysOnly = false, retrigger = true;
    uint8_t switchAfter = KSDrumGridEdit::DefaultSwitchStep;
    uint8_t selectedPattern = 0;

    std::optional<uint8_t> unrestrictedPattern() const noexcept {
        std::optional<uint8_t> only;
        for (size_t i = 0; i < patterns.size(); ++i) {
            if (!patterns[i].hasHits()) continue;
            if (only || keyRanges[i].assigned()) return {};
            only = uint8_t(i);
        }
        return only;
    }

    std::optional<uint8_t> patternForKey(uint8_t key) const noexcept {
        if (key >= KSDrumGridEdit::Unassigned) return {};
        if (!assignedKeysOnly) return selectedPattern;
        std::optional<uint8_t> assigned;
        for (size_t i = 0; i < keyRanges.size(); ++i) {
            if (keyRanges[i].contains(key)) {
                assigned = uint8_t(i);
                if (patterns[i].hasHits()) return assigned;
                break;
            }
        }
        const auto unrestricted = unrestrictedPattern();
        return unrestricted ? unrestricted : assigned;
    }

    std::bitset<128> assignedNotes() const noexcept {
        std::bitset<128> notes;
        if (unrestrictedPattern()) return notes.set();
        for (const auto range : keyRanges)
            if (range.assigned())
                for (unsigned note = range.low; note <= range.high; ++note) notes.set(note);
        return notes;
    }

    void apply(KSDrumGridEdit edit) noexcept {
        auto& pattern = patterns[edit.pattern];
        switch (edit.kind) {
        case KSDrumGridEdit::Kind::SelectUser: selected = edit.value != 0; break;
        case KSDrumGridEdit::Kind::SelectPattern: selectedPattern = edit.value; break;
        case KSDrumGridEdit::Kind::AssignedKeys: assignedKeysOnly = edit.value != 0; break;
        case KSDrumGridEdit::Kind::Retrigger: retrigger = edit.value != 0; break;
        case KSDrumGridEdit::Kind::SwitchAfter: switchAfter = edit.value; break;
        case KSDrumGridEdit::Kind::Note: pattern.notes[edit.index] = edit.value; break;
        case KSDrumGridEdit::Kind::Velocity: pattern.velocities[edit.index] = edit.value; break;
        case KSDrumGridEdit::Kind::Probability: pattern.probabilities[edit.index] = edit.value; break;
        case KSDrumGridEdit::Kind::Ratchet: pattern.ratchets[edit.index] = edit.value; break;
        case KSDrumGridEdit::Kind::Remove:
            pattern.velocities[edit.index] = 0;
            pattern.probabilities[edit.index] = KSDrumGridEdit::MaxProbability;
            pattern.ratchets[edit.index] = 1;
            break;
        case KSDrumGridEdit::Kind::Length: pattern.steps = edit.value; break;
        case KSDrumGridEdit::Kind::KeyLow:
        case KSDrumGridEdit::Kind::KeyHigh: {
            auto& range = keyRanges[edit.pattern];
            if (edit.value == KSDrumGridEdit::Unassigned) range = {};
            else if (!range.assigned()) range = {edit.value, edit.value};
            else if (edit.kind == KSDrumGridEdit::Kind::KeyLow)
                range = {edit.value, std::max(range.high, edit.value)};
            else
                range = {std::min(range.low, edit.value), edit.value};
            break;
        }
        }
    }
};

using KSUserDrumPatterns = std::array<KSUserDrumBank, KSDrumGridEdit::Slots>;
using KSUserDrumBytes = std::array<uint8_t, 1 + KSDrumGridEdit::Slots *
    (2 + KSDrumGridEdit::Patterns * (3 + KSDrumGridEdit::Rows + 3 * KSDrumGridEdit::Cells))>;

inline KSUserDrumBytes encodeKSUserDrums(const KSUserDrumPatterns& patterns, bool includeRetrigger = true) noexcept {
    KSUserDrumBytes bytes{};
    auto next = bytes.begin();
    const bool extended = includeRetrigger && std::any_of(patterns.begin(), patterns.end(),
        [](const KSUserDrumBank& bank) { return !bank.retrigger; });
    *next++ = extended ? 6 : 5;
    for (const auto& bank : patterns) {
        *next++ = uint8_t(bank.selected | (bank.assignedKeysOnly << 1) | ((extended && !bank.retrigger) << 2));
        *next++ = bank.switchAfter;
        for (size_t i = 0; i < bank.patterns.size(); ++i) {
            const auto& pattern = bank.patterns[i];
            *next++ = bank.keyRanges[i].low;
            *next++ = bank.keyRanges[i].high;
            *next++ = pattern.steps;
            next = std::copy(pattern.notes.begin(), pattern.notes.end(), next);
            next = std::copy(pattern.velocities.begin(), pattern.velocities.end(), next);
            next = std::copy(pattern.probabilities.begin(), pattern.probabilities.end(), next);
            next = std::copy(pattern.ratchets.begin(), pattern.ratchets.end(), next);
        }
    }
    return bytes;
}

inline KSUserDrumPatterns decodeKSUserDrums(const uint8_t* bytes, size_t size) noexcept {
    constexpr size_t legacyRows = 8;
    constexpr size_t v1Size = 1 + KSDrumGridEdit::Slots * (1 + legacyRows * (1 + KSDrumGridEdit::Steps));
    constexpr size_t v2Size = 1 + KSDrumGridEdit::Slots *
        (1 + KSDrumGridEdit::Patterns * (2 + KSDrumGridEdit::Rows + KSDrumGridEdit::Cells));
    constexpr size_t v3v4Size = v2Size + KSDrumGridEdit::Slots * KSDrumGridEdit::Patterns * 2 * KSDrumGridEdit::Cells;
    if (!bytes || !size)
        return {};
    const auto version = *bytes++;
    if (!((version == 1 && size == v1Size) || (version == 2 && size == v2Size) ||
          ((version == 3 || version == 4) && size == v3v4Size) ||
          ((version == 5 || version == 6) && size == KSUserDrumBytes{}.size())))
        return {};
    const bool legacy = version == 1;
    const auto maxFlags = version == 4 || version == 6 ? 7 : version == 5 ? 3 : 1;
    KSUserDrumPatterns patterns{};
    for (auto& bank : patterns) {
        const auto flags = *bytes++;
        if (flags > maxFlags)
            return {};
        bank.selected = (flags & 1) != 0;
        if (version >= 4)
            bank.assignedKeysOnly = (flags & 2) != 0;
        if (version == 4 && !(flags & 4))
            bank.switchAfter = KSDrumGridEdit::SwitchImmediately;
        if (version == 6) bank.retrigger = (flags & 4) == 0;
        if (version >= 5) {
            bank.switchAfter = *bytes++;
            if (bank.switchAfter > KSDrumGridEdit::SwitchAtEnd) return {};
        }
        std::array<bool, 128> usedKeys{};
        const size_t count = legacy ? 1 : bank.patterns.size();
        const size_t rows = legacy ? legacyRows : KSDrumGridEdit::Rows;
        for (size_t i = 0; i < count; ++i) {
            auto& pattern = bank.patterns[i];
            if (!legacy) {
                const auto low = *bytes++;
                const auto high = version >= 5 ? *bytes++ : low;
                const auto steps = *bytes++;
                if (high > KSDrumGridEdit::Unassigned || low > high ||
                    (high == KSDrumGridEdit::Unassigned && low != high) ||
                    steps < 1 || steps > KSDrumGridEdit::Steps)
                    return {};
                if (version < 5) {
                    if (low >= usedKeys.size() || usedKeys[low]) return {};
                    usedKeys[low] = true;
                }
                bank.keyRanges[i] = {low, high};
                pattern.steps = steps;
            }
            for (size_t row = 0; row < rows; ++row) {
                const auto note = *bytes++;
                if (note < KSDrumGridEdit::FirstNote || note > KSDrumGridEdit::LastNote)
                    return {};
                pattern.notes[row] = note;
            }
            for (size_t step = 0; step < rows * KSDrumGridEdit::Steps; ++step) {
                const auto velocity = *bytes++;
                if (velocity > 127)
                    return {};
                pattern.velocities[step] = velocity;
            }
            if (version >= 3) {
                for (auto& probability : pattern.probabilities) {
                    probability = *bytes++;
                    if (probability > KSDrumGridEdit::MaxProbability)
                        return {};
                }
                for (auto& ratchet : pattern.ratchets) {
                    ratchet = *bytes++;
                    if (ratchet < 1 || ratchet > KSDrumGridEdit::MaxRatchet)
                        return {};
                }
            }
        }
    }
    return patterns;
}

}
