# K-Station controller research (macOS)

Standalone diagnostic tools for the original K-Station H8/DSP engine. These do
not add a replacement controller and are not part of the product build.

`kstation_research_clean` compiles the existing `kstationH8.cpp`,
`kstationHardware.cpp` and `kstationDSP.cpp` without editing them. The observer
executable uses a generated H8 source copy with PC/HDI counters. Both use the
same original DSP/JIT, 16 MHz H8, 112 MHz DSP, 128-clock controller slices and
native sample rate. Neither loads a plugin or changes system scheduling.

Supply decoded firmware accepted by the production RomLoader. The Python
runners validate its SHA-256. Keep firmware, snapshots, PCM, reports and maps
in an ignored private directory; these tools never download or publish inputs.

## Build

Use a fresh scratch root with an `evidence` subdirectory. Before configuring,
generate `<scratch>/build/observe/kstationH8.cpp`:

```sh
python3 prepare_observer.py <checkout>/source/novation/kstation/kstationLib/kstationH8.cpp <scratch>/build/observe/kstationH8.cpp
cmake -S <checkout>/tools/kstationResearch -B <scratch>/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DSILVER_SOURCE=<checkout> -DDSP_SOURCE=<pinned-dsp-source> -DASMJIT_SOURCE=<pinned-asmjit-source>
cmake --build <scratch>/build --target kstation_research_clean kstation_research_observe -j 6
```

Dependency revisions are in the checkout's `cmake/Dependencies.cmake`. The
research CMake builds the sources locally and does not fetch dependencies.

## Run and interpret

```sh
python3 run_correctness.py <scratch> <decoded-firmware>
python3 run_measurements.py <scratch> <decoded-firmware> detailed
python3 run_measurements.py <scratch> <decoded-firmware> stacks
python3 summarize_samples.py <scratch>/evidence/stacks
python3 run_measurements.py <scratch> <decoded-firmware> cpu
python3 run_measurements.py <scratch> <decoded-firmware> offline
```

Run these sequentially. Do not overlap clean CPU windows with builds, other DSP
work or profiling. Output directories must be absent; use `--run-id` on the
measurement runner for a separate series. The scenarios are idle, one/eight
held keys, eight note pairs per second, 50 Hz cutoff changes, and internal arp.
Setup uses the firmware's Init program and MIDI control messages, then checks
the effective packed parameters. No guest memory is patched by the harness.

Correctness compares raw stereo PCM, the full 16 MiB H8 memory and 256 KiB
Flash between stock and observed executables. Detailed diagnostics also must
match audio/state and guest counters. The PC observer records the PC before
`step()`: IRQ admission can charge the first IRQ instruction to the interrupted
PC. These histograms show observed routes and guest instruction frequency, not
host CPU cost or exact function-call counts. HDI aggregates are not full tapes.

`detailed` uses the existing every-256th-advance wall timers. Their ratio is a
sampled H8/(H8+DSP) wall-time estimate, including timer/peripheral/HDI work in H8.
It is not exact thread CPU accounting. Deterministic sampling can alias guest
phases. `stacks` samples a separate offline process using macOS `sample` and
must be interpreted independently; unknown JIT frames remain explicit.

`cpu` uses one ordinary-priority paced thread at the native sample rate and
256-frame batches; `offline` runs the same fixed work without pacing. Both
report process user+system CPU excluding boot, setup, 3 s warm-up and file IO.
100% means one CPU. Hashing and this small host's loop overhead are included.
There is no resampler, plugin worker, physical driver or nonblocking audio
callback, so zero native ring drops does not establish zero host underruns.
Block compute overruns must be retained and preclude a realtime PASS. Offline
cost and paced cost must not be pooled.

The optional `--include-program` correctness case is diagnostic only: reloading
the stored Init clears held notes and restores envelope values. It can fail
the sounding/dry admission equally in all modes; preserve that failure and
exclude it from CPU conclusions. It is not included in the default six cases.
