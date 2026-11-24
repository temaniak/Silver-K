# Build

macOS requirements: Xcode command-line tools with both arm64 and x86_64 SDK support, CMake 3.22+, Python 3, and internet access for the first configuration. Dependencies are fetched from pinned source revisions and compiled locally; prebuilt libraries are not required.

```sh
git clone https://github.com/temaniak/Silver-K.git
cd Silver-K
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target silverK_Standalone silverK_VST3 silverK_AU silver_check silverClock_test silverResamplerRealtime_test silverWorkflow_test -j 6
SILVER_K_DATA=/tmp/silver-k-tests ctest --test-dir build --output-on-failure -R '^silver_'
python3 tools/check_repository.py
```

Products are under `build/source/novation/silverK/buildStandalone/silverPlugin/silverK_artefacts/Release/`. New macOS builds default to Universal 2 (arm64 and x86_64) and deployment target 12.0. The standalone CMake entry points use the same defaults before compiler detection, including when an old cache contains an empty deployment target. Pass `-DCMAKE_OSX_ARCHITECTURES=arm64` or `x86_64` for a single architecture.

An optional filter/arpeggiator diagnostic uses private inputs with the workflow test:

```sh
SILVER_K_DATA=/tmp/silver-filter-clock SILVER_TEST_FILTER_CLOCK_ONLY=1 build/source/novation/silverK/buildStandalone/silverPlugin/silverWorkflow_test /path/to/k-firmware.bin /path/to/ks-firmware.syx /path/to/ks-data.mid /path/to/wave-rom.bin
```

It measures audio attack phase at 100 BPM through the processor's parameter and audio-block paths, for both engines, Internal/Host Clock, MIDI notes 36/60/84, fixed cutoff 64/127 and a live sweep. The isolated data folder retains local WAV captures. This is an offline diagnostic on a dry test program; it does not establish timing for every preset or realtime host.

Set `SILVER_TEST_FILTER_PATCH` to a private single-program SysEx file to run the long preset probe instead. It preserves the program's envelopes, wave phase and arp division, tests held-note and arp playback, and captures 17 bars at 100 BPM. Each mode checks cutoff 64/127 with keytrack 0, then cutoff 127 with keytrack 127, then a 127→0→127 sweep during bars 9–16. WAVs contain the sound on Left and sound plus a quarter-note click on Right; XML and before/after program dumps retain the scenario settings. The click is relative to Note On, with host PPQ reset to zero at capture start. Use MIDI note numbers rather than DAW octave names.

For a Windows tester build, after building `silverWorkflow_test`:

```powershell
$env:SILVER_K_DATA = Join-Path $env:TEMP 'silver-filter-clock'
$env:SILVER_TEST_FILTER_CLOCK_ONLY = '1'
$env:SILVER_TEST_FILTER_PATCH = 'C:\private\program.syx'
$env:SILVER_TEST_FILTER_NOTE = '24'
$env:SILVER_TEST_FILTER_SOURCE = 'internal' # or host
build\source\novation\silverK\buildStandalone\silverPlugin\Release\silverWorkflow_test.exe C:\private\k-firmware.bin C:\private\ks-firmware.syx C:\private\ks-data.mid C:\private\wave-rom.bin
```

`SILVER_TEST_SKIP_K=1` or `SILVER_TEST_SKIP_KS=1` limits the engine. The filter diagnostic selects the current production KS controller **before** firmware load. Set `SILVER_TEST_FILTER_KS_CONTROLLER=hybrid` or `h8` for the test-only comparison backends. `SILVER_TEST_FILTER_CASE=0..7` selects one case (0–3 held, 4–7 arp, in the order above). `SILVER_TEST_FILTER_CC_FLOOD=1` repeats cutoff CC105 every host block, including identical values. The long probe is offline by default. `SILVER_TEST_FILTER_REALTIME=1` paces callbacks outside audio processing and records missed frames with block offsets and callback deadline lateness. PASS means a finite, audible capture with no missing frames or C++ callback allocations. All captures and program dumps remain private.

