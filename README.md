# Silver-K 0.2.4 Beta

A beta release for continued development. CPU performance, latency and optimization are still being refined.

Optional **Setup → Performance diagnostics** records K/KS performance sessions and exports JSON on Windows and macOS. See the [measurement guide](doc/PERFORMANCE_DIAGNOSTICS.md) and shared [optimization journal](doc/OPTIMIZATION_LOG.md) for reproduction conditions, validation status and the next investigation.

Production KS OS 2.1.02 playback now uses C++ from reset. The KS H8 interpreter is linked only into comparison/test executables; normal plugin builds have no KS H8 switch or fallback. Existing project/controller choices resolve to C++ in production. K-Station's separate controller is unchanged. The [KS migration status](doc/KS_NATIVE_CONTROLLER.md) describes the remaining compatibility register/flag/stack model: its complete semantic removal is **not finished**. DSP firmware, sound clocks and JIT settings are unchanged.

Live KS DSP address requests, word writes and read replies now use native value/timing tasks in normal C++ playback. The old `SILVER_KS_DSP_TRANSPORT=events` setting remains accepted for diagnostic compatibility but no longer selects the retired register-driven HDI batches; those are test-only. Caller/IRQ projections, the fault handler and other control logic still retain the compatibility model. Exact command/audio checks are recorded in the journal; full semantic removal and separate Windows validation remain pending. CPU comparison is deferred until full removal.

The next opt-in control jobs use `SILVER_KS_CONTROL_JOBS=1` with the C++ controller. The [KS migration status](doc/KS_NATIVE_CONTROLLER.md) separates the verified stages from the remaining bootstrap and compatibility-state removal. Fresh C++ KS instances now also reset and warm up without H8 interpreter calls. The compatibility register/PC/stack model remains; a complete controller-emulation removal is not yet delivered.

