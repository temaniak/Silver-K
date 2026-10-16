# Silver-K 0.2.4 Beta

A combined Windows x64 Standalone/VST3 and macOS arm64 Standalone/VST3/AU instrument for original K-Station OS 2.1.01 and KS OS 2.1.02 firmware. The project was extracted from ungeermoolator and uses Gearmulator components. See the [installation guide](INSTALL.md) for platform installation and ROM/Data setup.

The original H8/DSP engines run internally. Firmware is validated by contents and SHA-256, not filename. The integration uses Gearmulator patchDB for folder scanning, caching, tags, favourites and metadata changes. K-Station retains the Gearmulator/libresample rate converter; KS retains its four-channel stream. ROM code is not embedded in the application or DAW project state.

## Getting started

1. On Setup, select **Silver-K folder**. Firmware in its `ROMs` subfolder is scanned; decoded BIN, OS/SYX and firmware ZIP updates are supported. Select firmware in the menu at the top.
2. For KS, select our `assets/ks-init-flash.bin` (524288 bytes) under **KS factory data**. It contains only Silver-K initial sounds. Alternatively, supply an official All Data MIDI file locally. A separate Wave ROM can be selected (129024 bytes); without one, the engine uses an in-memory zero bank. For a zero bank, ROM-dependent fields are silenced in a working Flash copy before boot; subsequent edits are adapted through original MIDI commands. Original levels remain in snapshots and saved state. This adaptation is disabled with a real bank.
3. The main folder’s `Patches` subfolder is added automatically. On Patches, add another folder, SYX, MID, V-Station FXB/FXP or ZIP. Files and subfolders are scanned in the background. Selecting a row loads a patch into the selected part. All column headers sort in both directions, with the selected patch retained across reordering and filters. Tags and favourites are stored separately from source files.

On macOS, the library is stored in `~/Library/The Usual Suspects/Silver-K/patches`. Isolated checks can use `SILVER_K_DATA` to select a separate database; this does not supply factory ROMs.

## Interface

The 1280 × 752 layout follows the existing Silver-K K-Station panel. OSC/Mixer/Filter/LFO/Env and the lower effects sections are retained. The top pages are Main, Extra, Perf, Patches and Setup.

Tabs 1–4 select a part. K-Station exposes only the first part; the remaining tabs are dimmed. For four KS parts, enable Performance on Perf. This page controls part activity, MIDI channel, keyboard range, transpose, level and Main 1+2 / Aux 3+4 / Mono 1–4 output routing. Switching tabs does not itself stop notes on other parts.

KS editor changes and automation address individual parts, including tuning and Part volume, even when parts share a MIDI channel. Incoming MIDI notes and controllers still affect all matching parts for layered playing. User MIDI channels are preserved in Performance and project state; internal engine channels are hidden. Level and tuning edits do not reload the Performance or interrupt held notes.

KS **Program boost (dB)** in Voice / Performance provides 0–30 dB of gain, matching the firmware's ENV ProgBoost menu. At 0 dB, the program uses its normal unboosted level. This is separate from Part volume (0–127). The erroneous boost range of 0–127 has been corrected; host ID `voice_level` is preserved, but normalized automation now maps to 0–30. Legacy program values above 30 reset to 0 dB; valid values are preserved. High boost with loud oscillators or polyphony can still overload the signal path. See the [KS Rack manual, pages 39 and 63](https://fael-downloads-prod.focusrite.com/customer/dev/s3fs-public/novation/downloads/10720/english102.pdf).

Auto mode uses the original dark lower panel for K-Station and a grey panel with dark labels for KS. Setup offers Silver-K, KS Grey, Classic and custom JSON skins based on the supplied skin. The template has a fixed 1280 × 752 size.

VST3/AU expose two stereo buses; the host can disable Aux. Standalone supports two or four physical output channels. K-Station outputs 3–4 are silent. KS routing preserves its original MIDI parameters and four-part engine.

## Patch compatibility

