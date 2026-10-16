# Silver-K optimization journal

This versioned journal is shared between Windows and macOS work. Append dated
entries; do not replace earlier measurements with results from another machine.
Raw local reports are not committed. A hypothesis is not a measured result.

## Current state — 2026-10-16

- `ks-native-parameter-calls-20261016` connects the signed coefficient task
  to normal KS execution from entry through return. Its 26 old instruction
  bodies and retired register-shift batch live only in the comparison object.
  All 48 correctness stages pass; 13,933 actual coefficient calls execute in
  the nine musical captures with zero test recovery. A temporary caller/IRQ
  projection remains. The global register/flags/stack/PC removal is incomplete;
  CPU comparison and push of the completed transfer remain deferred.

- `ks-native-keycalls-20261016` now connects both adjacent-key searches to
  actual KS playback. The native task owns all key-selection and wait progress;
  old instruction bodies are confined to the comparison object. A temporary
  output projection still supplies the remaining callers/IRQ saves. All 47
  correctness stages pass (cold boot, complete DSP tapes, raw audio and focused
  checks), with no normal-path partial-frame fallback. This is actual runtime
  integration, not completion of global register/flag/stack/PC removal. CPU
  comparison and push remain deferred until the complete port is verified.

- Earlier core admission `ks-native-control-core-20261016` recovered
  processor-independent coefficient and live-key-search tasks. The key-search
  connection is described above; the coefficient connection is now also
  admitted by the latest checkpoint.
  The production compatibility register/flag/stack/PC model remains. CPU
  comparison is deferred until the full port; neither checkpoint is a completed
  native controller or a performance result.

- At the earlier reference-isolation checkpoint, KS production became
  independent of the H8 core. Original H8 is linked only
  into comparison executables. The generic 128 MiB device-map table is removed;
  the remaining compatibility frame is 16,777,472 bytes. Production starts C++
  from reset and normalizes legacy H8/hybrid preferences to C++. The register,
  flags, stack and continuation model is **still present**: the requested full
  semantic port remains unfinished. Test ID `perf-ks-reference-isolation-20261016`
  passes 16 final stages plus independent reset-vector and legacy-AU checks.
  Matched AU Multi FX median is 46.65479 -> 46.02389% of one CPU (-1.35%
  relative); Single median -1.73%, with one pair regressing. DSP/JIT are unchanged.

Earlier admitted checkpoints are retained below; references to selectable or
initial-default H8 describe those earlier builds, not current production.

- KS C++ cold reset now runs without the H8 interpreter, including initial
  DSP upload and warm-up; the original reset path remains the independent
  selectable reference. Four cold timelines plus 4,128 reset boundary cases
  pass with zero C++-path interpreter calls. This still uses the compatibility
  register/PC/flag/stack frame, so full controller-emulation removal is **not
  complete**. All 13 final cold/musical/plugin/CPU stages pass under test ID
  `perf-ks-native-reset-20261016`. H8 has not yet been confined to test targets.
- KS full-removal follow-up is isolated from preset-manager work. Full native
  bootstrap/typed-state conversion is **not complete**. The final fetch/UART/
  opt-in control-job candidate passes full admitted H8 command/audio/state tests.
  Versus the frozen accepted event-transport AU, total CPU medians decrease
  **47.09771 -> 46.60725% (-1.04% relative)** Single FX and
  **46.91181 -> 46.22541% (-1.46%)** four-part FX, three alternating pairs.
  This is a small local saving, not a complete controller conversion. Earlier
  trials are separate and cannot be pooled. H8 stays default/available and
  no installed bundle or preset-manager files changed. See the final entry.
- First live event-transport stage on Mac: opt-in timed HDI readiness/address/
  word/reply jobs now include actual busy retries and timeout countdowns.
  Frozen previous C++ versus final event transport gives AU median total CPU
  **51.9601 -> 51.1810% (-1.50%)** Single FX and
  **50.7903 -> 50.1744% (-1.21%)** four-part FX (100% = one CPU), three pairs
  per case at 48 kHz/256. Offline four-second medians improve 1.21% Single,
  5.86% Multi and 2.84% dynamic FX. These are modest, workload-dependent results;
  one AU Single pair regresses. Nine passages match 8,085,451 complete port
  events and all 24 integer streams/state; pedal/FX H8 switching also matches.
  All 12 AU windows have zero lost frames/deadline failures and exact float
  output/tails. No sound/JIT/clocks/priority/thread/buffer changes. H8 remains
  default/reference; `SILVER_KS_DSP_TRANSPORT=events` plus C++ selects this Mac-
  tested experiment. Other controller work remains the native compatibility
  port; Windows and real-DAW validation remain separate. See the final entry.
- Mac FX/Multi follow-up: 24 unprofiled arm64 AU windows, 16 held keys total,
  48 kHz/256 and three repeats per case. Median whole-process CPU for KS C++
  is **49.2/49.1/48.0/48.2% of one CPU** in Single dry/FX and four-part dry/FX;
  installed OsTIrus 2.2.16/TI2 is **52.0/51.7/64.7/63.3%**. These basic patches
  establish the consumption scale, not identical synth/FX workloads or universal
  speed ranking. All measured windows pass; separate sampled profiles disrupt
  realtime and are excluded. A RAM-read trial gives mixed ~0.4% aggregate
  improvement and is reverted. Fixed-command replay costs 18.6–19.7% less CPU
  than the current native compatibility controller in exact four-second offline
  Multi/FX passages. This is evidence of remaining controller cost, not a live
  replacement or promised saving. The new FX bus/audio/state test passes with
  zero post-boot H8 calls. See the final entry below for limits and next steps.
- Mac total-CPU follow-up: a matched arm64 AU host compares installed OsTIrus
  2.2.16/TI2 with frozen and changed **sequential KS C++** builds. Three
  alternating runs per build, 0/1/8/16 held keys, 48 kHz/256, basic oscillator
  and effects off: KS note-phase median total CPU falls **6.1–7.7%**, idle
  **11.7%**. KS uses about **49–50% of one CPU**, the same scale as OsTIrus
  in these active-note passages. Float output hashes match all six KS runs;
  eight frozen-reference command/24-stream/state scenarios also match exactly.
  The change only moves the existing DSP catch-up loop into the DSP wrapper's
  translation unit; no additional threads, JIT or sound changes. Ranges and
  limitations are in the latest total-CPU entry below.
- CPU objective clarified: lower total process CPU at equal delivered audio is
  the acceptance criterion. The Mac handoff candidate below is **not accepted
  as a CPU optimization**: 9.09547 versus 6.83596 CPU seconds per 12 seconds
  of audio is +33.05%. Keep sequential execution as the optimization baseline.
  Installed OsTIrus AU 2.2.16 includes arm64. Its runtime comparison is now
  recorded below, separately from the earlier 2.2.25 source review.
- Mac handoff follow-up: excessive condvar parking at short KS slices is
  reproduced in stacks and an isolated handoff benchmark. The final Mac path
  runs DSP A on the existing coordinator and DSP B on one helper, with bounded
  active waiting before parking. Eight exact command/audio/state passages,
  12 paced 128/64-frame captures and six paced CPU controls pass. Total CPU is
  about 33% higher than sequential, while active worker wall/audio falls from
  ~0.53 to ~0.36. Ordinary-priority VST3 CLI-host captures still have intermittent
  losses; audio-priority VST3 passes 6/6 and AU passes. Keep the mode opt-in and distinguish host scheduling from
  correctness. See the latest Mac handoff entry below.
- Initial macOS baseline on Apple M1 Pro: Release arm64 Standalone/VST3/AU built;
  eight full H8/native command/audio/state scenarios and live switching pass.
  Same-current-binary native CPU medians are 2.5–4.8% lower in four passages;
  no frozen pre-instrumentation Mac baseline. Sequential DSP playback passes
  all eight realtime captures at 128/64 frames. **Two DSP helpers fail all
  eight Mac realtime captures**, despite exact offline audio equivalence;
  keep them opt-in/off by default. See the dated Mac entry below.
- Windows tester follow-up: Setup now has a per-instance **KS controller:
  H8 / C++** selector with worker-owned live handoff, project/default persistence
  and separate diagnostic sessions. Windows x64 VST3/Standalone tester package
  and Russian A/B instructions are prepared; final UI-switch, actual VST3 audio
  and Processor integration checks passed. Ryzen results are still pending;
  these binaries do not establish a new CPU saving or dropout remedy.
- Full post-boot follow-up: `SILVER_KS_CONTROLLER=native` now runs normal KS
  playback control in C++, with H8 retained for original bootstrap only. Eight
  passages match complete DSP tapes, 24-stream integer audio, state/Flash and
  MIDI output; the interpreter lock and actual counters show **zero H8 calls
  after boot**, including native preparation. Frozen original-H8 CPU controls
  show **2.4–5.3% lower process CPU seconds**, three pairs per passage on the
  local Windows Intel. This is a clock-compatible native port, not a batched
  OsTIrus-style redesign. Default H8 and the earlier hybrid remain available;
  keep the experiment off by default. Mac follow-up is recorded below;
  real DAW/driver and Ryzen checks remain pending.
- Historical live note/sustain stage: C++ handles Note On/Off decisions, key and
  velocity memory, held-key/mono priority, CC64 state and part/pedal release
  decisions. Complete DSP bus tapes/audio match H8 in four scenarios, including
  all four voice modes and pedal threshold/repeated-note cases. This remains a
  **hybrid**: original H8 handles UART/part routing and DSP voice parameter
  generation/upload. Default H8 unchanged; opt-in
  `SILVER_KS_CONTROLLER=native-midi`. CPU medians are **0.8–1.6% higher**, no
  saving on the local Windows Intel. Keep off by default; Mac/Ryzen pending.
- Controller follow-up on branch **Optimized**: recorded original H8 traffic to
  both KS DSPs (boot, Single and four-part passage), and added an opt-in checked
  C++ recorded-scenario controller. Original H8 remains the plugin default.
  Three matched pairs per passage show **23.4% / 25.4% lower CPU seconds** with
  identical 24-stream audio hashes and DSP counters. This prototype replays
  precomputed commands; it is not yet a live MIDI/patch/state replacement and
  these savings must not be attributed to a completed native controller.
- Phase: opt-in KS dual-DSP workers implemented with original H8 and exact
  slice barriers. All six Summary two-worker runs and one Detailed run had zero
  missing frames on the local Intel, while sequential controls failed. Exact
  native-audio/state equivalence passed; total process CPU time increased.
  This remains opt-in and unverified on the tester's Ryzen or a real VST3 host.
  The earlier P-core experiment also remains opt-in; its results are preserved
  below. JIT settings and guest scheduling targets are unchanged.
- CPU cost follow-up: equal-work native KS render (six runs, exact audio and
  guest counters) shows **126.3% more total CPU seconds** for two DSP workers
  (5.875 -> 13.297), while wall time falls 23.1% (5.872 -> 4.514 s). This is
  improved throughput at increased CPU cost, not CPU saving. Boot/warm-up and
  host polling are excluded; real DAW/Ryzen CPU cost remains unmeasured.
- Platforms measured: Windows x64 and macOS arm64, with separate results below.
  A Mac command-line wrapper check does not establish behavior in a user's DAW.
- K Windows notification response improved; active worker wall cost did not
  improve. KS retains its exact original polling path: event parking failed both
  Low and Balanced in extended repeats; later polling trials also failed, so the
  causal explanation remains open. Weak-CPU / DAW checks remain.
- Baseline source: record the revision embedded in each diagnostic report, plus
  dirty state and the patch/commit containing this instrumentation.
- No conclusion yet about the tester stutters or the fastest remedy.
- OsTIrus 2.2.25 source comparison confirms separate TI/TI2 DSP threads,
  a native C++ controller implementation and host-side output summing. KS's
  default runs original H8 and both DSPs serially with shared serial timing;
  native control and parallel DSP scheduling are now separate opt-in modes.
  The tester's successful OsTIrus run is a useful comparison, but no matched
  OsTIrus/Silver-K runtime benchmark exists yet. Follow-up: K-Station works;
  only KS stutters during playback, in VST3 on Windows 11 on a Ryzen 3 3100
  (4 cores / 8 threads, user reports 24 GB RAM and SSD storage). This is not a
  reported boot failure. Intel P/E-core
  placement findings must not be presented as this tester's cause or remedy.
- Scheduler trace follow-up: KS's largest reproduced losses coincide with the
  worker spending about 94% of its scheduled time on E-cores and nearly saturating
  one logical CPU. This occurs in the frozen original, poll override and address
  candidate. Ready-to-run delay is small in these failures. This is strong local
  evidence for a placement/throughput issue, not a proven fix or explanation of
  every tester report. The subsequent placement experiment below changes only
  the KS worker's in-process CPU-set selection; Windows settings remain unchanged.

## 2026-10-15 — source review, before instrumentation

- Reviewed Silver-K 0.2.4 Beta, Gearmulator release 2.2.25 and DSP dependency pins.
- Observed: K/KS use one emulation worker per instance; KS schedules H8 and both
  DSPs there. JIT block limit 32, DO limit 2, block linking disabled. Output
  exchange is per frame, and the worker sleeps for a requested 50 µs at its lead.
- Comparison: OsTIrus uses separate DSP workers and a batched trampoline;
  Xenia links JIT blocks and selectively enables dynamic peripheral addressing.
- Hypotheses: DSP dispatch overhead, worker scheduling/refill bursts, controller
  cost and repeated control snapshots may contribute. No timings or claimed
  speedups. Larger reserves do not fix sustained throughput below realtime.
- Reference: https://github.com/dsp56300/gearmulator/releases/tag/2.2.25

## 2026-10-15 — optional performance instrumentation

- Added Summary and Detailed sessions to Setup, JSON export, per-session
  callback/worker/queue counters and sampled H8/DSP/serial wall timers.
- Detailed probes sample every 256th board advance; summary worker timing spans
  up to 256 generated frames. Export/formatting stays off realtime threads.
- Reports describe CPU, OS, architecture, host/wrapper, compiler/configuration,
  configure-time source revision, model, rates, prepared/actual blocks, buffer
  reserve and missing/stale frames. Original data and personal paths excluded.
- Extended the existing realtime fixture with optional JSON reports. Added
  ROM-free counter/session/concurrent-reader tests. Instructions are in
  `PERFORMANCE_DIAGNOSTICS.md`; future agents should read both documents.
- Emulation algorithms, JIT settings, virtual clocks, buffers, queue algorithms,
  MIDI sequencing, resampler and thread priorities remain unchanged.
- Windows validation: Release Standalone, VST3, realtime fixture, editor, latency
  and diagnostics targets built with MSVC 19.44 / NMake. Counter/session/threaded
  reader tests, host-clock test and native-editor test passed. Setup screenshot
  inspected. Source-only repository audit and diff whitespace check passed.
- Repeating the editor test in its previous settings folder initially failed its
  pre-existing 75% default-scale assertion because the first run saves another
  scale. Re-ran successfully with a fresh isolated data folder. This was not an
  instrumented audio failure. Also corrected the existing latency test's buffer
  type to `synthLib::SysexBuffer` so it builds with Windows' PMR-enabled framework;
  production patch handling was not changed.
- Latency host-reentry test with private K/KS firmware passed (53.72 s): buffer
  and rate changes, state restore, asynchronous worker completion, offline audio
  and realtime transition. macOS compilation/audio remain unverified here.

## 2026-10-15 — first Windows captures (measurement, no optimization)

- Source identity in reports: `5aa9e31d06f5`, dirty instrumentation tree; Release,
  MSVC 1944, DSP pin `44353be2f0156cac970c6ab4f467ea1998b40605`.
- Machine: Intel Core i5-13600KF, x64, 14 physical / 20 logical CPUs, ~32 GB RAM.
  JUCE reports Windows 10. Host/wrapper Unknown/Undefined because this is the
  command-line fixture, not an audio device or DAW.
- Scenario: existing 12 s fixture, 48 kHz, prepared/actual 128 frames, Balanced,
  one instance, single-part K or KS, synthetic zero wave bank for KS, no editor,
  state saving off, first note then eight-note chords. Not a four-part stress
  test. No audio driver/backend is exercised. Power mode was not controlled.
- Sequential Off/Summary/Detailed, one run of each per model. All six exited 0;
  no startup or steady missing frames, no stale frames, no diagnostic callback
  deadline overruns. Note onset and peak agreed within the fixture's printed
  precision across modes; this is not a bit-exact sound-accuracy validation.
- Raw reports/logs/index are local at ignored
  `output/perf/20261015-120329/` (IDs `win32-{K,KS}-128-{off,summary,detailed}-r1`).

| Measurement | K Summary | K Detailed | KS Summary | KS Detailed |
| --- | ---: | ---: | ---: | ---: |
| Worker wall time / generated audio duration | 0.313 | 0.312 | 0.789 | 0.795 |
| Mean callback, ms | 0.02565 | 0.02613 | 0.00612 | 0.00612 |
| Mean actual worker sleep, ms | 1.621 | 1.623 | 1.563 | 1.578 |
| Missing frames | 0 | 0 | 0 | 0 |

Detailed mean time per sampled board advance (microseconds): K H8 1.209,
DSP A 1.481; KS H8 0.981, DSP A 1.648, DSP B 1.561, serial 0.193. These
deterministic samples include OS preemption and are not an exact CPU breakdown.

- Interpretation: this machine sustains the fixture, with materially less worker
  throughput headroom for KS. The requested 50 µs sleep actually spans about
  1.6 ms on this Windows setup. Investigate scheduling/reserve along with DSP
  throughput; neither observation establishes the cause on tester machines.
- One pass cannot establish a precise instrumentation overhead or comparative
  speedup: repeat/reorder captures and test real drivers before drawing that
  conclusion. No stutters were reproduced here. GUI/driver interference and
  low-latency/four-part/multiple-instance scenarios still need captures.
- Next Windows round: identical passage at 128/256/512 in a real DAW, then KS
  four-part and low-latency profiles, UI open/closed, on an affected weak CPU.
  Next macOS round: build the same source, repeat matched captures, append the
  results and mark macOS checks explicitly rather than inheriting Windows results.

## 2026-10-15 — Windows worker wake-up, implementation and comparison

- Authorized scope: replace full-output-queue polling on Windows, measure K and
  KS and keep JIT, guest clocks, buffer reserves, resampling, priorities and
  instruction semantics unchanged. macOS retains its previous polling path.
- Saved pre-change fixture binary at ignored
  `output/perf-wakeup/before/silverRealtime_test.exe`, SHA-256
  `62a747ac1be4f5d857e40bc1eac36668a9547cdcf47cd7baf1cac40d60915907`.
- Fresh pre-change captures: `output/perf-wakeup/before/20261015-121804/`,
  3 runs each for K/KS, 48 kHz, 128 frames prepared/actual, Balanced, Summary,
  12 s fixture, one instance, no editor/state saves, same private assets as above.
  All 6 returned 0, no missing frames. These fresh controls are preferable to
  comparing an old single run: CPU/power/scheduling conditions vary over time.
- Added a shared Windows wait helper: arm/recheck + sequence-based WaitOnAddress,
  notify after consumed blocks, notify pause/stop and offline refill, bounded
  10 ms safety timeout. Resolves APIs at construction and retains Win7/non-Windows
  polling fallback; no process/system-wide timer changes or busy spin.
- Added `worker_wake_latency` and actual `worker_wait_backend` to reports. The
  runner can force `--worker-wait poll` for same-binary A/B measurements. Total
  idle wait duration is no longer interpreted as the notification response time.
- Tests cover notification before parking, repeated progress races, stop while
  parked, timeout and forced-poll fallback. These and diagnostics, host-clock and
  editor tests passed on Windows. Release Standalone/VST3 and the fixture built.
- Measured executable after the change:
  `output/perf-wakeup/after/silverRealtime_test.exe`, SHA-256
  `fe64b2ec9f81445b2d6498420aebf790fcd7c521b3c0f4588347e9ae80843a42`.
  Both poll-control and address measurements below use this exact executable.
- Same machine/compiler/assets/scenario as the fresh controls: i5-13600KF,
  Windows reported as 10, Release/MSVC 1944, source `5aa9e31d06f5` + dirty changes,
  Summary, 48 kHz / 128 prepared and actual frames / Balanced / 12 s / single part,
  no editor, one instance and state saves off. No DAW/driver, controlled power
  mode, thermal control, randomized ordering or weak-CPU restriction.
- Address captures: `output/perf-wakeup/address/20261015-122713/`.
  Same-binary poll controls: `output/perf-wakeup/poll-control/20261015-122837/`.
  Three sequential runs per model per backend (address first, then poll).
  All 12 returned 0, with zero missing/stale frames and zero callback overruns.
  Together with the frozen original controls this is 18 successful main captures.
  A local summary is at ignored `output/perf-wakeup/comparison.json`.

The following table reports medians of the three run means, except worst-case
latency and minimum queue depth, which are the worst observation across all
three. Histogram p99 values are upper bounds, not exact percentiles.

| Metric | K poll control | K address | KS poll control | KS address |
| --- | ---: | ---: | ---: | ---: |
| Notify-to-wait-return mean, microseconds | 1258.810 | 16.346 | 1191.419 | 13.142 |
| Notify-to-wait-return p99 upper bound, microseconds | 2097.151 | 131.071 | 2097.151 | 131.071 |
| Worst notify-to-wait-return, microseconds | 2282.5 | 148.6 | 2078.9 | 157.8 |
| Worker wall time / generated audio duration | 0.261 | 0.281 | 0.524 | 0.593 |
| Callback mean, microseconds | 25.376 | 25.062 | 5.396 | 6.971 |
| Total idle wait mean, ms | 1.526 | 1.925 | 1.559 | 1.134 |
| Minimum ready queue at render entry, frames | 382 | 472 | 348 | 374 |
| Missing/stale frames and callback overruns | 0 | 0 | 0 | 0 |

- The frozen original controls had median active ratios K 0.253 / KS 0.519 and
  mean sleep K 1.532 / KS 1.554 ms, broadly consistent with instrumented polling.
  They have no notify-to-return metric, so the wake-latency comparison uses the
  new executable's poll override, not invented timings for the old binary.
- Result: much faster notification response and more queue reserve in these
  captures. Guest/audio latency stayed K 10.5417 / KS 10.6667 ms; printed note
  onset and peak matched all original/poll/address captures. No claim of a
  bit-exact audio validation or lower end-to-end MIDI/audio latency.
- Tradeoff: active worker wall ratios increased in the address runs, particularly
  KS (0.524 -> 0.593). This is not thread CPU usage and includes preemption, but
  it prevents claiming a throughput/CPU saving. The cause is not established;
  scheduling/core placement/power behavior need separate evidence. The candidate
  addresses timer-induced wake stalls; it does not solve insufficient emulation
  throughput. Keep the poll override for diagnosis if an affected CPU regresses.
- Small-buffer follow-up: 48 kHz / 64 frames / Low latency / Summary, one run per
  model/backend at `output/perf-wakeup/low-poll/20261015-123124/` and
  `output/perf-wakeup/low-address/20261015-123152/`. K poll missed 7 frames
  (6 startup / 1 steady), K address 0. KS poll missed 0, unrestricted KS address
  missed 333330 steady frames and failed. Fast notification response alone did
  not prevent loss of worker throughput; causal attribution remains incomplete.
- A second candidate used a per-worker high-resolution waitable timer for the
  requested 50 us (no global timer changes). Local archive:
  `output/perf-wakeup/timer-prototype/silverRealtime_test.exe`; reports at
  `output/perf-wakeup/low-timer/20261015-123736/`. Two runs each: KS missed 324556
  frames in one run and 0 in the other; K missed 0 in both. Notification response
  was roughly 0.45–0.48 ms. This candidate was rejected, and its code/runner
  option removed; no claim that it solves the regression.
- Reserve-guard candidate (subsequently rejected): keep address waiting for K, and for KS only
  at reserve durations >=512/48000 s. KS below that duration uses the existing
  polling wait, reported as `poll-small-reserve`. This protects the known failing
  scenario without increasing buffers or changing JIT/guest code. The address
  override does not bypass this safety gate. This is a bounded first step,
  not a solution to all weak-CPU / Low-latency failures.
- Guarded-build repeats used archived executable
  `output/perf-wakeup/reserve-guard-prototype/silverRealtime_test.exe`, SHA-256
  `01b37c61805e117152767b7d0ad427762a732e6b64045d3940d08d6b64f9aeb0`.
  Poll controls: `output/perf-wakeup/final-poll/20261015-124451/`, all six passed.
  Guarded-default captures: `output/perf-wakeup/final-default/20261015-124616/`.
  K passed all three. KS Balanced failed two of three, missing 259138 / 99853
  steady frames; the third run passed. Thus the small-reserve guard was not
  sufficient and was rejected, rather than promoted on the first three passes.
  The low-profile follow-up at `output/perf-wakeup/final-low/20261015-124740/`
  passed all four return codes: K event waiting missed 0 frames in both runs;
  KS polling had 15 startup frames in one run and 0 in the other, no steady
  misses. The fixture deliberately permits startup-only misses: return 0 must
  not be read as a guarantee of zero startup dropouts.
- Final implementation policy: event waiting is enabled for K on supported
  Windows hosts. KS uses its exact original 50 us sleep (`poll-50us`), without
  the new arm/recheck/notification path. KS still records total sleep duration,
  but has no notify-to-return observations. No KS improvement is claimed.
  The reserve gate and high-resolution timer were removed. JIT, buffers, guest
  timing, resampling and priorities remain unchanged.
- Cause of the KS regressions is still unproven; fast wake latency accompanied
  worse active wall cost and sometimes large losses. Investigate scheduler/core
  placement and sustained sections with a trace before enabling event parking
  for KS. Do not treat the failed trials as evidence against all event-based
  designs, or as a solved root cause of the testers' stutters.
- A further K-event / KS-helper-poll build (before restoring exact KS code) was
  archived at `output/perf-wakeup/production/silverRealtime_test.exe`, SHA-256
  `b2fc7aada5a562723c0ff2a744d2bee30b957d6d90e9807b0e9f15e8dc03cc23`.
  Reports: `output/perf-wakeup/production-results/20261015-125123/`.
  K passed all three, zero missing frames. KS polling failed once with 97196
  missing frames (60844 startup / 36352 steady), then passed twice with zero.
  Thus the failures cannot be attributed solely to address parking. Restoring
  exact KS polling avoids promoting additional synchronization overhead without
  demonstrated benefit; it is not a claim that the baseline issue is fixed.
- Frozen-original replay `output/perf-wakeup/original-recheck/20261015-125516/`
  failed all three KS runs with 196716 / 317617 / 202633 steady missing frames.
  This replay overlapped a Release build, so it is a contention experiment,
  **not an uncontended performance control**. The original implementation can
  also miss its deadlines under competing CPU load; this does not establish the
  cause of the other failed runs.
- Uncontended frozen-original KS replay after the build finished:
  `output/perf-wakeup/original-idle-recheck/20261015-125802/`. All three passed,
  zero missing/stale frames and callback overruns; median worker ratio 0.524,
  sleep 1.441 ms, callback mean 5.519 us. Do not mix the concurrent-build replay
  with this control or infer a stable regression from their difference alone.
- Final executable after restoring exact KS polling:
  `output/perf-wakeup/final/silverRealtime_test.exe`, SHA-256
  `024a52ae30b9570a42cf3a0d4130ef2e5c88c9ee5b80e4928d018930400395cf`.
  Reports: `output/perf-wakeup/final-restored-results/20261015-125848/`.
  Same 48 kHz / 128 / Balanced / Summary scenario, three runs per model,
  after the three uncontended original KS replays; no concurrent build/test.
  All six passed with zero missing/stale frames and callback overruns.

Final medians of run means (worker ratio is a median of run ratios):

| Metric | Fresh original K | Final K address | Fresh original KS | Final KS original polling |
| --- | ---: | ---: | ---: | ---: |
| Worker wall / audio duration | 0.253 | 0.262 | 0.519 | 0.539 |
| Callback mean, microseconds | 24.829 | 33.819 | 5.151 | 5.474 |
| Total idle wait mean, ms | 1.532 | 1.977 | 1.554 | 1.406 |
| Notify-to-return mean, microseconds | unavailable | 12.102 | unavailable | unavailable |
| Missing/stale frames and callback overruns | 0 | 0 | 0 | 0 |

- Final K's worst notify-to-return observation was 507 us. Its largest callback
  was 1.1761 ms, below the 2.667 ms deadline. Callback mean rose relative to the
  fresh original captures (the initial matched A/B table did not show that rise);
  do not claim lower callback cost or CPU usage. The original binary has no
  notify metric: the earlier same-binary polling comparison (K 1258.810 us vs
  16.346 us) provides the direct wake-latency A/B evidence. A longer final idle
  wait is compatible with sleeping until consumption; it is not a wake penalty.
- Final KS deliberately has zero wake-latency observations, not a measured zero
  latency. Its polling sleep and throughput are broadly consistent with the
  uncontended original replay. The successful final runs do not erase earlier
  intermittent failures or validate the rejected address/timer candidates.
- Windows Release fixture, Standalone and VST3 built successfully. All five final
  tests passed in 33.54 s: diagnostics, worker-wakeup, host-clock, native-editor
  and latency reentry. The last exercised both K and KS with real private OS
  inputs, synchronous host reentry, buffer/rate changes, state restore, firmware
  worker completion, offline audio and transition to realtime; it was not skipped.
  Local test transcript: `output/perf-wakeup/final/regression-tests.log`.
- Source-only repository audit, Python runner syntax check and diff whitespace
  check passed. Raw reports, archived executables and private assets remain
  local under ignored output paths.
- Next step: capture an OS scheduling trace for KS during a failing sustained
  section, including worker CPU time, core migrations and ready-to-run delay,
  with a matched original control and no concurrent build. Repeat on an affected
  weak CPU and in a real DAW before expanding the K change to KS.
- macOS wait behavior is unchanged; macOS build/performance remain unverified.

## 2026-10-15 — KS scheduler traces during reproduced missing audio

- Scope: measurements only. No JIT, guest, buffer, resampler or plugin runtime
  changes in this round; the production KS path remains original polling.
  No registry/policy, driver, power-plan, affinity or process/thread-priority
  changes, installs, reboots or manual deletions. User explicitly permitted UAC
  for the temporary collector; benchmark processes remained unelevated.
- Same Windows x64 / i5-13600KF / 20 logical CPUs / Release MSVC 1944 setup.
  Frozen original: `output/perf-wakeup/before/silverRealtime_test.exe`, SHA-256
  `62a747ac1be4f5d857e40bc1eac36668a9547cdcf47cd7baf1cac40d60915907`.
  Rejected address prototype: `output/perf-wakeup/after/silverRealtime_test.exe`,
  SHA-256 `fe64b2ec9f81445b2d6498420aebf790fcd7c521b3c0f4588347e9ae80843a42`.
  The latter was also run with `SILVER_WORKER_WAIT=poll` to isolate backend
  changes on one executable. It includes notification instrumentation, so it
  is not the same code as the frozen original. No experimental KS code was
  restored into the production source. Production fixture hash remains
  `024a52ae30b9570a42cf3a0d4130ef2e5c88c9ee5b80e4928d018930400395cf`.
- All cases: KS, one part, same private OS / synthetic init flash / zero wave
  assets, 48 kHz, prepared/actual 64 frames, Low profile, Summary diagnostics,
  12-second existing passage, eight-note chords after the initial note, no
  editor/state saves, one instance, isolated fixture data. Existing prepare call
  at second 6 remains. No concurrent compilation or other test suite.
  Case order: original, address, candidate-poll, address, original,
  candidate-poll, original, address. This is interleaved, not randomized.