Set `SILVER_TEST_ARP_STRESS=1` with `SILVER_TEST_FILTER_CLOCK_ONLY=1` to run a generated dry 1/16 arp at 100 BPM. It measures a quiet reference, repeated and changing CC105, Cutoff automation, and simultaneous controller or parameter edits. `SILVER_TEST_FILTER_SOURCE=internal|host|midi` selects the clock. `SILVER_TEST_ARP_BLOCK=64|128|256|512` selects the host block at 48 kHz. To run a single workload plus the quiet reference, use `SILVER_TEST_ARP_TRAFFIC` with `repeat`, `cutoff-cc`, `cutoff-parameter`, `mixed-cc`, `mixed-parameter`, or `keytrack-parameter`.

Every `ARP_STRESS` line reports the active controller, audio attack positions in samples, missing frames, MIDI overflows, and C++ callback allocations. Check `grid` and the actual intervals separately from `valid`, which describes capture health. `final_values` reports whether the final automation values reached the saved engine snapshot within 100 blocks. The first Host or MIDI case can include clock acquisition. Offline and paced runs exercise the same processor and worker used by the plugin. A Windows DAW with the user's driver is a separate validation.

For a K-Station A/B, launch a fresh process with `SILVER_EXPERIMENT_ARP_CONTROL_GUARD=1`. The experiment keeps the latest pending Cutoff, Resonance, Filter Key Tracking and LFO 1 Rate plugin values. It spaces changes to the same parameter at least 40 ms apart and separates the writes for different controls by at least 5 ms, rounded to host blocks. An ordinary MIDI message or UI packet lets the pending values go through in their regular block order. Raw CC and transport use the timed MIDI path. Fast automation can sound coarser because intermediate values are coalesced and a pending edit may wait for its next eligible block.

`SILVER_TEST_ARP_CONTROL_BLOCKS=N` changes only the test sender's update cadence. Use it to measure raw MIDI at a lower input rate. With `N>=4`, `SILVER_TEST_ARP_STAGGER=1` sends the simultaneous controls in successive blocks of each cadence period.

An optional macOS VST3 SDK host checks full-speed Prefetch (mode 1) against Offline (mode 2). Supply a private raw Silver-K processor state selecting an audible generated oscillator program and its locally accessible ROMs:

```sh
cmake --build build --target silver_vst3_prefetch_check -j 6
build/source/novation/silverK/buildStandalone/silverPlugin/silver_vst3_prefetch_check /path/to/Silver-K.vst3 /path/to/processor.state 1 /tmp/silver-prefetch.f32
build/source/novation/silverK/buildStandalone/silverPlugin/silver_vst3_prefetch_check /path/to/Silver-K.vst3 /path/to/processor.state 2 /tmp/silver-offline.f32
cmp /tmp/silver-prefetch.f32 /tmp/silver-offline.f32
```

Each run requires finite, audible stereo float PCM and zero missed frames. On macOS, Prefetch uses the existing full-speed render path with unchanged DSP quality; the realtime mode retains its nonblocking queue consumer. These full-speed checks do not measure realtime callback performance. Validate that separately in a DAW; retain private state and PCM locally.

macOS bundles receive an ad hoc signature for local testing. Signing for distribution is a separate step. Deployment target 12.0 is not a claim that all older macOS releases have been tested. Validate Apple Silicon natively, x86_64 under Rosetta, and physical Intel Macs separately.

`tools/package_release.py --platform macOS-universal` checks both architecture slices and rejects binaries or Info.plist entries requiring a version above macOS 12.0, including Wave ROM Builder. The individual check is:

```sh
python3 tools/check_macos_compatibility.py build/source/novation/silverK/buildStandalone/silverPlugin/silverK_artefacts/Release/Standalone/Silver-K.app
```

## Windows x64