The browser shows the SysEx source model, Program/Performance type, source, tags and compatibility status. Original data stays in the database; conversion operates on a copy during loading.

KS → K-Station:

- Extra oscillator waves, including noise variants, are replaced with sine.
- Every filter becomes LP 12 dB/oct, including an original LP 24 filter.
- Extra LFO and arpeggiator shapes use K-Station defaults.
- KS-only parameters, such as level modulation, extended LFO settings and part routing, are not transferred. The report lists lost settings that differ from defaults.
- A Performance transfers only the selected part. K-Station cannot reproduce four parts.

K-Station → KS preserves the shared block, unpacks wave fields and adapts additional-field encoding. KS settings start at defaults. In KS Performance mode, the selected part's sound is replaced while its MIDI channel, output and keyboard range are preserved.

A native KS Performance loads the complete setup and enables Performance mode. Native patches preserve their sound payload. Headers are normalized for loading into the current sound, without writing to the synthesizer's Flash. The status line and its tooltip show the replacement report while browsing.

A-Station programs preserve their 128-byte sound block on K-Station, except that dumps through version 08.07 clear the legacy filter-drive field, as the original K importer does. A-Station → KS clears filter drive for all versions and follows the original KS importer's wave, glide, keysync and level encoding. K/A attenuation becomes 0 dB on KS, which supports positive program boost only. X-Station programs preserve their 256-byte sound block when adapted to KS transport headers; X-Station → K follows the KS → K conversion above. SYX/MID/MIDI/ZIP import includes A/K Program Pair bank messages, which contain two adjacent programs. X-Station names are read from their payload; its firmware-version header distinguishes its shared model ID from KS. Export retains source-model bytes, including unknown X-Station tail fields.