[Download 0.2.4 Beta](https://github.com/temaniak/Silver-K/releases/tag/v0.2.4). Installation and first-run instructions are provided below in English. See the [detailed installation guide](doc/INSTALL.md) and [release notes](doc/releases/0.2.4.md).

This beta includes the patch-manager, V-Station import, host-clock, audio scheduling and data-folder fixes since 0.2 Alpha.

Silver-K combines **K-Station** and **KS** firmware emulation in one desktop synthesizer. The selected firmware determines the engine, available controls, parts and automatic skin. The build supports **Windows x64 Standalone/VST3** and **macOS Standalone/VST3/AU**. Windows builds, DSP JIT tests and ROM-free checks run in GitHub Actions; macOS arm64 is also verified locally.

- K-Station: one part, stereo output; tabs 2–4 are dimmed.
- KS: four parts and four output channels; routing and MIDI settings on **Perf**.
- Shared patch browser: single-selection loading, sortable columns, recursive folders, tags, favourites, source model and compatibility reports.
- A-Station and X-Station programs and banks import through SYX/MIDI/ZIP. A/K bank pairs are expanded into individual sounds. V-Station FXB/FXP import works directly, through folders and ZIP; V-Station is a distinct source model.
- Host tempo and transport drive the original firmware MIDI clock; realtime clock bytes take priority over queued parameter edits.
- K-Station ↔ KS patch conversion. KS-only oscillator waves become sine on K-Station, filters become LP 12, and unsupported features use target defaults.
- Automatic KS grey lower panel, built-in skins and custom JSON skins.

A-Station uses the compatible K-Station sound format; X-Station uses the compatible KS sound format. Loading into the other engine applies the existing conversion rules. KS external audio input is not connected.

## Release packages

| Platform | Included formats |
| --- | --- |
| [Windows x64](https://github.com/temaniak/Silver-K/releases/download/v0.2.4/Silver-K-0.2.4-Windows-x64.zip) | Standalone, VST3 |
| [macOS Apple Silicon / arm64](https://github.com/temaniak/Silver-K/releases/download/v0.2.4/Silver-K-0.2.4-macOS-arm64.zip) | Standalone, VST3, AU |

The Mac package is an arm64 build. Use an Apple Silicon Mac and a native arm64 DAW; Intel and Universal builds are not included in this release.

Each archive includes our test KS Data, a zero-filled Wave ROM substitute, installation instructions, licenses, source links and SHA-256 checksums. Supported original firmware must be supplied separately.

## Installation

### macOS

1. Extract the ZIP and close your DAW before replacing an existing plugin.
2. Copy `Standalone/Silver-K.app` to `/Applications` or your preferred applications folder.
3. Copy the entire `Plugins/VST3/Silver-K.vst3` bundle to `~/Library/Audio/Plug-Ins/VST3/`.
4. Copy the entire `Plugins/AU/Silver-K.component` bundle to `~/Library/Audio/Plug-Ins/Components/`.
5. Open these locations in Finder using **Go → Go to Folder…** (`Command+Shift+G`). Create the format folder if necessary.
6. Restart your DAW and rescan plugins. Silver-K is an instrument; load it on an instrument track. In Logic, use Plugin Manager to rescan the AU if needed.

Keep each `.app`, `.vst3` or `.component` bundle intact. When updating, replace the old bundle as a whole rather than copying its internal executable.

These Mac builds are ad hoc signed and are not Apple-notarized or Developer ID signed. If macOS blocks the application, follow its per-application **Privacy & Security → Open Anyway** process after checking the download source. [Apple's instructions](https://support.apple.com/102445).

### Windows

1. Extract the ZIP to a permanent folder and close your DAW before updating.
2. Run `Standalone/Silver-K.exe` from the extracted folder.
3. Copy the entire `Plugins\VST3\Silver-K.vst3` folder to `C:\Program Files\Common Files\VST3\`. Windows may request administrator permission for this copy.
4. Restart your 64-bit DAW and rescan VST3 plugins. Load Silver-K on an instrument track.
5. If Windows reports a missing `VCRUNTIME140`, `MSVCP140` or related runtime library, install the official [Microsoft Visual C++ v14 Redistributable x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

Keep the `.vst3` folder intact; do not extract its internal binary. AU is available only on macOS. VST2 is not included.

## Firmware and data locations

Silver-K uses one permanent main folder. By default it creates `Documents/Silver-K`:

```text
Documents/Silver-K/
├── ROMs/       Your firmware, KS Data and Wave ROM
└── Patches/    Your factory and user patch banks
```

Choose this main folder through **Setup → Silver-K folder**. Silver-K creates `ROMs` and `Patches`, supplies our synthetic `ks-init-flash.bin` and zero-filled `ks-wave-zero.bin` if absent, scans firmware in `ROMs` and automatically adds `Patches` to the browser. Existing files are preserved. Original firmware, factory banks and samples must be supplied separately. Old release archives may contain a `KS-Data` folder; copy those two synthetic files into `ROMs`.

Supported firmware versions are **K-Station OS 2.1.01** and **KS OS 2.1.02**. In **Setup → Silver-K folder**, select the main folder, then choose a firmware from the top selector. Alternatively, use **Load firmware** to select a supported file. OS/SYX updates, decoded BIN files and ZIP archives containing supported firmware are accepted. The loader checks the contents, not just the filename.

Firmware and Wave ROM contents are not embedded in DAW project state. Keep the selected files available and select them again if you move them. Official KS OS and factory-sound downloads are available from [Novation](https://downloads.novationmusic.com/novation/synthesisers-legacy/ks-rack).

## KS Data and Wave ROM

These two files have different purposes:

| File | Size | Purpose | Setup control |
| --- | --- | --- | --- |
| `ks-init-flash.bin` | 524288 bytes | Our test KS Data: simple `Silver Init` programs, four-part `Silver Multi` performances and startup settings | **KS factory data** |
| `ks-wave-zero.bin` | 129024 bytes | A zero-filled substitute for the missing sampled-wave bank | **Wave ROM** |

Neither file contains original OS code, DSP firmware, Novation factory patches or sampled waves. They are not firmware updates and cannot be loaded through **Load firmware**.

### First KS startup with the supplied test files

1. Select `ks-init-flash.bin` through **Setup → KS factory data**.
2. Select `ks-wave-zero.bin` through **Setup → Wave ROM**.
3. Select your own KS OS 2.1.02 through **Silver-K folder / Load firmware** and wait for `Loading firmware...` to finish.
4. In Standalone, select an audio output and MIDI input. In a DAW, enable monitoring on the instrument track and send MIDI to Silver-K.
5. Play the initial `Silver Init` saw program using the selected MIDI channel. For four parts, enable **Perf → Performance (4 parts)** and check each part's active state, channel, keyboard range and output routing.

The zero Wave ROM allows testing generated oscillator waves. Sounds that depend on sampled waves or drums may be silent or incomplete; the substitute does not recreate those original sounds. Use your own compatible 129024-byte Wave ROM through **Setup → Wave ROM** when available.

**Program boost (dB)** is a separate KS gain control ranging from 0 to 30 dB. At 0 dB, the program uses its normal unboosted level. Use **Part volume** on Perf for the part's 0–127 level. High program boost can cause distortion.

Editor changes and automation address individual KS parts, including tuning and volume, even when their MIDI channels are shared. External MIDI notes and controllers still address all matching parts so layered playing is preserved.

### Original KS factory Data

You can replace the test Init Flash with your own compatible full KS Data:

- Use **Setup → KS factory data** to select a `.mid` / `.midi` file containing KS Program/Performance SysEx, or a compatible full 524288-byte user Flash `.bin`.
- A normal MIDI song, a firmware update, a Wave ROM or a single program dump is not a full KS user Flash.
- Selecting new Data while KS is running reloads the engine and interrupts playback. Change these files between playback sessions.
- Factory Data and Wave ROM are independent: loading factory programs does not provide the samples they may need.

Loaded Data is represented by the `KS Factory` browser source. With the test Init Flash, that source contains our own initial programs, not original factory sounds. If changing Data leaves an old browser source visible, add the desired bank separately through **Patches → Add file / Add folder**.

## Factory presets and patch banks

1. Store your banks in the main folder’s `Patches` subfolder. The browser accepts supported `.syx`, `.mid`, `.midi`, V-Station `.fxb` and `.fxp` files and ZIP archives containing them.
2. Open **Patches → Add folder** to scan a folder and its subfolders, or **Add file** to add an individual bank. Use **Rescan** if necessary and check the source/model/tag filters.
3. Click a patch row to load it immediately, or select it with the arrow keys. Re-click the selected row or press Enter to reload it, including into another selected part. Click a column header to sort; click again to reverse the direction. Sorting and filtering preserve selection without loading another sound.
4. In KS Performance mode, select the destination part using tabs 1–4 before loading a Program. A Performance contains the full four-part setup. Some preset-loading paths reload the Performance and interrupt sound.
5. Use **Save current** for the current sound, **Export .syx** for a selected library entry, and **Save tags** after editing tags or favourites.

K-Station uses its own firmware and does not require KS Init Flash or KS Wave ROM. Add its factory patches through Patches in the same way. Cross-model conversion can replace unsupported functions; review the compatibility report when loading a converted patch.

## Beta limitations

- Keep original banks and allow queued edits to reach the engine before saving the current sound or DAW state.
- **Performance and optimization are unfinished.** CPU load, loading/warm-up and multitimbral operation at small audio buffers need further work.
- **Latency needs further improvement.** Setup's Plugin latency reports the internal delay; audio-driver, physical MIDI and DAW-chain delays are additional. Increasing the audio buffer can reduce dropouts but increases overall latency.
- KS external audio input is not implemented. Sound accuracy against physical instruments has not been established. X-Station-to-K conversion replaces unsupported oscillator/filter and modulation features; review the adaptation report.

This beta remains under development; test it with your projects before relying on it in a session.

## Source-only repository

This repository contains application and required framework source, skin assets, documentation, and **one synthetic user flash**: [`assets/ks-init-flash.bin`](assets/README.md). That file is generated by us from init settings; it contains no OS/DSP firmware, sampled waves or factory patches.

**No builds, plugin bundles, test WAVs, ROMs or factory banks are committed.** Firmware and optional wave ROMs must be supplied locally by the user. Do not place them in Git. The `.gitignore` and CI audit reject artifacts; the init flash is the only allowed `.bin`.

Release archives include our synthetic KS Init Flash (test KS Data) and a separately generated 129024-byte zero Wave ROM. Neither contains original firmware, factory sounds or sampled waves. The user's supported OS is still required; sample-dependent sounds remain silent/incomplete with the zero bank.

## Build

Requirements for the verified platform: macOS arm64, Xcode command-line tools, CMake 3.22+, Python 3, and internet access for the first configuration. Dependencies are fetched from pinned source revisions and compiled locally; prebuilt libraries are not required.

```sh
git clone https://github.com/temaniak/Silver-K.git
cd Silver-K
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target silverK_Standalone silverK_VST3 silverK_AU silver_check silverClock_test silverWorkflow_test -j 6
SILVER_K_DATA=/tmp/silver-k-tests ctest --test-dir build --output-on-failure -R '^silver_'
python3 tools/check_repository.py
```

Products are under `build/source/novation/silverK/buildStandalone/silverPlugin/silverK_artefacts/Release/`. macOS bundles receive an ad hoc signature for local testing. Signing for distribution is a separate step. Deployment target 12.0 is not a claim that all older macOS releases have been tested.

### Windows x64

Install Visual Studio 2022 or later with **Desktop development with C++**, a Windows SDK, CMake 3.22+, and Python 3. In PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target silverK_Standalone silverK_VST3 silver_check silverClock_test silverWorkflow_test --parallel 4
$env:SILVER_K_DATA = "$env:TEMP/silver-k-tests"
ctest --test-dir build -C Release --output-on-failure -R '^silver_'
python tools/check_repository.py
```

Standalone provides JUCE's Windows audio backends (WASAPI/DirectSound); an optional locally supplied ASIO SDK can be enabled with `-DSilverK_ASIO_SDK_PATH=C:/path/to/asiosdk`. The SDK is not redistributed. The core and firmware/patch logic are shared with macOS; AU and codesigning are conditional on macOS.

For offline builds, pass `-DJUCE_SOURCE=/path/to/JUCE -DDSP_SOURCE=/path/to/dsp56300 -DASMJIT_SOURCE=/path/to/asmjit` using the exact revisions in [`cmake/Dependencies.cmake`](cmake/Dependencies.cmake). Those must be **source checkouts**, not binary releases. The macOS JUCE compatibility patch applies to a build-local copy only.

## Run

In **Setup**, select your main Silver-K folder and firmware. For KS, select the supplied `assets/ks-init-flash.bin` in the KS data field. Optional external wave banks remain local; without them sample-dependent oscillators are silenced through original MIDI commands. Add your own patch folders in **Patches**. Use **Perf** for KS multi mode and output routing.

Details and current limitations: [user guide](doc/silver_k.md). Provenance and licenses: [THIRD_PARTY.md](THIRD_PARTY.md).

**Setup → Rescan ROMs** updates the saved firmware catalog after files are added or removed. Subsequent plugin launches remember the main folder and restore this catalog without rescanning ROMs. Loading a selected firmware still validates its contents. **Audio buffer → Balanced** is the default; Low latency reduces the reserve for light projects.

V-Station is a separate source synth type in the Model column and filter. Legacy `NvS0`/`FBCh` banks with 400 programs and `NvS0`/`FPCh` individual presets are supported, directly or in ZIP archives. The 128-byte sound blocks and original chunk names are retained; the K engine loads the shared sound format directly. KS uses its K-compatible importer and 16-character name field. Unknown FXB/FXP layouts and unrelated VST presets are rejected. This source type identifies presets; playback uses the existing K/KS firmware engines. SYX export carries the K sound data, while names and V-Station identity remain in library metadata. K/A SysEx filenames are retained, including inside ZIP; unnamed bank slots receive sequential model/number names. Existing imported sources are refreshed once after this importer update, preserving favourites and user tags.

In AU hosts such as Logic, the preparation maximum can exceed actual live slices. **Low latency** follows observed slice sizes; other profiles keep the preparation maximum as a reserve. Latency updates run on the message thread even with the editor closed. A sudden increase in slice size or CPU overload can still underrun a small reserve. The I/O buffer and Logic Process Buffer Range remain separate host settings.

The bank selector lists collections (populated folders, multi-program files and factory banks); individual preset files discovered inside folders appear in the table. Rescan keeps the selected collection. V-Station playback in the K engine uses 6 dB of firmware program-level reserve; source files and library data remain unchanged. KS and native K/A playback retain their level. Select the V-Station preset again to apply this reserve to an older saved project.


New instances remember and automatically load the last successfully selected firmware, KS Flash/factory Data and Wave ROM. Scale, skin, editor MIDI-channel preference, buffer profile, browser source/filter choices, column layout and sorting are saved in the per-user settings folder on macOS and Windows. DAW projects restore their own firmware and sound state; opening a project does not replace the defaults for new instances. Missing selected assets are reported instead of silently choosing another firmware or an external Wave ROM substitute.

The browser groups connected folders, banks and individually added presets in its source menu. FXP files inside a folder stay in the table. V-Station bank programs retain their original slot order through the **No.** column. **Show all bank slots** reveals unnamed and default `User NNN` bank slots; their original sound data is always retained, and individually imported FXP presets are not hidden by this option. V-Station import accepts legacy `NvS0/FBCh` chunk banks with 1–400 programs and `NvS0/FxBk` lists of supported `FPCh` programs. Name padding after a NUL terminator is ignored; legacy accented names are decoded without changing the sound bytes. Unsupported layouts produce a browser message.

**Rescan** refreshes every connected file/folder source, including nested sources, changed banks, new files and removed files. Re-adding a connected folder also refreshes it. **Remove source** disconnects the selected source and its subtree without deleting the original files. Removed paths are remembered and excluded from subsequent scans, including the main `Patches` folder; explicitly adding a path reconnects it. Unchanged sounds retain their favourites and user tags. Changing KS Flash/factory Data updates the `KS Factory` collection.
