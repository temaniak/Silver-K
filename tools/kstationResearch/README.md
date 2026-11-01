# K-Station controller research (macOS)

Standalone diagnostic tools for the original K-Station H8/DSP engine and the
opt-in first native K stage. These tools are not part of the product build.

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

## First native K stage

The engine now also accepts `H8::Stage1Mode::Timers` or `TimersAndHdi` in its
constructor. `Original` remains the default; no plugin UI or installation is
provided here. Timers replaces the K-only timer peripheral and pending IRQ
production. TimersAndHdi additionally runs five verified HDI status waits as
typed C++ phases after the original bootstrap/handoff. IRQ admission, ISR
firmware bodies and the rest of the controller remain original H8. The bridge
temporarily projects phase results into that caller frame; it is not a full
semantic controller or a decoder for arbitrary H8 instructions.

Freeze the original K library directory before edits (the stage was based on
commit `283bf00`). Use a fresh build and add
`-DKSTATION_STAGE1_REFERENCE=<frozen-k-lib>` to the CMake command above. This
configuration builds four explicit targets: `kstation_stage1_reference`,
`kstation_stage1_candidate` and their `_trace` variants. It does not require the
legacy observer preparation. Build the NativeTimers, HdiWait and Stage1Guard
test targets as well; HdiWait/Guard take the decoded firmware as one argument.

```sh
python3 compare_stage1.py <build> <decoded-firmware> <fresh-private-correctness>
python3 measure_stage1.py <build> <decoded-firmware> <fresh-private-offline> offline
python3 measure_stage1.py <build> <decoded-firmware> <fresh-private-paced> paced
```

Run correctness before CPU, and run every command sequentially. Correctness
includes original/default control, timers-only and combined modes in ten
scenarios. `_trace` targets record actual clocked HDI byte accesses and full
CPU/slice frames, from cold boot through the captured passage, into bounded
preallocated buffers. They must not be used for CPU claims. Final PCM, full
H8 RAM and Flash compare byte-for-byte. `clock` supplies F8 traffic with the
ROM-derived clock selection; stage4 records selector0 (Auto), correcting the
earlier unverified Internal label. `irq` is synthetic timer IRQ injection.
`program` is a deliberately non-sounding state-reload test in this stage, and
is excluded from CPU. This does not erase the earlier sounding-preflight failure.

Clean offline CPU interleaves frozen H8/timers/combined, three times per six
scenarios. Paced CPU interleaves frozen H8/combined three times per held8/sweep.
Snapshots, setup, file IO and three-second warm-up are excluded. Equal audio,
guest counters and state hashes are required. Refuse existing output folders,
retain regressions/overruns, and keep offline cost separate from realtime.

## Native IRQ stage

`TimersHdiAndIrq` adds complete Timer3 overflow (vector 38) and SCI error
(vector 52) services, including admission and return. Other IRQs still use H8.
Admission and the first handler instruction remain one atomic reference step;
each subsequent phase preserves the reference instruction/slice boundary.
This retains the temporary H8 caller frame. Original remains the default.
No audio or timing change is intended.

Freeze the stage1 K library from `733f260` before edits and configure with both
`KSTATION_STAGE1_REFERENCE` (original from `283bf00`) and
`KSTATION_STAGE2_REFERENCE` (stage1 from `733f260`). Build
`kstation_stage1_reference{,_trace}`, `kstation_stage2_reference{,_trace}`,
`kstation_stage2_candidate{,_trace}`, `kstationNativeIrq_test`,
`kstationStage2Guard_test` and the stage1 regression targets. The IRQ unit and
guard executables require the decoded firmware argument. The production CMake
option `KSTATION_BUILD_NATIVE_STAGE2_CHECK` is off by default.

```sh
python3 compare_stage2.py <build> <decoded-firmware> <fresh-private-correctness>
python3 measure_stage2.py <build> <decoded-firmware> <fresh-private-offline> offline
python3 measure_stage2.py <build> <decoded-firmware> <fresh-private-paced> paced
```

Correctness covers twelve scenarios, comparing original H8, frozen stage1,
current stage1 control and the IRQ mode. Ordered HDI and CPU slice tapes compare
original/stage1/IRQ; the untraced control compares raw PCM/RAM/Flash. `irq2`
injects alternating vectors 38/52; `scierror` injects vector 52. These are
synthetic interrupt tests, not physical UART error reproduction. Independent
unit tests compare per-phase state, ordered context-visible IO, nested IRQs,
flags, stack visibility and slice budgets against the original interpreter.

Only after correctness, run clean CPU sequentially without builds/profiling.
Offline interleaves original/stage1/IRQ triplets; paced interleaves adjacent
stage1/IRQ pairs. The incremental comparison is against frozen stage1.
Both handlers are rare in ordinary workloads: migration alone does not imply
a measurable CPU gain. Preserve all costs and compute overruns.

## KS-style K services: serial TX pilot