The [Novation compatibility table](https://support.novationmusic.com/hc/en-gb/articles/206865169-A-Station-K-Station-V-Station-X-Station-Xio-and-KS-Series-patch-compatibility) documents A/K and X/KS program compatibility. This implementation adds conversion for the other destination engine; conversion does not establish identical sound on physical instruments.

## Host MIDI clock

VST3/AU read host BPM, PPQ and transport and deliver 24 MIDI clocks per quarter note to the original UART, with sample offsets. Start/Stop, looping and timeline seeks reset the transport phase. Clocks continue while the host is stopped so tempo-synchronised effects remain usable while auditioning. Incoming clock/transport messages are suppressed while the host owns timing, preventing two clock sources from doubling the tempo. With no host playhead, incoming MIDI clock is passed through and firmware uses Auto clock source.

Both supported OS 2.1 images use clock-source values 0=Auto, 1=Internal, 2=External; the old manuals' 0/1 description does not match these images. The plugin selects External when host tempo is available. The firmware retains its own tempo-tracking filter, so external clock acquisition and large tempo changes take time to settle. Sync controls set to Off still use their manual rates.

System realtime bytes (F8 and transport) have reserved UART queue space and can interrupt queued CC/NRPN/SysEx bytes. Ordinary messages retain their byte order. This keeps parameter editing from postponing clock ticks; the callback uses fixed queues and does not allocate or block.

KS OS measures clock intervals with TPU1 at CPU clock / 256. That counter now advances, stops and wraps according to the configured hardware timer; without it, incoming clocks reached the firmware but its tempo filter could not measure them.

KS Single and Performance saves now serialize canonical controller mirrors without executing the guest. The previous Single dump request advanced the firmware while host audio counters stayed still, shifting clock and buffered sound after a state read. Saving a current program or DAW state now preserves that audio timeline.

## State and limitations

Project state stores the selected model, ROM paths and hashes, current native sound/Performance, user data, selected part, skin and parameters. Tags and folders live in a separate persistent database. Changing firmware cold-reloads the engine and interrupts sound. UI firmware selection loads and warms up the DSP in the background while showing `Loading firmware`; controls are temporarily unavailable and the host window continues processing events. The previous separate K-Station/KS products and plugin IDs are retained; Silver-K has its own ID, `Tskv`.

Both models have persistent host parameter IDs: KS uses the existing names and `part2_`/`part3_`/`part4_` prefixes; K-Station uses `k_`. The editor exposes the selected model's controls, while the host sees a stable complete parameter set so automation keeps its meaning after ROM selection.

K-Station retains a separate optional mono Vocoder/external input and the shared Gearmulator input converter. KS external audio input is not connected. Sound accuracy against hardware has not been established by this integration. The KS zero-bank limitations still apply.

CPU performance, latency and optimization remain under development. Keep original banks and verify saved patches by reloading them. Some preset-loading paths reload the Performance and interrupt sound.

Builds, local ROMs, test WAVs and validation states remain outside tracked source. Win64 is built in CI from the same source. Local audio validation was performed on modern macOS arm64; a deployment target of macOS 12 does not establish compatibility with all older releases, because dependencies are built with the current SDK.

## Validation

`silver_host_clock` checks sample offsets and 24 PPQN at 44.1/48/96 kHz with 1/127/256/511/4096-sample blocks, tempo changes, seek/loop, stop, missing host and negative PPQ. `silver_patch_workflow` covers every sortable column in both directions, selection/filter preservation, single-selection loading with firmware, four source models, A/K pairs, mixed MIDI and priority UART ordering. Its optional private-ROM run measures arpeggiator attack spacing at 90/180 BPM while continuously editing Pulse and reading state during a held arpeggio, rejects audio callback allocations, and compares A/X conversion with the original KS importer.

The actual VST3/AU check utilities accept `SILVER_TEST_HOST_BPM=90` or `180` to exercise host tempo callbacks, a quarter-note arpeggio and live Pulse changes. State is read between clock acquisition and capture; the test checks note spacing and that the capture adds no underruns beyond warm-up. Pulse widths stay around the centre so a silent zero/full-duty waveform cannot masquerade as a missing arp note. Startup/JIT and physical driver latency are outside that steady-playback check.

`silver_check` checks extra-wave/LP 12/arpeggiator conversion, native payload preservation, folder import, model detection, tags/favourites, a shared instance database and required skin controls. With local ROMs, additional checks cover audio, live cutoff on held notes, silent K-Station Aux, both models' state restoration, repeated `prepareToPlay` after K-Station state, and full KS Performance. Checked callbacks had no C++ allocations through `operator new`; this does not separately intercept every C `malloc`.

KST100 conversion was compared with original KS UART import: all 151 synthesizer bytes matched for each of the 16 source keysync values. This validates that patch and field variants, rather than sound identity between physical instruments.

The local compatibility run imported 158 A-Station and 200 X-Station programs, including bank messages, and compared their synthesizer bytes with original KS UART import. All 358 matched. KS CC70 was also verified to store the glide curve in program byte 5 bit 0, correcting the prior bit-6 descriptor and preserving K/KS glide conversion in both directions.

Earlier hosted VST3/AU checks used 48 kHz, 256-sample buffers and four seconds of real-time playback. KS output peaks were 0.0359975 / 0.0359963 / 0.0474905 / 0.0475713 with zero underruns. K-Station stereo peak was 0.105694 with zero underruns. Maximum callbacks on the test machine were: VST3 KS 0.113 ms, AU KS 0.065 ms, VST3 K-Station 0.192 ms and AU K-Station 0.159 ms. These checks exclude physical MIDI/audio-driver latency. See the [release notes](releases/0.2.4.md) for final release validation.

## Realtime buffering and external data (0.2.1)

Setup offers Low latency, Balanced (the default), Safe and Maximum reserve. The latter three apply minimum scheduling horizons of 512, 1024 and 2048 native frames; Low latency uses the block-dependent horizon. K-Station runs at approximately 48.611 kHz and includes the existing sample-rate converter delay. KS uses the host sample rate. Reported plugin latency follows the selected horizon; the audio interface buffer remains a host setting. Changing this choice preserves the running firmware, voices and effect state. A redundant host preparation with unchanged rate/block also preserves the running engine.

K-Station and KS publish preset/Flash snapshots from their worker threads. Host state reads and KS diagnostics no longer pause original firmware. Flash snapshots refresh after completed guest Flash writes; project state reads the latest published snapshot. Parameter and MIDI changes still go through original firmware. No sonic algorithm is replaced.

Both workers join the host audio workgroup where available. KS uses audio priority after boot/JIT warm-up. Both engines warm common polyphonic voice paths before opening audio. The K-Station low-latency horizon uses a 256-frame minimum and 128-frame reserve; KS uses max(256, host block + 128), capped at 4096 frames. Slow startup/JIT work and sustained CPU overload can still exceed a small reserve.

The default main folder is `Documents/Silver-K`, containing `ROMs` and `Patches`. Setup remembers another chosen root. ROMs holds original externally supplied firmware, KS Data, optional real Wave ROM, and our generated synthetic placeholders. Patches is scanned automatically. The metadata database remains in Application Support so favourites and tags survive migration. Project files use relative asset paths within the root, with legacy path support and existing hash checks.

`silverRealtime_test` exercises 12 seconds of live callbacks with rapid eight-note chords, state reads every 100 ms and a redundant host prepare. Optional buffer sweeps change the reserve while playing. It reports startup/steady missed frames, first-note onset and maximum callback/state-read duration. Private ROMs are command-line inputs; use `SILVER_K_DATA` for an isolated data folder. Run these timing tests without a concurrent compiler or another emulation benchmark.

The validated firmware index is persisted with the main folder. Startup restores this catalog without enumerating ROMs or decoding firmware archives. Setup → Rescan ROMs explicitly replaces the catalog after files change. The selected file is always validated before loading; cached metadata never substitutes for ROM content validation.

V-Station support validates legacy VST2 chunk banks (`CcnK`, `FBCh`, `NvS0`, 400 slots, 59608-byte chunk with leading byte 1) and individual chunk presets (`FPCh`, `NvS0`, one program, 149-byte chunk: 128 sound bytes plus a 21-byte name). Containers, seven-bit sounds and name tables are checked completely before importing. The chunk name takes precedence over stale FXP header labels. V-Station has its own source model metadata and browser filter, retained across cache loads; legacy V-tagged imports are recognized and refreshed once. The transport into the K firmware uses the common 128-byte sound format without changing sound data. KS receives its existing K-compatible import with a 16-character name. This does not add a separate V-Station DSP engine. K/A individual filenames and ZIP entry names are preserved as metadata; unnamed bank/edit-buffer dumps receive model/number names. Source refresh preserves patch hashes, favourites and user tags. Original files remain untouched; FXP support does not accept unrelated VST layouts.

ROM-free fixtures verify all 400 slot identities, names and sound bytes and reject truncated/foreign/version-mismatched/out-of-range banks. `SILVER_TEST_VSTATION` supplies a private ZIP for byte comparisons. With private K/KS firmware arguments, `SILVER_TEST_VSTATION_ONLY=1` also verifies 400 browser rows, named single-click loading, K firmware field preservation, KS name transfer and audible finite audio for all 16 demo sounds with zero C++ callback allocations.

In AU hosts such as Logic, the preparation maximum can exceed actual live slices. **Low latency** follows observed slice sizes; other profiles keep the preparation maximum as a reserve. Latency updates run on the message thread even with the editor closed. A sudden increase in slice size or CPU overload can still underrun a small reserve. The I/O buffer and Logic Process Buffer Range remain separate host settings.

The bank selector lists collections (populated folders, multi-program files and factory banks); individual preset files discovered inside folders appear in the table. Rescan keeps the selected collection. V-Station playback in the K engine uses 6 dB of firmware program-level reserve; source files and library data remain unchanged. KS and native K/A playback retain their level. Select the V-Station preset again to apply this reserve to an older saved project.


## Persistent settings and preset source management

Common preferences live in the user's Silver-K settings directory on both desktop platforms. The last firmware, selected KS Flash/factory Data and Wave ROM are stored as external paths, with relative paths for assets inside the main folder. A fresh instance boots the remembered firmware in the background even when its editor is closed. A DAW state restore cancels or supersedes that default boot and restores the project's own assets and native state. ROM bytes are not embedded in the settings file. A missing project ROM leaves the instrument unloaded with an error; it does not play the global default engine instead.

UI scale, skin, buffer profile, editor MIDI-channel preference, browser filters/source, column visibility/order/widths and sort order persist between instances. Changes are merged by key and the JSON settings file is replaced atomically, so an unchanged older instance does not overwrite another instance's preferences when it closes. Project restoration does not write its UI/sound settings back as new-instance defaults.

The source menu distinguishes folders, banks and individually connected presets. Individual FXP files discovered inside folders stay in the table. The No. column preserves bank order; normal text-column sorting remains available. Show all bank slots includes unnamed and default User-number V-Station bank slots. Those slots are retained in the database; the option filters their display and never discards their sound data. User-number names on individual FXP files are treated as ordinary preset names.

Rescan rebuilds all connected file/folder trees and detects added, modified and deleted files. It also works on manually connected sources nested inside another connected folder, without duplicating their programs. Re-adding an existing folder triggers a refresh. Remove source excludes its entire subtree from scanning and persists that exclusion across restarts. Re-adding a path reconnects it. Exclusions and the current managed folder are stored alongside the library database in silver-sources.json. Favourites and tags are retained for unchanged patch identities. The KS Factory local collection is replaced when its Flash data changes.

The V-Station importer supports the known separate sound/name-table chunk layout with 1–400 slots, as well as standard FxBk lists containing supported FPCh presets. It validates the complete container before appending patches. NUL-terminated names can contain arbitrary padding after the terminator, and legacy extended-byte names are decoded. ZIP archives can contain banks or individual presets; macOS resource-fork entries are ignored. Unknown VST layouts remain rejected and are reported in the browser.

silver_persistence_and_rescan is a ROM-free regression test for these behaviours, including Unicode paths, nested manual sources, add/change/delete/rescan, exclusions and restart, favourites, browser columns/sorting/search and settings isolation between instances/projects. Optional K and KS firmware arguments also check automatic boot without an editor and host-state precedence. The test is included in Windows CI.

## KS banks and Drum Maps

With KS firmware, Patches has a separate memory-bank selector: banks 1–4 contain Programs, banks 5–8 contain Drum Maps, and Performances have their own bank. The Bank column and No. column show original KS addresses, such as 207 or 500–548. The source selector still chooses the folder/file/collection; the memory-bank selector filters within that source. Bank selection is remembered.

A complete drum bank in KS Factory, SYX or MIDI is represented by one Drum Map entry containing all 49 sounds and the shared effects record. Search also matches the names of sounds inside a map. Export .syx on this entry exports the complete map with its original sound data. Incomplete banks remain labelled Drum sound or Drum Map FX; missing sounds/effects are never filled in automatically.

Selecting a map activates the keyboard layout on MIDI notes 36–84. Its bank is loaded into the current instance's guest Flash when necessary; original files on disk are unchanged. Parts referencing the same drum bank share its contents. In Performance, selection changes the selected part's bank assignment, including when MIDI channels are shared. Drum-bank identity is retained in DAW state and on returning from Performance to Program mode. Selecting an ordinary Program exits the drum layout.

K-Station hides the KS memory-bank selector, Bank column, drum entries and sources containing only drum entries. Ordinary KS Programs remain available through the existing conversion path. A separate per-key drum editor is not included in this browser change. Save current still saves the current edit buffer; use Export .syx on a Drum Map library entry to export its complete stored bank.

Validation on macOS arm64 (2026-10-15): ROM-free parser/browser regressions and original KS OS 2.1.02 playback using synthetic drum Programs. The playback check covers distinct sounds on adjacent keys, DAW state restore, shared-channel part assignment and return to a normal Program. This is not hardware sound-equivalence validation; Windows runtime validation is pending.