Collector and evidence quality:

- Built-in WPR 10.0.19041 needed administrator profiling rights; ordinary and
  Codex elevated attempts failed with `0xc5585011`. After user-approved UAC it
  recorded, but `-stop` failed with COM error `0x80010106`, leaving no usable ETL.
  A 15-second smoke repeated that failure. First fixture reports are retained
  at `output/perf-scheduler/ks-20261015-130516/` but are **not scheduler evidence**.
  Microsoft documents this WPR failure in its
  [start/stop notes](https://devblogs.microsoft.com/performance-diagnostics/wpr-start-and-stop-commands/).
  No registry repair, toolkit installation or reboot was attempted.
- Used a small standalone controller via `StartTraceW`/`ControlTraceW`, with a
  private system logger GUID/name, QPC clock, CSwitch, ReadyThread, process/thread
  and DPC/interrupt events. Optional CPU samples use the existing OS interval;
  no `TraceSetInformation`, stack walk or symbol/JIT tuning. Only the collector's
  process token temporarily enables its existing profiling privilege. Automatic
  stop at 240 s or a STOP file; sequential ETL limit 512 MB. Only its own session
  handle is stopped. A five-second smoke saved a valid ETL with zero lost events.
- Minimal trace: `output/perf-scheduler/ks-native-20261015-131453/`,
  ETL 298778624 bytes, SHA-256
  `b33a7b0b880d83d3adb72c62f454c85329650548c690e2178e9727fda19405d8`.
  All eight fixture return codes were 0. Seven had no missing frames; candidate
  polling case 06 had 132 startup-only misses and zero steady misses. Thus this
  trace did not reproduce the large sustained failure. In successful steady
  windows, original had 3004–3291 dispatches vs address 6139–6436, and original
  1820–2153 logical-CPU migrations vs address 4157–4613. More frequent parking
  changes scheduling behavior; these counts alone do not establish causation.
- Trace including CPU samples: `output/perf-scheduler/ks-sampled-20261015-131949/`,
  ETL 322174976 bytes, SHA-256
  `3552be337f80bfae668f7edfa201d7e1afb679b73dfef54b36839a3c2d04a430`.
  All eight cases completed; six fixtures failed their realtime criteria and
  two passed. Both native sessions stopped successfully (`ControlTrace=0`),
  with EventsLost / LogBuffersLost / RealTimeBuffersLost all 0. WPR is stopped.
- Local `capture-index.json` records PID, exact executable hashes, UTC/monotonic
  process boundaries and diagnostic session start. Worker identified by ETW
  thread name `KS H8S / dual DSP`, qualified by process and lifetime. CPU topology
  was read, not changed: group 0, logical 0–11 EfficiencyClass=1 (P), 12–19
  EfficiencyClass=0 (E). No affinity was applied.
- Offline decoder uses `OpenTrace`/`ProcessTrace` with 100 ns system timestamps.
  TDH MOF lookup returned 1168 here; the fallback accepts only known version/size
  layouts, checked against Microsoft's [CSwitch](https://learn.microsoft.com/en-us/windows/win32/etw/cswitch),
  [ReadyThread](https://learn.microsoft.com/en-us/windows/win32/etw/readythread)
  and [PerfView parser](https://github.com/microsoft/perfview/blob/main/src/TraceEvent/Parsers/KernelTraceEventParser.cs).
  Final sampled decode emitted 5772609 scheduler/name/sample records with zero
  unsupported records. All sixteen 8-second steady windows across the two
  captures have zero worker state inconsistencies and full time accounting.
  An audit found two inconsistent transitions in the first four seconds of
  minimal case 08, leaving 8.9 us unaccounted in its full 12-second reconstruction;
  that full-window result is flagged and excluded from comparisons. The sampled
  trace's full windows have no such inconsistencies. Raw events were preserved;
  no durations were imputed. A synthetic known timeline also verified durations,
  migrations and clipping.

The following are **individual runs**, not medians or a benchmark ranking.
Steady-window scheduler metrics cover diagnostic seconds 4–12 (8 s), excluding
firmware boot. Missing frames are whole-fixture totals; startup/steady counts
come from the fixture's separate approximately four-second warm-up boundary.

| Case / backend | Missing frames (startup / steady) | Scheduled on CPU, ms | E-core share of scheduled time | Ready-to-run total, ms | Max ready delay, us |
| --- | ---: | ---: | ---: | ---: | ---: |
| 01 original polling | 0 / 0 | 4238.0 | 4.31% | 25.895 | 112.2 |
| 02 prototype address | 0 / 0 | 4515.9 | 10.04% | 48.311 | 144.2 |
| 03 prototype forced poll | 64 / 649 | 4319.4 | 4.71% | 12.487 | 588.1 |
| 04 prototype address | 0 / 29539 | 4844.5 | 23.99% | 48.994 | 122.0 |
| 05 original polling | 82 / 326299 | 7543.7 | 93.53% | 4.348 | 106.7 |
| 06 prototype forced poll | 0 / 337097 | 7561.3 | 93.65% | 2.064 | 86.0 |
| 07 original polling | 0 / 319768 | 7538.2 | 93.55% | 3.307 | 106.1 |
| 08 prototype address | 0 / 334234 | 7593.0 | 93.66% | 8.508 | 99.5 |

- Strong measured pattern: in cases 05–08 the worker spends about seven seconds
  almost continuously on E-cores (CPU 12, or CPU 13 in case 08), starting around
  diagnostic second 5. In case 04 the same pattern appears late, around second
  11.25. The 250 ms timeline is in `scheduler-analysis-with-samples.json`.
  Scheduled occupancy reaches 94.2–94.9% of a single logical CPU over the last
  8 s, versus 53.0% in successful original case 01. No change was made to force
  or prevent these placements.
- Heavy-failure ready delays are only 2.1–8.5 ms total over 8 s; their maxima
  are <=106.7 us. Thus prolonged runnable-but-unscheduled starvation is not the
  dominant time category in these captures. The worker is mostly executing.
  Slow notification is not sufficient to explain the failure across original,
  forced-poll and address variants. This does not rule out separate rare stalls.
- Scheduled time includes interrupts and is not exact thread CPU accounting.
  CPU samples provide a cross-check: 99.56–99.80% of samples attributed to the
  heavy-failure workers point to user space; only 15–33 samples per case point
  to kernel space. DPC/ISR flags are recorded separately. Observed dispatch and
  sampled priorities remain 15 in good and bad runs. No evidence here for a
  priority drop or dominant interrupt storm; sampling cannot exclude rare IRQs.
- Interpretation / hypothesis: KS lacks throughput headroom when this workload
  stays on an E-core. Address parking increases dispatches/migrations in good
  runs, but the frozen original has the same E-core failure; changing the sleep
  alone does not resolve it. Why Windows settles the busy worker there remains
  open. Placement might also be influenced by the workload, power/thermal state
  or tracing overhead. No controlled placement counterfactual has been run.
- Limitations: synthetic host, zero wave bank, single part and one machine;
  not a real DAW/driver or weak tester CPU. Sampled and minimal captures were
  sequential; enabling profiling may perturb scheduling, and the late failures
  are not a randomized estimate of failure frequency. Current reports do not
  timestamp each missing audio block, so 250 ms scheduler bins cannot prove the
  exact first missing sample. Older untraced failures remain separate evidence.
- Added reusable standalone recorder/decoder and offline analyzer plus a
  parameterized comparison runner. Production plugin targets were not rebuilt
  or changed for this measurement. Tools compiled on Windows; Python syntax,
  known-timeline calculations and whitespace audit passed. The capture used a
  local driver with the same case sequence; the parameterized wrapper has CLI
  validation, not an additional full capture matrix. Earlier raw files remain
  untouched. Raw ETLs and binaries are local/ignored, never committed.
- Next plugin investigation: read the worker's existing QoS/placement state and
  test a narrowly scoped per-worker placement policy in a separate controlled
  A/B round. Keep an unchanged original control and avoid hard-coded CPU indices.
  Do not change Windows policies, affinity or priorities as a machine-wide fix.
  No such runtime change was made now. JIT remains outside this round.
- macOS code and wait behavior are unchanged; no macOS validation in this round.

## 2026-10-15 — opt-in KS worker P-core CPU Sets A/B

- Authorized scope: test P-core placement inside the plugin only. No Windows
  settings/power/registry/driver changes, process-wide placement changes,
  new thread priorities, timer changes, JIT changes or guest/resampler/buffer
  modifications. No files were deleted. Temporary ETW used the already approved
  UAC collector; all benchmark processes remained unelevated.
- Added `ksWorkerPlacement` and an opt-in `SILVER_KS_WORKER_CORES=p` flag, read
  once per KS instance. After original boot/warm-up and existing realtime-thread
  setup, the worker enumerates Windows CPU Sets and requests the highest
  EfficiencyClass through `SetThreadSelectedCpuSets(GetCurrentThread(), ...)`.
  No fixed CPU IDs, no hard-affinity setter and no process-default setter.
  Existing explicit host CPU-set assignments are left untouched; selection
  intersects the existing process/thread hard mask and excludes CPUs reserved
  for another process. Missing APIs, non-hybrid/multiple-group topology or no
  eligible P-cores yield a diagnostic fallback, with ordinary scheduling intact.
  K and macOS retain their prior behavior. Default/unset does not call the
  placement APIs. The flag is not stored in project state or preferences.
- Added `worker_placement` JSON: requested mode, applied/fallback status,
  Win32 error and CPU-set IDs. API success alone is not proof of actual CPU
  execution: that was checked separately with ETW. On this i5-13600KF the
  discovered selection was IDs 256–267: 12 logical processors / 6 physical
  P-cores, EfficiencyClass 1. These values are observations, not constants.
  The same-binary control records status `default` and no selected IDs.
- Release/MSVC 1944, Windows x64, source `5aa9e31d06f5` + dirty changes, same
  DSP dependency pin. Fixture, Standalone and VST3 built locally. No installed
  DAW/Standalone copy was replaced. Candidate archive:
  `output/perf-placement/candidate-20261015-134427/silverRealtime_test.exe`,
  SHA-256 `7e5d583269d417ffd08abdbbedc1e966fde2329a68bf80decfd3e5e635e6b678`.
  Default and P-core tests use this exact executable. Frozen-original control
  remains SHA-256 `62a747ac1be4f5d857e40bc1eac36668a9547cdcf47cd7baf1cac40d60915907`.
- Same fixture/assets/CPU as the scheduler investigation: KS single part,
  synthetic init flash, zero wave bank, eight-note chord passage, 48 kHz,
  64 prepared/actual frames, Low, Summary, 12 s, one instance, no editor/state
  saves, original prepare at second 6. All modes retain KS's original 50 us
  polling wait. No compilation, ETL decoding or other test suite during captures.
  Each series interleaves: candidate default, P, frozen original, P, candidate
  default, P, candidate default, frozen original. No forced E-core placement.
- Sampled ETW series: `output/perf-scheduler/ks-pcores-20261015-134445/`.
  `scheduler.etl` = 315424768 bytes, SHA-256
  `0aaab01093e8f77e0d0933167bfea9bfb47faa0ea5f12b7723f4527478cf7d21`.
  Own collector stopped successfully, no lost events or buffers. Decoder emitted
  5557844 records, zero unsupported records. All full and steady reconstructions
  have zero state inconsistencies and complete time accounting (16 windows).
  The large sustained collapse did not recur during this traced series; its
  controls had smaller misses. Do not substitute older severe traces for it.
- Repeated the identical sequence without ETW:
  `output/perf-placement/untraced-20261015-134714/`. Severe misses recurred in
  two same-binary default controls and one frozen-original control, while each
  interleaved P-core run had zero steady misses. No actual per-core execution
  trace exists for this second series; successful CPU-set application is in
  each JSON report. This avoids claiming unobserved placement from API success.

Counts below are **per-run sequences**, not estimates of population failure
rates. Startup is the fixture's initial approximately four-second window;
steady is its remaining passage. Worker ratio is the median of whole-run wall
time / generated-audio ratios, not total CPU usage.

| Series / mode | Runs | Startup missing frames, each run | Steady missing frames, each run | Median worker ratio |
| --- | ---: | --- | --- | ---: |
| ETW, candidate default | 3 | 0 / 0 / 133 | 63 / 0 / 32 | 0.534 |
| ETW, candidate P | 3 | 0 / 0 / 0 | 0 / 0 / 0 | 0.524 |
| ETW, frozen original | 2 | 0 / 0 | 79 / 10 | 0.564 |
| No ETW, candidate default | 3 | 0 / 0 / 0 | 0 / 138817 / 303678 | 0.655 |
| No ETW, candidate P | 3 | 78 / 33 / 0 | 0 / 0 / 0 | 0.514 |
| No ETW, frozen original | 2 | 15 / 0 | 0 / 333411 | 0.671 |

- Direct placement evidence: all three traced P-core workers spent **100% of
  their scheduled time on P-cores**, in both full and seconds 4–12 windows, with
  zero cross-efficiency-class migrations. Steady scheduled occupancy was
  52.0–53.9% of one logical CPU. Same-binary default controls spent about
  5.1–5.4% of their scheduled time on E-cores in this particular traced series.
  Observed dispatch and sampled priority stayed 15 in both modes.
- P-mode steady dispatch counts were 2588–2762 and logical CPU migrations
  1051–1222, compared with default 3064–3162 and 1930–2108. Different placement
  changes scheduling behavior without changing the polling algorithm. Ready
  delay maxima were <=567.8 us for the P runs; no claim that every latency
  percentile improved. These are scheduled durations, including any IRQ time.
- Across all six P runs: zero steady misses, zero stale frames and zero timed
  callback overruns. Four had zero missing frames throughout; two untraced runs
  still lost 78 and 33 frames in the initial window. Fixture return 0 permits
  startup-only misses; the result is **not a claim of complete dropout freedom**.
  Reported plugin latency remained 5.33333 ms and note onset 9.52083 ms in all
  modes. Largest P-mode timed callback was 0.1210 ms vs a 1.333 ms block deadline.
  Printed P-mode peak stayed 0.123063. No bit-exact audio comparison was performed.
- Interpretation: this targeted intervention materially improves the reproduced
  steady-playback failure on this hybrid CPU. It supports the previous
  placement/throughput hypothesis more strongly than observation alone: the
  same executable with ordinary placement loses large amounts of audio, while
  P-mode's steady part remains clean. It does not isolate every factor or prove
  a fix for all tester machines. Raw summary: `output/perf-placement/comparison.json`.
- Six final regression tests passed in 34.10 s with `SILVER_KS_WORKER_CORES=p`:
  CPU selection (hybrid classes, host mask, reservations, homogeneous/default
  no-op), diagnostics, worker wakeup, host clock, native editor and latency
  reentry. The last used both private OS inputs and exercised state restore,
  rate/block changes, firmware completion, offline rendering and transition to
  realtime; it was not skipped. Transcript is beside the archived candidate as
  `regression-tests.log`. CI build/test lists include the new selection test;
  CI itself has not run here. Source audit, Python syntax and whitespace checks
  passed. Raw reports/ETLs/binaries remain local and ignored.
- Decision: retain this as an **explicit opt-in experiment**, with default
  scheduling unchanged. Startup-window losses remain to investigate. A small
  sequential single-instance synthetic test is not a production-wide rollout
  gate: no real DAW/driver, long session, four-part/multiple-instance, constrained
  host CPU-set, multi-group or weak-CPU validation yet. Power/thermal state and
  order are not controlled; CPU Sets may be overridden by OS power management.
  Do not infer a universal emulation speedup or total-system CPU saving.
- Next step: test longer real-DAW sessions and several KS instances with this
  opt-in mode, then separately investigate initial-window misses. Keep the
  ordinary-scheduling control; combine P-core placement with another wait
  strategy only as a separate experiment. No JIT work in this round.
- macOS build/performance remain pending; no Windows CPU IDs are persisted into
  plugin state, and the macOS placement path performs no Windows action.

## 2026-10-15 — OsTIrus comparison (source review only)

- Trigger: the affected tester reports good OsTIrus performance on the same
  computer. This is user-reported evidence; CPU/model, host, buffer, OsTIrus
  version/ROM model and exact Silver-K symptom are still unknown. Asked whether
  "problem starting" means failure/hang during loading or playback stutters.
- Local source: `5aa9e31d06f5efb203a80193a74a1a9fa9caeeee`, dirty diagnostic/
  wakeup/placement tree described above. Windows x64 source review; no new
  builds, runtime changes, ETW sessions or performance captures in this round.
- External reference: Gearmulator **tag 2.2.25**, commit
  `06d0c0c9c02e6b3e0b9768295d003905cc8e4e08`, DSP submodule
  `e643fa89f0c11147963b315c0bd8afb283bba468`. Retrieved tagged files through
  GitHub; did not substitute moving `main` for the release. Local ignored
  snapshots, source URLs and Git blob identities:
  `output/research/ostirus-2.2.25-20261015/manifest.json`.

| Area | OsTIrus TI/TI2, tagged source | Current KS |
| --- | --- | --- |
| DSP ownership | `DspSingle::startDSPThread` creates one `DSPThread` per DSP; `Device::bootDSPs` starts both | One `Device` worker owns H8, DSP B and DSP A |
| Controller | `virusLib::Microcontroller` implements MIDI, patches, state and HDI08 protocol in native C++ | Original H8S OS executes via `h8->step()` with peripheral ticks, 25 MHz virtual clock |
| DSP coupling/output | `processAudioTI` supplies inputs to both DSPs and unpacks/adds their ESAI outputs on the host | A/B exchange serial frames; `Board::advance` advances B then A and services a shared ESAI slot clock |
| Work granularity | DSP worker calls trampoline `exec(&m_dsp, 128)`; device processes audio in blocks | H8 budget defaults to 128 guest cycles, then DSPs catch up through `step(1)`; output is exchanged per frame |
| Startup | Boots DSPs and initializes native controller state; factory patches read directly from ROM | Boots H8/DSPs and renders single/multipart warm-up passages before signaling readiness |

- Model qualification: TI and TI2 create `DspMultiTI`, with A and B; Snow creates
  `DspSingleSnow`, so an OsTIrus Snow run is not a dual-DSP control.
- The native controller is a major distinction: the name `Microcontroller`
  does not mean OsTIrus runs an instruction emulator for the original controller
  in this path. It parses MIDI/SysEx, queues DSP words and manages patches in C++.
  KS's controller cost is additional to its two DSPs. Do not infer that all
  Gearmulator products use the same controller strategy.
- KS's `step(1)` is an instruction-budget request, **not proof of one emulated
  instruction per call**: `execJit()` can execute a whole compiled block.
  Frequent C++ scheduling/peripheral checks are a source-level candidate cost;
  no new timing establishes their share of a failed run.
- JIT differences were inspected read-only: TI sets DO limit 32 and inherits
  linked blocks / uncapped instruction blocks from the pinned defaults; KS
  explicitly uses DO limit 2, block limit 32 and linking off. The two projects
  also pin different DSP revisions (Silver-K `44353be2f0156cac970c6ab4f467ea1998b40605`).
  No JIT edits, dependency updates or performance attribution to these settings.
  Batched trampoline execution is part of that execution path and remains
  outside the currently authorized non-JIT experiment.
- Interpretation: good OsTIrus behavior argues against dismissing this tester's
  computer as generally unsuitable. Two DSPs do not establish equivalent host
  work or a comparable single-core deadline. OsTIrus can use two cores for DSP
  work; KS adds both DSP costs and H8 work on one core. This fits the local
  throughput/placement evidence, but is not a measured explanation of this
  tester's failure and does not establish a speedup factor.
- Do not copy OsTIrus's output mixing or split KS into independent workers
  directly: KS has live inter-DSP serial dependencies and H8/HDI08 interactions.
  Such a change needs a separate timing/ownership design and audio correctness
  validation. Replacing H8 with native controller logic would change the
  emulation approach and is not proposed within this scope.
- Startup distinction: KS's constructor waits on `startCV` until boot and
  warm-up complete; current performance sessions attach only afterwards.
  Neither the previous P-core steady-state result nor its startup-window audio
  misses measure plugin construction/boot time. P-core selection currently
  happens after warm-up, so it cannot establish a faster firmware boot.
- Next non-JIT measurement: on the tester's machine, hold host/driver, host
  sample rate, actual block, instance count and UI state fixed. Record OsTIrus
  version, TI/TI2/Snow model, DSP clock percentage, internal latency and resampler
  settings; compare silence and a fixed eight-note MIDI passage with KS, marking
  different patches/firmware as different workloads. Capture per-thread CPU,
  actual core placement, callback deadlines, audible losses and KS diagnostics.
  Identical MIDI alone does not make different synth algorithms equally costly.
  If failure is before ready, first measure boot/warm-up phases separately;
  current diagnostics cannot observe them. No OsTIrus ROM or matched capture was
  supplied for this review, so no OsTIrus CPU/dropout/startup numbers are claimed.
- macOS: source architecture comparison only; no compilation or runtime result.
- Primary sources: [DSP creation, boot and configuration](https://github.com/dsp56300/gearmulator/blob/2.2.25/source/axel/virusLib/device.cpp),
  [DSP worker creation](https://github.com/dsp56300/gearmulator/blob/2.2.25/source/axel/virusLib/dspSingle.cpp),
  [TI output routing](https://github.com/dsp56300/gearmulator/blob/2.2.25/source/axel/virusLib/dspMultiTI.cpp),
  [native controller](https://github.com/dsp56300/gearmulator/blob/2.2.25/source/axel/virusLib/microcontroller.cpp),
  [pinned DSP worker](https://github.com/dsp56300/dsp56300/blob/e643fa89f0c11147963b315c0bd8afb283bba468/source/dsp56kEmu/dspthread.cpp).

## 2026-10-15 — tester clarification: KS playback on Ryzen

- User report: K-Station has no problems on the affected computer. KS opens
  but its audio stutters as if the processor were overloaded. Format: VST3;
  OS: Windows 11; CPU described as an older Ryzen, exact model unspecified.
  OsTIrus runs well there. DAW, driver, sample rate, actual block, patch/parts,
  plugin versions and OsTIrus TI/TI2/Snow model are not yet known.
- This resolves the earlier startup ambiguity: prioritize sustained/episodic
  playback losses rather than firmware boot diagnostics for this report.
  Preserve the previous source review as historical context.
- The Intel i5-13600KF measurements establish a local placement improvement;
  they do not explain the Ryzen report. On a homogeneous CPU, the current
  P-core experiment deliberately does nothing. Do not recommend enabling it
  as a general Ryzen fix or assume the unidentified Ryzen has Intel P/E classes.
- Working hypothesis: KS's single worker has less throughput headroom because
  it executes H8 and both DSPs sequentially. K working and OsTIrus working are
  useful controls, but neither proves the cause: firmware, synth workloads,
  internal latency and execution architecture differ. Aggregate CPU percentage
  alone cannot distinguish one busy worker from idle capacity on other cores.
- Next capture on the affected VST3 setup: matched K/KS summary sessions,
  then KS detailed probes, with fixed host/driver/rate/block and a repeatable
  passage. Correlate missing frames with queue depth, worker wall/audio ratio,
  H8/DSP timings and sleep duration. Ratios/timers include descheduling; if they
  are high, distinguish actual thread CPU work from scheduler delay before
  attributing the result to computation. Use OsTIrus as an additional control
  with its exact model, clock and internal latency recorded.
- No new measurements or reproduction on Ryzen; no code, JIT, priorities,
  buffers or Windows settings changed. Updated journal only; whitespace check
  passed. Windows/local Intel and pending Ryzen/macOS evidence remain separate.

## 2026-10-15 — affected machine identified: Ryzen 3 3100

- User supplied: AMD Ryzen 3 3100, AM4 motherboard, 24 GB RAM, SSD storage,
  Windows 11. Previous clarification identifies VST3, working K-Station and
  OsTIrus, with playback stutters only in KS. DAW/driver, rate/block, patch/parts,
  plugin versions, OsTIrus ROM model and actual runtime counters remain unknown.
- Verified manufacturer specifications: Zen 2, 4 physical cores / 8 hardware
  threads, base 3.6 GHz / maximum boost up to 3.9 GHz. These are specifications,
  not observed operating frequencies or a benchmark of the affected computer.
  Source: [AMD launch specifications](https://ir.amd.com/news-events/press-releases/detail/942/amd-expands-3rd-gen-amd-ryzen-desktop-processor-family-unleashing-powerful-zen-2-core-for-the-mainstream).
- This model has no Intel-style P/E-core distinction. The existing opt-in P-core
  selector is not a remedy for this report. Keep the Intel experiment's result
  separate; do not attribute the Ryzen failure to E-core migration.
- Four physical cores make a two-DSP-worker experiment relevant if sufficient
  independent DSP work exists. Eight SMT threads are not eight physical cores,
  and synchronization/dependency costs still determine the achievable gain.
  Native-controller replacement is not a prerequisite for parallel DSP workers.
- Current single-worker architecture makes a single-core throughput constraint
  plausible; hardware identification alone does not prove it. No evidence yet
  identifies RAM capacity or SSD throughput as the cause. Do not infer memory
  channel layout, timings, thermal state or background load from capacity alone.
- Continue with the affected-machine K/KS captures described above, measuring
  per-thread CPU versus wall time and queue starvation before choosing H8
  replacement or a multi-worker implementation. No performance result on Ryzen,
  new build or code/JIT/system change in this update; journal whitespace checked.

## 2026-10-15 — opt-in KS dual-DSP workers with original H8

- Authorized scope: user explicitly requested two KS DSP workers, original H8,
  an off switch, original controls, measurements and journal updates. No JIT,
  dependency revision, firmware, Windows settings, buffers, virtual clocks,
  priorities of existing threads, resampling or MIDI scheduling changes.
- Architecture: default stays exactly the sequential DSP B -> A catch-up path.
  `SILVER_KS_DSP_THREADS=2` enables two persistent DSP helpers after sequential
  boot/warm-up and initial queue fill. H8 remains on the coordinator. At each
  existing `advance(128)` guest-cycle boundary, both helpers independently
  reach their original per-DSP target using the unchanged `step(1)`/JIT path.
  The coordinator joins both, then runs shared ESAI TX/RX in the original order.
  It never advances H8, accesses HDI08, snapshots state or services serial IO
  while a DSP slice is active. Helpers never directly run the shared serial
  callbacks (their ESAI clocks remain disabled as before).
- New `ksDspWorkers` uses release/acquire command/completion handoffs, bounded
  256-pause spinning then blocking. Windows dynamically resolves WaitOnAddress/
  WakeByAddressSingle; unavailable APIs and non-Windows use a condition variable.
  Idle helpers block when the coordinator sleeps. Existing live KS priority and
  realtime parameters are used for the new helpers; no process/global scheduler
  policy is changed. Rate/block parameter updates are handed off between slices.
  Both helpers are drained before propagating an execution exception, and joined
  before Board/DSP/queue destruction. No unbounded busy spin or larger guest slice.
- Off switch: unset the variable or set `1` before creating/reloading the KS
  instance. No persisted preference/state change; K ignores the flag. No helper
  threads are created in default mode. The previous P-core option targets only
  the coordinator; it is not a claim of DSP-helper placement. All captures in
  this round clear placement/wakeup overrides and use ordinary scheduling.
- Diagnostics: root/session `dsp_execution_backend` verifies `sequential` or
  `two-workers-slice-barrier`. New `dsp_barrier_sampled` measures dispatch plus
  both-DSP completion wall time in Detailed (stride 256), including execution
  and scheduler delays. DSP timers now have their respective helper as sole
  writer in parallel mode. Timers overlap; do not sum them. Worker ratio remains
  coordinator wall/audio duration, including waits, not total CPU usage.
- Build identity: local Windows x64 Release, MSVC 19.44/NMake, configured source
  `5aa9e31d06f5`, dirty tree, DSP pin unchanged at
  `44353be2f0156cac970c6ab4f467ea1998b40605`. Standalone, VST3, realtime,
  worker/equivalence and regression targets built. Frozen pre-experiment fixture
  `output/perf-dsp-threads/before-20261015-141612/silverRealtime_test.exe` SHA-256
  `7e5d583269d417ffd08abdbbedc1e966fde2329a68bf80decfd3e5e635e6b678`.
  Candidate `output/perf-dsp-threads/candidate-20261015-142039/silverRealtime_test.exe`
  SHA-256 `93f4274bbc1442d4abc3f14610ed477fa50722947599073f98775c37d5005ef4`.
- Correctness before speed testing: `ksDspEquivalence_test` with private KS OS
  2.1.02, synthetic Flash and zero wave bank compared **65,536 native frames,
  all 24 streams, bit for bit** between sequential and parallel Boards. Passed
  Single, sustain/release, Program Change, four-part program load/notes and All
  Sound Off. H8 cycles/PC, both DSP cycle/instruction counters and serial queue/
  underrun/overflow counters agreed every 1,024 frames; final controller state,
  saved user Flash and consumed MIDI counts agreed. Non-silent peak 1,356,564;
  rolling word hash `3852514813786200426`. Log: `equivalence.log` in that root.
  This passage is not exhaustive firmware/patch/audio-input validation.
- ROM-free worker test verifies two distinct overlapping helper threads,
  10,000 handoffs, payload visibility, failure drain/recovery, wake after idle
  and immediate idle destruction. Regression run with threads=2: seven tests
  passed in 54.14 s (worker, placement, diagnostics, wakeup, clock, native editor,
  latency/reentry). Private latency test was **not skipped**: both K and KS,
  rate/block changes, state restore, asynchronous completion, offline rendering
  and return to realtime passed. Transcript: `regressions.log`. CI target/test
  lists include workers and equivalence (the latter skips without private args).

Measurement scenario: local i5-13600KF, 14 physical / 20 logical CPUs, Windows
x64 (JUCE OS label Windows 10), ~32 GB RAM. Existing 12 s fixture, 48 kHz, actual
block equals prepared block; 64/Low and 128/Balanced; one KS instance/part, fixed
initial note and eight-note chords, synthetic zero wave bank, no editor, state
save off, prepare again at second 6. No real audio driver/DAW is exercised.
For each block, interleave candidate sequential, parallel, frozen, parallel,
candidate sequential, parallel, candidate sequential, frozen. No concurrent
build/tests/trace analysis during captures. Power/thermal state not controlled.

| Scenario / variant | Steady missing frames per run (after initial ~4 s) | Median worker wall/audio ratio | Median whole-process CPU seconds, including boot |
| --- | --- | ---: | ---: |
| 64 Low / frozen original | 208103, 308468 | 0.8030 | 11.883 |
| 64 Low / candidate sequential | 328408, 315637, 335827 | 0.8056 | 11.984 |
| 64 Low / candidate two workers | **0, 0, 0** | **0.5664** | 18.063 |
| 128 Balanced / frozen original | 269430, 283221 | 0.8404 | 12.828 |
| 128 Balanced / candidate sequential | 299239, 306434, 316764 | 0.8086 | 12.109 |
| 128 Balanced / candidate two workers | **0, 0, 0** | **0.5678** | 19.250 |

- All six Summary parallel runs had zero **startup as well as steady** misses,
  stale frames and timed callback overruns. All 16 runs had zero timed callback
  overruns. One frozen 128 run additionally lost 88,472 frames in the initial
  window; other controls' startup counts were zero. Failed controls returned
  fixture code 1 for audio misses, not crashes. Reported latency stayed 5.33333
  ms (64/Low) or 10.6667 ms (128/Balanced) across all variants, with note onset
  9.52083 / 14.7292 ms respectively. Parallel peak printed 0.123063 in all six.
- Important CPU limitation: process CPU seconds include identical sequential
  boot/warm-up plus all threads for the 12 s passage. Parallel costs more CPU
  in these captures. Failed controls also produce/deliver less useful audio;
  these numbers are not equal-work CPU-efficiency or power measurements, and
  the lower worker wall ratio is not a claim of lower total CPU usage.
- Raw matrix: `output/perf-dsp-threads/20261015-142039/`; derived sanitized local
  summary `output/perf-dsp-threads/summary.json`. Reproducer:
  `tools/run_dsp_thread_comparison.py` with private input arguments. It records
  hashes, PID, UTC, report backend, process CPU time and fixture summaries;
  runs ordinary unelevated processes and changes no system configuration.
- Detailed follow-up at 128/Balanced, one run per candidate mode:
  `detailed/20261015-142504` sequential lost 294,542 frames, ratio 0.8082;
  `detailed/20261015-142520` parallel lost zero, ratio 0.5643. Mean sampled wall
  microseconds: sequential H8/A/B/serial = 0.971/1.690/1.569/0.198;
  parallel = 0.879/1.562/1.464/0.248, with dispatch-and-both-DSP barrier
  **2.169 us** (max 98.3 us). Barrier overlaps A/B execution. These deterministic
  samples include descheduling and are not a precise instruction-cost breakdown.
  No ETW capture in this round; actual core placement and scheduler contribution
  are not isolated from computational parallelism. No universal speedup factor.
- Decision: retain the implementation **off by default**, as an experiment worth
  testing on the Ryzen 3 3100 in Windows 11/VST3. Seven parallel captures were
  clean locally, including one Detailed; correctness and regression checks pass.
  This does not establish that the tester's failure is fixed. Need longer real
  DAW/driver sessions, varied patches/four-part/multiple-instance load, startup/
  shutdown stress and actual per-thread placement/CPU evidence on that machine.
  H8 replacement is not required to try this architecture.
- Repository audit (537 tracked files), Python syntax and whitespace checks
  passed. Source snapshots/reports, extracted private update and binaries stay
  local/ignored. Windows/macOS share these notes, but macOS compilation/runtime
  and non-Windows CV performance remain **unverified**. No release/install/commit
  or Windows configuration change was performed.

## 2026-10-15 — equal-work CPU cost of the two-DSP-worker experiment

- Request: measure CPU saving, distinguishing wall-time speed from total CPU
  work. Production emulation and JIT were **not modified** in this round.
  Added measurement-only targets `ksCpuCost_test` and `silverCpuCost_test`, plus
  `tools/run_cpu_cost_comparison.py`. Windows settings, placement, existing
  priorities, guest clocks/slices, buffers and DSP dependency pin are unchanged.
- Primary valid measurement: native Board executes original H8 and both DSPs,
  in one ordinary benchmark process. Same source/binary for both modes; read
  `SILVER_KS_DSP_THREADS=1`/`2`, boot sequentially, send identical patch setup and
  eight-note warm-up, enable helpers only after that warm-up, settle another
  4096 native frames, then measure the passage. No host consumer busy-wait,
  resampler, pacing, diagnostics session, report writing or file IO inside the
  interval. The native coordinator uses the existing live KS priority, with
  helper priorities unchanged from the experiment. No affinity or OS policy set.
- Work: **544,085 native frames**, all 24 integer streams, exactly
  12.0000039385 s at 45,340.4017857 Hz. Single-part synthetic Init/zero-wave-bank,
  original private KS OS 2.1.02. First note at 0.1 s, release at 1.1 s, then
  eight-note chords every 40 ms from 1.12 s, fixed absolute native-frame events.
  Original UART, H8, serial clocks and DSP catch-up remain in use. This passage
  is separately defined from the earlier 12 s host fixture; do not merge their
  numbers or claim an identical patch/MIDI benchmark across the two harnesses.
- Counter: **GetProcessTimes user + kernel**, summing CPU consumed by every
  process thread, including H8/coordinator, both DSP workers, their synchronization
  and the identical small integer-hash loop. Reads only; no scheduler/timer
  configuration. Independent steady-clock wall time. Boot/setup and warm-up
  measured separately and excluded. GetProcessTimes accounting is quantized;
  these multi-second intervals avoid interpreting sub-millisecond differences.
- Platform: same local Intel i5-13600KF (14 physical / 20 logical), Windows x64,
  JUCE label Windows 10, MSVC 1944 Release/NMake, source `5aa9e31d06f5` dirty,
  DSP pin `44353be2f0156cac970c6ab4f467ea1998b40605`. Native fixture SHA-256:
  `63a552cd6345297598d75726493bca3dd529f966d8432f740eced7470acbc54e`.
  Archived alongside reports at `output/perf-cpu-cost/20261015-144250/`.
  `source-sha256.json` records measurement/source identities. No concurrent
  build, other benchmark, test suite or trace analysis during these six captures.
- Order: sequential, parallel, parallel, sequential, sequential, parallel;
  three runs per mode, ordinary Windows core selection, no P-core override.

| Mode | Total process CPU seconds per run | Wall seconds per run | Median CPU / median wall |
| --- | --- | --- | --- |
| Sequential | 5.875, 5.875, 5.875 | 5.8861, 5.8643, 5.8724 | **5.875 CPU s / 5.8724 s** |
| Two DSP helpers | 13.1094, 13.2969, 13.5000 | 4.5138, 4.4745, 4.5203 | **13.2969 CPU s / 4.5138 s** |

- Result: **no CPU saving**. Median CPU cost increased **126.33% (2.2633x)**.
  Median wall duration decreased **23.14%**, equivalent to **1.3010x throughput**
  for this complete fixed-work render. Three active threads finishing sooner
  can consume more CPU seconds than one thread. These are not total-PC CPU
  percentages, power readings or a prediction of DAW CPU-meter values.
- Equal work verified in **all six** reports: no discarded/substituted host
  frames (native pull rendering has no missing-frame fallback); hash of all
  24 streams `8211242178193234287`, peak 5,145,840; identical final H8 cycles
  400,303,881 / PC 14,196; DSP A/B cycles 2,537,244,788 / 2,530,291,470 and
  instructions 1,912,864,755 / 2,148,277,550. `verification.json` stores the
  common signature. `summary.json` marks `valid_equal_work=true` and computes
  percentage changes only for complete, hash-matching captures.
- Interpretation: the experiment has a substantial CPU-efficiency cost even
  when both modes render the same complete audio. Frequent per-slice barriers
  and bounded active waits are plausible sources of extra work, along with
  placement/frequency/cache effects. Their individual shares are **not measured**;
  no ETW/core-placement or per-function CPU profile was collected in this round.
  Do not attribute all 126% to one mechanism or infer the same overhead on Ryzen.
- Exploratory full-Processor harness: `silverCpuCost_test` measures 576,000 host
  frames at 48 kHz, separately counts the caller via GetThreadTimes and excludes
  boot/warm-up. Ran offline 128/Low and paced 512/Maximum, three per mode each,
  with diagnostics off. Raw `output/perf-cpu-cost/20261015-143810/` is retained,
  but **excluded from the strict saving calculation**: hashes differ even across
  repeated sequential runs despite zero missing frames. Exact host-state/event
  reproducibility needs investigation; no cause is asserted. Offline caller
  queue-yield time also dominates total CPU, so process-minus-caller is recorded
  as background work, not an exact emulator-only counter. Both host scenarios
  have `valid_equal_work=false`;
  their apparent savings/speed ratios must not be promoted to a result.
- Earlier preflight at `20261015-143542` stopped before measurement because the
  new fixture requested a missing parameter (`filter_freq` instead of `cutoff`).
  Corrected only the measurement fixture; retained logs, no result inferred.
- Validation: native measurement target built; all six native cases returned 0,
  passed frame/hash/guest-counter checks. Python syntax, whitespace and tracked
  source-only audit passed. No production code changed, so the prior eight
  worker/audio/regression checks remain the production validation; they were
  not needlessly repeated here. Full-Processor fixture is exploratory, not a
  validated DAW CPU benchmark. Binaries/private assets/reports remain ignored.
- Decision: keep two DSP workers **off by default**, as a deadline/throughput
  experiment with a measured CPU cost. Next optimization candidate is reducing
  synchronization/active-wait cost while preserving exact guest barriers and
  audio; this round does not implement it. Real VST3/driver, multiple-instance,
  four-part, Ryzen 3 3100 and macOS measurements remain pending.

## 2026-10-15 — H8-to-both-DSP capture and first C++ controller prototype

- Request: branch **Optimized**, record KS H8 exchange with both DSPs, start an
  optional native-controller prototype and compare behavior/audio/CPU against
  original H8. Created that exact branch from the existing dirty checkout;
  previous work preserved. No JIT, DSP dependency, Windows policy, priority,
  affinity, guest slice target, serial order or audio buffer change.
- Scope delivered: bounded original-bus recorder plus a **finite recorded C++
  controller**. Playback executes the original port transactions and checks
  every read result and write acceptance without executing H8 instructions.
  Actual H8 slice endpoints drive the existing DSP/serial scheduler. This
  establishes a comparison fixture and protocol corpus, not a finished live
  controller like OsTIrus's. No plugin setting enables it; normal K/KS still
  run their original controller. Opt-in is through explicit Board API/private
  fixture. A fresh original-H8 Board disables it.
- Guards: original H8 boot/preparation required, matching deterministic prefix
  fingerprint, trace validation/checksum, original 128-cycle budget, terminal
  mismatch/exhaustion, rejected live MIDI and unavailable state/Flash/boot API.
  H8 stays allocated and frozen during replay; no switch back to its stale
  state. Fingerprint is a partial compatibility check (selected RAM/IO, assets,
  clocks, serial data and queue sizes), **not a full machine snapshot**.
  Complete DSP register/memory equivalence is not claimed.
- Recorder retains controller cycle/PC, DSP A/B, host byte offset/value,
  read/write/acceptance and slice boundaries. It reserves at most 3,000,000
  events and 1,000,000 slices (~68.7 MiB vector payload); overflow invalidates
  the tape. File IO and checksumming happen outside the measured interval.
  Portable explicit little-endian `KSHTR001` format; existing files refused.
  Raw tapes contain private firmware/preset material and remain ignored.
- Source identity: `5aa9e31d06f5` dirty, branch Optimized, Windows x64 local
  i5-13600KF (14 physical / 20 logical), MSVC 1944 Release/NMake, unchanged DSP
  pin `44353be2f0156cac970c6ab4f467ea1998b40605`. Main comparison binary SHA-256
  `bc587c65064763a535eabf94bc9fa5dc22201cce8a981b65e0d6b3e14faeb837`.
  Binary archived with `source-sha256.json` and raw reports at
  `output/perf-controller/20261015-150503/`. Normal private KS OS 2.1.02,
  synthetic Init Flash/zero wave fixture; no private asset committed.
- Work: **90,681 native frames**, 2.0000116 s at 45,340.4017857 Hz, all 24
  integer streams. Single and four-part passages, eight notes total per chord,
  100 ms chord changes, sustain, original program change and filter control.
  H8 boots/prepares/warms each fresh process; both DSPs sequential for this
  primary comparison. No host pacing, resampling or missing-frame fallback.
  Distinct workload from the earlier 12 s CPU/DSP-thread fixture.
- CPU is Windows **GetProcessTimes user + kernel for the whole process**;
  wall uses steady_clock. Boot/setup, loading/saving tapes, fingerprinting,
  statistics/report formatting excluded. Recording run excluded from savings.
  All 15 processes succeeded: boot capture, two passage captures, six H8 and
  six replay runs. Three interleaved pairs per passage: H8/replay, replay/H8,
  H8/replay. No concurrent build/test/trace analysis during these captures.

| Passage / mode | CPU seconds per run | Median CPU s | Median wall s |
| --- | --- | --- | --- |
| Single / H8 | 1.000000, 0.984375, 1.046875 | **1.000000** | 1.0034873 |
| Single / recorded C++ | 0.750000, 0.765625, 0.765625 | **0.765625** | 0.7583863 |
| Four parts / H8 | 0.968750, 0.984375, 0.984375 | **0.984375** | 0.9891197 |
| Four parts / recorded C++ | 0.734375, 0.750000, 0.734375 | **0.734375** | 0.7389352 |

- Result: recorded playback CPU cost fell **23.4375% Single / 25.3968% four
  parts**, wall duration fell **24.4249% / 25.2937%**. CPU accounting is
  quantized (15.625 ms observed), intervals short and repeat count three.
  These are CPU seconds for equal rendered work, not total-PC or DAW meter
  percentages. No ETW placement/frequency or per-function profile collected.
- Audio/behavior: every H8/replay run matched its recorded reference's frame
  count, all-24-stream integer hash/peak, virtual controller endpoint, both DSP
  cycles/instructions and serial queue depth/underflow/overflow. Single hash
  `58781405289897816`, peak 6,653,235; four-part hash `13074848859789201745`,
  peak 4,715,204. Every replay checked **736,364 / 537,112** port transactions
  and kept original H8 cycles frozen. Serial depths [1,1], no under/overflow.
  This is exact sample comparison by hash, not a listening test or complete
  live-state comparison. The early raw aggregate label `exact_audio_and_dsp_state`
  means these counters/checks only; new runner uses `exact_audio_and_dsp_counters`.
- Captures: boot 1,663,623 events / 570,993 slices, cycle 0..75,460,599;
  Single 736,364 / 374,913, 20,779,792 bytes; four-part 537,112 / 372,896,
  17,543,352 bytes. Boot recording is retained for protocol analysis only;
  it has not been replayed without H8. Boot recording CPU/wall includes
  instrumentation and is not an improved-boot result.

| Capture | DSP A status reads / accepted 24-bit low-byte writes | DSP B status reads / accepted low-byte writes |
| --- | --- | --- |
| Boot | 80,820 / 8,146 | 1,277,461 / 82,028 |
| Single | 207,032 / 17,714 | 285,982 / 23,899 |
| Four parts | 121,365 / 10,434 | 251,427 / 20,854 |

- Observed: status offset 4 polling, ICR offset 0 writes and 24-bit data
  bytes at offsets 10/12/14 dominate this corpus. Zero accepted CVR bit-7
  commands in these traces. Do not assume all controller operations are CVR
  interrupts; next work must decode ICR flags/data handshakes and actual
  payloads. Register counts do not yet identify high-level command meanings.
- Interpretation: eliminating H8 execution is a promising CPU candidate in
  these passages. Recorded commands precompute firmware decisions and read
  contiguous tape memory; they do not implement native MIDI/patch decisions.
  A live controller may have different CPU/cache/synchronization costs and
  different functional coverage. No claim that KS stutters disappear, no
  predicted Ryzen 3 3100 saving, and no extrapolation to real VST3/multiple
  instances, arbitrary patches/waves or macOS.
- Completion validation: Release Standalone and VST3 rebuilt successfully.
  Original-H8 `ksDspEquivalence_test` passed with its prior reference hash
  `3852514813786200426` / peak 1,356,564 for 65,536 native frames (Single,
  sustain, program change, four parts, H8/DSP clocks, serial and saved state).
  Eight CTests passed: controller trace, DSP workers, worker placement,
  diagnostics, wake-up, host clock, native editor and latency/reentry (33.99 s
  total); private K/KS OS provided, fresh local data directory, DSP helpers
  requested. No private-ROM skip used for the latency check. Python syntax,
  whitespace and source-only tracked-file audit passed; raw files ignored.
  NMake's localized include scanning does not reliably rebuild header consumers:
  explicitly refreshed Board consumer timestamps and rebuilt before regression
  and plugin checks. The primary native fixture directly constructs Board and
  was already freshly compiled when measured; archived binary unchanged.
- Independent compatibility check, `output/perf-controller/20261015-151301/`:
  both passages also passed H8/recorded comparison with **two DSP workers**,
  one pair each, identical audio hashes, port counts, DSP and serial counters.
  Rebuilt fixture SHA-256
  `69efae8d558ac6c1460dc3611e2c2e791c7588f8088a8593c43be5abfac0f70b`.
  Do not pool these single pairs into the primary sequential-DSP CPU estimate.
  Pure trace test added to both CI build/CTest lists; CI itself not run here.
  Windows build/test results do not validate macOS; source/file format portable
  by construction, macOS compile/audio pending and CPU counter not implemented.
- Next step: infer the observed ICR/flag/24-bit protocol and implement live
  MIDI-to-DSP command logic incrementally, with original H8 as oracle and
  shadow comparison before enabling any replacement in the normal plugin.
  LCD, MIDI output, arbitrary SysEx, state/Flash and save/restore remain
  unimplemented in recorded playback.

## 2026-10-15 — first live C++ Note/CC decoder, original H8 handlers retained

- Request: start live KS MIDI notes/basic CC controller, compare commands and
  sound with H8, keep opt-in, no JIT changes. Work stays on **Optimized** and
  preserves previous changes. Implemented `ksLiveMidiController`: a handwritten
  C++ port of the verified OS 2.1.02 byte-decoding stage for Note On/Off and CC.
  It processes new live bytes/running status, not precomputed recordings.
- Explicit boundary: this is a **hybrid native front end**, not a finished
  native voice/controller implementation. Original H8 UART, channel/part
  dispatch, keyboard range, velocity-zero handling, sustain actions, voice
  allocation, CC-to-parameter calculations and DSP writes still execute.
  Unsupported message types retain their original H8 path, including program,
  pressure, bend, SysEx and system messages. No claim that H8 can be stopped,
  that it was removed from the live render or that the old replay saving applies.
- Why this stage: a Note On produces a substantial DSP voice-parameter upload,
  not a simple MIDI packet. The earlier tapes also contain periodic DSP state
  reads. Replacing those decisions prematurely would confound command/audio
  comparison. The live decoder preserves firmware ABI, guest registers/flags,
  call state, instruction fetch/data clocks and interrupt boundaries, then
  hands completed messages to original handlers. A Board-local CPU adapter
  exposes existing bookkeeping; shared H8 core and DSP JIT code/config unchanged.
- Enable: worker-local `Board::enableLiveMidiController()` after original boot,
  or process-local `SILVER_KS_CONTROLLER=native-midi` for a fresh plugin instance.
  Normal/unset keeps original H8. Explicit Board disable is valid at quiescent
  boundaries because partial message state stays in the guest ABI. Environment
  switch is not saved or persisted to Windows. Recreate an instance without it
  for the default. Native/replay are mutually exclusive. No clocks, buffers,
  resampling, priorities, placement or OS settings changed.
- Diagnostics records `controller_backend` at root/session, independently of
  DSP-worker mode. Existing H8 wall bucket includes native decoding in hybrid
  mode; it does not isolate H8 instruction execution. Private fixture reports
  native bytes/messages/boundaries and Note On/Off/CC coverage.
- Private differential test: `ksLiveMidiController_test` runs C++ and independent
  original-H8 decoding against isolated memory buses. Initial exhaustive pass:
  **12,336 bytes / 6,144 messages / 308,688 boundaries**, all 16 channels,
  all 128 data values with varied velocities/control values, running status
  and varied starting flags. Every register/flag/PC/clock, call bookkeeping,
  bus-pair state and each read/write match. Interrupt arbitration and unknown
  firmware/status guards checked. Later completion pass adds alternating
  native/original execution inside messages (results below).
- Development failures retained: `20261015-152522` differed in audio/commands
  from an extra two guest cycles on a short subroutine call. `primary-run2`
  then caught a trace discrepancy; isolated differential testing found an
  incorrect 6/8-byte boundary for a status store. Both corrected before valid
  results. No failed case was counted as equivalent or as a speedup.
- Initial correct matrix `output/perf-live-controller/20261015-152907/` passed
  all bus/audio comparisons, but native dispatch was checked by function call
  on every H8 instruction. CPU medians were 1.6–3.2% above H8, with a 2.0625 s
  multi H8 outlier retained. Moved the native/default branch outside the loop,
  restricted native calls to the decoder range; default H8 loop now has no
  per-instruction native dispatch. The following matrix measures that revision.
- Primary valid matrix: **`output/perf-live-controller/20261015-153151/`**,
  local Windows x64 i5-13600KF (14 physical/20 logical), MSVC 1944 Release/NMake,
  source `5aa9e31d06f5` dirty, branch Optimized, unchanged DSP dependency pin
  `44353be2f0156cac970c6ab4f467ea1998b40605`. Archived native fixture SHA-256
  `90674c03ecaf19b7bdd856e3cfd23d1dfc3d173eaa2cab106f86e36fdd5568b7`,
  controller source hashes retained alongside it. Private KS OS 2.1.02,
  synthetic Init/zero wave fixture. Raw tapes and binaries stay ignored.
- Protocol/audio: original and live modes both receive the same actual MIDI.
  `record` and `live-record` tapes match **byte for byte**, including every
  read response/write acceptance, cycle/PC and slice clock. This compares
  736,364 Single / 537,112 four-part / 579,064 MIDI-scenario bus events. Single
  and multi tapes also equal the prior original-H8 reference tape hashes.
  All 24 integer audio streams match by hash, peaks, frames and final
  H8/DSP clocks/instructions/serial counters in all unrecorded comparisons.
- Work: 90,681 native frames (~2 s) per case. Same Single/four-part, eight-note
  chord passage as prior controller fixture (sustain/program/cutoff). New `midi`
  passage adds fragmented messages, running status, repeated pitches, zero
  velocity, CC 1/7/10/11/64/105/120/123, program and F1/F8 fallback paths.
  Single/four-part each exercise 299 native messages (152 On, 144 Off, 3 CC),
  24,219 native boundaries. MIDI case exercises 29 messages (9 On, 3 Off,
  17 CC), 2,349 boundaries. Original handlers remain responsible for their
  sonic effect; decoder coverage is not proof those handlers are ported.
- Audio hashes: Single `58781405289897816`, four-part
  `13074848859789201745`, MIDI `3819855541230884524`. Original/native recordings
  and all three unrecorded pairs agree; no substitute/dropped native frames.
- CPU: **whole-process GetProcessTimes user+kernel**, separate steady-clock
  wall duration. Excludes original boot/setup/warm-up, trace/report IO and
  recording runs. Sequential DSPs isolate the live controller; three pairs
  each, interleaved H8/live, live/H8, H8/live. No concurrent compilation, test
  suite or trace analysis during valid captures. All 24 fixture processes
  succeeded (six recorded controls and eighteen unrecorded renders).

| Scenario | H8 CPU s, three runs | Live CPU s, three runs | Median CPU H8 / live | Median wall H8 / live s |
| --- | --- | --- | --- | --- |
| Single | 1.000000, 1.000000, 1.000000 | 1.000000, 1.000000, 1.000000 | **1.000000 / 1.000000** | 0.999491 / 1.001034 |
| Four parts | 0.984375, 0.984375, 1.062500 | 1.000000, 0.984375, 0.984375 | **0.984375 / 0.984375** | 0.997120 / 0.992286 |
| MIDI/CC | 1.015625, 1.015625, 1.000000 | 1.031250, 1.015625, 1.000000 | **1.015625 / 1.015625** | 1.024410 / 1.011463 |

- Interpretation: **no measurable CPU saving**, identical medians at the
  observed 15.625 ms process-accounting granularity. Short intervals/repeat
  count do not establish exact zero overhead. Wall differences are small and
  not attributed to a controller speedup. H8's heavier voice/DSP-control work
  remains; prior 23–25% recorded-playback saving is a different experiment.
  No DAW stutter, actual core placement/frequency or listening test result.
- Completion validation: Release Standalone/VST3 and all requested test targets
  built. Refreshed Board/Device consumers before rebuilding because localized
  NMake include scanning is unreliable (same workaround as the previous round).
  Final isolated differential pass, including alternating native/original
  execution inside messages: **12,626 bytes / 6,288 messages / 315,950
  boundaries**, every bus access/state/clock equal. Default H8 audio regression
  retains hash `3852514813786200426` for 65,536 frames and passes saved-state,
  serial and H8/DSP-clock checks.
- Additional complete-Board validation:
  **`output/perf-live-controller/20261015-153603/`**, two DSP workers, native
  disabled at 0.65 s and re-enabled at 0.95 s, all three scenarios, one pair
  each plus recorded controls. All twelve processes succeeded; complete bus
  tapes and audio hashes still match uninterrupted original H8. Native
  execution resumes for the remaining live notes/CC. These are separate
  compatibility/switching checks, not additional primary CPU repeats. Final
  fixture (adds switching case/report wording; unchanged decoder) SHA-256
  `06e73bd6ed30399605b9c401bd1812853bafc9c4218b4edfdc641c90db311756`;
  archived there with source hashes.
- Full-Processor checks: eight CTests passed with
  `SILVER_KS_CONTROLLER=native-midi`, `SILVER_KS_DSP_THREADS=2`, fresh data and
  private K/KS OS (55.37 s). Controller trace, DSP workers, placement,
  diagnostics, wake-up, host clock, native editor and latency/reentry passed;
  the firmware-dependent latency test was executed, not skipped. Separate
  default-H8 latency/reentry also passed (53.15 s), with a fresh data directory.
  Python syntax, whitespace and source-only tracked-file audit passed; raw
  captures/private assembly/binaries remain ignored. Shared H8/JIT source and
  DSP pin have no changes. macOS, Ryzen 3 3100, real DAW/multiple instances and
  arbitrary patches/waves remain untested. macOS CPU accounting still unavailable.
- Next: port actual note/voice and selected CC actions behind this ABI bridge,
  including range/velocity/sustain/all-notes-off state, then voice parameter
  generation. Continue exact command/audio comparison with H8 before replacing
  its periodic DSP reads or removing H8 from the live path. Keep this hybrid off
  by default; no CPU-efficiency claim from functional coverage alone.

## 2026-10-15 — live C++ Note On/Off and sustain/key-state handlers

- Request: move actual note/pedal handling beyond MIDI decoding, compare DSP
  commands, audio and CPU with original H8, retain opt-in/no JIT changes,
  update diary and push verified changes to GitHub **Optimized**. Starting
  checkout clean at `865b2f8e85b3`, tracking origin/Optimized at the same commit.
  Shared H8 CPU source, DSP source/JIT settings/dependency pin, clocks, slice
  targets, serial ordering, buffer sizes, resampling, priorities and Windows
  policy remain unchanged. Only in-plugin native controller dispatch changes.
- Implemented `ksLiveNoteController`: C++ note handlers for range checks,
  velocity-zero release, note/velocity list, per-key velocity, physical-down
  and sustain-held bitmaps, ordered-key removal, mono replacement-key search,
  pedal threshold (below 64 / 64..127), repeated pedal state, held-voice
  release and part all-notes-off/reset decisions. Four voice modes retain
  their original control flow. Remaining original voice code still performs
  DSP allocation, parameter calculations and upload; boot/UART/part routing,
  arpeggiator timing and other CC conversion remain H8. No claim of complete
  native controller/H8 removal or recovered DSP-parameter implementation.
- Conservative implementation: 462 fixed C++ state transitions preserve the
  original interruptible guest ABI and fetch/data bus clocks. They execute
  without decoding/executing the corresponding H8 instructions. Live control
  state remains in guest memory, allowing original/native alternation inside
  calls/messages. Remaining H8 voice-engine calls retain exact targets and
  timing; the DSP comparison does not stub or shortcut that engine. Extended
  firmware fingerprint rejects unknown note/pedal/helper revisions.
- Default original loop retains no per-instruction native dispatch. Opt-in
  uses a cheap page filter before exact controller-PC dispatch. Initial
  private preflight (`20261015-155105`, one pair, 2 s pedal case) matched all
  bus/audio checks but CPU was ~7.9% higher; excluded from final estimate.
  Removed a redundant 128-vector interrupt scan when no IRQ is pending and
  replaced the hot multi-range filter with a page bitmap. Neither changes
  interrupt arbitration or guest semantics. Actual IRQ delivery still runs
  through the unchanged H8 core.
- Isolated differential verification: original H8 is the independent oracle
  for 12,626 live bytes / 6,288 decoded messages / 315,950 decoder boundaries,
  plus **14,784 note/pedal boundaries** (all 462 transitions, 32 varied
  operand/flag sets each). Registers/flags/PC/clock, stack/call and bus-pair
  bookkeeping, every bus read/write and final RAM match. Calls stop at their
  actual ABI destination in the one-boundary test. Pending-interrupt,
  firmware/status guards and alternating native/original execution checked.
  First harness run had an unsynchronized starting CPU clock after its guard
  test; corrected test setup before collecting this passing result.
- Primary native comparison: **`output/perf-live-controller/20261015-155316/`**,
  Windows x64 i5-13600KF (14 physical / 20 logical), MSVC 1944 Release/NMake,
  source `865b2f8e85b3` dirty, DSP pin unchanged
  `44353be2f0156cac970c6ab4f467ea1998b40605`. Archived fixture SHA-256
  `2d6bf78483cf27ddaaeaf082f111c502a26b99d9a8aed88ddf4f7218b5355bf7`;
  source hashes retained. Private KS OS 2.1.02, synthetic Init Flash and zero
  wave bank. Raw tapes/binaries/private disassembly stay ignored and are not
  part of the GitHub upload.
- Work: **181,362 native frames** (~4.0000231 s), sequential DSPs for both
  modes, all 24 integer streams. Same live MIDI to original H8 and native;
  each fresh process boots/prepares/warms through original H8 before selecting
  the measured path. `single` and `multi` extend the earlier chord passage to
  four seconds; `midi` extends the earlier control passage with its release
  tail. New `pedal` has four parts: Mono / Mono autoglide / Poly / Poly
  retrigger, key range 36..84 with notes outside it, repeated notes, Note On
  velocity zero, release velocity, CC64 values 63/64/127/0 and repeated values,
  all-notes-off/all-sound-off interactions and two event cycles. No sampled
  wave-bank/drum result inferred from the zero-wave fixture.
- Exact comparison: original/native recorded tapes are **byte-identical**,
  including host reads, writes, acceptance, guest cycle/PC and slice clocks.
  All unrecorded pairs match audio hash/peak/frames, H8/DSP cycles/instructions
  and serial depth/underflow/overflow. All 32 cases succeed (eight recorded
  controls plus three interleaved H8/native pairs per scenario). No concurrent
  build, other test suite or trace analysis during the measured renders.

| Scenario | DSP bus events compared | All-24-stream audio hash | Native note-state boundaries |
| --- | --- | --- | --- |
| Single | 1,450,157 | `2387145891285364963` | 698,075 |
| Four parts | 1,061,786 | `3176463666096680485` | 1,300,613 |
| MIDI/CC | 1,125,213 | `1205626428641651500` | 785,283 |
| Pedal / four voice modes | 703,901 | `3541542743908256581` | 1,548,572 |

- Pedal corpus executes 120 Note On handler entries, 64 explicit Note Off
  entries, 80 sustain actions and 24 pedal-up voice-release walks in native
  code. Velocity-zero release branches inside Note On, so decoded Note Off
  counts and explicit Note Off entry counts intentionally differ. Coverage
  assertions require note, note-off and sustain handlers to run, not merely
  that the option was requested. Audio equality is verified by hashes; no
  listening or real DAW dropout test performed.
- CPU: whole-process Windows GetProcessTimes user+kernel; wall via steady_clock.
  Original boot/setup/warm-up, recording, file IO and formatting excluded;
  common sample hashing and native execution counters included. Order per
  scenario H8/native, native/H8, H8/native; three runs per mode. Process CPU
  accounting is quantized (15.625 ms observed). Same equal-work render, no
  host pacing/resampling/underrun substitute. New four-second figures are not
  pooled with the earlier two-second decoder/replay measurements.

| Scenario | H8 CPU seconds per run | C++ CPU seconds per run | Median H8 / C++ | CPU change | Median wall H8 / C++ s |
| --- | --- | --- | --- | --- | --- |
| Single | 1.984375, 2.000000, 1.968750 | 2.078125, 2.015625, 2.000000 | **1.984375 / 2.015625** | **+1.57%** | 1.984394 / 2.026747 |
| Four parts | 1.937500, 1.953125, 1.968750 | 2.015625, 1.953125, 1.968750 | **1.953125 / 1.968750** | **+0.80%** | 1.986578 / 1.983301 |
| MIDI/CC | 1.968750, 1.937500, 1.984375 | 1.984375, 2.015625, 1.984375 | **1.968750 / 1.984375** | **+0.79%** | 1.985780 / 1.992141 |
| Pedal | 1.937500, 1.937500, 1.953125 | 1.968750, 1.968750, 1.968750 | **1.937500 / 1.968750** | **+1.61%** | 1.955155 / 1.975182 |

- Result: functional note/sustain port verified, **no CPU saving**. Prototype
  medians are slightly higher by one/two CPU-accounting quanta; three short
  repeats do not establish an exact overhead or cause. Conservative boundary
  dispatch/counters, cache/placement/frequency and remaining H8 work are not
  separately profiled. No ETW in this round. Keep native mode off by default;
  no claim that the Ryzen 3 3100 stutter is fixed or that a live H8-free
  controller would match these costs. Previous replay CPU saving is unrelated.
- Final validation: Release Standalone and VST3 built successfully. Refreshed
  header consumers before the build because localized NMake dependency scanning
  is unreliable; no shared H8/DSP source was edited. Final private differential
  test passed, including new handler fingerprint and all native page-filter
  entries. Default H8 regression retains 24-stream hash `3852514813786200426`
  / peak 1,356,564 for 65,536 native frames, and passes saved-state, serial and
  H8/DSP-clock checks.
- Switching/helper validation: **`output/perf-live-controller/20261015-160226/`**,
  Single and four-part pedal scenarios, two DSP workers, native disabled at
  0.65 s and re-enabled at 0.95 s. All eight processes (recorded controls plus
  one pair per scenario, two seconds each) passed full-tape/audio comparisons
  with uninterrupted H8. Actual native note/pedal handlers ran after re-enable.
  These compatibility/switching runs are separate from the primary CPU sample.
  Rebuilt fixture archived with source hashes; SHA-256
  `e04ef645a7c73331750b156bdeb75c7dffb1e7234aeb83bd65ffc2619da2280f`.
- Full Processor checks: eight CTests passed with native mode and two DSP
  workers (53.91 s): trace, DSP workers, placement, diagnostics, wake-up, host
  clock, editor and latency/reentry. Private K/KS OS and fresh data directory
  supplied; firmware checks executed, not skipped. Separate default-H8
  latency/reentry also passed (51.87 s). Python syntax, staged whitespace and
  source-only repository audit passed (574 tracked files). Private traces,
  disassembly, firmware, executables and personal reports are all ignored and
  excluded from the source commit. Normal CI cannot execute private-ROM checks.
- Publication requested to existing `origin/Optimized` (source/tests/docs only);
  final push receipt/commit identity is reported in the chat. Windows evidence
  does not validate macOS; Mac compile/audio, real VST3 host/multiple instances,
  other patches/waves and Ryzen remain open.
- Next: port/measure the remaining DSP voice parameter engine or reduce ABI
  transition cost while preserving exact clocks/interrupts. Note state now has
  an independent C++ implementation and a strict H8 oracle for that work.

## 2026-10-15 — Native KS voice lifecycle and DSP host transport (migration incomplete)

- Request: finish the entire C++ controller, keep it optional, leave JIT/Windows
  policy unchanged, update this shared diary and publish to `Optimized`.
  **The full-controller request is not complete in this change.** This is a
  verified additional migration stage; it still requires H8 during playback.
  The open scope question is whether original H8 boot alone may remain in the
  eventual complete mode. Neither answer changes the unfinished live work below.
- Source: parent `ab97a51d9fa5792a39d4c560b41c96844683dc1c`, branch `Optimized`,
  this entry/source patch. Windows x64, Core i5-13600KF, 32 GB; MSVC 19.44,
  Release/NMake. DSP dependency/configuration and JIT unchanged from the prior
  entry (`44353be2f0156cac970c6ab4f467ea1998b40605`). No Windows settings,
  system timer policy, process/thread priority or affinity edits, UAC collection,
  installs, deletions or changes to the user's ROM/Flash files.
- Added `ksLiveVoiceController`: 640 fixed C++ state transitions for DSP host
  address/status/24-bit read/write on A/B, selected voice/part address windows,
  voice allocation/reuse/stealing for mono/auto/poly/retrigger, velocity curves,
  voice slot initialization, note-start/glide/retrigger sequence, and matching
  note/pedal release. Original timing/polling and timeout calls retained. This
  is an interruptible guest-ABI migration, not a new opcode decoder or JIT.
  The bulk synthesis parameter calculators and periodic voice service still
  execute H8 callees. Threshold tables are read from the private input ROM;
  no ROM image, table dump or raw recording is included in the repository.
- Original H8 stays default. Existing `SILVER_KS_CONTROLLER=native-midi` opts in;
  unset/recreate to disable. The worker-only toggle remains available and the
  `live-cpp-midi-hybrid` backend name is preserved. No K engine behavior changes.
  A separate fingerprint verifies every newly ported range before activation.
- Shared boundary helper retains registers/flags, call stack, fetch/data clocks,
  pairing and in-flight PC visible to mapped devices. The latter matters now
  that C++ accesses actual DSP ports: original H8 exposes entry PC + 1 during
  the remainder of each instruction. The isolated oracle now compares PC and
  cycle at **every** bus access, as well as final machine state and RAM.
- Validation actually run: Windows Release `ksLiveMidiController_test`,
  `ksControllerProbe_test`, VST3 and Standalone built successfully. Private
  OS 2.1.02 differential test passed: 12,626 live bytes, 6,288 messages,
  315,950 decoder boundaries, 14,784 note/pedal boundaries and **20,480 new
  voice/transport boundaries** (640 x 32 varied operand/flag sets). Revision
  guard rejects changed voice code; pending-interrupt guards cover host access,
  voice selection/start and allocation. No DSP voice engine is stubbed.
- Internal whole-Board smoke ID: **`output/perf-live-controller/20261015-174241/`**.
  Probe EXE SHA-256:
  `d8fd0ddb22c44fb761d5fa3f11ec89fd6294c757813c50990de6e2c309185117`.
  Private synthetic Flash and zero wave bank; same setup as prior entry;
  single process at a time, sequential DSPs. Four scenarios, 2 s / **90,681
  native frames** each; original/native bus controls plus one unrecorded pair
  each (16 processes). Full tapes are byte-identical, including read responses,
  writes/acceptance, PC, cycle, slice and checkpoint; all 24-stream integer
  audio hashes/peaks, guest/DSP counters and serial state match exactly.

| Scenario | 24-stream hash | DSP port events | Native allocation/start/release calls | Remaining original-core calls |
| --- | --- | ---: | --- | ---: |
| Single | `58781405289897816` | 736,364 | 152 / 152 / 167 | 2,987,534 |
| Multi | `13074848859789201745` | 537,112 | 152 / 152 / 212 | 3,019,849 |
| MIDI | `3819855541230884524` | 579,064 | 9 / 9 / 9 | 3,389,358 |
| Pedal | `17487164177108066277` | 352,175 | 40 / 40 / 35 | 3,312,608 |

- The probe now reports native voice/transport execution and actual calls to
  original H8. Detailed page counts are explicitly enabled only in recording
  fixtures; normal plugin and unrecorded timing runs keep that vector empty.
  Counts are calls to the core, including interrupt arbitration, **not CPU
  time, instructions or percentage of features ported**. Toggle resets the
  current controller/counter segment. These nonzero calls demonstrate that
  this stage is not a standalone replacement.
- Preflight retained privately at `output/perf-live-controller/20261015-174041/`:
  audio, data, cycles, slices and counts matched, but all port records showed
  PC one byte behind the original. This run failed exact-tape comparison and
  is excluded from passing evidence. Fixed the shared native boundary's
  in-flight PC, strengthened the isolated oracle, rebuilt and reran all four
  scenarios successfully; no recorder normalization or comparison relaxation.
- Toggle/helper smoke ID: **`output/perf-live-controller/20261015-174405/`**,
  Pedal scenario, 2 s, two DSP workers, disable at 0.65 s/re-enable at 0.95 s;
  four processes (bus pair + unrecorded pair). Full bus tapes and all audio,
  clocks/serial counters match uninterrupted original H8. Native voice code
  executed after re-enable. Counter/page totals describe only that final
  enabled segment; this check does not measure a CPU benefit.
- Performance is **pending**: the short, single-pair smoke runner records CPU
  seconds but is only correctness evidence here. No CPU saving, DAW stability,
  real-time dropout fix or Ryzen 3 3100 improvement is claimed. User-facing
  listening/host tests and repeated performance measurements remain the next
  validation phase. No macOS build, audio or performance evidence this round.
- Remaining implementation before full migration: oscillator/filter/envelope/
  LFO/effects calculators; periodic voice maintenance and DSP feedback; all
  channel/part and remaining CC/NRPN/bend/pressure handling; clock/arpeggiator;
  preset/performance/SysEx/state/Flash loaders; main-loop, interrupt/UART
  servicing, plus native boot if required. These need original-H8 command,
  state and audio oracles; stopping H8 now would break supported behavior.
- Publication: source/tests/docs only to the existing `origin/Optimized`.
  Final commit/push identity and verification are reported in the chat.

## 2026-10-15 — Complete native KS playback, original H8 bootstrap retained

- Request clarified: retain H8 for DSP loading/initial setup, move all normal
  playback control to C++, keep it optional, compare original commands/audio/
  CPU, publish source and this diary to `Optimized`. This supersedes the
  previous entry's open boot question and incomplete playback migration.
- Source: parent **`6d9369456c92fb333cc40cac4309e384658c7ed4`**, branch
  `Optimized`, this patch. Windows x64, Core i5-13600KF, 32 GB; MSVC 19.44,
  Release/NMake. DSP pin **`44353be2f0156cac970c6ab4f467ea1998b40605`**, JIT
  and shared H8 core unchanged. No Windows settings, system timer policy,
  priorities, affinity, buffers, resampling or serial/slice targets changed.
  No system trace collection, installs or deletions in this round; private
  input ROM/Flash/wave files are read only. Test state lives in ignored output.
- New `SILVER_KS_CONTROLLER=native` / `native-cpp-runtime` takes over immediately
  after `Board::boot()` returns, before Device program restore/voice warm-up.
  Original H8 remains the default; unset and recreate the instance to disable.
  The earlier `native-midi` hybrid and recorded-fixture prototype stay separate.
  K remains unchanged. The worker-owned handoff API also supports explicit
  disable/re-enable at quiescent boundaries, retaining current register/RAM
  state so original H8 can resume without resetting the sound.
- Implemented fixed C++ flow for **13,766 playback boundaries** (12,664 new,
  reusing 1,102 existing note/voice boundaries): UART/interrupt/main-loop work;
  all channel/system/realtime MIDI, CC/NRPN/bend/pressure and SysEx routing;
  notes/sustain/voice modes/stealing; oscillator/filter/envelope/LFO/FX parameter
  calculations and DSP feedback; clock/arpeggiator; program/performance editing,
  state and user Flash handling. This consumes live input and DSP replies;
  it does not replay captured commands or call H8 for remaining callees.
- This is a **native compatibility port**, retaining the controller's register/
  RAM ABI and original clock grid. It removes runtime opcode decoding, not the
  original polling/state-machine frequency. It is not an event-batched
  OsTIrus-style controller and does not inherit the earlier tape-replay saving.
  C++ fetch boundaries account for clocks without reading immutable opcodes;
  ordinary RAM/stack bypasses device-map dispatch while actual MMIO/DSP/Flash/
  wave access remains mapped. Native TPU0 uses an O(1) counter advance with
  exact reference wrap/compare/IRQ behavior; original H8 retains its old loop.
- Guards: fingerprint the entire verified OS 2.1.02 and compare active SRAM
  executable ranges at handoff. Reject executable writes/unknown native states
  explicitly; lock the original interpreter after handoff, with **no implicit
  fallback**. The H8 object retains timing/frame storage, not interpreted work.
  Firmware-update/bootloader execution is outside the native playback range;
  loading firmware through the plugin recreates the engine and uses original
  boot. Private firmware/tables are still required externally, never embedded.
- Production VST3 and Standalone, probe and affected Processor test consumers
  rebuilt successfully in Release, including consumers of the changed Board/
  Device headers. Frozen final probe SHA-256:
  **`d2ab50c74031ae22f8084510967ee637f499836c89ebab8bac398f68ece47e13`**.
  Local evidence root: **`output/perf-native-controller/20261015-185859-final/`**.
  `source.json` records per-file source hashes/configuration; comparison metadata
  records executable identities (and the final runner hash for frozen controls).
- Differential validation passed against original H8: **440,512 boundaries**
  (13,766 x 32 full-width operand/flag sets) plus **80** interrupt/mask/tie
  entries. Registers, CCR/EXR, PC, clocks, stack bookkeeping, pending interrupts,
  data/IO access values/order/cycles/in-flight PC and final RAM match. Alternate
  samples use mapped instruction fetch and production clock-only fetch; an
  independent reference disassembler identifies only the fetch callbacks that
  intentionally disappear. Actual data/IO accesses are never filtered away.
  ROM/image changes, unknown PCs and interpreter-lock guards were exercised.
  Additional **150,000** RAM/ROM-overlay operations match mapped byte order,
  clocks and IO side effects; **101,232** timer cases match the original loop,
  including wrap, compare zero, clearing and coalesced repeated matches.
  Existing hybrid decoder, note and voice differential tests also passed.
- Preflight caught an incorrect indirect bit-store translation; fixed its
  register-indirect mask and reran the complete boundary oracle successfully.
  The first whole-Board smoke already matched commands/audio but used 3.1%
  more CPU in one Single pair; this was not a performance result. Timer/fetch/
  RAM paths above were then optimized only inside native mode and revalidated.
- Final correctness ID: **`correctness/20261015-185859`**, 8 scenarios x 2 s /
  **90,681 native frames**; bus pair plus one unrecorded pair each, 32 processes.
  Full binary bus tapes are byte-identical, including all A/B read responses,
  writes/acceptance, PC, clocks, slices and checkpoints. All 24 integer audio
  hashes/peaks, guest/DSP clocks/instruction counters and serial state match.
  Final state/Flash/MIDI-output digests and received/transmitted MIDI counts
  match as well. `state` completes one real in-memory Flash page write; input
  files are not modified. All native preparation/playback reports show actual
  post-boot interpreter calls **0**, and positive native execution.

| Passage | 24-stream audio hash | DSP port events | H8 calls after boot |
| --- | --- | ---: | ---: |
| Single | `58781405289897816` | 736,364 | 0 |
| Multi | `13074848859789201745` | 537,112 | 0 |
| MIDI | `3819855541230884524` | 579,064 | 0 |
| Sustain / voice modes | `17487164177108066277` | 352,175 | 0 |
| All CC / NRPN / bend / pressure | `1977305875963167696` | 388,261 | 0 |
| Program / performance / state / Flash | `2430842298004455044` | 362,922 | 0 |
| Clock / arpeggiator / transport | `6672608335291489240` | 308,108 | 0 |
| Voice stealing / unison | `6863044123991515266` | 654,626 | 0 |

- CPU baseline audit: a new per-step interpreter guard/counter could penalize
  the current H8 control. Therefore headline numbers use an **archived original
  EXE without that new instrumentation**, same compiler/Release configuration,
  DSP pin/JIT, inputs and exact passage. Frozen H8 SHA-256:
  **`2d6bf78483cf27ddaaeaf082f111c502a26b99d9a8aed88ddf4f7218b5355bf7`**
  (`output/perf-live-controller/20261015-155316/ksControllerProbe_test.exe`).
  Runner now accepts `--reference-executable` and records both binary hashes.
- Headline performance ID: **`frozen-reference/20261015-190920`**, sequential
  DSPs, four scenarios x 4 s / **181,362 native frames**, three interleaved
  unrecorded pairs per scenario (H8/native, native/H8, H8/native), plus separate
  trace controls, 32 processes. One fixture process at a time, no concurrent
  builds/tests. CPU is whole-process user+kernel seconds for equal work;
  bootstrap/setup, trace/report file IO are excluded. No DAW CPU-meter estimate.
  Full command tapes, audio and guest/serial counters also match the frozen
  original. That older fixture lacks state/Flash/MIDI-output snapshots; those
  are explicitly marked unavailable and verified with the current H8 above.

| Passage | Original H8 CPU s, median | C++ CPU s, median | CPU reduction | Wall s H8 -> C++ |
| --- | ---: | ---: | ---: | --- |
| Single | 2.062500 | 1.953125 | **5.30%** | 2.0982 -> 1.9907 |
| Multi | 1.968750 | 1.921875 | **2.38%** | 1.9840 -> 1.9124 |
| MIDI | 2.000000 | 1.921875 | **3.91%** | 2.0076 -> 1.9335 |
| Sustain | 1.937500 | 1.875000 | **3.23%** | 1.9710 -> 1.8822 |

- Raw CPU seconds in order, original / native: Single
  `[2.046875, 2.062500, 2.062500]` / `[1.937500, 1.953125, 2.000000]`;
  Multi `[1.968750, 2.015625, 1.968750]` / `[1.937500, 1.921875, 1.890625]`;
  MIDI `[1.953125, 2.000000, 2.000000]` / `[1.875000, 1.921875, 1.937500]`;
  Sustain `[1.968750, 1.937500, 1.937500]` / `[1.875000, 1.859375, 1.906250]`.
  These are modest local savings; three short pairs and quantized process
  timing do not establish a precise universal percentage or a dropout fix.
- Separate same-current-EXE control ID: **`primary/20261015-190218`**, same
  4 s/three-pair protocol. CPU medians H8 -> native: Single 2.062500 -> 1.921875
  (-6.82%), Multi 2.015625 -> 1.921875 (-4.65%), MIDI 2.031250 -> 1.937500
  (-4.62%), Sustain 2.000000 -> 1.875000 (-6.25%). Commands/audio/state/Flash/
  MIDI snapshots all match. Do not prefer these larger numbers over the frozen
  baseline; different runs also contain normal timing variance. The short
  2 s single-pair correctness series is not CPU evidence (it includes noisy
  reversals). Earlier **23–25% tape-replay** results are not live-controller
  performance and are not carried forward.
- Two-worker/toggle ID: **`toggle/20261015-191155`**, final native probe, Pedal,
  2 s, disable at 0.65 s /
  re-enable at 0.95 s, separate bus and unrecorded pair. Complete commands,
  audio, clocks/serial/state/Flash/MIDI snapshots match original H8. The disabled
  window executes 797,572 original-core calls as intended; calls since final
  native handoff remain **0**. This is compatibility evidence, not a CPU-saving
  claim for parallel DSPs.
- Production Processor integration: `silverLatency_test` passed with native
  control and two DSP workers, private K/KS firmware and a fresh isolated data
  directory. Both models passed synchronous host reentry, 44.1/48 kHz and
  256/512/1024 blocks, buffer profiles, state restore, asynchronous firmware
  completion, nonsilent offline rendering and return to realtime; 9 latency
  notifications each. `silverClock_test` passed host tempo/transport/sample
  offset checks. `silverEditor_test` passed geometry/navigation with actual
  K/KS firmware and the same experimental flags in its own isolated directory.
  Logs are `latency.log`, `clock-test.log`, `editor-test.log` in the evidence
  root; no user's existing data directory was used for these writes.
- Publication: source/tests/docs only to existing `origin/Optimized`; raw tapes,
  private ROM/banks, disassembly/generators, reports, audio and executables stay
  ignored. Final commit/push identity is reported in the chat. Public CI skips
  private-ROM checks and cannot reproduce those validations without inputs.
  Staged whitespace/UTF-8/Python syntax checks passed; repository audit passed
  for **590 tracked files**, with no private data or build artifacts. All C++
  source hashes still match the measured binary snapshot; only the comparison
  runner subsequently gained the frozen-reference option documented above.
- Limitations: private synthetic Flash and zero wave bank, one OS revision and
  local Intel only. No macOS build/audio/CPU validation, real VST3-host listening,
  multi-instance deadline test, full real-wave/patch corpus or Ryzen 3 3100
  result. Exact offline audio equality is not a real-time dropout measurement.
  No conclusion that the tester's stutter is fixed. Next: real host A/B tests,
  then profile native control vs DSP cost before considering any further
  controller batching; keep original H8 as the oracle.

## 2026-10-15 — Windows tester build and H8/C++ Setup selector

- Request: Windows x64 VST3 and Standalone for the Ryzen 3 3100 tester, with a
  settings switch to compare sound/CPU, preserving JIT. Source: parent
  **`9b81aaf9c3db6b9e3c3a140ccfc561b24b87a9bd`**, `Optimized` working-tree
  patch/source snapshot. Windows x64, Core i5-13600KF; MSVC 19.44,
  Release/NMake, `/O2 /MD`. Diagnostics embed that parent revision and dirty
  state; the package includes the exact corresponding local source ZIP/hash.
  DSP dependency/JIT, Windows settings, priorities, affinity, buffering,
  resampling and DSP scheduling unchanged. No firmware or user files deleted.
- **Setup -> KS controller -> H8 (original) / C++ (experimental)** now changes
  the current instance without a firmware reboot. A serialized control request
  is applied/acknowledged by the worker between host output samples, including
  when its lead is full. Audio callbacks never wait for this command or access
  its mutex. Current voices, sustain, DSP/effect state, queues and interpolation
  history stay intact. Native validation failure is returned to the UI and
  preserves running H8. Initial default remains H8; no implicit native fallback.
- The choice is saved in project state and merged new-instance preferences.
  Old projects without the new field use H8, independently of the current
  default; opening a project does not replace new-instance defaults or another
  running instance's choice. Developer environment selection remains the
  fallback when no new-instance preference exists; the old hybrid path remains
  available. K is unchanged and its selector is disabled.
- Changing mode stops an active diagnostics session with
  `stop_reason=controller_changed`. The recorded session retains its old
  backend while the top-level report shows the current backend; new sessions
  record the applied backend. No mixed-mode CPU/session comparison. Corrected
  the existing Setup MIDI-channel/buffer overlap and visually inspected the
  final Setup capture; the new selector is contained and readable.
- Evidence root: **`output/tester-controller-20261015-193117/`**. Final builds
  passed: `silverK_VST3`, `silverK_Standalone`, `silverController_test`,
  `silverEditor_test`, `silverLatency_test`, `silver_vst3_check`. Affected header
  consumers were recompiled; final source/artifact SHA-256 values are retained
  in local `final-build.json`. Standalone EXE SHA-256:
  **`5258110d54912cb524a93a7445be0b3593e651fdd946ddbb16ab358ff16427da`**;
  VST3 module SHA-256:
  **`e4fd2a2b065e1887248dcbe24ee05393cc41fd4acf3dc4c2de4f3564f4443ee7`**.
- `silverController_test` passed: five real UI handoffs during held notes and
  sustain, **61,440 exact float samples** (15,360 host frames x 4 buffer channels)
  against an unchanged H8 Processor, nonsilent output, zero offline underruns,
  no firmware-revision/latency change. Also checked instance isolation,
  diagnostic segmentation/backend tags, global preferences, native project
  restoration, old-project H8 default and stale-instance preference protection.
  A separate Device with one changed inline LCD byte in its private in-memory
  OS copy booted H8, rejected native fingerprint validation, acknowledged the
  failure and kept H8 valid. No original input file was changed.
- Preflight: the new test EXE initially exhausted its Windows stack because
  its large Device fixture was a local variable (exit `0xc00000fd`). Moved
  large fixtures to the heap, rebuilt and reran successfully. This failed
  test-process run is excluded from passing evidence; no system change was
  involved. Only final passing builds were used for packaging.
- Actual final VST3 module loaded by `silver_vst3_check` in separate original/
  native project states: two 4 s offline renders at 48 kHz / 256, **192,000
  stereo frames** each, zero underruns and nonzero signal, 512-frame reported
  latency in both modes. Both resulting 24-bit WAVs are byte-identical,
  SHA-256 **`e5d814ae1e3a270a03198e7406967e4774528989ece98f595f60f131c81e8361`**.
  Re-read the returned VST3 states to verify mode 0 / 1, not just the same
  backend accidentally tested twice. Raw audio/state/screenshots remain private.
- Final editor test passed with actual K/KS OS, correct selector visibility/
  availability and non-overlapping settings. Final latency/reentry test passed
  for K and KS with C++ selected and two DSP workers: rates/blocks/profiles,
  sync/async firmware loads, state restoration, nonsilent offline audio and
  return to realtime; 9 latency notifications per model. All test data writes
  were isolated under the evidence root, not the user's active data directory.
- Packaging: separate **`KS-Controller-Test-20261015`** Windows x64 variant,
  VST3 bundle + Standalone, Russian test instructions, corresponding source,
  licenses, manifest and SHA-256 checksums. The standard release packaging path
  is preserved; experimental variants get a distinct archive name/channel.
  Original firmware, factory/wave banks, private captures/reports and build
  intermediates are excluded. Only authored synthetic init Flash and zero Wave
  substitute are included as in the regular package; testers keep their own
  existing ROM/data files. Products retain the same plugin identity/version,
  documented in the tester guide rather than installing duplicate plugins.
- No new CPU percentage is claimed from these functional checks; the previous
  2.4–5.3% result belongs to the earlier exact binary and short Intel workload.
  Next: tester's matched H8/C++ passages with unchanged DAW/buffer/patch and
  separate summary JSON reports, then repeat CPU-meter comparison with
  diagnostics off. Ryzen stability/CPU, real DAW deadlines, multiple instances,
  real wave/patch corpus and macOS compilation/testing remain pending.

## 2026-10-15 — macOS H8/C++ validation and OsTIrus core scheduling

- Source: **`f52e397`**, branch `Optimized`, plus this patch's test-only macOS
  CPU measurement and documentation. Apple M1 Pro, 8 physical/logical cores,
  16 GiB; macOS 27.2 (`26B5101f`), AppleClang 21.0.0.21000334, Release arm64,
  deployment target 12.0. No production controller/DSP, JIT, guest clocks,
  slice/serial ordering, wait policy, priorities, buffers or core placement
  changed. Per-process test settings are isolated; installed plugins and user
  preferences were not replaced. No Windows/Ryzen result is inferred.
- Build: `/private/tmp/Silver-K-optimized-mac-20261015`, Ninja, `-j 6`.
  Cached JUCE/AsmJit/DSP sources were compared file-by-file with their downloaded
  pinned archives: 3,593 / 197 / 8,734 files, **no differences**. The existing
  build-local JUCE SDK compatibility patch still applies. DSP pin remains
  `44353be2f0156cac970c6ab4f467ea1998b40605`. Standalone, VST3, AU, native/hybrid
  controller probes, DSP equivalence/workers, UI-switch, realtime and wrapper
  checks built. Local tested bundles are copied to ignored
  `build/optimized-mac-2026-10-15/` (Standalone and Plugins/AU,VST3).
- Test change: `ksControllerProbe_test` now measures process user+kernel time
  with `getrusage(RUSAGE_SELF)` on macOS; bootstrap/preparation, trace/report IO
  remain outside the measured render. The runner records OS/architecture and
  emits CPU changes on Mac. Other platforms' unavailable CPU results remain
  excluded. This changes measurement only, not sonic behavior.
- Private evidence root: **`output/perf-mac-20261015/`**. `source.json`,
  `binaries.json` and `dependencies.json` record source, toolchain, binary and
  archive identities. Raw tapes, reports, states, audio and inputs remain
  local/ignored. Probe SHA-256:
  `7a7e3de6b0660d206c734bc7fbd20b14697cddde1c6fc611f6c60d0d19f2105b`.
- Eight direct unit/differential checks pass: **440,512** native runtime
  boundaries and 80 interrupt/mask/tie entries, **101,232** timer cases,
  **150,000** mapped/native memory operations, existing hybrid MIDI/note/voice
  differentials, tape serialization/guards, DSP handoff/failure/shutdown,
  diagnostics and host clock. Native runtime guards reject interpreter fallback.
- Full-Board correctness ID **`correctness/20261015-201440`**: all eight
  scenarios (Single, Multi, MIDI, pedal/voice modes, all CC/NRPN/bend/pressure,
  state/Flash, clock/arp, stealing/unison), 2 s / **90,681 native frames** each,
  separate bus pair plus one unrecorded pair, 32 processes. Complete tapes are
  byte-identical, all 24 integer audio streams, guest/DSP/serial counters and
  state/Flash/MIDI digests match. Audio hashes/event counts also equal the
  recorded Windows scenarios above. Every native report has zero actual H8
  calls after boot and since handoff, with positive native runtime steps.
- UI/Processor check: **61,440 exact float samples across five live H8/C++
  switches**, held notes/sustain, no firmware reboot, latency change or underruns;
  diagnostics segmentation, instance/default preference isolation, native and
  legacy project restore and rejected-ROM handoff all pass. This correctness
  check ran during the last build steps; no CPU conclusion is taken from it.
- CPU ID **`cpu/20261015-201636`**: sequential DSPs, four scenarios x 4 s /
  **181,362 native frames**, three alternating H8/native pairs per scenario,
  separate trace pair. One process at a time after compilation finished;
  commands/audio/guest/state/Flash/MIDI equivalence also passes. All CPU values
  below are medians of whole-process user+kernel seconds for equal rendered work.

| Passage | Current H8 CPU s | C++ CPU s | Change |
| --- | ---: | ---: | ---: |
| Single | 1.978352 | 1.893731 | -4.28% |
| Multi | 1.968846 | 1.874458 | -4.79% |
| MIDI | 1.980986 | 1.910627 | -3.55% |
| Sustain | 1.923952 | 1.875733 | -2.51% |

- Interpretation: modest local savings versus **the current instrumented H8**,
  not a frozen original Mac reference. H8 guard/counter overhead can affect
  attribution. Power/thermal/background load were not controlled; three short
  pairs do not establish a universal percentage, DAW CPU saving or dropout fix.
  The earlier recorded-tape replay saving is not used here.
- Parallel compatibility ID **`parallel-toggle/20261015-201845`**: Pedal, two
  DSP helpers, 2 s, disable at 0.65 s / re-enable at 0.95 s. Full tapes,
  audio/state/Flash/MIDI and counters match; original H8 executes only during
  the intended disabled window (797,572 calls), with zero calls since final
  handoff. Separate `ksDspEquivalence_test` passes **65,536 frames x 24 streams**,
  Single, sustain, program change and four parts, hash `3852514813786200426`.
  Correct offline output does not establish realtime throughput.
- Actual built wrapper checks: VST3 and AU, H8 and C++, **four 4-second
  realtime captures**, 48 kHz / 256 frames, sequential DSPs, identical saved
  sound and controller-specific state. All pass with finite audible output,
  peak 0.0151513 and **zero underruns**. VST3 onset is 722 samples in both modes;
  reported latency is 512 samples / 10.6667 ms. These are isolated JUCE/native
  AU wrapper hosts, not Logic/Cubase or a physical driver/listening test.
- Realtime ID **`realtime/`**, reproducer `run_realtime_matrix.py` in the same
  ignored evidence root: one KS instance, 48 kHz, 12 s fixture, rapid eight-note
  chords, state reads every 100 ms, redundant prepare at 6 s, Summary diagnostics,
  fresh settings per process. At 128 frames/Balanced, three interleaved runs per
  controller/backend; at 64 frames/Low, one run each. CPU placement is ordinary
  OS scheduling. No compiler or other DSP fixture ran concurrently.

| Block/profile | Controller | DSP mode | Passed | Missing frames per run | Median worker wall/audio ratio |
| --- | --- | --- | --- | --- | ---: |
| 128 / Balanced | H8 | Sequential | 3/3 | 0, 0, 0 | 0.4983 |
| 128 / Balanced | C++ | Sequential | 3/3 | 0, 0, 0 | 0.4823 |
| 128 / Balanced | H8 | Two helpers | 0/3 | 575415, 575350, 575387 | 2.0378 |
| 128 / Balanced | C++ | Two helpers | 0/3 | 575287, 575414, 575342 | 2.0129 |
| 64 / Low | H8 | Sequential | 1/1 | 0 | 0.4955 |
| 64 / Low | C++ | Sequential | 1/1 | 0 | 0.4787 |
| 64 / Low | H8 | Two helpers | 0/1 | 575311 | 2.0025 |
| 64 / Low | C++ | Two helpers | 0/1 | 575369 | 1.9854 |

- **Mac two-helper realtime validation fails**, in both controller modes.
  All sequential captures have zero startup and steady misses, nonzero output
  and matching onset/peak within each profile. All two-helper captures have
  startup and steady losses and zero delivered peak; worker wall/audio >1
  establishes insufficient fixture throughput. Coordinator wall ratio includes
  both-helper waits and is not summed CPU utilization. Synchronization/wakeup
  overhead is a hypothesis, not isolated proof or a demonstrated core-placement
  problem. Keep `SILVER_KS_DSP_THREADS` unset/`1` on this Mac; default stays
  sequential. The suite's aggregate nonzero exit records these eight failures,
  not a controller-equivalence failure. No automatic enablement or scheduler
  workaround was introduced.
- OsTIrus source check, **tag 2.2.25** / DSP pin
  `e643fa89f0c11147963b315c0bd8afb283bba468`: re-read `dspSingle.cpp`,
  `dspthread.cpp`, `threadtools.cpp`; local snapshots/hashes at
  `output/research/ostirus-2.2.25-mac-20261015/`. TI/TI2 create one worker per
  DSP; inspected worker creation/priority/execution assigns **no physical CPU
  IDs/hard affinity**. The OS schedules cores; macOS QoS and Mach time-constraint
  policy influence scheduling, not fixed DSP A/B core ownership. Snow has one
  DSP. Silver-K helpers likewise do not pin cores. No OsTIrus runtime benchmark
  or actual core-placement trace was collected; do not extrapolate to every
  release or host-imposed affinity. Primary sources are linked in
  `PERFORMANCE_DIAGNOSTICS.md` and the earlier source review.
- Next: separately profile/design Mac helper dispatch and barriers before
  recommending that experiment; preserve default sequential execution and exact
  offline audio as the control. Real DAW/physical-device listening/deadline
  behavior, four-part realtime stress, other patches/waves and Ryzen remain
  separate validation. Public source audit and whitespace checks run after the
  documentation update; private assets/audio/binaries stay untracked.

## 2026-10-15 — Mac DSP handoff profiling and coordinator/DSP A execution

- Authorized follow-up: investigate the Mac two-DSP slowdown and try a fix
  without changing sound/JIT. Same M1 Pro/macOS/compiler/pins as the preceding
  Mac entry; source `f52e397` plus the local Mac measurement patch and this
  change. Frozen pre-fix executables and `ksDspWorkers.cpp` are retained at
  ignored **`output/perf-mac-dsp-sync-20261015/before/`**. The parent evidence
  root preserves rejected experiments; **`one-helper/` is the final candidate**.
- Cause established for the initial slowdown: at each original ~128-controller-
  cycle boundary, the coordinator dispatches two very small tasks and waits
  for both. The 256-probe spin window is too short for this Mac workload;
  `pthread_cond_wait`/`pthread_cond_signal` dominate the sampled handoff path.
  This is not evidence that hard core affinity is required.
- Before profile: same 12 s / 48 kHz / 128 / Balanced / H8 fixture, Detailed,
  state reads off, plus a separate four-second `sample` capture. Mean sampled
  DSP A/B work is **1.753 / 1.917 us**, but dispatch + both-DSP barrier is
  **10.784 us**. The coordinator has 847/1410 samples in condvar waits and
  370/1410 in notifications; DSP A/B have 1152/1410 and 1232/1410 in waits.
  Sampling perturbs timing and includes blocked/preempted time; this is not
  a precise CPU-percentage breakdown. Raw profile is `sample-before.txt`.
- ROM-free isolated benchmark: `handoff.cpp` in the evidence root, 100,000
  submissions, three alternating runs, identical integer result hashes. Empty
  jobs take median **0.596544 s** with the original wait, versus **0.030467 s**
  with a longer bounded spin. With 1024 dependent integer operations per DSP,
  medians are **0.958645 -> 0.284512 s**; process-reported context switches fall
  from roughly **306,000 to tens**. This separates dispatch cost from firmware,
  JIT, resampling and host buffers; it is not a synth speedup factor.
- Rejected intermediate: retain two helper threads, increase Mac spin probes
  from 256 to 8192. Exact comparisons and all 12 paced audio checks pass, but
  three runnable realtime threads waste CPU at the barrier. Paced H8 controls
  consume 16.25–20.57 CPU s per 12 s of delivered audio in the completed
  controls, and ordinary VST3 host runs still lose frames. Results remain in
  the parent evidence root. This is not the final implementation.
- Final Mac implementation: dispatch DSP B to its persistent helper, execute
  DSP A on the already existing audio/coordinator worker, then join B before
  any next H8/control step, host-port access, serial-slot IO or snapshots.
  Both DSP computations overlap, as verified by the ownership/overlap test;
  this is not a sequential fallback. The command and completion polling on
  Mac is bounded at **8192 probes**, then uses the existing condvar sleep.
  Idle threads still park. There is no new OS API/deployment requirement.
- Preserved: exact virtual slice/target/cycle boundaries, DSP firmware and JIT
  pin/configuration, serial ordering, MIDI/state semantics, resampling, buffers,
  thread priorities and ordinary OS core scheduling. Audio callback unchanged.
  Default remains sequential. Non-Apple builds retain two helpers and the
  original 256-probe window; Windows was not rebuilt in this Mac round.
  The legacy `two-workers-slice-barrier` backend identifier is retained for
  compatibility; on Mac it now means coordinator/DSP A plus one DSP B helper.
- Worker test passes 10,000 payload/completion handoffs, actual overlap and
  platform-specific ownership, failures in A/B/both with complete draining,
  recovery, 20 parked idle resumes and idle shutdown. Exact sequential/parallel
  Board test passes **65,536 frames x 24 streams**, including Single, sustain,
  program change, four parts, clocks/serial/state/Flash;
  hash `3852514813786200426` is unchanged.
- Final correctness ID: `one-helper/correctness/` (timestamped subdirectory),
  eight passages x 2 s / **90,681 native frames**, original pre-fix H8 reference
  versus final native mode, both with parallel DSP selected. Complete bus tapes,
  all 24-stream audio hashes/peaks, guest/DSP/serial counters, state/Flash/MIDI
  digests match. Native execution reports zero H8 interpreter calls after boot.
  This validates the tested input/scenario set, not every possible firmware/patch.
- Final paced captures: `one-helper/smoke/` + `one-helper/repeated/`, one KS
  instance, 48 kHz, eight-note fixture, state reads every 100 ms and redundant
  prepare at 6 s, Summary diagnostics, fresh per-process settings. Three runs
  per controller/profile: **12/12 pass with zero startup/steady misses**.

| Controller | Block/profile | Median active worker wall/audio | Missing frames, all three runs |
| --- | --- | ---: | --- |
| H8 | 128 / Balanced | 0.35994 | 0, 0, 0 |
| C++ | 128 / Balanced | 0.35371 | 0, 0, 0 |
| H8 | 64 / Low | 0.36403 | 0, 0, 0 |
| C++ | 64 / Low | 0.35359 | 0, 0, 0 |

- Onset/peak and plugin latency match sequential controls within each profile:
  14.7292 ms / 0.146099 / 10.6667 ms at 128/Balanced, and
  9.52083 ms / 0.123063 / 5.33333 ms at 64/Low. State reads on their ordinary
  background thread can still be slow (maximum **531.762 ms** in this matrix);
  do not claim a general GUI/state-serialization latency improvement.
- CPU conclusion uses **paced** captures `one-helper/paced-cpu/`, not an
  unbounded realtime-priority offline loop. Same binary, H8, 48 kHz/128/Balanced,
  three alternating pairs, Summary, state reads off; all six pass, with equal
  12 s of delivered audio and matching onset/peak. `silverRealtime_test` now
  reads `getrusage(RUSAGE_SELF)` before/after playback, outside the callback;
  bootstrap, firmware preparation and report IO are excluded. It prints
  `process_cpu_seconds` and `process_wall_seconds` (CPU unavailable = -1 off Mac).

| Mode | Median whole-process CPU s / 12 s audio | Median active worker wall/audio |
| --- | ---: | ---: |
| Sequential | 6.83596 | 0.52874 |
| Final Mac parallel | 9.09547 | 0.36486 |

- Interpretation: ~31% less active coordinator wall time than the same-binary
  sequential control, at **33.05% more total CPU**, including the helper.
  This is throughput/headroom, not an energy/CPU saving. Unpaced probe timing
  with a normal-priority coordinator differs from production; an attempted
  continuous audio-priority offline cost run was also unstable and stopped.
  Neither is used as a production performance headline; raw trials are retained.
- Final separate Detailed + `sample` profile: dispatch/barrier mean
  **1.739 us** (includes inline DSP A), sampled A/B **1.265 / 0.998 us**.
  Same sampling/OS-preemption caveats as before; do not add overlapping timers.
  Profiled fixture passes with nonzero audio. These sampled timings and the
  isolated ablation support the handoff diagnosis without assigning CPU IDs.
- Live UI/Processor check with parallel mode passes **61,440 exact float
  samples across five H8/C++ switches**, held notes/sustain, diagnostic-session
  isolation, native/legacy state restore and firmware guard rejection.
- Actual AU checks (H8 and native, parallel, 48 kHz/256) both pass, nonzero
  output, zero underruns. Ordinary-priority VST3 CLI-host results remain mixed:
  initial H8/native runs lose 2190/9854 frames; repeats lose 5672 in one H8 run,
  with the other three parallel repeats and both sequential controls passing.
  Main-thread callback maxima in failing captures reach 24.877 ms. Preserve
  this limitation rather than describing the entire wrapper suite as passing.
- The VST3 checker gains an explicitly opt-in Mac
  `SILVER_TEST_HOST_AUDIO_PRIORITY=1` model, requesting the same audio priority
  and period as the existing realtime fixture and checking setter success.
  It prints the selected host model. Its default ordinary-priority mode stays
  available; production plugin priorities are unchanged. Audio-priority capture
  results are stored separately in `one-helper/audio-priority-results.json`.
- Final explicitly audio-priority VST3 checks pass **6/6** (three alternating
  H8/native pairs, 48 kHz/256, parallel mode), zero underruns, identical peak
  0.0151513, onset 722 samples and latency 512 samples. All reports confirm
  `host_audio_priority=1`. This supports a caller-scheduling distinction; it
  does not erase the ordinary-priority failures or establish real-DAW behavior.
- OsTIrus comparison: tagged TI/TI2 workers execute independently through audio
  queues and host-side output processing; they do not have KS's H8-clocked
  two-DSP barrier at every short slice. The final KS change reduces handoffs
  and uses the waiting coordinator for DSP work while preserving that required
  barrier. No OsTIrus JIT settings, larger guest slices, mixing or firmware
  algorithms were imported. Tagged source snapshot: `ostirus-dspMultiTI.cpp`
  in the evidence root; primary references remain in PERFORMANCE_DIAGNOSTICS.
- Default remains opt-in/off; no installed application or plugin is replaced.
  Final signed local bundles: `build/optimized-mac-dsp-sync-2026-10-15/`;
  source/binary identities and copy/signature checks are in the final candidate's
  `one-helper/identity.json`.
  Real DAW/physical driver, other Macs (including Intel), multiple instances,
  broader patches/waves and Windows runtime remain unverified. Keep ordinary
  and audio-priority host results distinct. Source audit, diff check, copied
  bundle signatures and binary identities are checked before handoff.

## 2026-10-15 — Mac CPU objective and OsTIrus comparison preparation

- User clarification: optimize aggregate CPU consumption and compare KS with
  OsTIrus on this machine. Reduced coordinator wall time alone is insufficient.
  Sound and JIT preservation remain required; no new engine change in this entry.
- Same M1 Pro and local Optimized revision/configuration as the preceding entry.
  Existing paced controls establish sequential H8 median 6.83596 CPU s / 12 s
  audio and parallel H8 median 9.09547 CPU s / 12 s audio. The latter fails the
  clarified CPU objective even though it improves completion wall time.
- Read-only inventory: installed OsTIrus.component reports version 2.2.16 and
  universal x86_64/arm64 executable. Its runtime model, firmware, settings and
  CPU consumption have not yet been measured. The 2.2.25 source study must not
  be represented as profiling this installed 2.2.16 binary.
- Next measurement: use one native-arm64 AU host for installed OsTIrus and the
  tested KS build; record model/firmware, native and host rates, DSP clock,
  buffers/latency, voice/part counts, patch complexity and effects. Compare
  paced whole-process user+system CPU after warm-up, idle and note workloads,
  repeated alternating runs and audio deadline failures. Report the instruments'
  different firmware workloads rather than claiming equal instruction work.
- Next optimization decision: profile sequential KS to distinguish controller,
  DSP execution and host/peripheral/synchronization overhead; prioritize changes
  that lower aggregate CPU while retaining exact command/audio/state behavior.
  OsTIrus batching and its different JIT configuration are comparison subjects,
  not authorization to change KS JIT or guest timing. Parity is an objective,
  not an established achievable result.

## 2026-10-15 — Matched Mac AU total CPU: OsTIrus and sequential KS C++

- User objective: reduce aggregate CPU at unchanged sound/JIT, rather than
  shorten completion wall time by adding compute threads. Branch Optimized,
  `f52e397` plus the preceding local Mac work and this small catch-up change.
  Same M1 Pro/macOS 27.2/AppleClang 21/Release arm64 and pinned dependencies as
  the preceding Mac entries. Frozen baseline bundle/probe/realtime binaries
  and edited-file originals are in ignored `output/perf-mac-cpu-20261015/before/`.
- OsTIrus admission: installed AU 2.2.16 universal executable, arm64 execution;
  isolated bundle copy loads the private installer image found in the supplied
  desktop archive. Saved state identifies **TI2**, DSP clock defaults to 100%
  (no DSPC override), host/native rate 48 kHz, clock log 133.12 MHz. Exact local
  tag source is now retained: `b0fe0cb37d1ca10f8e09797423ef7b889d413aa1`, DSP
  gitlink `554f0fabdc97040517ea1d8d25e48c7ce0a5f0e0`. This source review is
  separate from the earlier 2.2.25 review. The stock installed executable is
  measured; the local tag is not proof that its build is otherwise unmodified.
- Measurement fixture: new explicit `silver_au_cpu_benchmark` target loads one
  AU per process using its bundle factory. Same host executable, 48 kHz/256,
  audio priority and main message loop for every case. Setup/boot, four seconds
  of voice-path warm-up, state reads and parameter enumeration are outside each
  eight-second CPU window. Process user+system CPU includes every thread and
  the host. Three alternating full runs per case; 36 measurement windows and
  288 seconds of measured audio. No concurrent agent builds/tests/profiles.
- Workload: single part, 0/1/8/16 held notes, notes 48 upward, velocity 100,
  polyphony/unison/arp explicitly set, basic oscillator and effects sends off.
  All requested AU values are checked against final effective parameter values.
  KS uses the native C++ controller, sequential DSPs, original 162.5 MHz clocks,
  synthetic zero wave bank and 45,340.4018 Hz native rate. KS latency is 512
  host frames; OsTIrus reports 472. Synth algorithms, firmware work, clocks and
  internal rates differ. Held keys are controlled input, not a proof of equal
  firmware instruction counts or identical voice algorithms.

Median aggregate CPU, **100% = one CPU**, with min–max across three runs:

| Held keys | OsTIrus TI2 | KS C++ before | KS C++ after | KS median change |
| --- | --- | --- | --- | --- |
| 0 | 41.52% (38.03–53.16) | 55.93% (53.34–56.51) | 49.41% (49.12–49.81) | -11.66% |
| 1 | 51.37% (49.77–54.67) | 53.56% (53.02–55.37) | 49.44% (49.36–50.96) | -7.70% |
| 8 | 55.36% (51.47–57.91) | 53.00% (52.70–53.30) | 49.75% (49.45–53.66) | -6.12% |
| 16 | 50.24% (46.47–58.40) | 53.26% (53.15–54.19) | 49.78% (49.66–53.24) | -6.54% |

- Interpretation: active-note consumption is already of the same order as
  OsTIrus on this Mac, and the change reduces median aggregate KS CPU. OsTIrus
  is cheaper in the median idle case. Variability matters: the third KS
  eight-note pair regresses despite the lower overall median; this is not a
  guarantee of a saving in every short window. OS placement/frequency effects
  were not independently measured, so they are hypotheses about the spread.
- Initial sequential KS C++ stack sample: about 70% of 1694 render-stack
  observations lie on DSP catch-up paths, about 17% on native control, with
  the remainder in the board loop/serial work. These are sampled stack
  proportions, not precise process CPU attribution. Detailed timers include
  preemption; the profiled native fixture loses 27 steady frames and 77 startup
  frames. These profiled captures are excluded from the unprofiled CPU table.
  One exact two-second passage records 6,220,388 native controller boundaries,
  zero post-boot H8 interpreter calls: compatibility polling still has a cost.
- Separate final stack captures retain the DSP-dominated KS path and OsTIrus's
  two DSP threads with direct native controller servicing. The changed KS
  profile loses 5398 steady frames under `sample`; the OsTIrus profile has one
  5.819 ms callback (5.333 ms budget). Neither is a passing realtime capture or
  a CPU-table input. Additional unprofiled dynamic-note controls are recorded
  separately in `realtime-final-index.json` to distinguish profiling effects
  from normal operation.
- Unprofiled changing-note control: two frozen and two changed native KS runs,
  12 s/48 kHz/128/Balanced, state reads off, Summary diagnostics. All four pass
  with zero startup/steady lost frames and unchanged peak/onset/latency. Median
  aggregate CPU is **6.39149 -> 5.949865 CPU s per 12 s audio (-6.91%)**.
  These are separate from the AU table's held-note measurements.
- Accepted implementation: `DSP::catchUpToCycle(target)` contains the identical
  `discardAudio(); step(1)` sequence and target comparison formerly in Board.
  This keeps the hot loop and step implementation in one translation unit,
  allowing compiler inlining and removal of wrapper call/return overhead.
  DSP order, every controller/serial boundary, instruction count and JIT
  configuration stay unchanged. No new allocation, lock, wait or thread.
- Correctness: eight two-second frozen-original-H8 versus changed-native
  scenarios match complete DSP bus tapes, 24-stream integer audio, guest
  counters, controller state, user Flash and MIDI; zero native interpreter
  fallback. Actual AU float hashes and peaks match **all six KS runs** in
  every phase. All 24 KS windows have zero lost frames; all 36 windows have
  finite output, audible note phases and zero over-budget callbacks. OsTIrus
  does not expose the KS lost-frame state counter: its `-1` is unavailable,
  not a claim of zero internal lost frames.
- Shared-path regression check: the existing sequential/parallel DSP test also
  passes 65,536 native frames x 24 streams, exact clocks/serial/state/Flash,
  retaining the frozen known audio hash `3852514813786200426`.
- Raw identity/results: ignored `output/perf-mac-cpu-20261015/`, including
  `baseline-identity.json`, `comparison-index.json`, `comparison-summary.json`,
  `correctness/`, exact 2.2.16 source manifest and separate stack captures.
  Private state/parameter/firmware data stays local.
- Final Release arm64 Standalone, VST3 and AU are built and copied to ignored
  `build/optimized-mac-cpu-2026-10-15/`, with verified binary copies and ad-hoc
  signatures. The rebuilt VST3 passes its ordinary-priority realtime CLI host:
  zero underruns, 512-frame latency, unchanged 0.0151513 peak and 722-sample
  onset. AU was exercised by the comparison host itself. No installed plugin
  was replaced. Repository audit and `git diff --check` pass; full private
  source/binary/input identities are retained alongside the reports.
- Limitations: one Mac, three short repeats, basic single-part patches and no
  physical driver/DAW or Windows validation. Broader wave banks, effects-heavy
  and multipart CPU comparisons remain unmeasured. This comparison establishes
  the tested consumption scale, not universal KS/OsTIrus speed parity.

## 2026-10-15 — Mac FX/multipart and native-controller follow-up

- User scope: compare OsTIrus and KS with effects and in Multi on this Mac,
  investigate further aggregate CPU savings with sound/JIT unchanged, and
  assess removing the remaining controller emulation overhead.
- Same machine, Release arm64, Optimized `f52e397` plus the preceding local
  changes. Current accepted KS AU/probe and edited header are frozen under
  ignored `output/perf-mac-fx-multi-20261015/before/`; the RAM-read candidate
  and common comparison host are frozen separately. No shipping default,
  DSP/JIT settings, guest clocks, buffers or production priorities changed.
- Fixture extended for ordered MIDI/SysEx setup, four-channel part probes,
  a specified held-note phase and measured post-release FX tails. Explicit
  single dry/FX and four-part dry/FX cases use 16 total held notes and the
  original 48 kHz/256 host conditions. Parameter values, raw private setup,
  host identity and state are retained in the ignored evidence root.
- Admission: KS and TI2 each sound on all four isolated channel probes. KS
  Multi uses four active parts/channels 1..4; TI2 returned Multi state confirms
  the same channels, four enabled parts and twelve disabled parts. An initial
  TI2 probe was silent because USB routing targets a different AU bus; this
  capture was rejected, outputs corrected to analog stereo bus 1 and admission
  repeated successfully. Both wet preflights have nonzero settled release
  tails. These four-second preflights are not headline CPU comparisons.
- Rejected candidate: recognize a contiguous native RAM/ROM span once for a 16/32-bit
  read, retain every original bus-clock update, and use the existing byte path
  at IO/overlay/address-wrap boundaries. Memory/IO/clock tests pass 150,100
  operations, including new crossing/unaligned/error cases. All 440,512 native
  boundaries and 80 IRQ entries also match H8. Four first-round matched cases
  sum to 15.479632 baseline versus 15.419660 candidate CPU seconds (-0.3874%);
  individual changes are -1.97%, +2.09%, -1.66%, -0.014%. All four audio/tail
  hashes match. This is too small and mixed to accept; restore the header
  byte-for-byte. A second-round candidate part probe was silent before the CPU
  window and was rejected. Separate diagnostic host runs of baseline and trial
  both pass with identical audio; the admission failure is not reproduced and
  its cause remains unestablished. Do not attribute it to the candidate alone.
  The candidate matrix was stopped; its four pilot measurements and one failed
  admission are not pooled into the primary comparison.
- Primary comparison: frozen accepted sequential KS C++ versus installed
  OsTIrus 2.2.16/TI2, same common host, three alternating repeats per case,
  eight seconds per measurement: **24 windows / 192 seconds measured audio**.
  All requested AU parameters and all 24 returned Single/Multi states are
  validated. Single has one part/16 notes; Multi has four parts/four notes each.
  Basic saw programs, no unison/arp, zero amp release. Wet cases enable delay,
  reverb and chorus, including nonzero settled release tails on both synths.
  Synthetic zero KS wave bank, native rates/clocks and differing synth/FX
  algorithms remain as in the preceding entry; this is equal input/feature
  classes, not equal instruction work or matched FX loudness.

Median aggregate CPU, **100% = one CPU**, with min–max across three runs:

| Passage (16 total held keys) | OsTIrus TI2 | KS C++ sequential |
| --- | --- | --- |
| Single, dry | 52.04% (42.47–58.35) | 49.22% (48.00–49.57) |
| Single, delay/reverb/chorus | 51.71% (42.77–60.12) | 49.10% (49.07–50.36) |
| Four-part Multi, dry | 64.70% (45.92–72.63) | 48.04% (48.02–49.02) |
| Four-part Multi, delay/reverb/chorus | 63.30% (52.90–65.00) | 48.21% (47.16–53.15) |

- All 24 main measurement windows are finite, audible and have zero callback
  deadline failures; all 12 KS windows have zero lost frames. OsTIrus's internal
  lost-frame counter is unavailable, not measured zero. Initial part admission
  and warm-up are outside these windows and can include slow startup callbacks.
  All three KS repeats match their scenario's float audio/tail hashes. Wet KS
  settled-tail RMS is 0.004192 Single / 0.002868 Multi, versus about 0.00005905 /
  0.000046–0.000048 TI2. These confirm active effects, not equal reverb level.
  The dry/FX CPU differences are within run variability; do not infer free or
  CPU-saving effects. OS core placement/frequency was not measured separately.
- Separate Multi+FX stack samples: 110 of 140 KS render-stack observations
  (~79%) are DSP catch-up, about 22 (~16%) native control, the remainder mostly
  serial/board work. These coarse stack proportions are not exact process CPU
  attribution. Profiling disrupts realtime: KS loses 2861 frames and TI2 has a
  6.090 ms callback against a 5.333 ms budget. Both profiled runs are excluded
  from the primary table. TI2 has two DSP threads and direct native controller
  servicing; exact tag 2.2.16 source snapshots remain in the earlier evidence
  root. Its missing local DSP gitlink object limits inspection of that version's
  DSP-library internals.
- New four-second `fx` regression: four parts, 16 notes, changing delay/reverb/
  chorus sends, sustain, Note Off and release tails. Current original-H8 versus
  native C++ compares the complete 739,356-event bus tape byte-for-byte, all
  24 integer streams (181,362 frames, hash `6021595737639781281`), guest/serial
  counters, controller state, user Flash and MIDI output. All match. Native
  executes 10,750,110 compatibility boundaries and 22,196 IRQ entries with zero
  H8 calls after boot/handoff. An initial fixture omitted sustain and failed its
  own coverage assertion; sustain events were added and the full test rerun.
  Memory (150,100 operations) and runtime (440,512 boundaries/80 IRQ entries)
  checks pass again after the rejected header was restored.
- Controller-cost diagnostic: three alternating original-H8/native/recorded-
  replay runs for each four-second offline passage, sequential DSPs; whole-
  process user+system render CPU excludes boot/preparation/recording/report IO.
  Multi medians are **1.894003 / 1.792881 / 1.458515 CPU s**; FX medians are
  **1.955012 / 1.824031 / 1.465334 CPU s**. Replay is 18.65% / 19.67% below
  native. Audio, guest DSP cycles/instructions and serial counters match in
  every run; replay verifies every recorded port event. Replay bypasses live
  decisions and has no live controller state equivalence: it is a bounded cost
  diagnostic, not an implemented controller, strict ceiling or promised saving.
  H8/native timing here is same-current-binary, not a new frozen-baseline claim.
- Controller finding: native mode already locks out all H8 interpreter calls
  after bootstrap. It retains H8-compatible PC/register/RAM, instruction
  boundaries, timer/IRQ and port scheduling. Replacing that compatibility loop
  with semantic MIDI/voice/parameter/clock jobs is a different architecture
  from changing the Setup default. It requires preserved bus/audio/state
  behavior and an explicit state bridge for live reference switching.
- Recommendation: a semantic event-based C++ controller is a plausible next
  CPU project. Start with timed HDI transport/background polling; then MIDI,
  voice allocation and parameter jobs, original tables/math, exact guest
  timestamps, timer/IRQ and readiness ordering. Preserve DSP firmware/JIT and
  a coherent state bridge to the selectable H8 reference. Gate each stage on
  full command/audio/state comparisons (FX, Multi, sustain, stealing, clock,
  save/restore) and repeated aggregate CPU. Simply changing the Setup default
  or removing the one-time H8 boot cannot remove steady compatibility work.
  DSP remains the main cost; this experiment does not support a 2x promise.
- Evidence: ignored `output/perf-mac-fx-multi-20261015/`, especially
  `primary-summary.json`, `comparison-index.json`, `state-validation.json`,
  `controller-cost/summary.json`, `fx-exact/20261015-223040/` and separate sample
  traces. Four candidate pilots and one failed admission are retained separately
  from the 24 primary windows. Production code retains the previous accepted
  catch-up optimization; no further CPU optimization or default change accepted
  this round. No plugin installation, Windows/real-DAW validation or universal
  patch/ROM coverage is established by this Mac run.

## 2026-10-15 — Live timed DSP transport jobs on Mac

- Scope: first event-controller stage for KS, preserving sound, DSP JIT,
  clocks, slices, serial ordering, buffers and priorities; H8 remains the
  reference/default. Same Optimized `f52e397` plus prior local changes,
  M1 Pro/macOS 27.2/Release arm64/pinned dependencies as preceding entries.
  Frozen accepted C++ AU, probe, runtime test and host are under ignored
  `output/perf-mac-dsp-events-20261015/before/`; originals and SHA-256 identity
  are retained before edits. No installation or publication is part of this run.
- New opt-in `SILVER_KS_DSP_TRANSPORT=events`: live readiness observations,
  address requests, 24-bit data writes and reply reads are timed protocol jobs.
  No tape, opcode decoder or H8 interpreter is involved in these jobs. Fixed
  protocol timing preserves every reference port event and in-flight PC;
  registers/flags remain materialized for the compatibility controller.
  Jobs fit wholly before the existing slice end and next UART/TPU0/TPU1/ADC
  event; otherwise the existing native boundary path handles preemption.
  Busy replies retain the actual read/timeout path, never cached readiness.
  Other controller logic is still the native compatibility port, so this is
  a bounded first stage rather than a completed semantic controller rewrite.
- Initial validation: 440,512 existing native boundaries and 80 IRQ cases,
  plus 1,216 complete HDI jobs with randomized live words/replies/flags and
  both DSP selections match original H8 data/IO, access clocks/PC, full frame
  and RAM. Deadline/pending-IRQ guards preserve state. Nine four-second full
  musical scenarios (Single, Multi, MIDI, pedal, controls, state, clock, steal,
  FX) match frozen original H8 full bus tapes, all 24 integer audio streams,
  guest/serial counters, state/user Flash/MIDI. Each renders 181,362 frames;
  post-boot/handoff interpreter calls remain zero and event-job counts are
  positive in every native scenario.
- Initial CPU pilot: three frozen-native/event pairs per four-second Single,
  Multi and FX passage produce only -0.59%, -0.24%, -0.07% median changes;
  this variant is not accepted as a CPU improvement. The first dispatch called
  a transport predicate for every controller boundary. A cheap HDI range gate
  and continuation of adjacent protocol stages before the event deadline remove
  that overhead. An intermediate repeat gives -2.99%, -2.13%, -1.60%, all nine
  individual pairs lower. The final source also leaves the disabled experiment
  on its original tight compatibility loop. Final binary measurements follow.
- Final protocol capture: 9/9 scenarios, **8,085,451 port events** byte-for-byte
  against the retained frozen-H8 oracle, **1,632,258 frames x 24 streams**,
  identical state/Flash/MIDI and guest counters. H8 calls after boot remain zero.
  The switching fixture initially rejected FX because its coverage assertion
  examined only the new native instance after .95 s, while Note On occurred
  before destruction at .65 s. Retain the first native segment's handler counts
  for the coverage assertion; original events, audio and equality checks stay
  unchanged. Per-instance exported metrics and last-handoff H8 counts keep
  their existing meanings. The rejected run is retained. Both pedal and FX
  switching passages now pass full tapes/audio/state with the corrected fixture;
  the intentional H8 window has positive interpreter calls and the final native
  handoff has zero interpreter calls. Correctness-run timing is not used as an
  optimization result.
- Further pilot evidence: the first complete AU matrix gives Single FX
  **50.6508 -> 49.7249% (-1.83%)**, four-part FX **48.8354 -> 48.9539% (+0.24%)**.
  All 12 windows pass and output/tails/program snapshots match. Final offline
  medians in that variant are -0.27% Single, -2.09% Multi, +0.67% FX, with
  overlapping ranges; no universal CPU saving is established. These matrices
  are retained under `*-unmasked-guard` and are not pooled with later trials.
- Masked-IRQ hypothesis: permit a job when mode-2 pending IRQ priority
  cannot preempt the current EXR; keep every bit and the current mask intact.
  Independent H8 tests include masked IRQs in both banks and rejection of
  unmasked low/high IRQs. All full tapes still match, but the nine scenarios'
  job counts are unchanged: this hypothesis does not explain their CPU cost.
  Its partial CPU matrix is stopped/retained under `*-masked-only`; it is not
  an accepted performance result.
- Live retry follow-up: Multi records 736,678 actual ISR-ready reads per four
  seconds, versus 61,996 low-data writes. Lift the timeout initialization and
  countdown/branch into the timed retry job when its complete original interval
  fits before the next deadline. Preserve every actual port observation,
  countdown wrap/overflow flags and the timeout continuation. No guessed DSP
  completion or suppressed polling. Differential cases now force busy replies
  and countdown 0/1/2/32767/-32768/-1/-2/3 as well as random live replies.
  Final Multi dispatches fall from 605,744 to 228,298 while covering 3,075,265
  reference boundaries; every actual port event remains unchanged. Dispatch
  reduction is not itself a CPU measurement.
- Final validation repeats all nine full bus/audio/state captures against the
  retained frozen H8, both live-switch passages, and the independent runtime
  oracle (440,512 boundaries/80 IRQ cases/1,216 complete protocol jobs). All pass.
  Deadlines and both masked/unmasked IRQ banks, busy countdown edges, full RAM,
  registers/flags, data/IO access clocks and in-flight PC are covered. The final
  source, fixture and bundle identities are frozen before CPU measurements.
- Final paced AU measurement: previous accepted **C++ compatibility controller**
  versus final event transport, identical host, three alternating pairs per
  Single FX and four-part FX case, 16 held keys total, 48 kHz/256, eight seconds
  after warm-up. **12 windows / 96 seconds measured audio**, user+system CPU
  for all process threads. No concurrent build, profiler or DSP test.

Median aggregate CPU, **100% = one CPU**, min–max across three runs:

| AU passage | Previous C++ | Event transport | Median change |
| --- | --- | --- | --- |
| Single, delay/reverb/chorus | 51.9601% (50.5946–53.2668) | 51.1810% (50.8302–53.0224) | -1.50% |
| Four-part Multi, delay/reverb/chorus | 50.7903% (50.7502–54.4722) | 50.1744% (49.8977–50.2472) | -1.21% |

- All 12 AU windows pass finite/audible output, zero lost frames and zero
  callback deadline failures. Actual float audio/tail hashes match all six
  runs of each passage, as do complete returned program snapshots. All requested
  AU values and all returned C++/Single/Multi/active-part/channel states validate.
  Wet release tails are nonzero. KS uses the same synthetic zero wave bank and
  generated saw programs as the previous comparison; no claim for every ROM,
  patch or wave bank. Initial setup/probes/tails/state reads are outside CPU.
- Separate final unrecorded offline render: three alternating frozen-C++/event
  pairs per four-second passage; **18 windows / 72 seconds measured audio**.
  Audio, state/Flash/MIDI and guest/serial counters match in all 18 runs.
  CPU medians/ranges in seconds per four seconds of native audio:

| Offline passage | Previous C++ | Event transport | Median change |
| --- | --- | --- | --- |
| Single | 1.946890 (1.944976–1.998989) | 1.923290 (1.816914–1.958586) | -1.21% |
| Multi | 1.931908 (1.916010–1.940372) | 1.818724 (1.791157–1.822111) | -5.86% |
| Dynamic Multi FX | 1.874350 (1.845315–1.899880) | 1.821183 (1.753601–1.915733) | -2.84% |

- Interpretation/decision: retain this as the first **opt-in live event-
  transport stage**, with modest median aggregate CPU savings in the tested
  final cases. Do not pool pilot variants or call the earlier fixed-replay
  19–20% diagnostic a realized saving. One AU Single pair and individual
  offline Single/FX pairs regress; small samples and overlapping ranges do not
  establish a saving in every run. OS placement/frequency was not independently
  measured. AU and offline workloads differ and are reported separately.
  H8 remains default/reference and C++ compatibility transport remains available
  by omitting the environment flag. MIDI/voice/parameter/other controller logic
  still uses the native compatibility implementation; this is not the complete
  event-controller conversion. DSP/JIT, clocks, slices, buffers, priorities,
  serial ordering and worker count are unchanged. No new allocation/lock/wait.
- Delivery: Release arm64 AU/VST3/Standalone and local launch instructions are
  in ignored `build/optimized-mac-dsp-events-2026-10-15/`; binary copies and
  ad-hoc signatures validate. AU is exercised by the actual comparison host;
  VST3/Standalone are built, not separately driver-tested in this round.
  No installed plugin replaced. Repository audit and diff checks pass.
- Raw evidence: ignored `output/perf-mac-dsp-events-20261015/`, especially
  `final-measurement-identity.json`, `final-bus/summary.json`, latest `toggle/`,
  `offline-cpu/summary.json`, `cpu/summary.json`, `au-state-validation.json` and
  `final-results.json`. Earlier pilot/failed/cancelled captures are retained
  under their explicit suffixes; none enter the final matrices. Windows,
  physical driver/DAW and broader patch/wave-bank validation remain pending.
  Next bounded investigation: profile remaining controller work outside HDI,
  then lift periodic voice/parameter jobs with the same command/audio/state
  gates and repeated whole-process CPU measurements.

## 2026-10-15 UTC — KS native-control jobs, isolated Mac follow-up

- Request: finish the KS controller removal first, preserve sound/JIT/timing,
  test against H8 and avoid concurrent preset-manager work. **Full controller
  removal is not delivered by this stage.** Original startup and the H8-like
  compatibility state/dispatch remain. The precise remaining gates are in
  `KS_NATIVE_CONTROLLER.md`; renaming the register/PC model or removing a link
  dependency is not treated as completing the semantic migration.
- Isolation: managed checkout `ks-native-controller`, based on `Optimized`
  parent `f52e397`, plus the preceding verified engine changes. Source matches
  the previous source-identity allowlist before this change. Browser/library/
  patch/Processor/workflow changes from the other chat were excluded; the
  shared checkout, its build directories and installed plugins are untouched.
  Pinned JUCE/asmjit/DSP source copies and generated framework headers live in
  the private build, independently of the other chat.
- Platform/build: Apple M1 Pro arm64, macOS 27.2, AppleClang 21, C++17 Release,
  deployment 12.0; standalone CMake build. Firmware OS 2.1.02 hash and the
  external synthetic Flash/zero-wave test inputs are recorded in local identity
  reports. No original/private input is committed. KS only; K-Station unchanged.
- Frequency profiling identified repeated part-state/table selection and scale
  work outside HDI. `SILVER_KS_CONTROL_JOBS=1` now enables bounded live jobs for
  these functions. The scale job computes the permitted shifts directly,
  including zero-as-256 byte-counter wrap, final carry and reference INC flag
  quirks. It does not run a hidden opcode loop. Part jobs retain the compatibility
  frame at yields and read actual selector/stack data. Jobs reject mode zero,
  unmasked IRQs and insufficient time, and stop at the event/slice boundary.
  No queue count, DSP thread, JIT, clock, catch-up order, serial ordering,
  priority, resampling, latency or buffer setting changed. No render allocation,
  lock, sleep or I/O added. The prior C++ path and original H8 remain selectable.
- Diagnostic backend publication now reports the Board's actual selected
  backend through an atomic literal pointer instead of labeling every native
  Device identically. The combined mode is `native-cpp-events-control`.
  Optional boundary profiling allocates only before playback and resets at
  the passage; it is excluded from CPU runs. Optional raw capture preallocates
  all 24 streams and writes the file after rendering; capture runs are not CPU
  evidence. The probe accepts bounded 2–30 second passages.
- Arithmetic admission: **86,016** control-job state/deadline/IRQ cases compare
  **471,684** independently executed original boundaries. All selector/counter
  bytes, random registers/flags, edge words, short/full-loop deadlines, masked
  pending IRQs and rejection of both unmasked banks are exercised. Results,
  flags, clocks, PC, stack bookkeeping and admitted data accesses match. Existing
  **440,512** boundary, **80** IRQ and **1,216** HDI-job cases also pass, as do
  **150,100** native memory operations. Interpreter count stays zero after
  handoff. The initial return-budget mistake was detected/fixed before admission;
  reference stack RAM in this emulator costs three clocks per byte. Loop-job
  prototypes and intermediate captures remain separately labeled and are not
  pooled with the final direct-arithmetic candidate.
- First candidate final checks: nine frozen H8 bus tapes match all
  **8,085,451** events/cycles/PC/responses/slices. Raw native audio compares
  **byte-for-byte** in 14 passages: the nine four-second scenarios, 30-second
  Multi/pedal/clock and two H8/C++ toggle passages. **6,075,618 frames x 24
  streams (583,259,328 bytes per implementation)** match, as do state/Flash/
  MIDI/counters. Five live UI switches compare 61,440 float samples, project/
  preference/instance/diagnostic checks pass, and paced 128/256 callbacks with
  concurrent state saves have no startup or steady frame loss/latency change.
- First candidate CPU, three alternating pairs per case, all process threads:
  18 unrecorded offline windows / 72 seconds of native audio give medians
  1.835025 -> 1.781483 seconds Single (-2.92%), 1.805687 -> 1.789120 Multi
  (-0.92%), 1.770809 -> 1.753599 dynamic FX (-0.97%). Some pairs regress.
  Same-binary AU with jobs OFF/ON, 12 windows / 192 seconds measured at 48 kHz/
  256 with 16 held keys total and active FX: Single 48.74323 -> 48.73647% of
  one CPU (-0.014% relative), Multi 48.61798 -> 48.15311% (-0.96%).
- Essential frozen-bundle control: previous accepted event-transport AU versus
  the first candidate, same 12-window/192-second protocol. Single medians
  48.25578 -> 48.48819% (+0.482%), Multi 47.96348 -> 48.03503% (+0.149%).
  All 24 AU windows have matching audio/tail hashes/programs, audible parts,
  zero lost frames and zero callback deadline failures. These small changes
  and overlapping ranges do **not** establish an improvement over the previous
  plugin. Keep this candidate research-only and off by default; do not claim
  that its dispatch reduction is a realized CPU saving. The first candidate
  still compiled in the optional profile guard; the next trial removes it.
- Research/identity root: ignored `output/perf-native-controller-20261015/`.
  `isolation-identity.json`, `final-source-identity.json` and
  `final-binary-identity.json` identify admitted sources, fixtures and AU.
  `runtime-final-86016.log`, `control-bus/`, `audio-exact/`, final host checks,
  offline/AU matrices and final stage results hold local evidence. Earlier
  `*-loop-prototype`, initial host/session-name checks and pre-direct-scale
  checks are not final CPU results. Windows execution and CPU remain pending;
  the new jobs are portable C++17 with no macOS scheduling dependency.

## 2026-10-16 Mac local — whole part lookup and lean Release trial

- Same isolated KS-only checkout and firmware/dependency/platform pins as above.
  First-candidate source/binaries/results remain frozen in the previous local
  root; no matrices are pooled. This is another bounded controller-CPU trial,
  **not completion of the requested full controller removal**.
- Lift the whole remaining part-state/parameter-table lookup to direct C++
  arithmetic when its original observations fit before the next event. Handle
  every partial-entry frame too, including unconstrained multiply/add inputs.
  Bounded formulas derive final C/V/Z/N/H directly; general partial frames
  retain exact reference arithmetic flags. Selector and return-stack reads
  retain original cycles/PC; short deadlines use the previous phase path.
- Move frequency profiling behind the separate CMake option
  `SILVER_KS_CONTROLLER_PROFILING`, default OFF. Ordinary Release contains no
  frequency vector or per-boundary profile check. The diagnostic ON branches
  compile in all eight affected units (compile-only check); production OFF
  is the actual built/tested/measured configuration. No DSP/JIT setting changed.
- Admission: the same 86,016 state/deadline/IRQ cases / 471,684 original
  boundaries pass for the complete formulas. All previous 440,512 boundary,
  80 IRQ and 1,216 HDI cases also pass. Nine final H8 command tapes match all
  8,085,451 events. All 14 full raw-audio passages and host checks pass again
  for this frozen version, including 30-second Multi/pedal/clock and H8 toggles.
- Offline comparison is V2 partial lookup versus V3 complete lookup/profile OFF,
  three pairs per four-second passage. Median CPU seconds: Single
  1.784172 -> 1.783389 (-0.044%), Multi 1.767489 -> 1.761360 (-0.347%), FX
  1.726016 -> 1.743217 (+0.997%). No broad improvement established.
- Actual AU control is the previous accepted event-transport bundle versus V3,
  three alternating pairs each Single FX / four-part FX, 16 held keys total,
  48 kHz/256 and 16 seconds measured per run (12 windows / 192 seconds).
  Single median total CPU is 46.46118 -> 46.06234% of one CPU (-0.858% relative);
  Multi 45.59552 -> 45.68498% (+0.196%). Ranges overlap. All outputs/tails and
  returned programs match; no lost frames/callback deadline failures. Again,
  this is not a noticeable or general CPU improvement. Keep control jobs opt-in.
- After CPU matrices finished, sample the admitted 30-second offline Multi
  fixture separately for five seconds at 1 ms. Its audio/state/guest counters
  still match the admitted passage and it executes zero post-boot interpreter
  calls. 3,752 samples: approximately 78.76% in DSP/catch-up, 14.53% native
  controller/service, 4.32% serial, 1.79% coordinator and 0.61% fixture. These
  are stack-sample shares in this evolving offline passage, not AU CPU shares
  or a universal patch distribution. Profiled times are excluded from all CPU
  matrices. Native `tickNative` itself accounts for 70 samples (~1.87%), and
  repeatedly attempts empty atomic MIDI queues; investigate that bounded cost.
- Local evidence: ignored `output/perf-ks-full-lookup-20261016/`, with separate
  source/binary identities, final stage runner/results, raw bus/audio/host
  checks and CPU matrices. No installed plugin or other chat's files changed.
  Source of preset-manager/Processor/workflow files still equals this checkout's
  HEAD. The earlier changed-engine-file manifest did not record those GUI file
  hashes; do not treat missing identities as evidence of either equality or
  differences in the frozen AU. Frozen AU results are whole-artifact controls;
  no editor is opened in the CPU host. Windows checks remain pending.

## 2026-10-16 Mac local — native UART availability snapshot

- Same isolated KS scope, hashes/pins and Release arm64 configuration. The
  previous candidates and their negative/inconclusive CPU results stay separate.
- `Board` MIDI input is coordinator-owned: Device host methods enqueue into
  its separate event queue, then the worker feeds Board between `advance()`
  calls. All production Board enqueue call sites were audited in that worker.
  Snapshot pending UART bytes once at the start of a native slice, decrement
  only after a real priority-queue pop, and use the same count for the next
  UART deadline. Empty queue polls have no guest-visible side effect. Actual
  byte order, UART spacing, RX flags/interrupts and all guest clocks stay intact.
  Original H8 tick, shared queue implementation and host event-queue atomics
  are unchanged. No new synchronization, waiting, allocation or OS API.
  This applies to the C++ compatibility and control-job paths; it is not a new
  thread or a changed MIDI admission policy. Board input methods are explicitly
  documented as coordinator-only, as required by their existing mutable parsers.
- New independent H8/C++ UART test passes in both compatibility and combined
  job modes: **12,000 frames x 24 streams per mode**, **post-boot UART bytes at exactly matching receive cycles**
  (the then-reported total was 252 per mode; the later reporting-only audit
  below verifies 0 bootstrap and 252 monitored bytes), **77,692 complete port events**
  and **56,561 complete slices** per mode. Budgets 1/2/3/7/15/31/63/97/127/128,
  empty-to-burst transitions, partial packets/realtime priority and two H8/native
  round trips are covered. Every observation spans at most one received byte;
  the exact last-receive cycle is derived from the existing RX deadline with
  no render bookkeeping overhead. All registers/flags/PC/clocks/bus history,
  IO, final RAM/Flash/state/MIDI output and audio agree. Reference arithmetic
  diagnostic messages remain in the logs; no host failure or divergence.
- All 86,016 control-job cases and existing 440,512 boundary / 80 IRQ / 1,216
  HDI cases pass after this change. Nine full frozen H8 command tapes again
  match 8,085,451 events. All 14 complete raw-audio passages, host switching/
  restore/instance tests and paced 128/256 captures pass without frame losses.
- Three offline V3/V4 pairs: CPU seconds Single 1.776630 -> 1.749493 (-1.53%),
  Multi 1.740694 -> 1.734468 (-0.36%), FX 1.728585 -> 1.715980 (-0.73%).
  Ranges overlap and some individual runs regress. Primary frozen accepted-AU
  comparison (12 x 16-second windows, all threads, same 16-note FX programs,
  48 kHz/256): Single 47.30961 -> 47.37749% of one CPU (+0.143% relative),
  Multi 46.79391 -> 47.00770% (+0.457%). No AU CPU saving established. All
  audio/tail/program checks pass; no lost frames/deadline failures. Do not
  promote this as a total-CPU improvement or pool it with other trials.
- Local evidence: ignored `output/perf-ks-uart-events-20261016/`, including
  `uart-final-results.json`, both `uart-final-*` logs/tapes, final source/binary
  identity and stage results. Profiling remains compiled OFF. Windows actual
  build/audio/CPU remains untested for these changes; all new engine work is
  portable C++17. Full semantic controller/bootstrap removal is still pending.

## 2026-10-16 Mac local — aggregate native fetch timing trial

- Same isolated KS scope and pins. After the previous matrices finished and
  verified their source identity, address the separately sampled constructor
  cost: Boundary constructor appears in 93 top-of-stack samples (~2.48%) in
  the admitted offline Multi profile. This remains outside DSP/JIT changes.
- In clock-only native mode, aligned low code and a nonzero even fetch length
  entirely below 0x400000 have exactly three clocks per 16-bit word, independent
  of preceding reads. Charge that duration directly and preserve final read
  history and in-flight PC. Keep the entire original byte-clock/read path for
  mapped mode, odd/zero lengths, odd addresses, boundary crossings and high
  code. No data/IO reads, instruction boundaries or interrupt acceptance move.
  Native execution already suppresses immutable code-fetch callbacks; this
  change only removes their per-byte timing loop. Original H8 core unchanged.
- New independent byte-clock oracle passes **10,880** combinations of fetch
  mode, length, alignment, preceding read, 0x400000 boundary, off-chip/on-chip
  ranges and 24-bit wrap fallback. Cycles, PC, read/write history, next boundary
  and mapped callback addresses/cycles/in-flight PC agree. All 440,512 runtime
  boundaries / 80 IRQ / 1,216 HDI jobs and 86,016 control-job cases also pass.
- UART fixture reporting now explicitly subtracts bootstrap bytes; its monitored
  admission is stricter and no equality criterion was removed. This particular
  fixture has **0** bootstrap UART bytes and **252 monitored bytes per mode**,
  so the earlier total 504 is confirmed rather than assumed. Both modes again
  pass 12,000 frames x 24 streams and each exact byte delivery cycle, all
  registers/flags/IO/memory/state and complete 77,692-event/56,561-slice tapes.
  Preserve the old fixture/hash and reporting-change audit in the UART trial
  root; production sources were unchanged by that reporting correction.
- Frozen final candidate and separate evidence root:
  ignored `output/perf-ks-fetch-timing-20261016/`. Full musical/host/CPU matrix
  completed; see the final matrix entry below. Profiling is compiled OFF. Full bootstrap,
  register/PC state and semantic controller removal remain unfinished; Windows
  actual checks remain pending. No files from the preset-manager chat changed.


## 2026-10-16 Mac local — final fetch/UART/control-job matrix

- This closes the measurement matrix above, not the full semantic controller
  migration. Full bootstrap, register/PC/flags/address-space frame and semantic
  lifecycle removal remain unfinished. Do not remove H8 or describe this as
  the completed native controller. Next independent gate is native initialization
  with full-boot command/state/timing equivalence, followed by foreground/parser/
  clock/voice/state lifecycle extraction. K-Station remains unchanged.
- Final local test ID: `perf-ks-fetch-timing-20261016`. All eight final stages
  pass and every KS source hash remains equal to the frozen pre-test identity.
  Measured AU executable SHA-256:
  `afe1be2b80e2a8d68f459ce031ddc28d136e1f90271eb5f188b05fb7242e21f1`.
  Parent revision remains f52e3971842bb25e92d3681f23441f16ddc56805 plus the
  documented isolated uncommitted engine changes. Release arm64, C++17,
  AppleClang 21 / SDK 27.2, deployment macOS 12, M1 Pro 6P+2E, macOS 27.2.
  JUCE/asmjit/DSP pins remain unchanged. Profiling is compiled OFF. No changes
  to DSP JIT, clocks, slices, serial order, buffers, priorities or DSP threads.
- Nine complete frozen H8 tapes match **8,085,451 ordered port events** and
  **1,632,258 native frames**. Fourteen complete raw-audio comparisons match
  **6,075,618 frames x 24 streams / 583,259,328 bytes per implementation**,
  including three 30-second evolving passages and two live-switch passages.
  State/Flash/MIDI output and guest/DSP/serial counters agree. Host tests pass
  **61,440 exact float samples across five live UI switches**, preferences,
  native/legacy project restore, instance isolation and corrupt-ROM/handoff
  guards. Paced 128/256-frame operation has no new frame loss. Separate focused
  checks cover 10,880 fetch-clock cases, 86,016 control-job cases (471,684
  original boundaries), 440,512 runtime boundaries, 80 IRQ cases, 1,216 HDI
  jobs and 504 exact-cycle post-boot UART bytes across both C++ modes.
- Primary final AU comparison: **12 x 16-second unprofiled windows / 192 seconds
  of audio** after preparation, three alternating before/candidate pairs per
  scenario, 48 kHz/256, same programs, 16 held notes total, delay/reverb/chorus.
  Total process user+system CPU is counted; **100% = one CPU**, not all 8 cores.
  Before is the frozen accepted event-transport AU; candidate is the combined
  fetch/UART/opt-in control-job build. Do not attribute the whole difference
  to one sub-change or pool results from earlier trials.

| AU, FX | Frozen accepted event-transport AU | Final candidate | Relative change |
| --- | ---: | ---: | ---: |
| single-fx | 47.097706% | 46.607250% | -1.041% |
| multi-fx | 46.911813% | 46.225413% | -1.463% |

- Single FX ranges: before [47.075425,47.399450], candidate
  [46.382350,46.627581]% of one CPU; saving 0.490456 percentage points.
  Four-part FX: [46.799306,47.450931] / [46.067700,46.340219]%; saving
  0.686400 points. All three pairs improve per case; ranges do not overlap
  in this series. All 12 windows have exact output/tails, matching returned
  programs and channels, finite audible output and zero lost frames or
  over-budget callbacks. This is a small local improvement, not a large or
  universal CPU gain. Earlier UART/part-only trials were mixed.
- Offline V4/V5 baseline is the immediately preceding UART candidate, distinct
  from the frozen accepted AU baseline. Three alternating four-second pairs,
  total CPU seconds (medians and relative change):
  single 1.760828 -> 1.738883 (-1.246%); ranges [1.754975, 1.763404] / [1.738505, 1.740908].
  multi 1.748462 -> 1.721816 (-1.524%); ranges [1.740714, 1.756252] / [1.685986, 1.722089].
  fx 1.719284 -> 1.715176 (-0.239%); ranges [1.718473, 1.722266] / [1.70177, 1.724456].
  FX ranges overlap; do not generalize that small result.
- Local package: ignored `build/optimized-mac-ks-control-jobs-2026-10-16/`.
  Exact measured AU, separately built VST3/Standalone, source snapshot, licenses,
  source identity and checksums. Verify strict signatures before handoff. AU
  was hosted/tested/measured; VST3/Standalone were built but have not had a
  separate DAW/audio-device session in this round. Installed plugins and the
  original shared checkout are unchanged; seven excluded preset-manager/
  Processor/workflow files equal the isolated parent revision. Work is not
  merged into the shared Optimized checkout.
- Inputs are authenticated KS OS 2.1.02, synthetic Flash and zero Wave ROM.
  Original sampled banks, physical-device equivalence and arbitrary untested
  states/inputs are not proven by finite comparisons. Private ROMs, captures,
  binaries and raw traces remain ignored. New algorithms use portable C++17,
  not M1-only code; actual Windows x64 build, audio/state and CPU checks remain
  pending. See KS_NATIVE_CONTROLLER.md for the full remaining removal gates.


## 2026-10-16 Mac local — C++ reset and initial DSP setup

- Isolated KS OS 2.1.02 scope, same authenticated OS/dependency pins and
  Mac Release arm64/C++17 compiler configuration as the preceding final matrix.
  No K-Station, preset-manager, DSP JIT, clock/slice/serial/buffer/priority or
  worker-count change. The new work removes original-H8 instruction execution
  from a fresh C++ Device's reset/initialization/warm-up. No saved startup image,
  prerecorded DSP response or interpreter fallback is used. The complete ROM
  is authenticated before entry; executable SRAM is checked and sealed at the
  actual overlay transition. The H8 path remains separately selectable.
- New reset path covers reset vector, internal SRAM clear, hardware setup,
  Flash copy/checksum/validation, copied overlay trampoline and existing initial
  DSP/controller setup. This is a fixed C++ **compatibility** path that retains
  the same register/PC/flags/stack frame, not the completed semantic initializer.
  Native mode now uses it from construction; a live H8-to-C++ switch still
  validates and adopts the current state through the existing handoff.
- Independent full-reset differential fixture compares two fresh Boards on
  every coordinator observation, complete port/slice bytes, all controller
  memory/IO/IRQ priorities and masks, Flash/musical state/MIDI and serial state,
  then 24,000 audible frames x 24 streams. Results (all exact; zero candidate
  interpreter steps): Single 503,282 cold slices / 1,809,727 port events;
  irregular budgets 432,116 / 1,433,980; Performance 503,674 / 1,692,077;
  synthetic nonzero wave mapping 503,282 / 1,809,727. Reference step counts
  are respectively 14,806,723 / 12,799,189 / 14,641,837 / 14,806,723.
  Counts include the test's complete trace through warm-up and notes; cold
  slices are counted only through first readiness. These are instrumented
  fidelity checks, not CPU measurements. The mapping variant is synthetic,
  not an admission of original sampled-sound fidelity.
- Independent operation tests add **4,128 reset cases** with arbitrary partial
  values/flags and both mapped/clock-only fetches, including rare branches.
  Existing **440,512 runtime boundaries, 80 IRQ cases, 1,216 HDI jobs and
  86,016 control-job cases / 471,684 original boundaries** also pass.
  A first-address error in hardware setup and two long-branch lengths were
  caught by the cold oracle and corrected before final validation.
- The per-operation reset oracle additionally caught H8's signed immediate
  PC on the copied trampoline: logical PC 0xffffec00 versus physical bus
  0xffec00. Preserve that exact original state while masking physical fetch
  addresses to 24 bits. The fetch-clock oracle now uses actual original read8
  calls, not a parallel clockMem loop; **11,520** cases pass including signed
  PC and physical wrap. The earlier 10,880-case oracle proved equivalence to
  its old byte-clock helper but did not establish this signed/physical case.
  The original H8 core and DSP JIT remain untouched.
- Final local test ID: ignored `output/perf-ks-native-reset-20261016/` with
  frozen engine/binary identities, four full cold logs/tapes and musical/host
  revalidation. All 13 final stages pass and the frozen KS source identity was
  verified after completion.
  Full semantic register/state/frame removal and H8-only-test linking are
  still outstanding; never call this the completed full controller port. No
  Windows execution validation for this stage yet. New source is portable
  C++17. Do not reuse the previous binary's CPU claims for this new binary.


### Final cold-reset delivery validation

- All nine complete frozen original-H8 musical tapes match **8,085,451**
  ordered port events and **1,632,258** native frames. Native boot interpreter
  counts are explicitly zero in every native musical capture. All 14 complete
  audio cases again match **6,075,618 frames x 24 streams / 583,259,328 bytes
  per side**, with exact Flash/state/MIDI and guest/DSP/serial counters.
  Long passages and live H8 round trips are included; deliberately toggled
  H8 segments are counted separately from initial/native interpreter counts.
- Actual JUCE controller host passes **61,440 exact float samples across five
  UI switches**, held notes/pedal, instance isolation, preferences, native and
  legacy projects and diagnostic boundaries. Paced 128/256 sessions have zero
  startup/steady missed frames in this series, stable latency and concurrent
  state reads. Windows firmware/audio/CPU was not exercised locally.
- New unprofiled AU series: three alternating 16-second pairs per Single FX
  and four-part FX (12 windows / 192 seconds total), 48 kHz/256, 16 notes total,
  same returned programs/channels, delay/reverb/chorus, sequential DSPs, all
  process user+system CPU; **100% = one CPU**. Before is the frozen accepted
  event-transport AU; candidate includes prior fetch/UART/control jobs and
  this new C++ reset. Do not attribute all savings to the reset change or pool
  this series with preceding variants.

| AU, FX | Frozen accepted event-transport AU | C++ reset + current jobs candidate | Relative change |
| --- | ---: | ---: | ---: |
| single-fx | 44.863262% | 44.445200% | -0.932% |
| multi-fx | 44.716487% | 43.866525% | -1.901% |

- Single ranges: before [44.753306,45.704925], candidate
  [44.413012,44.537306]%; median saving 0.418063 percentage points. Multi:
  [44.579844,45.160663] / [43.807225,43.931119]%; saving 0.849963 points.
  All three pairs improve per case and ranges do not overlap in this local
  series. All 12 windows have exact output/tails, finite audible output,
  matching returned programs, zero lost frames and zero over-budget callbacks.
  This is still a modest whole-process saving, not a completed semantic port
  or a universal performance claim. No cold-start CPU speed-up is inferred
  from instrumented cold timelines.
- After all stages and source-identity verification, correct only the probe's
  obsolete notes sentence claiming H8 booted all modes. Preserve the measured
  fixture source and exact before/after replacement hashes in the ignored
  `fixture-notes-audit.json`. No production algorithm or plugin binary changed.
  The corrected fixture is separately rebuilt and its native report is checked
  against the frozen Single audio/state/counter reference. This annotation
  correction does not replace or relabel any measured CPU window.
- The Windows Desktop CI now compiles reset/runtime/UART/timing fixtures and
  runs ROM-free timing/timer/memory checks. Missing private-input cases skip
  explicitly with 77; no ROM or captured data is uploaded. Actual CI status
  must be read after push and must not be inferred from the workflow change.
- Local package: ignored `build/optimized-mac-ks-native-reset-2026-10-16/`,
  exact measured AU plus built VST3/Standalone, strict signature verification,
  public source snapshot/identity, checksums, licenses and explicit limitations.
  AU was hosted and measured; VST3/Standalone were built, not separately hosted.
  Shared Optimized, installed plugins and seven excluded preset/Processor/
  workflow files are unchanged. Work is on `codex/ks-native-controller`,
  isolated from the ongoing preset-manager work.
- **Full controller migration remains unfinished.** The compatibility
  register/flag/PC/stack/address-space state still owns normal control flow;
  H8 remains selectable in the product, not confined to comparative tests.
  Retiring it requires semantic typed-state foreground, parser/clock/arp,
  voice/parameter/transport and state/Flash lifecycles, including preemption
  and data-dependent timing. Removing inheritance, renaming fields or deleting
  the H8 link would not complete that work. Do not publish this as full removal.

## Entry template

Copy for each next round:

- Date, platform and CPU:
- Source revision/configuration/compiler and diagnostics mode:
- Test ID and local raw report reference:
- Host/version/backend/device, rate, actual/prepared block and reserve:
- Model, patch, parts/voices, instances, UI state, power mode and passage:
- Hypothesis and exact change (or measurement only):
- Validation actually run and result:
- Metrics (units), audible outcome and interpretation:
- Limitations / untested platform / instrumentation overhead:
- Next step:

## 2026-10-16 — Independent KS storage, test-only H8 and control arithmetic

**Scope/source.** Isolated `codex/ks-native-controller` worktree based on
`6fc9cad7b285847cad8b4d53b94c85fc3c56dca8`, arm64 Release on the same M1 Pro,
AppleClang 21 / macOS 27.2 machine. Test ID
`perf-ks-reference-isolation-20261016`. Engine source and candidate binaries
were frozen before the matrix. Source-manifest SHA-256:
`263ebdb02dad6e69cef228cbba938e9bcf2224b79829eacf35579ee0f780bcea`;
measured AU executable SHA-256:
`53a5ebc31803f29210b4fbabdd0a18f6dbad2395e5a0ce8e1a430a0e015677bf`.
The executable was compiled before the commit containing this entry; embedded
revision metadata therefore identifies parent `6fc9cad` plus dirty state.

**Implementation.** KS storage no longer inherits the H8 emulator and has no
16,777,216-entry device-pointer map. One explicit Board bus replaces that map;
128 MiB of pointer storage is eliminated per KS instance. The retained frame
is 16,777,472 bytes. Integer continuation tokens also avoid forming out-of-array
PC pointers. `ksLib` has no `h8s` link, H8 type dependency or original decoder
symbols. Production AU/VST3/Standalone link commands contain no KS reference
object. K-Station retains its separate original controller.

The unchanged original H8 is compiled in `ksH8Reference_test.cpp`, a test-only
object. It synchronizes frame/Board observations around actual original-core
steps. It adds host overhead to the reference and must not be treated as a
historically comparable production CPU baseline. Production has no registered
adapter: its interpreter entry rejects work without state mutation/counting.
It always selects C++ before reset, including legacy projects with H8/hybrid
settings, and Setup reflects C++ without an H8 selector. Comparison executables
retain reversible H8/C++ selection and legacy preference tests.

Add/subtract control arithmetic is pure C++17 code, independent of H8 methods.
Unsigned intermediates define wrap; original status and sticky-zero behavior
remain exact. Only the original-reference arithmetic test gets `-fwrapv` on
Clang/GCC. No DSP/JIT compiler option or DSP algorithm changes.

**Correctness.** All 16 frozen-matrix stages pass; source identity verified:

- Four independently initialized cold timelines (Single, irregular budgets,
  Performance, synthetic mapped wave endpoints), 96,000 exact 24-stream frames,
  full command/slice files, RAM/IO/state/Flash/IRQ/serial checks; candidate H8
  interpreter calls remain zero. Reference coordinator slice counts are
  503,282 / 432,116 / 503,674 / 503,282 respectively.
- 283,113,088 independent original-H8 arithmetic cases (268,435,456 byte,
  12,451,840 word, 2,225,792 wide cases). 440,512 normal boundaries,
  4,128 reset boundaries, 80 IRQ/mask/tie entries, 1,216 timed HDI jobs,
  86,016 control-job cases equivalent to 471,684 original boundaries,
  150,100 memory operations and 11,520 fetch-timing cases pass. The memory
  and fetch oracles call original H8 accessors directly.
- Nine full frozen H8 command tapes match 8,085,451 events and all scenario
  state/counters. Fourteen raw-audio comparisons match 6,075,618 frames per
  implementation x 24 streams (583,259,328 bytes per implementation), including
  30-second Multi/pedal/clock and deliberate reference-toggle windows.
- 61,440 exact floating-point samples through five test-build UI switches;
  held notes/pedal, latency, diagnostics, project restore and instance isolation
  pass. Paced 128/256 tests report no missing frames.
- Twelve actual production AU CPU windows retain exact output/tails/programs,
  correct Single/Multi channels and active parts, finite audible output,
  zero missing frames and zero over-budget callbacks.

After this matrix, a new reset-vector test and its CMake/CI target were
added, and the test-container compile issue described below was repaired.
No production source or measured binary changed. Delivery audit verifies
that separation. Its direct original `H8SEmulator::boot()` independently admits
the comparison starting state across all **16,777,216** 24-bit reset targets,
varying all upper vector bytes, prior status/priority mask/pending IRQs and
interrupt mode; clocks/history/retained working values match. This supplements
the synchronized step adapter rather than using that adapter to admit itself.

Two additional actual production AU runs restore paired legacy H8 and C++
projects as C++, with identical output/tail/program and no lost/deadline failures.
Strict ad-hoc signature validation passes AU, VST3 and Standalone. VST3 and
Standalone were built but not separately hosted in this follow-up.

**Total-process CPU.** Frozen preceding reset-stage C++ AU versus this AU;
both use event transport and control jobs, sequential DSPs, profiling OFF,
48 kHz/256, 16 held keys total with delay/reverb/chorus, three alternating
pairs per case, 16-second windows (192 seconds measured audio in total).
100% denotes one CPU. Neither benchmark side uses the synchronized H8 adapter.

| AU / FX | Frozen preceding C++ median | Current C++ median | Relative change |
| --- | ---: | ---: | ---: |
| Single | 47.167506% | 46.351675% | -1.729647% |
| Four-part Multi | 46.654788% | 46.023894% | -1.352259% |

Single before [47.484756, 46.937313, 47.167506], after
[46.328906, 46.351675, 47.850544]: two pairs improve and the third regresses;
ranges overlap. Its median does **not** establish a consistent saving.
Multi before [46.463694, 46.716456, 46.654788], after
[46.079356, 45.960863, 46.023894]: all three improve with separated ranges
in this local series. These are modest, workload-specific observations, not
pooled with older series and not a promised universal speedup. The storage,
bus and arithmetic changes are measured together, not attributed separately.

**Limits/outstanding request.** The original register/flags/stack/continuation
ABI and fixed compatibility control flow remain in independent KS storage.
This is **not completion of the requested semantic controller conversion**.
H8 confinement to tests and removal of its map/base class do not remove that
model. Native typed foreground/IRQ/voice/parameter/Flash jobs remain necessary.
Inputs use authenticated OS 2.1.02, synthetic Flash and zero wave bank; mapped
endpoint testing does not validate the original sampled bank. No private ROM,
PCM, bus tape or raw report is committed. Preset-manager algorithms and shared
Optimized checkout are untouched; editor/Processor changes only handle the
controller selection. Windows CI includes ROM-free arithmetic, production and
reset-vector guards; Windows ROM/audio/CPU validation remains separate.

**Additional final checks.** Both focused UART configurations (default and
transport/control jobs enabled) pass after separation: each matches 252 exact-
cycle bytes, 77,692 port events, 56,561 slices and 12,000 frames x 24 streams,
including budgets 1..128, running-status/partial messages, realtime-byte priority
and two deliberate H8 round trips. These are separate from the 14 audio totals.

The broader check build exposed an existing PMR/std::vector mismatch in
`silverPersistence_test.cpp`, present in parent `6fc9cad` and confirmed as the
cause of the previous Windows CI failure
[38829816025](https://github.com/temaniak/Silver-K/actions/runs/38829816025).
Only its fixture now explicitly copies SysEx bytes into the std::vector expected
by addFactory; no Library/Browser/Patch/preset-manager algorithm changes.
All check targets then build. A fresh isolated CTest session reports **18 pass,
5 expected skips, 0 failures** (23 selected tests); ROM-dependent tests omit
private inputs in that session. Reusing the first session's preferences caused
workflow/editor fixture failures, which disappear in a new session without
source changes. Raw earlier logs are retained privately. The private-ROM KS
runtime/reset/UART and live-plugin checks above run separately with their inputs.
The new Windows CI run still has to establish its own result after publication;
Mac passing does not claim Windows ROM/audio/CPU validation.

## 2026-10-16 — Native control-task arithmetic and live keyboard observations

Source: `1724c1b` plus the local semantic-core changes, isolated branch
`codex/ks-native-controller`. Test ID `ks-native-control-core-20261016` retains
source/executable SHA-256 identities and raw logs privately. Platform: this
M1 Pro Mac, arm64 Release, AppleClang 21, C++17; controller profiling OFF.
Pinned JUCE/asmjit/DSP dependencies and admitted KS OS 2.1.02 identity are
unchanged. Windows has not run these new tests yet.

Implementation:

- Direct signed coefficient decoding now has a fixed-size `ParameterScaleTask`.
  Its arithmetic depends only on the encoded parameter. Its completion clock
  can stop before every admitted boundary and resume without CPU context.
  There are no processor registers, flag byte, code address, emulated stack,
  opcode decoder or firmware execution in this task.
- `nativeControl::nextKey` / `previousKey` recover strict adjacent-key search
  over the 128-key physical/held bitmap. `KeySearchTask` performs live byte
  observations at the original controller cycles and latches each observed
  byte while inspecting it. A later handler's keyboard changes affect future
  observations, not an already observed byte. Arithmetic and duration use
  native key/group predicates, without a processor-state ABI.
- Original H8 is used directly by the test executables as the independent
  procedure/read-timing oracle. No synchronized adapter predicts its values
  or cycles. All supplied private firmware remains external.

Actual checks:

- Coefficient: all 65,536 inputs x eight retained masks, 524,288 cases and
  22,266,088 independently executed original boundaries. Coefficients,
  preparation/full durations and every task boundary match. An additional
  8,382,551 clock-budget/pause/resume combinations pass.
- Keyboard: 2,269,600 private cases, 82,647,146 independently executed original
  boundaries. Strict next/previous values, body durations, boundary costs and
  each live byte read's value/order/cycle match. Inputs include every MIDI key,
  singleton/empty/full maps, every byte value in each position with sparse and
  dense surrounding maps, and deterministic random maps/retained flags.
  7,952 cases mutate keyboard storage at legal preemption points, including
  after a byte was read. These are live-state mutation tests, not proof of the
  complete interrupt controller or a complete musical timeline.
- Both new public property tests pass. A separate task-only ASan/UBSan C++17
  executable passes all 65,536 coefficients and 131,072 key searches. The
  task states are fixed-size and trivially copyable, with no H8 dependency.
  This checks these tasks; it does not certify every plugin callback path.

Delivery boundary and remaining work:

The new tasks are **not yet connected to production playback**. Production
controller `.cpp`/`.h` hashes still match the preceding accepted controller;
DSP, JIT, transport order, clocks, buffers, priorities and preset-manager code
are unchanged. Registers, flags, stack and PC are still present in the old
live dispatcher. This checkpoint does not finish their removal and is not a
replacement end state. Continue connecting a complete semantic controller,
including reset, byte/message admission, note ownership/allocation, parameter
and voice calculations, timer/clock/arp, SysEx/state/Flash and DSP exchange.
Full boot/command/state/audio proof and H8-versus-C++ whole-process CPU runs
remain required **after** that removal; no new CPU comparison was performed
for this checkpoint. Push of the finished transfer remains pending.

### Earlier coefficient adapter trial (rejected, not delivered)

The earlier test ID `perf-ks-native-scale-phases-20261016` tried connecting a
native preparation/calculation through the retained compatibility frame.
Its 17 correctness stages passed, including complete admitted bus/audio
checks, but the three-pair actual-AU CPU trial regressed: Single median
+0.962%, Multi +1.615% relative to its preceding C++ baseline. That trial was
removed from the working engine. Frozen trial sources/reports remain private.
It was C++-versus-C++ research, not the final requested H8 comparison. The
retained formulas/tasks above are correctness work for the complete semantic
port, not a claim that the rejected runtime optimization is shipping.

The Windows-only persistence-test entry fix also remains in this worktree:
pinned JUCE's Windows `JUCEApplicationBase::main()` has no argc/argv overload.
Mac public persistence validation passed before this core-only step. Actual
Windows compilation/CI, firmware/audio fidelity and CPU remain separate checks.

## 2026-10-16 — Connect native key-search tasks to real KS playback

Source: `1724c1b` plus local native-key-call changes on isolated branch
`codex/ks-native-controller`. Test ID `ks-native-keycalls-20261016` retains
frozen executables, source/binary SHA-256 identities and raw evidence privately.
Platform: this M1 Pro Mac, arm64 Release, AppleClang 21, C++17, controller
profiling OFF, the same pinned dependencies and authenticated KS OS 2.1.02.
The shared `Optimized` checkout, installed plugins and preset-manager files
were not edited. Windows execution remains unvalidated for this checkpoint.

Runtime change:

- Both next/previous held-key searches now execute `KeySearchTask` in normal
  KS C++ playback, independently of the opt-in transport/control-job flags.
  The native task owns input, byte observations, candidate selection and timed
  continuation. It does not read a CPU register bank, flags, PC or guest stack.
- Internal byte-key sentinels are preserved too: all 256 input bytes are
  admitted, including the original wrapping observations of adjacent state.
  Ordinary MIDI keys still use the 128-key bitmap. A native caller supplies
  live byte observations, so an interrupt cannot retroactively change a byte
  that was already observed.
- `KeySearchCalls` is an explicitly temporary caller/IRQ projection. It copies
  native progress to the old calling convention and source-location trace,
  without using that projected state to choose the native search result.
  It still reads the old return stack and exposes old flags/PC to callers.
  These are remaining removal work, not a completed register-free controller.
- The 49 old key-search instruction bodies move from production note handling
  into `ksH8Reference_test.cpp`. Only comparison executables can recover an
  arbitrary pre-existing partial H8 frame. A fresh native instance rejects
  that fallback; the new actual-runtime fixture does not link its factory.
- Diagnostic reset preserves in-flight native calls and resets only counters.
  The call slots are fixed-capacity and allocated with the owning controller;
  no task allocation, lock, I/O or waiting was added during execution.

Actual independent checks:

- The real `RuntimeController` / note path executes 926,866 complete calls
  against directly executed original H8: 37,204,297 original steps. Every
  register/flag/PC projection, cycle, retained field and ordered data/return
  access matches at every boundary. Cases cover all input bytes, singleton/
  empty/full/random 48-byte state regions, SRAM and low aliases, mapped and
  clock-only fetches. No synchronized H8 adapter or partial-test factory is
  linked into this fixture; native interpreted-step counts stay zero.
- 5,170 interrupt-entry/RTE/resume points also change live key memory. Already
  observed bytes stay latched and future reads see changes. This uses a
  synthetic vector to the admitted RTE, not a claim that this leaf fixture
  alone covers complete musical interrupt handlers. There are also 58,873
  in-flight metric resets, without discarding native continuation.
- Four complete cold/reset timelines pass with full RAM, IO, IRQ, state,
  Flash, bus/slice and audible 24-stream equality, including irregular budgets,
  Performance startup and a mapped-wave marker case. Candidate H8 steps are 0.
- Nine complete frozen original-H8 DSP tapes match, 8,085,451 events. The
  connected task actually runs 40 times in the pedal passage, seven in controls
  and 12 in clock/arp; all 59 return normally with zero partial-test entries.
- Fourteen full audio comparisons pass, including 30-second Multi/pedal/clock
  and pedal/FX H8 round trips: 6,075,618 frames x 24 streams per side,
  583,259,328 raw bytes per side. Audio bytes, controller/DSP clocks and
  instruction totals, serial state, musical state, Flash and MIDI counters
  match. Normal native passages all report zero test-partial entries.
- Existing parser/note/voice boundary checks, general runtime/HDI/control-job
  and coefficient/keyboard property checks pass. The historical arbitrary-
  partial-frame fuzz uses the comparison-only key helper; the separate whole-
  call fixture above is the proof for the actual new production algorithm.
- A private ordinary-C++ allocation probe executes 39,208 real calls and
  1,074,944 original comparison steps, including 2,280 IRQ/live-state mutation
  points, with zero `new/new[]` allocations inside native runtime steps.
  A separate build of the new bridge and task passes the same cases under
  ASan/UBSan. Its original H8 header is excluded from sanitization and unchanged
  libraries are uninstrumented; this is not a sanitizer claim about DSP/JIT or
  the entire plugin. No installed product is replaced.

Additional product/call-lifecycle checks for the same engine source:

- A private interleaved-caller test keeps eight native tasks live at once:
  86,464 child calls, 2,205,496 directly executed original child steps and
  12,352 resets while multiple tasks are suspended. Every parent's projected
  continuation survives its children. Caller contexts are restored explicitly
  in this test; it is not an IRQ-prologue timing claim.
- Freshly linked public CTest selection: 20 pass, six skip without private ROM,
  zero fail (26 selected). The skipped complete key/runtime checks run with the
  private ROM in the correctness matrix above. Existing UI controller changes
  pass; Processor callbacks at 128/256 retain finite audible output and zero
  startup/steady lost frames while saving state and re-preparing. Their process
  CPU collector is disabled, not used for a performance result.
- AU/VST3/Standalone build and strict ad-hoc signature checks pass. Their target
  commands contain no KS H8 reference object; `ksLib` has no H8 core/decoder
  symbols. K-Station's separate H8 remains valid. Product hashes and library
  identity are retained in the private frontend audit. These three bundles
  were built, not installed or individually hosted for an audio recording.

These checks use synthetic Flash and the zero Wave ROM fixture, plus the
explicit mapped markers at boot; real sampled banks and physical hardware are
not admitted by this matrix. DSP code/JIT, B/A order, clocks, resampling,
buffers, worker waits/priorities/placement are unchanged. Process CPU values
present in the existing offline probe are ignored; no CPU comparison runs in
this checkpoint. The callback-only fixture explicitly disables process CPU
collection. See the private frontend audit for product build/signature and
callback checks; those checks do not replace an actual AU/VST3 host recording.

Next: connect the independently admitted signed coefficient task and continue
converting its callers and the remaining reset/parser/voice/parameter/clock/
arp/state/Flash/transport paths. Remove the temporary projections and the
shared register/flag/stack/PC model once those callers are semantic. The full
controller remains incomplete. Only after that removal, run final command/
audio/state checks, a fair direct-H8 versus completed-C++ total-CPU comparison,
and the requested push.

## 2026-10-16 — Native signed-coefficient calls in normal KS playback

Source: `271e572` plus local coefficient-call changes, isolated branch
`codex/ks-native-controller`. Test ID `ks-native-parameter-calls-20261016`
retains frozen tests, source/executable SHA-256 identities and raw evidence
privately. Platform: this M1 Pro Mac, arm64 Release, AppleClang 21, C++17,
controller profiling OFF. Pinned dependencies and authenticated KS OS 2.1.02
are unchanged. The shared checkout, installed plugins and preset-manager
sources were not edited. Windows private execution is pending.

The complete signed-coefficient calculation now uses `ParameterScaleTask` in
normal playback, independently of the experimental flags. The task computes
its coefficient from the encoded input and owns its completion clock without
registers, flags, PC or guest-stack data. `ParameterScaleCalls` is a temporary
output projection for the unconverted callers and IRQ saves; it still uses
that old caller/return ABI and must be deleted with those callers. This is not
the completed controller conversion.

All 26 old body boundaries move from the production extended dispatcher into
the comparison-only object. The former register-driven shift batch also moves
there. `ControllerJobs::candidate` no longer selects it, so a batch cannot
advance the old registers behind a native task's continuation. The remaining
opt-in part-lookup jobs stay intact. Fresh native instances reject both partial
coefficient recovery and the retired shift batch; comparison executables may
recover fabricated partial states or an existing H8 test handoff.

Actual checks:

- 1,049,756 calls through the real runtime, 44,600,604 directly executed
  original-H8 steps. All 65,536 inputs x eight retained masks x mapped/clock-only
  fetch, odd/even SRAM return locations, and every intermediate caller value,
  flag, cycle, PC projection, data/return access and final memory match. This
  target deliberately links no synchronized H8 adapter or recovery factory.
  Its native interpreted-step and partial-recovery counters remain zero.
- 1,132 IRQ save/RTE/resume points and 66,280 in-flight metric resets pass.
  168 explicitly interleaved child calls / 7,114 original child steps preserve
  eight simultaneously live tasks. That caller-context test does not claim
  real ISR-prologue timing; the separate RTE test and full Board timelines
  establish their declared interrupt scopes. Negative-minimum saturation and
  the original INC flag quirk remain exact.
- Four full cold-reset timelines pass: normal/irregular coordinator budgets,
  Performance and mapped markers, full RAM/IO/IRQ/state/Flash, exact DSP tape
  and 24-stream audible output; candidate H8 steps and test recovery are zero.
- Nine complete frozen original-H8 tapes match all 8,085,451 events. Actual
  native coefficient calls: Single 4,241; Multi 4,162; MIDI 275; pedal 598;
  controls 424; state 197; clock/arp 156; stealing 3,120; FX 760. All 13,933
  return normally with zero partial recovery. These are work counts, not a
  CPU saving. The previously connected key tasks also remain exercised.
- Fourteen complete raw-audio comparisons pass, including 30-second Multi,
  pedal and clock/arp and pedal/FX H8 round trips: 6,075,618 frames x 24 streams
  per side, 583,259,328 bytes per side. Audio, state, Flash, MIDI, controller/
  DSP clocks/instruction totals and serial state match. New native passages
  all report zero coefficient/key test recovery.
- All 48 correctness stages pass against the frozen source identity. Existing
  arithmetic/core, decoder/note/voice and historical partial-frame checks pass;
  the latter use comparison-only recovery and do not replace the actual-call
  proof above. A private allocation probe passes 9,452 representative/edge/
  preemption/nested calls with zero `new/new[]` allocations inside native
  runtime steps. The new coefficient bridge/task pass the same cases under
  ASan/UBSan; original H8 is excluded and unchanged libraries are uninstrumented.
- Fresh public CTest: 20 pass, seven skip without private ROM, zero fail
  (27 selected). Actual private call/runtime checks run in the matrix above.
  UI controller checks pass. Processor callbacks at 128/256 remain finite,
  audible and free of lost frames while saving/re-preparing; their process CPU
  collector is disabled and their C++ controller choice is asserted.
- AU/VST3/Standalone build and strict ad-hoc signatures pass. Product target
  commands exclude the KS H8 comparison object; `ksLib` has no H8 core/decoder
  symbols. K-Station's separate core is unchanged. These builds are not installed
  or separately hosted AU/VST3 audio recordings. Product/library hashes remain
  in the private frontend audit.

Limits: synthetic Flash, zero Wave ROM and explicit mapped boot markers are
admitted; real sampled banks and physical hardware are not established by
this matrix. DSP/JIT, Device, B/A order, clocks, resampling, buffers, waits,
priorities and placement are unchanged. Existing probe CPU fields are ignored;
no new CPU comparison runs before the complete port. Windows build/private
fidelity/CPU evidence remains separate from these Mac checks.

Next: continue the native DSP-exchange path with typed word/address/readiness/
timeout state, and convert the remaining callers, reset, parser, note/voice,
parameter, timer/clock/arp, state/Flash paths. Remove all transitional caller
projections and the shared register/flag/stack/PC model. Only after that full
removal, run final command/audio/state tests, compare total CPU with a direct
H8 production baseline and push the completed transfer.


## 2026-10-16 — Mac: native live DSP-word writes, caller projection retained

Test ID: `ks-native-dsp-word-calls-20261016`. Isolated source parent
`a48e2c6db6797f810410cb5f2618ee191b01e792` plus the DSP-word task/caller
changes; exact source/binary manifests and private reports remain in ignored
output. Same M1 Pro arm64 Release build, AppleClang 21/C++17 and pinned
JUCE/asmjit/DSP dependencies as the preceding coefficient-call entry.
No CPU comparison or push has been performed; both wait for the complete port.

`DspWordTask` owns native payload/address values, live readiness, retry/timeout
state and the original interruptible admission durations. Sequential and
post-address writes both use this task. The real port is read afresh on every
retry; neither readiness nor response data is cached or replayed. A timeout
parks the task while the existing fault service runs, then explicitly resumes
it. Its own state contains no register bank, processor flags, guest stack or
PC. `DspWordCalls` temporarily supplies the existing caller/IRQ/trace projection
and return convention. Those projections and the global compatibility model
remain incomplete work, not a completed controller replacement.

The 34 old body boundaries move into the comparison-only object. The six old
register-driven word-batch entries are also comparison-only and no longer
production candidates, so the optional transport path cannot advance caller
registers behind the native task. The remaining address/read batches are
unchanged. `LiveVoiceController` metric reset now preserves its live tasks.
Fabricated partial-frame fuzzing deliberately uses only the test recovery;
actual-call tests deliberately link no synchronized H8/recovery factory.

Actual validation:

- 336,480 complete outer runtime calls / 13,648,207 directly executed original
  H8 outer steps, plus 56 interleaved child calls / 928 child steps. Both DSP
  ports, sequential/post-address variants, every 16-bit address, all byte
  positions, random words, retained masks and mapped/clock-only fetch match
  after every original boundary: values, flags, clocks, PC projection, complete
  data/return accesses and final full memory. Candidate interpreter work and
  test recovery remain zero. Eight simultaneously live tasks, 560 IRQ
  save/RTE/resume points and 336,536 in-flight metric resets pass. Nested
  context interleaving is separate from the actual IRQ/RTE timing checks.
- Full 16-bit polling timeouts exercise the real error handler, its call/return
  and GPIO writes: 24 original fault entries across both ports/variants/fetch
  modes. No fabricated timeout completion replaces this behavior.
- 196,608 public native-task payload/deadline/readiness/address cases pass,
  including no access before a full interval fits, latched admission, wrapped
  address and a complete timeout/fault park/resume with exact own duration.
- Four complete cold/reset/warm/playback timelines pass (normal and irregular
  budgets, Multi and mapped markers). Full RAM/IO/IRQ/Flash/state, exact DSP
  tape and audible 24-stream output match; interpreter/test recovery is zero.
- Nine complete frozen original-H8 tapes match all 8,085,451 events. Actual
  native word calls/returns: Single 33,969; Multi 33,238; MIDI 2,553; pedal
  5,224; controls 6,432; state 2,543; clock/arp 1,437; stealing 25,185; FX
  6,697. All 117,278 return with zero partial recovery. These are work counts,
  not CPU savings.
- Fourteen complete raw-audio timelines match: 6,075,618 frames x 24 streams
  per side, 583,259,328 bytes per side, including 30-second Multi/pedal/clock
  and pedal/FX H8 round trips. State, Flash, MIDI, controller/DSP clocks and
  instruction totals and serial state match as well. All 50 correctness
  stages pass against the frozen source identity.
- Private allocation/sanitizer checks exercise 14,186 sampled outer calls plus
  nested calls, including actual fault/IRQ paths. Zero `new/new[]` allocations
  occur inside native runtime steps. ASan/UBSan pass for the new bridge/task;
  original H8 is excluded and unchanged libraries are uninstrumented.
- Fresh public CTest: 21 pass, eight skip without private ROM, zero fail (29
  selected). The private actual-call/runtime checks run above. UI controller
  checks pass; callbacks at 128/256 remain finite, audible and free of lost
  frames while saving/re-preparing. The callback process CPU collector is
  disabled and C++ selection is asserted.
- Fresh AU/VST3/Standalone build and strict ad-hoc signatures pass. Normal
  target commands exclude the KS H8 comparison object, and `ksLib` has no H8
  core/decoder symbols. Products are not installed or separately hosted
  AU/VST3 audio recordings. K-Station remains unchanged.

Limits: synthetic Flash, zero Wave ROM and explicit mapped boot markers are
admitted; real sampled banks and physical hardware are not established here.
DSP/JIT, Device, B/A order, clocks, resampling, buffers, waits, priorities and
placement are unchanged. Windows build, private fidelity and CPU evidence
remain pending separately; these are Mac results only. Existing probe CPU
fields are ignored. No performance comparison is performed before the full
model removal requested by the user.

Next: lift the remaining address/read/timeout exchange into native jobs, then
convert reset, foreground, parser, note/voice/parameter, clock/arp and state/
Flash callers. Remove every temporary projection and shared register/flag/
stack/PC model. Only after that removal, perform final equality tests, compare
whole-process CPU with direct H8 and push the completed migration.


## 2026-10-16 — Mac: native DSP address/read tasks; historical HDI batches test-only

Test ID: `ks-native-dsp-address-calls-20261016`. Isolated source parent
`2cc9cd5897e0cbc56d61e196f59de076474fb93e` plus address/read task changes;
exact source/binary manifests and private reports remain in ignored output.
Same M1 Pro arm64 Release build, AppleClang 21/C++17 and pinned dependencies.
No CPU comparison or push has been performed; the complete global port remains
active and unfinished.

`DspAddressTask` owns address/request values, A/B read-port selection, both live
readiness phases, retries/timeouts, HF3 request/release and optional three-byte
reply assembly. It contains no CPU registers, flags, stack, PC or instruction
addresses. `DspAddressCalls` remains a temporary old-caller/IRQ/trace projection.
A timeout still parks for the actual existing GPIO fault service; that service
and the caller/cache/selection wrappers still require semantic conversion.
The separate bootstrap firmware-stream loop at `0x23ce` also remains in the
compatibility path. These task connections do not finish the requested global
model removal.

The remaining 68 transport body boundaries move to the comparison object.
All historical HDI batch calculations, accesses and IRQ/deadline guards also
move there. Normal `DspTransport` has no register-driven implementation and
returns no production candidates. Its closed-by-default test hook preserves
historical partial-frame comparisons without exposing fallback in real jobs.
The old event flag remains accepted for diagnostic compatibility; native
address/word/read jobs run independently of it. The opt-in part-lookup jobs
remain unchanged.

Actual validation:

- 819,920 complete outer runtime calls / 47,307,578 directly executed original
  H8 steps, plus 84 explicitly interleaved child calls / 2,460 child steps.
  Every 16-bit address, both DSPs/selectors, all reply byte positions, retained
  masks, readiness delays and mapped/clock-only fetch are exercised. Every
  intermediate value, flag, clock, PC projection, ordered/timed data/return
  access and final full memory match. The actual-call fixture deliberately
  links no synchronized H8/recovery factory; candidate H8 and recovery are zero.
- Both phases exercise full 16-bit timeouts and the real error GPIO handler,
  totaling 72 original fault-service entries. 636 IRQ save/RTE/resume points,
  820,004 in-flight metric resets and eight live tasks pass. Child context
  interleaving is separately scoped from actual IRQ entry timing.
- 393,216 public native-task payload/deadline/dual-readiness/selection cases
  pass, including no premature accesses and exact own durations. Full fault
  park/resume runs independently in each readiness phase.
- Four cold/reset/warm/playback timelines pass (normal/irregular budgets,
  Multi, mapped markers), with full RAM/IO/IRQ/state/Flash, exact DSP tape and
  audible 24-stream output. Candidate H8 and partial recovery remain zero.
- Nine complete frozen H8 DSP tapes match all 8,085,451 events. No retired
  HDI batches or partial recovery execute. Native address admissions in the
  measurement windows: Single 47,921; Multi 28,758; MIDI 53,728; pedal 32,033;
  controls 30,399; state 32,980; clock/arp 29,312; stealing 33,376; FX 32,801
  (321,308 total). The windows record 321,311 returns: calls already in flight
  at metric reset can finish inside the window, or remain live at its end.
  These independent work counters are not paired calls or a CPU result.
- Fourteen complete raw-audio timelines match: 6,075,618 frames x 24 streams
  per side, 583,259,328 bytes per side. Includes 30-second Multi/pedal/clock
  and pedal/FX H8 round trips. State, Flash, MIDI, controller/DSP clocks and
  instruction totals and serial state match. All 52 correctness stages pass
  against the frozen source identity.
- Two additional Single/Multi checks remove both old experiment flags and
  match complete original-H8 tapes plus all raw audio bytes: another 362,724
  frames / 34,821,504 bytes per side across 24 streams. Native address/word
  jobs execute with zero recovery/batches/H8 work on the default path.
- Private allocation/sanitizer probes run 23,288 sampled outer calls plus 84
  nested calls, actual dual-phase fault and IRQ paths. No `new/new[]` occurs
  inside native runtime steps. ASan/UBSan pass for the new address bridge/task;
  original H8 is excluded and unchanged libraries are uninstrumented.
- Fresh selected component CTest: five pass, four skip without private ROM,
  zero fail (nine selected). The private actual-call/runtime cases execute
  above. `ksLib` has no H8 core/decoder symbols and normal product target
  commands exclude the comparison object. This stage rebuilds engine tests;
  AU/VST3/Standalone and callback evidence remains the preceding word-write
  checkpoint, not a fresh validation of this source. Final products/callbacks
  must be rebuilt and revalidated after the full semantic removal.

Limits: same synthetic Flash, zero Wave ROM and explicit mapped boot markers;
real sampled banks/physical hardware are not established. DSP/JIT, Device,
B/A order, clocks, resampling, buffers, waits, priorities and placement are
unchanged. Existing probe CPU fields are ignored. Windows build, private
fidelity and CPU evidence remain pending separately; these are Mac results.

Next: lift the actual transport fault notification, then the remaining caller/
cache/selection, reset, parser, note/voice/parameter, clock/arp and state/Flash
lifecycles. Delete every temporary projection and all shared registers, flags,
stack and PC. Only after that full removal, run final equality/product checks,
compare whole-process CPU with direct H8, and push the completed migration.


## 2026-10-16 — Mac: native DSP timeout notification; old fault bodies test-only

Test ID: `ks-native-dsp-fault-calls-20261016`. Isolated source parent
`48fdf29016707a1d56a496b86d02f3ba55b54e7b` plus the fault task/caller changes;
source/binary manifests and raw private validation remain in ignored output.
Same M1 Pro arm64 Release / AppleClang21 / C++17 and pinned dependencies.
The full controller port remains active; no CPU comparison or push is done.

`DspFaultTask` implements the actual timeout latch/display pulse using semantic
read/write callbacks, native latch/tick values and calibrated admission/release
intervals. No CPU registers, flags, emulated stack, PC, instruction addresses
or device addresses occur in this task. Both read/modify/write observations
remain live; the pulse is not fitted to a constant cached port response.
Independent original execution admits the 122-clock pulse body, the initial
20-clock call, inner 17-clock release and outer 17-clock return: 176 clocks
for the complete service. The task owns 159; the outer caller-return projection
accounts for the final 17 until the old calling convention is removed.

`DspFaultCalls` owns fixed task contexts and temporarily supplies the old
caller/IRQ/stack/trace view, including changing stack depths across the service.
The 18 old wrapper/pulse boundaries move to the comparison-only object.
In-flight tasks survive metric resets. Fresh native jobs reject unowned
partial states; only comparison tests may use the historical partial adapter.
The separate bootstrap transfer and all remaining generic caller state still
need semantic conversion; this checkpoint is not complete model removal.

Actual validation:

- 525,056 complete outer runtime calls / 11,551,936 independent original-H8
  outer steps, plus 224 child calls / 4,928 child steps. Every latch byte x
  every initial flag byte, and every pair of live control bytes, are exercised
  with mapped/clock-only fetch and odd/even SRAM frames. Each intermediate
  register/flag/PC projection, clock, complete data/stack/return access, final
  memory and exact eight GPIO observations/writes match. Normal uninterrupted
  services all consume 176 clocks. No synchronized H8/recovery factory is
  linked, and candidate H8/recovery counters remain zero.
- 704 IRQ save/RTE/resume points, 525,280 in-flight metric resets and eight
  simultaneously live tasks pass. Explicit child context interleaving is
  separately scoped from IRQ-prologue timing.
- 133,120 public task cases pass for native latch/control values, all control
  pairs, deadlines, live reads, output order and 159 own clocks. An interval
  that does not fit performs no access or state advancement.
- Existing actual word/address fixtures now assert completed native fault-job
  counts against the independently derived 16-bit timeout count. All 24 word
  and 72 address-phase fault services run natively, with no partial recovery;
  all original GPIO accesses and state remain exact. Normal musical windows
  contain zero faults; their equality does not replace this forced-error proof.
- Four complete cold/reset/warm/playback cases, nine complete frozen original
  DSP tapes (8,085,451 events) and 14 complete raw-audio timelines pass. Audio
  covers 6,075,618 frames x 24 streams per side / 583,259,328 bytes per side,
  including 30-second Multi/pedal/clock and pedal/FX H8 round trips. State,
  Flash, MIDI, controller/DSP clocks/instructions and serial state match. All
  54 stages pass the frozen source identity, with zero test recovery/batches.
- Single and Multi also match complete tapes and all raw audio bytes without
  the old experiment flags: another 362,724 frames / 34,821,504 bytes per side
  across 24 streams. No retired HDI batch executes on either path.
- Private allocation/sanitizer probes pass 2,816 sampled outer calls plus 224
  children, including all declared IRQ/nested cases: zero `new/new[]` inside
  native runtime steps. ASan/UBSan cover the new fault bridge/task; original
  H8 is excluded and unchanged libraries remain uninstrumented.
- Fresh component CTest: six pass, five skip without private ROM, zero fail
  (11 selected); the private actual-call/runtime cases execute above. `ksLib`
  has no H8 core/decoder symbols and normal product commands exclude the KS
  comparison object. Product/callback evidence is still the earlier word-write
  checkpoint, not a fresh validation of this fault source. Rebuild/revalidate
  final products and callbacks after complete semantic removal.

Limits: same synthetic Flash, zero Wave ROM and mapped boot markers; no new
real sampled-bank or physical-hardware admission. DSP/JIT, Device, B/A order,
clocks, resampling, buffers, waits, priorities and placement remain unchanged.
Probe CPU fields are ignored; no performance comparison is performed. Windows
build/private fidelity/CPU checks remain separate pending work.

Next: native bootstrap byte streaming (`0x23ce`), then its header/boot-ready/
table initialization and remaining reset, caller/cache, parser, note/voice/
parameter, clock/arp and state/Flash lifecycles. Remove every temporary bridge
and shared register/flag/stack/PC model. Only then run final equality/product
checks, compare whole-process CPU with direct H8 and push the full transfer.


## 2026-10-16 — Mac: native DSP firmware byte stream, bootstrap parent pending

Test ID: `ks-native-dsp-boot-stream-20261016`. Source parent
`474d185f0e37b8b413651c579b9e33ddf2974c7c` plus stream task/caller changes;
M1 Pro arm64 Release, AppleClang21/C++17, same pinned dependencies. Frozen
source/binary manifests and private evidence are in
`/private/tmp/ks-native-dsp-boot-stream-20261016`. Automatic approval review
of an escalated test launch timed out; the identical frozen binaries ran with
ordinary permissions and temporary-directory outputs. No work remains blocked
by that transient review failure. No CPU comparison or push is performed.

`DspBootStreamTask` owns logical word count (zero encodes 65,536), native byte
cursor/latched data, live source observations after ready, retry/timeout state
and calibrated duration. No CPU registers, flags, stack, PC or addresses occur
in its state. The temporary bridge binds mapped three-clock ROM/Flash/SRAM
spans and projects old callers/IRQs/traces. One-clock IO and crossing spans are
rejected. Reads are not prefetched and a source byte can change between reads
or after reading, without changing the already latched transmission byte.
The 20 old body boundaries move to comparison-only code. A repeated entry
starts the next word of the existing job, not a new partial-frame replay.

Actual validation:

- 132,336 complete outer calls / 63,156,160 direct original-H8 outer steps,
  plus 112 child calls / 3,312 child steps. All byte seeds/eight retained masks,
  both DSPs, low/high/full-tagged source pointers, live source changes, mapped/
  clock-only fetch, odd/even SRAM frames and count edges through 65,536 words
  match after every boundary. Values/flags/PC projection, cycles, source/data/
  stack/return accesses and final full memory match. No reference/recovery
  factory is linked; candidate H8 and partial recovery remain zero.
- 1,024 IRQ save/RTE/resume points, 132,448 in-flight metric resets and eight
  live tasks pass. Explicit child interleaving is separate from IRQ timing.
  All 48 forced fault services execute complete native fault jobs, including
  the boundary between 65,536 busy reads without timeout and the next busy
  observation that times out. Stream state survives the handler unchanged.
- 163,864 public native-task source/readiness/deadline/mutation/count cases
  pass. Ordinary data uses 96 body clocks per no-busy word, followed by the
  caller's 17-clock return. Insufficient intervals perform no access or
  progress. Full timeout park/resume and multiword count wrap are checked.
- Four complete cold/reset/warm/playback timelines, nine complete frozen H8
  tapes (8,085,451 events) and 14 raw-audio timelines match. Audio is 6,075,618
  frames x 24 streams / 583,259,328 bytes per side, including long Multi/pedal/
  clock and H8 test round trips. State/Flash/MIDI/controller/DSP/serial fields
  match. All 56 stages pass exact frozen-source identity. Each cold start and
  each musical preparation proves both native firmware streams complete:
  1,871 words on A and 2,840 on B, with zero test recovery/interpreter work.
- Additional default Single/Multi cases match complete tapes/raw audio without
  the old flags: another 362,724 frames / 34,821,504 bytes per side. No retired
  HDI batch or partial recovery runs.
- Allocation and ASan/UBSan probes pass 9,456 sampled outer calls plus 112
  children, including large-count/IRQ/fault cases: zero new/new[] inside native
  runtime steps. The new stream bridge/task are instrumented; original H8 is
  excluded and other libraries are uninstrumented. Fresh component CTest:
  seven pass, six skip without private ROM, zero fail (13 selected); actual
  private calls run separately above. `ksLib` has no H8 core/decoder symbols,
  and normal product commands exclude the comparison object.

Product/callback evidence remains the earlier word-write checkpoint, not a
fresh validation of this source. Full semantic removal and final products,
callbacks and Windows build/private fidelity/CPU checks remain pending.
Same synthetic Flash/zero Wave ROM/mapped markers; no new real-bank or physical
hardware admission. DSP/JIT, Device, B/A order, clocks, resampling, buffers,
waits, priorities and placement are unchanged. Probe CPU fields are ignored.

Next: native parent header/delay/boot-ready/frequency-table setup (`0x2332`),
then outer bootstrap/reset and the remaining controller/caller lifecycles.
The parent still has a compatibility frame: native byte loading alone is not
the full requested port. Delete every bridge and shared register/flag/stack/PC
model before final equality/product checks, CPU comparison and push.


## 2026-10-16 — Mac: native DSP header, startup wait and tuning tables

Test ID `ks-native-dsp-boot-setup-20261016`; source parent
`511515cd6f123d7121972e5aed52ff7fa9bc98d6` plus the setup change.
Mac M1 Pro arm64 Release AppleClang21/C++17, pinned dependencies unchanged.
Frozen manifests and private evidence: `/private/tmp/ks-native-dsp-boot-setup-20261016`.
No CPU comparison or push. Windows is not validated by these Mac results.

`DspBootSetupTask` owns header size, stream request, calibrated settling counters,
live boot-ready polling/timeout, address request, both 512-word generated tuning
tables and typed child acknowledgements. Native control flow uses no registers,
CPU flags, stack or PC. The old 54 setup bodies are comparison-only; the bridge
still projects old caller/IRQ state and must disappear in the final migration.
Mutable inputs stay live and children run with their original clocks.

Validation:

- All 65,536 header values and 112 complete table/readiness/deadline/fault cases
  pass. The original setup has 279,056 own clocks excluding children/return.
  Actual firmware A: 1,871 stream words / 546,913 total clocks; B: 2,840 words /
  639,937 clocks. Independent original reference admission confirms these.
- 721 complete actual setup calls / 106,048,286 original outer steps, all 256
  initial flag values, source/count edges through 65,536, both DSPs, mapped/
  clock-only fetch and odd/even SRAM, all 1,024 table words, each access/time
  and final memory match. 206 selected IRQ save/RTE points (103 first-visited
  locations in each fetch mode, not every dynamic boundary), 735 metric resets,
  48 native fault entries and 14 nested setup calls / 1,177,278 child steps,
  with eight live tasks, pass. No synchronized reference/recovery factory.
- Four full cold/reset/warm/playback timelines, nine full DSP tapes (8,085,451
  events), 14 raw-audio timelines (6,075,618 frames x24 / 583,259,328 bytes per
  side) and two default Single/Multi cases (362,724 frames / 34,821,504 bytes)
  match. All 58 main stages and two defaults pass frozen-source identity.
  Each preparation asserts two complete native setups/streams, no interpreter
  or partial recovery. State/Flash/MIDI/controller/DSP/serial fields match.
  The first long Clock capture failed solely because storage was exhausted;
  prior captures were losslessly compressed and that capture plus remaining
  cases reran successfully. The empty failed output is retained separately.
- Allocation and ASan/UBSan probes pass 385 sampled outer calls / 77,399,438
  original outer steps plus 14 children, 206 IRQ points and 48 fault entries:
  zero new/new[] in native steps. Only the new bridge/task is instrumented;
  original H8 and other libraries are not. Component CTest: eight pass, seven
  skip without private ROM, zero fail (15 selected); private actual fixtures
  run above. No H8 core/decoder symbols in ksLib; product commands exclude
  the comparison object.

Fresh products/callbacks remain pending until the full port; last such validation
was the word-write checkpoint. Synthetic Flash/zero Wave and mapped markers do
not prove real banks or physical hardware. DSP/JIT, B/A order, clocks, buffers,
resampling, waits, priorities and placement are unchanged. No raw evidence or
private data is committed. The global register/flags/stack/PC model remains;
next is native reset/select/outer startup, then the other controller domains.
Delete the shared projection model before final comparisons and publication.


## 2026-10-16 — Mac: native reset, port selection and two-image DSP startup

Test ID `ks-native-dsp-startup-20261016`; source parent `3bb71ba` plus the
startup change. Mac M1 Pro arm64 Release AppleClang21/C++17, same pinned
libraries. Frozen source/binaries and private evidence:
`/private/tmp/ks-native-dsp-startup-20261016`. No CPU comparison or push;
Windows is not validated. Full products/callbacks remain pending.

`DspResetTask` owns its single live control observation, retained byte and both
settling intervals. It preserves every unrelated bit and does not add a reset
effect to the current DSP/Board implementation. `DspSelectTask` publishes A/B
and invalidates the sequential mirror. `DspStartupTask` requests reset, image A,
image B and default A in order, waiting for actual child completion. All are
CPU-state-independent; old caller/IRQ projections remain transitional. Forty-five
old reset/selector/outer bodies are comparison-only. Metrics reset preserves jobs.

Validation: independent original timing admission confirms reset 320,353 clocks,
either selector 44, and whole two-DSP startup 1,507,370 including children/returns.
All 256 control bytes pass native deadline/bit-latching checks; both selector
ports and every startup child transition pass. 1,642 actual calls / 70,356,926
direct original-H8 steps, all initial flags/control bytes, both ports, odd/even
SRAM and mapped/clock-only fetch match every access, cycle, projected state and
final memory. Ninety selected first-visited IRQ points (not all loop iterations),
602 metric resets and four nested calls pass. No synchronized reference or
partial recovery is linked to the actual fixture. Production rejects unowned
partial work without mutation. Allocation/ASan/UBSan samples: 202 calls /
19,168,766 original steps, 90 IRQ points, 122 metric resets and four nested
calls; zero new/new[] in native runtime steps. New bridge/tasks instrumented,
original H8 excluded, other libraries uninstrumented.

Four full cold/reset/warm/playback variants (Single, irregular, Multi, mapped)
and existing whole-runtime fuzzing pass. Focused default-path Multi/FX full
DSP tapes/raw audio/state/Flash/MIDI/clocks/serial data match the admitted H8
reference. Every startup proves one complete native reset/outer job plus two
native image/setup children, zero H8 or recovery. All seven focused stages pass
frozen-source identity. The large final musical matrix is deferred until global
model removal, instead of repeating it for each small conversion. Synthetic
Flash/zero Wave and mapped markers retain the previous evidence limitations.
No DSP/JIT, B/A execution order, timing, buffers, resampling, threads, waits,
priorities or placement changes. Raw private evidence stays outside Git.

Next: native DSP address/voice-window preparation, then parser/voice/parameters/
clock/state and the remaining bootstrap/caller lifecycle. The broad register/
flag/stack/PC model still exists; this is not the complete requested migration.


## 2026-10-16 — Mac: native DSP voice-window and command preparation

Test ID `ks-native-dsp-routing-20261016`; source parent `7408f47` plus routing
change, M1 Pro arm64 Release AppleClang21/C++17, same pinned dependencies.
Frozen identity/binaries and private evidence: `/private/tmp/ks-native-dsp-routing-20261016`.
No CPU comparison/push. Windows and fresh products/callbacks are pending.

`DspRoutingTask` owns both signed voice/part address windows, alternate reads,
B voice strides (101/22), A voice/base byte-difference and signed clamp (53/5),
address mirror publication and typed child requests. Mirrors/indices are read
live at the admitted clocks. Preserve the original unconditional address
request even when its mirror compares equal. This is preparation and child
orchestration, not a cached-request optimization. Absolute write preparation
hands address-then-word to existing native wire tasks. Fifty-one old bodies are
test-only; caller/IRQ projections still require removal with their callers.

524,544 native property/deadline cases pass, including every 16-bit address,
every A voice/base byte pair, all B voice bytes and every root. 529,238 complete
actual calls / 13,293,680 direct original steps match every access/value/cycle,
projected flags/state and final full memory, with all initial flags, both DSPs,
odd/even SRAM, mapped/clock-only fetch and mid-task mirror/index mutations.
126 selected first-visited IRQ points, 529,256 metric resets and 18 nested
calls pass. No synchronized reference/recovery factory in the actual fixture.
Native production rejects unowned partial routing without mutation.

Allocation/ASan/UBSan sampled checks: 2,678 calls / 67,024 original steps,
126 IRQ points, 2,696 metric resets and 18 nested calls; zero new/new[] inside
native steps. New routing bridge/task instrumented; original H8 and other
libraries uninstrumented. Four cold/reset/warm variants, whole-runtime fuzzing
and focused default Multi/FX full tapes/raw audio/state/Flash/MIDI/controller/
DSP/serial data match H8. Seven focused stages pass exact frozen source.
The final large musical/product matrix remains deferred until global removal.
Synthetic Flash/zero Wave limitations remain; no real-bank/physical-hardware
proof. DSP/JIT, instruction-boundary timing, B/A order, rate adapters, buffers,
waits, threads, priorities and placement are unchanged.

Next: typed part/voice lookup and control-mirror copy, retire their historical
register-driven batch, then remaining parser/voice/parameters/clock/state and
reset/caller lifecycles. Broad register/flags/stack/PC state remains; this is
not the complete requested port. Raw private evidence is not committed.


## 2026-10-16 — Mac: native part/voice lookup and live control-mirror copy

Test ID `ks-native-part-control-20261016`; source parent
`971fe7e0a76e3dc0e07329644920604bbb461081` plus the part-control change.
M1 Pro arm64 Release AppleClang21/C++17, same pinned dependencies. Frozen
manifests/binaries and private evidence: `/private/tmp/ks-native-part-control-20261016`.
No CPU comparison or push. Windows and fresh product/callback validation pending.

`PartLookupTask` computes native offsets for parameter (334-byte), part-control
(20-byte) and voice-slot (20-byte) layouts, with either a live or provided index.
`PartMirrorTask` performs the 20-byte live copy with original latching/access
order, preserving overlap. Bus publication latency is an explicit input: paired
adjacent byte stores share a transfer, while an intervening writer can break
the pair. Neither task has registers, flags, a stack or PC. Old 29 bodies and
all historical register-driven control batching move to comparison-only code.
Normal batch candidate is false; its ABI is a closed-by-default test hook.
Both legacy experiment flags remain recognized without bypassing native tasks.
Caller/IRQ/trace projections still have to disappear with their remaining callers.

Validation: 1,536 public property/deadline cases cover all selectors/layouts,
provided indices, live byte latching and uninterrupted/intervening-writer timing.
Bodies are 53/42/30 clocks for the lookup forms; contiguous copy is 480,
with every store unpaired 510. Full returns add 17. An initial duration check
caught the omitted 30-clock saving for ten paired stores; a selected IRQ case
then caught an omitted adapter timing argument. Both are fixed, with actual
bus clocks checked at each native lookup/copy boundary.

655,518 complete actual calls / 16,913,534 direct original-H8 steps match every
index x256 initial flags, both fetch modes, odd/even SRAM, live source/index
mutation, self-overlap, every data access/cycle/projected value and final full
memory. 68 selected first-visited IRQ save/RTE points, 655,528 metric resets
and ten nested calls pass. Actual fixture links no synchronized reference or
partial recovery. Production rejects unowned partial copy and every retired
part-batch entry without mutation. Allocation/ASan/UBSan samples: 2,718 calls /
71,294 original steps, 68 IRQ points, 2,728 metric resets, ten nested calls;
zero new/new[] in native runtime steps. New bridge/tasks instrumented;
original H8 excluded, unchanged libraries uninstrumented.

Four complete cold/reset/warm variants, whole-runtime fuzzing (including the
historical batch in its test-only object) and default Multi/FX full tapes/raw
audio/state/Flash/MIDI/controller/DSP/serial data match H8. Seven focused stages
pass frozen-source identity; each musical case exercises real native lookup
and mirror calls without recovery. Fresh component checks: one public pass,
one private-ROM skip, zero fail; the private actual fixture passes separately.
ksLib has no H8 core/decoder symbols and normal product commands exclude the
reference object. The first focused launch ran out of disk while saving the
cold tape; it is retained as an incomplete storage case. Verified duplicates
of earlier generated captures were removed only after exact comparison to
retained copies; the complete current focused suite reran successfully.

No DSP/JIT, timing, B/A order, rates, buffers, threads, waits, priorities or
placement changes. Same synthetic Flash/zero Wave limitations; no new real-bank
or physical-hardware proof. Broad register/flag/stack/PC state remains and
final musical/product/CPU validation and publication remain deferred until
its removal. Next: native velocity/voice parameter calculations and remaining
voice allocation, MIDI/parser/clock/state/reset/caller lifecycles. No private
firmware, tapes, audio or personal reports enter Git.


## 2026-10-16 — Mac: native integer velocity curves and threshold decisions

Test ID `ks-native-velocity-curve-20261016`; source parent `0a0afd2` plus
velocity change. M1 Pro arm64 Release AppleClang21/C++17, pinned dependencies
unchanged. Frozen source/binaries and private evidence:
`/private/tmp/ks-native-velocity-curve-20261016`. No CPU comparison or push;
Windows and fresh products/callbacks remain unvalidated at this stage.

`VelocityCurveTask` owns repeated integer quantized products (including zero
power encoding 256 rounds), inversion, squared/mixed curves, unsigned threshold
comparisons and suppression. Original byte/word overflow is retained, without
floating approximations or input normalization. Mutable threshold cells are
read exactly when needed, never prefetched. Native state has no CPU fields or
addresses; the temporary bridge still projects callers/IRQs/flags/stack/trace.
Its 43 old bodies, including the empty return, are comparison-only. Both new
and former selector counters survive metric resets without losing live jobs.

Validation: 393,216 native formula/deadline cases cover all velocity/curve byte
pairs at powers 0/1/2/3/5/255, including full counter wrap and threshold reads.
525,722 actual complete calls / 114,953,148 independent original-H8 steps
match every access/cycle/projected value and final full memory. Coverage includes
all velocity/curve pairs at powers 1/2/3; all velocity/power pairs with selected
curves; all initial flags, mapped/clock-only fetch, odd/even SRAM, mutable
thresholds and the empty entry. 298 selected first-visited IRQ save/RTE points,
525,224 metric resets and 14 nested calls pass. The actual fixture has no
synchronized H8 or partial-recovery factory; candidate H8/recovery are zero.

Allocation/ASan/UBSan samples: 2,522 calls / 568,934 original steps, 298 IRQ
points, 2,504 metric resets and 14 nested calls; zero new/new[] inside native
runtime steps. New task/bridge instrumented; original H8 excluded and other
libraries uninstrumented. Public component check: one pass, one private-ROM
skip, zero fail; the actual private fixture runs separately above. Production
rejects unowned partial velocity work without mutation. ksLib has no original
H8 symbols and product command lines exclude the reference object.

Four cold/reset/warm variants, whole-runtime fuzzing and default Multi/FX
full tapes/raw audio/state/Flash/MIDI/controller/DSP/serial comparisons pass
exact frozen identity. Each musical case exercises native velocity work with
zero recovery. Extra Multi with both legacy experiment flags enabled matches
the full default tape/audio/state exactly; both historical batch counters are
zero. The final large musical/product matrix is deferred until global removal.
Same synthetic Flash/zero Wave/mapped-marker limits: no new real-bank or
physical-hardware proof. DSP/JIT, original clock/IRQ boundaries, B/A order,
rates, buffers, threads, waits, priorities and placement remain unchanged.

Next: native voice allocation/reuse/stealing and population, then remaining
parameter/MIDI/parser/clock/state/reset/caller lifecycles. Broad compatibility
register/flag/stack/PC storage remains, so this is not the completed requested
migration. Complete those removals before final equality/product checks,
whole-process H8/native CPU comparison and publication to Optimized. Private
firmware, tapes/audio/binaries and personal reports stay outside Git.


## 2026-10-16 — Mac: native voice-slot selection/reuse and gate-off

Test ID `ks-native-voice-selection-20261016`; source parent
`0dd0fc5cf4222399c371b73a261191df70ad544a` plus selection/gate change.
M1 Pro arm64 Release AppleClang21/C++17, same pinned dependencies. Frozen
source/binaries and private evidence: `/private/tmp/ks-native-voice-selection-20261016`.
No CPU comparison or push. Windows and fresh products/callbacks pending.

`VoiceSelectionTask` implements the original four selection modes, held-key
branching, live part/unison ownership, minimum/maximum unsigned packed age,
active-age bits, strict tie behavior, repeated-note reuse, zero count encoding
256 slots and the original release sequence. Mutable indices/slot fields are
observed individually at their admitted clocks; lookup children return typed
part/slot handles. A writer supplies paired-store bus latency. It requests
existing native lookup/routing/wire children and `VoiceGateOffTask`, which owns
the zero gate command. Native core state has no registers, condition flags,
stack, PC or device addresses. Temporary caller projections still bind inputs,
child responses, IRQs and trace context. The 129 old bodies move to tests.
Selection completes at the existing population handoff; it does not claim the
following population/voice-start calculations are already native tasks.

Validation: 72 independently admitted original scenarios cover all four modes,
held/nonheld, nine age/ownership/note shapes and 14/256-slot scans. Native
results and own clocks match each. 8,192 independent random high-level policy
cases and gate command/deadline properties pass. 95,226 actual selection/gate
jobs / 71,766,674 direct original steps cover all modes, held kinds (including
high bytes), ten age shapes, all initial flags, random ownership/note states,
both DSPs, mapped/clock-only fetch, odd/even SRAM and live field mutations.
Every access/value/cycle/projected state and final full memory matches. 4,398
selected first-visited IRQ save/RTE points, 95,236 metric resets and ten nested
jobs pass. Actual fixture links no synchronized H8/recovery factory; candidate
interpreter and recovery counts are zero. Native gate return acknowledges its
parent selection job without requiring another instruction at the handoff.
The first live projection check caught missing two-clock additions for long
branches; corrected projections now verify every native boundary's bus clocks.
Production rejects unowned partial selector/gate states without mutation.

Allocation/ASan/UBSan samples: 9,786 calls / 5,859,014 original steps, 4,398 IRQ
points, 9,796 metric resets and ten nested jobs; zero new/new[] in native steps.
New bridge/core instrumented, original H8 excluded, other libraries uninstrumented.
Public component checks: one pass, one private-ROM skip, zero fail; actual
private fixture passes separately. ksLib has no original H8 symbols and normal
product commands exclude the comparison object.

Four full cold/reset/warm variants, whole-runtime fuzzing and focused default
Multi/FX/voice-steal/pedal full tapes/raw audio/state/Flash/MIDI/controller/DSP/
serial checks pass exact frozen identity (nine focused stages). Every musical
case exercises native selection/gate/velocity/part work with zero recovery.
Verified duplicate captures are retained once with SHA-256/storage manifests;
older cold tapes are losslessly compressed. Synthetic Flash/zero Wave/mapped
markers retain prior limitations: no new real-bank/physical-hardware proof.
No DSP/JIT, clock/IRQ timing, B/A order, rates, buffers, threads, waits,
priorities or placement changes. The final large musical/product matrix and
whole-process direct-H8/native CPU comparison wait for global model removal.

Next: native selected-slot population/transposition/velocity/glide setup, then
remaining voice startup, parameter, MIDI/parser/clock/state/reset/caller paths.
The broad compatibility register/flags/stack/PC model is still present. Complete
its removal before final validation and push to Optimized. Private firmware,
raw reports/tapes/audio/binaries are not committed.
