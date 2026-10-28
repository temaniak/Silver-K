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

`SILVER_TEST_SKIP_K=1` or `SILVER_TEST_SKIP_KS=1` limits the engine. This filter diagnostic explicitly selects the production KS compatibility controller; the test executable also links the original H8 reference, whose default must not be mistaken for production behavior. `SILVER_TEST_FILTER_CASE=0..7` selects one case (0–3 held, 4–7 arp, in the order above). `SILVER_TEST_FILTER_CC_FLOOD=1` repeats cutoff CC105 every host block, including identical values. The long probe is offline by default; `SILVER_TEST_FILTER_REALTIME=1` paces callbacks outside audio processing and records missed frames with block offsets and callback deadline lateness. PASS means a finite, audible capture with no missing frames or C++ callback allocations; timing still needs analysis. This exercises the processor and worker without a physical audio driver; it does not replace a DAW test. All captures and program dumps remain private.

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

For offline builds, pass `-DJUCE_SOURCE=/path/to/JUCE -DDSP_SOURCE=/path/to/dsp56300 -DASMJIT_SOURCE=/path/to/asmjit` using the exact revisions in [`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake). Those must be **source checkouts**, not binary releases. The macOS JUCE compatibility patch applies to a build-local copy only.

The ROM-free `silver_resampler_realtime` test streams five minutes of synthetic audio through the K-Station rate pairing and checks that the audio callback does not allocate.
