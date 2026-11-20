#pragma once

#include "kExpressionPacket.h"

namespace novation::controller::command
{
    // Tunings and lookup data for one initialized K part. The loader owns
    // their provenance. The compiler only consumes musical and DSP values.
    struct KVoiceTunings
    {
        uint16_t oscillatorPitch=0, filterPitch=0, attack=0, envelope=0, lfo=0, portamento=0;
        uint16_t pitchBend=0;
        uint8_t partTranspose=0, finePitch=0, velocityCurve=0, retainedGlideNote=0;
        std::array<uint32_t,34> lfoRates{};
        std::array<std::array<uint8_t,3>,8> slotScales{};

        friend bool operator==(const KVoiceTunings& a,const KVoiceTunings& b) noexcept
        {
            return a.oscillatorPitch==b.oscillatorPitch && a.filterPitch==b.filterPitch && a.attack==b.attack &&
                   a.envelope==b.envelope && a.lfo==b.lfo && a.portamento==b.portamento && a.pitchBend==b.pitchBend &&
                   a.partTranspose==b.partTranspose && a.finePitch==b.finePitch && a.velocityCurve==b.velocityCurve &&
                   a.retainedGlideNote==b.retainedGlideNote && a.lfoRates==b.lfoRates && a.slotScales==b.slotScales;
        }
    };

    struct KPolyNoteImage
    {
        static constexpr size_t CutoffWord = 41;
        std::array<uint32_t,80> words{};
        uint32_t mode=0, level=0, trigger=0;
        uint8_t velocity=0, glideNote=0;
    };

    struct KRepeatNoteUpdate
    {
        std::array<uint32_t,3> velocityDepths{};
        std::array<uint32_t,4> pitch{};
        uint32_t trigger=0;
        uint8_t velocity=0,glideNote=0;
    };

    constexpr size_t KVoiceModeOffset=0x80;
    constexpr size_t KUnisonOffset=0x81;
    constexpr size_t KCutoffPatchOffset=0x2e;
    enum class KVoiceMode : uint8_t { Mono, MonoAutoglide, Poly, PolyRetrigger };

    constexpr bool isKMono(uint8_t mode) noexcept {return mode<=uint8_t(KVoiceMode::MonoAutoglide);}
    constexpr bool supportsKNote(uint8_t mode,uint8_t unison,uint8_t unit) noexcept
    { return mode<=uint8_t(KVoiceMode::PolyRetrigger) && unison<=7 && unit<=unison; }
    struct KVoiceStart
    {
        uint8_t unit=0;
        uint8_t excludedSlots=0; // reservations made for other units of this same note
        bool legato=false;
        bool velocityIsShaped=false;
    };

    constexpr uint32_t kFilterCutoffWord(uint8_t cutoff,uint16_t tuning) noexcept
    { return (uint32_t(cutoff)*256u*0xaaabu-0x48001400u+(uint32_t(tuning)<<16))>>8; }
    bool validKPolyNoteInput(const KExpressionSource& source,const KVoiceTunings& tunings,
        VoiceSlot slot,uint8_t note,uint8_t velocity,uint32_t previousPitch,KVoiceStart start) noexcept;

    // Full single-part voice image. Velocity curves beyond the
    // arithmetic modes need a separately verified threshold table.
    std::optional<KPolyNoteImage> compileKPolyNoteImage(const KExpressionSource& source,
        const KVoiceTunings& tunings,VoiceSlot slot,uint8_t note,uint8_t velocity,uint32_t previousPitch,
        KVoiceStart start={}) noexcept;
    std::optional<KRepeatNoteUpdate> compileKRepeatNoteUpdate(const KExpressionSource& source,
        const KVoiceTunings& tunings,uint8_t note,uint8_t velocity,uint32_t previousPitch,KVoiceStart start={}) noexcept;
    std::array<uint32_t,3> compileKOscillatorPitches(const KExpressionSource&,const KVoiceTunings&,uint8_t unit) noexcept;
}