Install Visual Studio 2022 or later with **Desktop development with C++**, a Windows SDK, CMake 3.22+, and Python 3. In PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target silverK_Standalone silverK_VST3 silver_check silverClock_test silverResamplerRealtime_test silverWorkflow_test --parallel 4
$env:SILVER_K_DATA = "$env:TEMP/silver-k-tests"
ctest --test-dir build -C Release --output-on-failure -R '^silver_'
python tools/check_repository.py
```

Standalone provides JUCE's Windows audio backends (WASAPI/DirectSound); an optional locally supplied ASIO SDK can be enabled with `-DSilverK_ASIO_SDK_PATH=C:/path/to/asiosdk`. The SDK is not redistributed. The core and firmware/patch logic are shared with macOS; AU and codesigning are conditional on macOS.

### SoulTrain native editor on Windows

Clone the private repository from an account with access. The first checkout
creates a separate directory for SoulTrain.

~~~powershell
git clone --single-branch --branch codex/drum-seq https://github.com/temaniak/Silver-K-SoulTrain.git Silver-K-SoulTrain
Set-Location Silver-K-SoulTrain
~~~

Build a clean checkout of the requested commit. For local tip builds, pull
updates in the dedicated checkout with `git pull --ff-only`. Record the full
commit before configuring.

~~~powershell
git status --short
git rev-parse HEAD
cmake -S . -B build -A x64 -DSILVER_SOULTRAIN_JUCE=ON -DSILVER_SOULTRAIN_PREVIEW=OFF
cmake --build build --config Release --target silverK_VST3 silverK_Standalone --parallel 4
~~~

The build defaults to the JUCE editor on Windows and macOS. Its checked-in
artwork and fonts are copied during the build. No Node.js or WebView2 runtime
is required for this editor.

Use the same `codex/drum-seq` revision for both platforms. CMake selects
the platform sources and formats. The MSVC compatibility fixes are included
in the shared source. Use `SILVER_SOULTRAIN_JUCE=ON` and
`SILVER_SOULTRAIN_PREVIEW=OFF` for matched native-editor checks.

Copy the whole `VST3/Silver-K.vst3` bundle from
`build/source/novation/silverK/buildStandalone/silverPlugin/silverK_artefacts/Release/`.
The Standalone executable uses the `SoulTrainJuce` directory beside it in
`Standalone/`. Supply your firmware and data through Setup after opening.
Verify the Windows file version, architecture and packaged resources for both
targets. Launch the Standalone with an isolated `SILVER_K_DATA` directory and
verify the exact VST3 bundle loaded by the test host.

Run `silverWorkflow_test` with `SILVER_TEST_BROWSER_ONLY=1` to verify that an
existing Patches index discovers User without Rescan. Use a fresh
`SILVER_K_DATA` for each test mode. For the KS Performance clock regression,
set `SILVER_TEST_PERFORMANCE_CLOCK_ONLY=1` and pass the local KS firmware,
KS Data MIDI and Wave ROM paths in that order. The probe renders SEQ on Main
and a melodic arp on Aux, with both set to sixteenths and Key Sync off.
It checks starts, a transport jump while holding a note, and Note On at the
downbeat on shared and separate MIDI channels. It then switches between Host,
Internal and MIDI Clock while holding notes on both parts. The clock lines
report the expected and audible timing at 48 kHz. This check is offline.

To build with the browser renderer as a fallback, run
`npm --prefix design-soultrain ci` and
`npm --prefix design-soultrain run build:native` with Node.js 22.12 or newer.
Configure with `-DSILVER_SOULTRAIN_PREVIEW=ON` alongside JUCE. The context
menu in the JUCE editor switches to WebKit on macOS or WebView2 on Windows.
Windows uses the [WebView2 Evergreen Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/#download-section)
for this mode. A local WebView2 SDK can be selected with `-DSILVER_WEBVIEW2_SDK_PATH`.

For offline builds, pass `-DJUCE_SOURCE=/path/to/JUCE -DDSP_SOURCE=/path/to/dsp56300 -DASMJIT_SOURCE=/path/to/asmjit` using the exact revisions in [`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake). Those must be **source checkouts**, not binary releases. The macOS JUCE compatibility patch applies to a build-local copy only.