The third stage adapts KS `SerialIrqTask` / `InterruptService`'s typed phase,
budget and live IO design to K. `SerialTxService` owns only serial data and a
continuation; it has no CPU dependency. K-specific realtime-latch priority,
queue resources, status publications and clock credits are recovered from K.
`NativeSerialTx` is the temporary caller-frame projection while other K paths
remain H8. It owns fixed-capacity continuations and never calls the decoder.
KS mode2 arbitration is not used for K's mode0 interrupts. KS sources stay
unchanged and are not a runtime dependency of the K service.

`H8::Stage1Mode::TimersHdiIrqAndSerialTx` enables the new complete IRQ54 path.
Original remains the default. Freeze stage2 from `98fbd74` before editing and
configure `KSTATION_STAGE3_REFERENCE=<frozen-stage2-k-lib>` plus
`KSTATION_STAGE3_ORIGINAL=<frozen-original-k-lib-from-283bf00>`. Existing
stage1/stage2 reference options can also be supplied for their regression
targets. Build `kstation_stage3_{original,reference,candidate}{,_trace}`,
`kstationSerialTxService_test`, `kstationStage3Guard_test` and the older checks.
The production test option `KSTATION_BUILD_NATIVE_STAGE3_CHECK` defaults off.
The service and guard test executables take the external decoded firmware.

```sh
python3 compare_stage3.py <build> <decoded-firmware> <fresh-private-correctness>
python3 measure_stage3.py <build> <decoded-firmware> <fresh-private-offline> offline
python3 measure_stage3.py <build> <decoded-firmware> <fresh-private-paced> paced
```

Correctness covers the preceding twelve scenarios and three explicit synthetic
transport fixtures. `txirq` injects vector54 with an empty queue; `txclock`
publishes an F8 byte in K's realtime latch and injects54; `txqueue` publishes
one byte/producer index in the K TX ring and injects54. The latter two use
diagnostic guest-state IO, not an ordinary MIDI-to-firmware route. They are
marked separately in CPU results. Musical scenarios never write this state.
The comparison includes raw MIDI output bytes; `_trace` ports tapes now contain
ordered HDI and SCI register byte accesses (addresses distinguish them), with
the same five-word format. Full RAM/Flash/PCM and CPU slice frames remain exact.
Keep trace executables out of CPU measurement. Whole-service unit checks cover
live data mutations, queue wrap, realtime priority, nested IRQs, dynamic bus
costs, short-budget no-ops and zero processing allocations. Unit sanitizers do
not establish whole-DSP sanitizer coverage or physical UART behavior.

## Timer1 and SCI RX with incoming F8

`H8::Stage1Mode::Timer1AndRx` extends stage3 with complete Timer1/encoder/panel/
LCD and SCI RX services, plus the shared internal/external clock graph. The
pure tasks follow KS typed-service/child-budget design; numeric constants,
resources and timings belong to K. Separate adapters still project into the
temporary H8 caller frame. Bootstrap, ADC and the remaining Main/MIDI/voice
services continue to use H8. Original remains the default.

Freeze K stage3 from `21b2bd4`. Add `KSTATION_STAGE4_REFERENCE=<frozen-stage3>`
and `KSTATION_STAGE4_ORIGINAL=<frozen-original-from-283bf00>` to the preceding
research configuration, retaining earlier reference options for regressions.
Build `kstation_stage4_{original,reference,candidate}{,_trace}`,
`kstationClockService_test`, `kstationSerialRxService_test`,
`kstationTimer1Service_test`, `kstationStage4Guard_test` and previous checks.
The production test option `KSTATION_BUILD_NATIVE_STAGE4_CHECK` defaults off.

```sh
python3 compare_stage4.py <build> <decoded-firmware> <fresh-private-correctness>
python3 measure_stage4.py <build> <decoded-firmware> <fresh-private-offline> offline
python3 measure_stage4.py <build> <decoded-firmware> <fresh-private-paced> paced
```

Run final default component tests, not their explicitly named intermediate
original-clock sidecar configurations. Both RX and Timer1 reports must show
zero original-clock child steps. All admitted child waits must be no-ops for
insufficient budget or absent child completion. Full-engine native graph gaps
fail explicitly rather than silently using the interpreter inside that graph.

Correctness covers20 scenarios: the prior15 plus `clockext`, `clockjitter`,
`clockgap`, `clockburst` and `rxrunning`. External scenarios select K clock via
real MIDI NRPN38=2, then send actual F8 through the UART input. Source/route
witnesses verify the selection; these scenarios do not patch guest RAM.
`clockburst` mixes dense F8 and cutoff CC; `rxrunning` uses running-status note
traffic. The prior synthetic TX fixtures remain explicitly labeled as such.

Stage4 trace copies wrap real timer and MCU device mappings without changing
their state/time behavior. Ports tapes now contain HDI, timer MMIO and all
mapped MCU IO, including SYSCR and panel/LCD. The unchanged five-word format
uses addresses to distinguish them. It records guest accesses, not timer
backend implementation reads. Stock-vs-trace admission must pass before use.
These trace targets must never be used for CPU claims.

Offline CPU uses eight musical scenarios including External and jitter;
paced CPU uses held8/sweep/External, with adjacent stage3/native pairs. Keep
all CPU regressions, compute overruns and legacy diagnostic output. Original
H8 divide-by-zero behavior is preserved, not normalized to a new algorithm.
