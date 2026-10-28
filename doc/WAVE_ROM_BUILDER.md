# Silver-K Wave ROM Builder

The companion application creates and edits the 129024-byte KS sampled-wave bank from your own WAV files. It has an English interface and requires no firmware or original sampled-wave bank. It does not replace the firmware or program data needed by Silver-K.

## Prepare a slot

1. Select a fixed **ROM Slot**. Positions 08–16 are tonal; 17–31 are rhythmic. Generated oscillators 00–07 are outside this bank.
2. **Load WAV**, or drop one WAV onto the window. Mono WAVs and the left channel of multichannel WAVs are supported, including PCM and floating-point files. Input limits are 8–384 kHz and 4000000 frames per file.
3. For tonal slots, the application finds a repeating cycle. For rhythmic slots, it finds the first attack and its decay, stopping at a quiet gap before a later hit. Detection is a starting point: complex, noisy or changing sounds may need manual boundaries.
4. Drag **Start** and **End**, or enter their times. Use the overview to navigate, the zoom buttons or mouse wheel to zoom, and Shift + wheel to pan. **Full WAV** restores the full view. Tonal slots offer **Snap to Zero** and a **Loop Seam** view; matching value and slope should still be checked by listening.
5. **Normalize** changes the prepared slot's peak level; its target is adjustable from -60 to -0.2 dBFS. The initial target is -1 dBFS. Rhythmic slots also offer **Fade In** and **Fade Out**, including zero. A long fade-in can soften an attack.
6. Select **Original** or **Result** and press **Play**. **Audio Output** chooses the output device. **Volume** changes monitoring only. **Note** selects a MIDI note for the result, using C4 = MIDI 60 and A4 = 440 Hz. Tonal results play as repeating cycles; **Loop** can be disabled. Rhythmic results play once unless Loop is enabled; C4 preserves the selected source duration and other notes transpose its playback speed. Original plays at its recorded rate.
7. **Save Slot** commits the result to the working bank. **Build ROM** also includes uncommitted prepared slots and writes a complete bank. Empty positions contain correctly encoded silence. Put the result in the main Silver-K folder's `ROMs` directory and select it under **Setup → Wave ROM**, between playback sessions.

Slot sizes, PCM encoding, byte order and placement are fixed. Fitting a source into a slot resamples it and can change its spectral detail; two positions use 8-bit PCM. Normalization and fades deliberately change level and envelope. Result monitoring uses the quantized PCM, with filtering appropriate to the selected preview note. This is an editor preview, without Silver-K's firmware voice engine, filters or envelopes; final playback should be checked in Silver-K.

## Edit an existing ROM

Use **Open ROM / Project** and select a compatible `.bin` or `.rom`. The file must be exactly 129024 bytes. All 24 slots are decoded and selectable. Normalization and fades start disabled for decoded data. Unedited slots retain their original bytes; opening a bank and exporting it without edits is byte-for-byte lossless. Export uses a new filename to preserve the imported original.

A raw ROM contains PCM only. Original WAVs, edit boundaries, normalization settings and recording sample rates cannot be recovered from it. The editor exposes the complete decoded block; tonal audition treats that block as one period. Rhythmic ROM-only audition uses a 32000 Hz reference at C4. These preview conventions do not establish the firmware's pitch or timing. If a tonal block contains several cycles, choose the desired cycle before rebuilding that slot.

The all-zero Silver-K placeholder is recognized as silence. An unchanged placeholder exports exactly; its first slot edit converts unused 8-bit positions to correctly encoded silence so they do not become full-scale DC.

## Keep an editable project

**Save Project** writes `.skwave`, including the mono source audio, every slot's boundaries and processing settings, the audition note, and the current ROM. Reopen it with **Open ROM / Project**. Source WAV files are never modified. Projects contain your audio and should be kept private alongside your own banks; they are ignored by the source repository. Projects are limited to 32000000 source frames in total. **New Bank** starts an empty bank.

Saving ROM preserves final PCM; saving a project also preserves how it was made. Use the project when you expect to change boundaries or processing later.

## Build

The application shares Silver-K's pinned JUCE source and builds as a separate target, without linking the synthesizer engine. A dedicated entrypoint downloads JUCE only:

```sh
cmake -S source/novation/waveRomBuilder/standalone -B build-wave -DCMAKE_BUILD_TYPE=Release
cmake --build build-wave --config Release --target silverWaveRomBuilder silverWaveBank_test --parallel 4
ctest --test-dir build-wave -C Release --output-on-failure -R '^silver_wave_bank$'
```

Dedicated-build products are in `build-wave/app/silverWaveRomBuilder_artefacts/Release`. The normal Silver-K build also includes these targets, with products in `waveRomBuilder/silverWaveRomBuilder_artefacts/Release` beside `silverPlugin`. Desktop release packaging includes the application and this guide in `Tools`. Windows uses the same targets with `-A x64` at configure time. New macOS builds default to Universal 2 (Apple Silicon and Intel) with a macOS 12.0 deployment target; older OS versions and physical Intel Macs need separate runtime validation. macOS builds are ad hoc signed, not notarized.

The core check covers all 8/16-bit PCM codes, exact ROM decode/export, isolation of slot edits, silent placeholders, cycle and transient detection, normalization, fades, downsampling rejection, note pitch, and allocation-free preview rendering and buffer replacement. GUI smoke mode (`--smoke <directory>`) uses only generated signals to check projects and ROM reopening, and captures tonal/rhythmic screens. Platform results are recorded with the delivered build; a macOS test does not validate Windows.