The ROM-free `silver_resampler_realtime` test streams five minutes of synthetic audio through the K-Station rate pairing and checks that the audio callback does not allocate.


### SoulTrain editor verification

Apply the [Windows SoulTrain editor rules](WINDOWS_UI.md) when changing the
native editor.

On the physical Windows test machine, compare the candidate and the accepted
baseline using separate Release builds and copies of the same test profile.
Use the same VST3 host, firmware, preset, renderer, display scaling, interface
size, sample rate and audio buffer for both runs. Use Software for the baseline
comparison and measure OpenGL separately when evaluating that renderer.

Check cold startup through ROM ready on Main. Load two different sounds from
the Browser and verify the engine-applied name and preset arrows. Enable and
disable KS Performance, toggle a Part in both directions, and load a different
program in it. Exercise rapid button presses, menu opening and dismissal,
and steady knob and fader drags with audio running. Compare initial and
repeated interactions. Record any visible stalls, local input-to-paint
measurements where available, process and UI-thread CPU, and the test
conditions. Remote screenshot round-trip time includes capture and transport.
Keep private firmware, banks and raw captures on the test machine.

With a Release build, compare the closed editor, visible SoulTrain Main at
75%, and the original skin at the same scale. Use a stable patch and audio
buffer. Measure idle, held notes and a repeatable knob sweep separately.
Record the OS, revision, host, audio backend, process CPU and GUI thread CPU.
Specify whether 100% means the whole machine or one logical CPU. For
example, 5% of a 20-logical-CPU machine is one logical CPU. Compare results on
the same computer.

For fader performance, compare each Release build in its own build directory
using the same preset, host, buffer, and editor size. Sweep Amp Attack and Mod
Decay on Main for each firmware, then Part 1 Volume with KS Performance on.
Repeat Cutoff as a rotary reference. Check the LCD readout, the fader position
during DAW automation, and GUI stalls at the same mouse sweep rate. Record
Windows display scaling separately from the plugin's interface size.

Leave Browser open during a switch in `Recall last patch` mode. Select a
different preset on each firmware. After each return, check the LCD, browser
row and tags, then step once in each direction. On KS Extra, move Ring LFO1,
hold the drag for over two seconds, and release. The LCD should keep showing
the value briefly before returning to the preset. Check that long LCD names
still scroll and that menus leave the surrounding panel clear.

### Native controllers and external input

The plugin targets use the native K-Station and KS command controllers.
The semantic and original H8 controllers remain available in comparison tools.
The JUCE SoulTrain editor is the default desktop skin in this branch.

Build `ksAudioInput_test` and `silverExternalInput_test`, then run:

```sh
ctest --test-dir build --output-on-failure -R '^(ks_audio_input|silver_external_input_layout|silver_external_input_standalone_layout)$'
```

Private audio checks accept four external paths (K firmware, KS firmware, KS
Data MIDI/full Flash, Wave ROM). Set `SILVER_EXTERNAL_EXTENDED=1` to include the
filter/envelope and centre-balance vocoder paths. `SILVER_EXTERNAL_RATE` and
`SILVER_EXTERNAL_BLOCK` select host rate/block; `SILVER_EXTERNAL_STANDALONE=1`
checks the Standalone bus configuration. `SILVER_EXTERNAL_STATE_DIR` saves local
host states for `silver_external_host_check vst3|au bundle processor.state`.
The host tool loads the given bundle without installing it. AU uses a private
in-process registration and may need an unrestricted graphical session.

`silverExternalInputReference_test` is an explicit H8 comparison target.
`SILVER_EXTERNAL_KS_H8=1 SILVER_EXTERNAL_MODEL=KS` selects its original KS oracle;
it never enables H8 in the product. `SILVER_EXTERNAL_PCM_DIR` optionally records
private PCM outside the callback for exact comparison. Keep all input files,
processor states and captures outside source control. Finite offline host tests
do not establish physical driver, DAW realtime or Windows audio performance.

The latency/reentry test accepts external KS Data and Wave paths through
`SILVER_TEST_KS_DATA` and `SILVER_TEST_KS_WAVES`.
