# Performance measurements (Windows and macOS)

## Paced whole-process AU CPU comparison on macOS

Build the explicit `silver_au_cpu_benchmark` target in a Release arm64 build.
It loads one requested AU bundle directly in-process without installing it:

```sh
AU_BENCH_SECONDS=8 AU_BENCH_PARAMETERS=/absolute/path/parameters.json \
  /absolute/path/silver_au_cpu_benchmark \
  /absolute/path/Instrument.component /absolute/path/plugin.state \
  /absolute/path/private-results/run-1
```

Pass `-` instead of a state file to use the instrument's initial state. The
optional parameter JSON maps exact AU parameter names to AU values **in their
reported ranges**; continuous parameters may be normalized 0..1 while choices
use integer indices. Missing/ambiguous names and out-of-range values fail the
run. Metadata records the parameter ranges, overrides and final effective
values; the runner must check that requested settings actually took effect.

The host uses 48 kHz, 256 frames and one audio thread with the same existing
audio-priority helper for every instrument. It pumps the message loop on the
main thread. Four phases request 0, 1, 8 and 16 held notes (48 upward, velocity
100, channel 1). Each phase has four seconds of paced voice-path warm-up,
followed by 4..60 seconds of measured playback and two seconds of release.
Initial setup, compilation warm-up, parameter enumeration and state snapshots
are outside the CPU windows. `getrusage(RUSAGE_SELF)` differences include user
and system CPU of **all** threads, including the host. CPU seconds divided by
delivered audio seconds, multiplied by 100, use **100% = one logical CPU**.
This is different from render wall time or a DAW's audio-deadline meter.

JSON reports include callback overruns, finite/non-silent output, float audio
hashes and KS's state-exposed lost-frame counter. `missing_frames = -1` means
that another instrument does not expose that counter through this KS-specific
state field; it does not mean zero. Callback overruns, KS lost frames or silent
note phases fail the run. Compare identical KS state/overrides before and after
an optimization; exact AU hashes strengthen the offline 24-stream/bus checks.
For different synths, match basic oscillator/FX/polyphony settings and document
the remaining firmware, native-rate and latency differences. Equal held-key
counts do not establish equal DSP instruction work or identical synthesis.

Use at least three alternating before/after runs without concurrent builds or
profiling. Keep the AU host executable identical across the comparison. Save
bundle, source, host, state and private-input identities alongside the reports.
The `.plist` state and parameter snapshots may contain private presets, full
paths and firmware references: retain them in ignored local output only.

For FX and multipart follow-ups, `AU_BENCH_SCENARIO` accepts an optional JSON
file with `parts` (1 or 4), `phases` (total held-note counts, 0..16), `setup`
(ordered objects containing a `midi` status/data1/data2 triple or an absolute
`sysex_file` path), `probe_parts` and `require_fx_tail` booleans. Setup and
single-channel probes run before parameter overrides and measured playback.
Use admitted dry edit-buffer programs for probes; a previous part's FX tail
must not be mistaken for the next part sounding. Each note is assigned round
robin to the requested parts/channels. Route all test parts to the AU's main
stereo bus, and verify the returned performance/Multi state as well as the
per-channel probes. OsTIrus analog and USB outputs occupy different AU buses.

The host now records `settled_peak`/`settled_rms` after the first quarter of a
render interval. After Note Off it renders a two-second tail before All Sound
Off; `tail_settled_rms`, `tail_settled_peak` and `tail_audio_hash` describe the
last 1.5 seconds, excluding normal host latency and a short amplitude release.
`require_fx_tail` rejects a note phase whose settled tail RMS is below 1e-5.
Tail playback, setup and probes are outside the main CPU window. The new tail
sequence differs from the original host's immediate All Sound Off: freeze one
host binary and compare the new scenarios within the same measurement round.

Diagnostics are opt-in per plugin instance. They do not change guest clocks, JIT
configuration, audio buffers, MIDI scheduling, resampling or thread priorities.
They add instrumentation overhead; compare Off, Summary and Detailed on the same
machine before attributing a marginal dropout to the engine. No audio, firmware,
full file paths or machine/user name is collected. Reports contain CPU/OS/build,
host type, firmware filename and patch label: review these before sharing.

## Capture in a DAW or Standalone

1. Use an optimized build and load K-Station or KS normally. Set the patch, parts,
   buffer profile, DAW buffer and sample rate **before** starting a measurement.
2. Open **Setup > Performance diagnostics > Start new summary measurement**.
3. Play a repeatable MIDI passage for 30–60 seconds. Stop the measurement after
   the passage, including the part where stutters occur.
4. Use **Save performance report...** to write JSON. Saving is on the message
   thread, never on the audio callback or emulation worker. Prefer exporting
   after Stop: the final in-flight operation may finish shortly after Stop.
5. Repeat with **Start new detailed measurement**. Every Start starts a fresh
   session. Deep timers sample every 256th board advance; summary has no deep
   clock reads. Reopening the editor does not stop a recording.

Firmware reload or an engine/audio reconfiguration stops the session; its report
remains available with `stop_reason=engine_rebuilt_or_stopped`. Use a new session
after the change. Parameter/preset/profile/part changes do not create new sessions:
hold them fixed for comparisons, or document the changes as a separate scenario.
Diagnostics mode is not saved in DAW state or preferences, and is off in a new
instance. Recording begins after firmware warm-up, so boot time is excluded.

Record alongside **each** report: test ID, DAW and version or Standalone, audio
driver/backend (ASIO/WASAPI/CoreAudio), device, actual buffer and sample rate,
patch/bank identity, MIDI passage/duration, part count, number of instances,
UI open/closed, power mode, audible outcome and other running CPU-heavy tasks.
Host detection is best-effort and does not determine a DAW version or driver.
Use the same passage and settings on Windows/macOS, but compare within each
machine first; absolute times from different CPUs are not equivalent evidence.

## Reading `silver-k.performance.v1`

Durations are **wall time in nanoseconds**, including OS preemption. They are not
thread CPU usage. Each metric includes observations, total, mean, minimum,
maximum, p99 upper bound, work frames and over-budget observations. Percentiles
use fixed logarithmic bins; the upper bound can be nearly twice the real value.
Use maximum and over-budget counts as well. Zero observations means **not
measured**, not free. Live atomic reads are approximate, not a single instant.

| Metric | Meaning |
| --- | --- |
| `audio_callback` | Complete `processBlock`, deadline = actual block frames / host rate. Includes MIDI/control and rendering. |
| `audio_controls` | MIDI/control/snapshot work before rendering; excludes audio rendering. K host-clock preparation is outside this sub-timer. |
| `audio_render` | Host-side queue consumption; K also includes input/output resampling. Offline waits are included. |
| `worker_segment` | Contiguous worker work between waits, split after at most 256 output frames. K units are native frames; KS units are host frames. |
| `worker_sleep` | Total idle wait: K on Windows waits for progress with WaitOnAddress (10 ms safety timeout); KS, other platforms and the polling override retain the 50 µs sleep. This is not wake-up latency. |
| `worker_wake_latency` | K on Windows only: time from the first notification of an armed wait to the worker returning, including scheduler delay. Available with both address waiting and the polling override. No observation for an unnotified timeout or skipped wait. KS retains its original sleep and records no observations here. |
| `worker_snapshot` | Publishing control/flash snapshots; may overlap inclusive worker time. |
| `h8_sampled` | Sampled controller execution including its peripheral tick work. |
| `dsp_a_sampled`, `dsp_b_sampled` | Sampled DSP execution including peripheral work and any inline JIT compilation. B is absent for K. |
| `serial_sampled` | KS shared serial-clock servicing, measured separately from DSP execution. Absent for K. |
| `dsp_barrier_sampled` | Experimental two-worker KS only, every 256th advance in Detailed: coordinator dispatch plus waiting for both DSP slices. Includes DSP execution and scheduling; overlaps DSP timers and must not be summed with them. Zero observations in sequential mode. |
| `queue_depth` | Ready output frames at entry to device rendering, not minimum depth over all samples. K can render multiple chunks per callback. |
| `missing_frames` | Frames substituted by silence during this session; total is the count, observations are device calls. |
| `stale_frames` | Already-late output frames discarded by the consumer. |
| `host_block_frames` | Actual callback block sizes, useful when prepared maximum differs from live sizes. |
| `offline_callback` | Total counts callbacks marked non-realtime by the host. Interpret their deadlines separately. |

`worker_wall_to_audio_ratio` = measured worker segment wall time / duration of the
frames it generated. Above 1 suggests insufficient throughput in those segments;
it can also reflect preemption. Below 1 with missing frames can suggest scheduling,
bursts, queue reserve or a rare slow path. This ratio is **not total CPU %** and
excludes sleeps, some loop bookkeeping, and an unfinished segment at Stop.
K snapshot work preceding segment start can be outside the ratio. Do not sum the
inclusive worker/callback metrics with their child timers.

Deep sampling is deterministic and may miss a rare cold-JIT spike or repeatedly
sample a particular firmware phase. Repeat runs; do not extrapolate a precise CPU
breakdown from one capture. JIT compilation is not separately instrumented in this
phase; a slow DSP sample alone does not prove that the JIT compiler was responsible.

## Opt-in KS dual-DSP worker experiment

Set `SILVER_KS_DSP_THREADS=2` in the launching process environment **before**
opening Standalone or the DAW. It is read once for each KS device. Unset it or
set it to `1`, then reload the instance to restore sequential execution. The
setting is not saved in presets/preferences/DAW state and has no effect on K.
This is an experiment, not an enabled-by-default optimization.

The first 2026-10-15 M1 Pro round failed all eight original two-helper realtime
captures. The subsequent Mac handoff fix passes twelve paced captures plus
exact command/audio/state comparisons: Mac executes DSP A on the coordinator
and DSP B on one persistent helper, with bounded 8192-probe polling before the
existing sleep. Other platforms retain two helpers and 256 probes. Total Mac
CPU still exceeds sequential (~33% in the paced control), and ordinary-priority
VST3 CLI-host captures remain intermittent; keep the experiment off by default.
Read the latest Mac handoff entry in `OPTIMIZATION_LOG.md` for actual limits.

The original H8, DSP boot and warm-up stay sequential. Afterwards the two DSP
executors perform only each DSP's catch-up at the existing H8 slice boundary.
On Mac the coordinator is also the DSP A executor; a helper executes DSP B.
On other platforms both DSPs use separate persistent helpers.
The coordinator waits for both, then services shared serial IO in the original
order. Guest targets, JIT, buffers, polling and existing worker priorities stay
unchanged; helpers use the same live priority as the existing worker. No Windows
setting or process placement is changed. The previous P-core option targets
the coordinator only; do not interpret it as verified placement of DSP helpers.

JSON `dsp_execution_backend` in the root/session identifies `sequential`,
`two-workers-slice-barrier`, or `not-applicable` for K. The parallel identifier
is retained on Mac despite its coordinator/one-helper ownership. In parallel mode the
worker wall/audio ratio measures coordinator wall time including its DSP waits,
not summed CPU usage across three threads. Detailed DSP timers run on their
respective execution threads and overlap. Compare process CPU separately and distinguish
steady playback from the unchanged sequential boot/warm-up cost.

`tools/run_performance_matrix.py --dsp-threads 2` verifies the reported backend.
`tools/run_dsp_thread_comparison.py` interleaves frozen original, same-binary
sequential and two-worker cases with ordinary placement, clearing inherited
wait/placement/thread overrides. It records executable SHA-256, diagnostics,
fixture results and, on Windows, whole-process CPU seconds **including boot**.
It runs the existing single-part 12 s synthetic fixture, not a real ASIO/VST3
host or a reproduction on the tester's Ryzen. Raw evidence stays ignored.

Build/run `ksDspWorkers_test` for ROM-free handoff/failure/shutdown checks.
`ksDspEquivalence_test <KS update/decoded OS> <wave> <Flash>` compares exact
24-stream audio, H8/DSP timing, serial counters and state in Single and four-part
passages; this private-input test returns 77 without arguments. Do not commit
firmware, wave banks or captured audio. Windows and macOS require separate
build/runtime verification; portable synchronization code alone is not validation.

For Mac paced CPU comparisons, `silverRealtime_test` prints whole-process
`process_cpu_seconds` and `process_wall_seconds`, measured outside the callback
around playback only. Set `SILVER_REALTIME_SAVE=0`, use fresh `SILVER_K_DATA` per
run, interleave `SILVER_KS_DSP_THREADS=1`/`2`, and require equal delivered frames
and zero missed frames before comparing CPU. Other platforms print CPU=-1.
Keep continuous offline-probe timing distinct from this paced audio workload.

The Mac VST3 checker can model an audio-priority caller with
`SILVER_TEST_HOST_AUDIO_PRIORITY=1` in realtime mode. It checks the priority/
period setters and prints `host_audio_priority=1`; default `0` keeps the old
ordinary-priority main-thread caller. This is an explicit test-host distinction,
not a plugin-priority change or a substitute for a real DAW/device test.

## Equal-work CPU cost (Windows measurement tools)

Build `ksCpuCost_test` explicitly. Run `tools/run_cpu_cost_comparison.py`
with `--modes native --executable <ksCpuCost_test.exe> --ks-rom <KS update/decoded
OS> --flash <Flash> --wave <wave bank>`. The native target expects an unzipped
supported update or decoded OS. Default three interleaved runs per mode compare
the same binary with `SILVER_KS_DSP_THREADS=1` and `2`, clearing placement/wakeup
overrides. This does not change production code or Windows policy.

The fixture renders 544,085 native frames (about 12 s) through the original H8
and both DSPs, hashes all 24 integer streams, and counts **all process threads'
user + kernel CPU seconds** via GetProcessTimes. Boot/setup, warm-up, helper
startup, formatting and file IO are outside the measured interval. Wall duration
is separate. There is no host queue, resampler, missing-frame fallback, pacing
or diagnostic session in this native cost test. It measures active emulator
cost; it is not a DAW CPU meter or power measurement. Check final guest cycles/
instructions as well as audio hashes, and keep CPU and wall-time changes distinct.

`summary.json` sets `valid_equal_work` only if all runs complete with the expected
frame count, zero missing frames and identical audio hashes. Never claim a saving
from invalid comparisons. Keep raw reports and supplied firmware local/ignored.

`silverCpuCost_test` and the runner's `offline`/`paced` modes are exploratory
full-Processor measurements. They also record caller CPU (callback, hashing,
offline queue waits and pacing); process-minus-caller includes all background
threads and is not a precise emulator-only timer. The current host runs differ
in audio hashes even between sequential repeats, so they do **not** validate a
strict CPU saving. See the dated journal before reusing them. These CPU-accounting
fixtures currently run on Windows only; macOS returns 77 and needs separate work.

## Command-line fixture

Build `silverRealtime_test` explicitly (it is excluded from the default build).
It runs the existing 12-second synthetic passage at 48 kHz, with a first-note
phase, eight-note chords, optional state saves, and a prepare call at second 6.
This is a regression fixture, not a substitute for a real DAW/driver test and not
a four-part KS benchmark. It requires private original firmware arguments; never
commit them. The JSON path must be absolute and its parent must exist.

Windows PowerShell, with paths replaced by your local private files:

```powershell
$env:SILVER_K_DATA = "$PWD/output/perf-fixture-data"
$env:SILVER_REALTIME_K = '1' # 1=K, 0=KS
$env:SILVER_REALTIME_BLOCK = '128'
$env:SILVER_REALTIME_PROFILE = '1' # 0 Low, 1 Balanced, 2 Safe, 3 Maximum
$env:SILVER_REALTIME_SAVE = '0'
$env:SILVER_PERF_MODE = '1' # 1 summary, 2 detailed
New-Item -ItemType Directory -Force output/perf | Out-Null
$env:SILVER_PERF_REPORT = "$PWD/output/perf/win-k-128-summary.json"
& 'build/source/novation/silverK/buildStandalone/silverPlugin/Release/silverRealtime_test.exe' 'C:/private/K.os' 'C:/private/KS.syx' 'C:/private/ks-init-flash.bin' 'C:/private/ks-wave-zero.bin'
```

For a single-configuration NMake build, omit `/Release` from the executable path.
On macOS the same environment variables and arguments apply:

```sh
mkdir -p output/perf
SILVER_K_DATA="$PWD/output/perf-fixture-data" \
SILVER_REALTIME_K=1 SILVER_REALTIME_BLOCK=128 SILVER_REALTIME_PROFILE=1 \
SILVER_REALTIME_SAVE=0 SILVER_PERF_MODE=1 \
SILVER_PERF_REPORT="$PWD/output/perf/mac-k-128-summary.json" \
build/source/novation/silverK/buildStandalone/silverPlugin/silverRealtime_test \
/private/K.os /private/KS.syx /private/ks-init-flash.bin /private/ks-wave-zero.bin
```

Repeat for K/KS and buffers 128/256/512, summary/detailed and Off control runs
(omit `SILVER_PERF_REPORT` for Off). Compare no state-save and state-save runs.
In a DAW add KS Performance with four parts and UI closed/open. Keep the initial
note phase separate from sustained playback in listening notes; this first report
format aggregates the session rather than emitting a sample-by-sample timeline.

For a sequential matrix on either OS, `tools/run_performance_matrix.py` accepts
`--executable`, `--k-rom`, `--ks-rom`, `--flash`, `--wave` (all file paths),
`--output output/perf`, `--blocks 128 256 512`, `--models K KS`,
`--modes off summary detailed`, `--repeats 2`, `--profile 1` and optional
`--save-state`. It creates a fresh timestamped result directory, isolates fixture
preferences, saves JSON/logs and an `index.json`, and continues across failed
fixtures. Exit 1 means at least one failed run or missing/invalid report; inspect
the reports rather than discarding evidence from a failed realtime run. Off
reports have no active session and zero recorded metrics: use their fixture
summary for baseline callback time and missing-frame counts.

## Windows worker wake-up comparison

Windows 8+ uses dynamically resolved [WaitOnAddress](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress) / [WakeByAddressSingle](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle).
The K worker arms its wait, rechecks progress/stop/pause, then parks. KS retains
the original polling wait (`poll-50us`), without the new arm/recheck/notification
path: event parking failed its 64-frame Low and 128-frame Balanced tests in
extended repeats. Polling also failed intermittently in later trials, so causality
is unresolved. A reserve-duration guard was tested and rejected too. Neither
reserve sizes nor plugin latency are increased.
The callback
notifies after publishing the consumed block; offline consumption and control
pause/stop paths also notify. No callback wait, timer-resolution change, busy
spin or thread-priority change is introduced. A 10 ms timeout is a safety path
when no notifications arrive, not the normal refill cadence. Hosts without the
Windows API and non-Windows platforms retain the old polling behavior.

For matched A/B measurements on the same Windows executable use the runner's
`--worker-wait poll` and `--worker-wait address` in separate result folders.
It sets `SILVER_WORKER_WAIT=poll` for the former; the latter selects the default
address backend and verifies `worker_wait_backend` in the JSON. Read the override
once before creating an instance; it is not saved in plugin state or preferences.
Reports record the actual backend even with diagnostics Off. Do not set the poll
override in the DAW's environment for normal production use.
The `address` request does not bypass KS's polling policy: the runner refuses to
label KS polling as address waiting. Use `--worker-wait default` for a mixed K/KS
matrix and `--models K --worker-wait address` for a forced address comparison.

Compare `worker_wake_latency` (mean, histogram p99 bound and maximum), missing
frames, queue depth, callback deadlines and worker ratio. Longer total idle
waits can be healthy: the worker now sleeps until a block is consumed instead
of repeatedly polling a full queue. Do not treat the change in average
`worker_sleep` alone as a speedup. Preserve an executable/hash from before the
change as a further control; the same-binary poll override includes the new
notification telemetry and is not bit-identical to the original polling code.

## Windows scheduler traces for KS

The 2026-10-15 comparison found the sustained failure in the frozen original,
the rejected address prototype and its forced-poll control. See the journal's
individual cases: heavy failures coincide with near-continuous execution on an
E-core, while reconstructed ready delay is small. This is evidence for the next
plugin investigation, not a runtime fix. Keep KS production polling for now.

Use the standalone native ETW controller when built-in WPR 19041 fails to save
with `0x80010106`. The PowerShell WPR wrapper is retained as an unsuccessful
experiment; it does not repair that COM failure. Do not change registry policy,
install components, restart Windows, change power settings or apply affinity
as part of this measurement workflow.

In a Visual Studio developer shell at the repository root, **before recording**:

```powershell
New-Item -ItemType Directory -Force output/perf-scheduler | Out-Null
cl /nologo /std:c++17 /EHsc /O2 tools/record_scheduler_etw.cpp /Fo:output/perf-scheduler/record_scheduler_etw.obj /Fe:output/perf-scheduler/record_scheduler_etw.exe /link advapi32.lib rpcrt4.lib
cl /nologo /std:c++17 /EHsc /O2 tools/decode_scheduler_etw.cpp /Fo:output/perf-scheduler/decode_scheduler_etw.obj /Fe:output/perf-scheduler/decode_scheduler_etw.exe /link advapi32.lib tdh.lib
```

Only elevate the recorder. Continue running the benchmark from the ordinary
shell. The recorder has a private logger GUID/name, automatically stops after
the requested duration (maximum 600 s), accepts a STOP file, caps its sequential
ETL at 512 MB and stops only its own session handle. It never calls a system-wide
WPR cancel, changes the sampling interval or modifies scheduling policy.
Use a fresh directory and retain all existing evidence:

```powershell
$traceDirectory = Join-Path $PWD ('output/perf-scheduler/ks-' + (Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory $traceDirectory | Out-Null
$traceExe = Join-Path $PWD output/perf-scheduler/record_scheduler_etw.exe
Start-Process $traceExe -Verb RunAs -WindowStyle Hidden -ArgumentList ('"' + $traceDirectory + '" 240 sampled')
# Confirm Windows UAC and wait for native-status.txt to say "recording".
# Omit "sampled" for the minimal CSwitch/ReadyThread trace.
```

Then run `tools/run_scheduler_comparison.py` in the unelevated shell with
`--session $traceDirectory`, `--original` and `--candidate` executable paths,
plus `--k-rom`, `--ks-rom`, `--flash` and `--wave` private input paths. It records
CPU topology, PID, process/session boundaries and executable hashes, then runs
the same eight interleaved KS / 64 / Low / Summary / 12 s cases used in the journal.
Exit 1 can be the desired reproduced audio failure: retain its reports.

The candidate must be the archived **rejected KS address prototype**, whose hash
is in the journal. The current production binary always polls for KS, and the
runner refuses to label it as an address run. Do not modify the production
source to recreate that experiment. Both executable versions must support the
existing fixture's diagnostic JSON interface and use the same private assets.

After `native-status.txt` says `complete 0`, check `native-recorder.log` for zero
EventsLost / LogBuffersLost / RealTimeBuffersLost. Failed start/stop or lost events
must not be presented as a valid precise scheduling comparison. Then decode
offline, to new filenames:

```powershell
& output/perf-scheduler/decode_scheduler_etw.exe (Join-Path $traceDirectory scheduler.etl) (Join-Path $traceDirectory events.tsv) (Join-Path $traceDirectory schemas.txt)
python tools/analyze_scheduler_trace.py $traceDirectory
```

The decoder also requires zero unsupported records. Known fixed layouts are
audited in `schemas.txt` when TDH MOF lookup is unavailable; unknown versions or
sizes fail the decode. The analyzer qualifies the worker name by process and
lifetime, reconstructs ReadyThread-to-dispatch and ready preemption intervals,
and separates waiting from scheduled time. Its steady window is diagnostic
seconds 4–12; the fixture's missing-frame warm-up boundary is approximate.
It currently supports x64, one processor group. Use WPA for other layouts.

`scheduled_ms` includes any interrupts during a scheduled slice; it is not exact
thread CPU time. CPU samples separately count user/kernel addresses, DPC/ISR
flags and observed priority, without symbolizing or tuning JIT code. Compare
core placement and 250 ms bins alongside missing frames; avoid interpreting
run/wait totals alone as proof of causation. No per-missing-block timeline is
available in the archived executables. Trace overhead, power/thermal state and
case order remain confounders. Raw ETLs, process metadata and private fixture
data stay under ignored local output, with only sanitized findings in Git.

## Opt-in KS P-core placement experiment (Windows)

`SILVER_KS_WORKER_CORES=p` requests performance-class CPU Sets for the **KS
emulation worker only**, after the original boot/warm-up. Default/unset retains
normal scheduling; K and non-Windows behavior are unchanged. This is an
experiment, not a saved plugin preference or an automatic production policy.
No priority, wait strategy, reserve, JIT or guest timing changes accompany it.

The worker enumerates topology using `GetSystemCpuSetInformation`, selects the
highest [EfficiencyClass](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_cpu_set_information)
on a heterogeneous single-group system, and calls
[`SetThreadSelectedCpuSets`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadselectedcpusets)
on the current worker. It respects the existing
process/thread hard mask and declines to override an explicit host CPU-set
assignment. Unavailable APIs, homogeneous/multiple-group topology and unavailable
P-cores fall back with a recorded status. CPU-set IDs are discovered at runtime.
No process-default, system policy or hard-affinity setter is used.

Reports contain `worker_placement` in the session and at the top level:
`requested`, `status`, `win32_error`, `selected_cpu_set_ids`. Status `p-cpu-sets`
means the API accepted the request; actual P/E execution must still be checked
in ETW because CPU Sets are compatible with Windows power management.

For matched tests, the scheduler runner accepts `--comparison placement`.
Pass the frozen original as `--original` and the new opt-in-capable fixture as
`--candidate`. It interleaves three same-binary default controls, three P-core
requests, and two frozen controls. All use the original KS polling wait.
`--untraced` runs the identical sequence in a fresh directory without starting
a collector; otherwise use the bounded native ETW recorder as above. A P-core
request that did not apply is rejected rather than mislabeled. Keep every
control and failed report, and verify topology/placement before drawing a result.

The 2026-10-15 matched experiment found zero steady misses in all six P-mode
runs (three traced, three untraced). ETW confirmed P-only execution for the traced
runs. Two untraced P runs still lost 78 and 33 frames in the initial ~4 s window.
Same-binary untraced default controls lost 138817 and 303678 steady frames in
two runs. This supports the placement hypothesis on this machine, while leaving
startup losses, real DAW/multiple-instance behavior and other platforms open.
The general matrix runner also accepts `--worker-cores p`; default clears an
inherited override. It checks KS's applied status and retains fallback reports.

## KS H8 bus capture and recorded C++ controller prototype

The normal plugin still defaults to original H8. Recorded-scenario playback
has no plugin preference or environment switch (the separate live hybrid is
described below). The explicit Board API and
private-input `ksControllerProbe_test` fixture opt into a finite recorded
scenario: `h8` is the uninstrumented reference, `record` captures original H8,
and `replay` executes the captured host-port accesses in C++ without stepping H8.
`boot` captures original boot only; pre-boot playback is not implemented.
Use a fresh Board with original H8 to disable the experiment. Do not switch a
replayed Board back to H8: its CPU/peripheral state has intentionally frozen.

Recording covers both DSP host windows, reads and writes, original controller
cycle/PC, byte offset/value, write acceptance and actual H8 slice boundaries.
The existing DSP catch-up and serial ordering remain common to both modes.
Playback checks every returned DSP byte and write acceptance. Mismatch, trace
exhaustion or a budget other than the original 128 cycles stops playback;
there is no silent fallback. Live MIDI/part-MIDI is rejected, and state/Flash
queries and boot are unavailable in this mode. It cannot yet replace H8 for
arbitrary playing, patch edits, LCD, MIDI output or session restoration.

The starting fingerprint covers firmware/Flash/waves, selected H8 RAM and IO,
clocks, serial state, queue sizes and DSP counters. It is **not** a complete
serialized machine snapshot: registers, DSP memories and queue contents are
not all hashed. Always recreate the identical deterministic setup with the
same executable and inputs; passing the fingerprint alone does not prove
equivalence. Compare all 24 audio streams, DSP counters and port responses.

The recorder reserves bounded buffers before capture (3 million events and
1 million slices, about 68.7 MiB payload at the default maximum); overflow makes
the trace incomplete and unusable. There is no file IO in the measured render.
`KSHTR001` files have explicit little-endian fields, bounded counts/length and
a checksum. Existing trace/report files are refused. Raw values can contain
private firmware and presets: keep tapes, reports and binaries under ignored
`output/perf-controller/`, never in Git.

Build `ksControllerProbe_test` explicitly (excluded from the normal build).
The ROM-free `ksControllerTrace_test` / CTest `ks_controller_trace` verifies the
format, both DSP routes and failure guards without private assets. Example:

```powershell
cmake --build build-nmake --target ksControllerProbe_test ksControllerTrace_test
python tools/run_controller_comparison.py --executable build-nmake/source/novation/silverK/buildStandalone/silverPlugin/ksControllerProbe_test.exe --ks-rom output/perf-dsp-threads/private-KSOS2102.SYX --flash "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-init-flash.bin" --wave "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-wave-zero.bin"
```

The runner creates a fresh directory, captures boot, then Single/four-part
passages and three interleaved H8/replay pairs per passage. Every process first
boots and prepares through H8; the measured 90,681-frame native passage is
about two seconds, eight notes total, with sustain, program and control changes.
Boot/setup, trace loading/saving and report generation are excluded. Windows
`GetProcessTimes` counts whole-process user+kernel CPU; wall duration is separate.
On macOS CPU accounting is not implemented: zero in this field is unavailable,
not a measured saving. Build/audio portability still requires validation there.

Default comparison uses sequential DSPs to isolate the controller. Add
`--dsp-threads 2` for an independent compatibility check; do not pool those
results. The runner clears inherited placement/wait overrides and never changes
OS policy. Stop concurrent builds/tests before collecting performance evidence.
It requires matching audio hashes/peaks, frame counts, DSP cycles/instructions,
virtual controller endpoint and serial counts; replay must consume every port
and keep original H8 cycles frozen. These checks do not compare complete DSP
memory. Early reports named their aggregate check `exact_audio_and_dsp_state`;
the runner now accurately names it `exact_audio_and_dsp_counters`.

Recorded commands precompute the original firmware decisions. Any saving is
evidence for this finite playback prototype, **not** an estimate for a completed
live native controller. Next work is to decode the observed flag/data protocol
and implement live MIDI-to-DSP decisions against the H8 reference.

## Opt-in live KS note/sustain controller (hybrid)

`SILVER_KS_CONTROLLER=native-midi` opts a newly created KS instance into the live
C++ MIDI decoder, Note On/Off/sustain handlers, voice lifecycle and DSP host transport after original H8 boot. Unset it and recreate the instance
to return to original H8. K is unchanged. This process-local environment switch
is not a saved preference and does not change Windows settings. The explicit
worker-owned `Board::enableLiveMidiController(bool)` API can also switch at a
quiescent boundary: the guest ABI stays current, including partial messages.
Recorded playback and this live mode are mutually exclusive.
Performance session/root JSON records `controller_backend` as `original-h8`
or `live-cpp-midi-hybrid` (K: not applicable). The existing H8 timing bucket
includes this C++ controller work when the hybrid is selected; it is not an
isolated measurement of H8 instructions in that mode.

This stage ports the OS 2.1.02 byte decoder, Note On/Off decisions and key/pedal
state into C++. It handles keyboard range, velocity-zero release, ordered key
and velocity memory, physical/held bitmaps, mono replacement-note search,
sustain threshold 63/64, pedal-up held-voice release and part release/reset.
It also selects/reuses/steals voices (Mono, Mono auto, Poly, Poly retrigger),
maintains the voice slots and velocity curves, performs the note-start/glide
and release command sequence, selects DSP/voice address windows, and transfers
24-bit host data/status/address words to both DSPs. It accepts live values and
does not load a tape. UART, channel/part routing, the bulk parameter calculators,
periodic voice servicing, arpeggiator/clock and preset/state loaders still run
in original H8. Native voice-start calls into those remaining calculators;
porting the command transport alone does not remove that dependency.
Other message types (program, pressure, bend, SysEx, system common/realtime)
retain original H8 decoding/dispatch; shared voice/transport helpers run in C++
when reached from those paths. It is **not** a complete native DSP controller
and cannot run with H8 stopped. No H8 CPU-core or DSP JIT source/config changed.

The C++ controller preserves original instruction boundaries, fetch/data bus
cycles, registers, flags and call ABI. Pending interrupts go through original
H8 arbitration; both DSPs keep the same slice targets and serial ordering.
A fingerprint rejects unverified parser, note/pedal, voice lifecycle, transport and helper revisions. This
conservative ABI bridge allows the original handlers to remain the oracle
while later stages port DSP voice/parameter logic. It is a compatibility step;
do not reuse the recorded-playback saving as a live-controller result.

`ksLiveMidiController_test` is a private-input isolated differential test:

```powershell
cmake --build build-nmake --target ksLiveMidiController_test ksControllerProbe_test
& build-nmake/source/novation/silverK/buildStandalone/silverPlugin/ksLiveMidiController_test.exe output/perf-dsp-threads/private-KSOS2102.SYX
python tools/run_live_controller_comparison.py --executable build-nmake/source/novation/silverK/buildStandalone/silverPlugin/ksControllerProbe_test.exe --ks-rom output/perf-dsp-threads/private-KSOS2102.SYX --flash "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-init-flash.bin" --wave "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-wave-zero.bin"
```

The isolated test compares C++ against independent original-H8 decoding at
every boundary (registers/flags/PC/clocks and each bus access, including its clock and in-flight PC), all 16 channels,
all 128 data values, varied velocities/CC values and running status. No private
firmware is embedded. The no-argument CTest returns skip 77; run it explicitly
with private OS for evidence. Normal CI cannot validate this private-ROM case.
It also compares all 462 fixed note/pedal and 640 voice/transport transitions,
32 varied operand/flag sets each, at their real call boundaries. No DSP engine is stubbed to make the
full-Board audio comparison pass.

The runner first compares original-H8 `record` with `live-record`: complete
binary tapes must match byte-for-byte, including reads, writes, acceptance,
cycle/PC and slice boundaries. It then runs three interleaved unrecorded pairs,
requiring exact audio hashes/peaks across all 24 streams, guest and serial
counters. `single`/`multi` retain the earlier passage; `midi` adds fragmented
messages, running status, repeated pitches, velocity zero, CC 1/7/10/11/64/
105/120/123, program and system-message fallback. Eight notes total maximum in
the chord passages; no claim for all possible patches/voices/CC combinations.
`pedal` exercises four parts with Mono, Mono autoglide, Poly and Poly retrigger,
key range 36..84 plus out-of-range notes, repeated keys, velocity zero, pedal
63/64/127/0 and repeated pedal changes, CC120/123 with and without sustain.
Default `--seconds 4` renders 181,362 native frames for recordings and each
CPU/audio run; `--seconds 2` reproduces the shorter work. Recording is bounded
to 2..4 seconds to fit the existing trace capacities, with overflow rejected.
Reports expose native decoder and actual note/pedal-handler counts. Add
`--dsp-threads 2 --repeats 1` for separate helper compatibility validation.
Raw tapes/failed trials stay ignored, following the same private-data rules.
Add `--toggle --repeats 1` to verify disabling at 0.65 s and re-enabling at
0.95 s against the same uninterrupted H8 reference. The isolated differential
test also alternates native/original execution inside partial messages.

## Keeping results current

Update `doc/OPTIMIZATION_LOG.md` for each implementation or measurement round.
Keep raw reports under ignored `output/perf/` or elsewhere locally; commit only
sanitized findings and the exact reproduction conditions. Build metadata records
the Git revision and dirty state at CMake configure time, configuration and
compiler. Reconfigure after source changes; on a dirty tree attach the patch or
commit it before comparisons. Dependencies may be overridden with local source:
the report's DSP revision is the repository pin, not verification of an override.


### Historical hybrid status (2026-10-15, voice lifecycle stage / 6d93694)

At that revision this was an **incomplete, opt-in hybrid migration**. The public
switch and `live-cpp-midi-hybrid` backend name are preserved for compatibility.
H8 remains necessary during playback in that historical hybrid mode. The
separate complete post-boot mode is documented below.

| Responsibility | Current execution |
| --- | --- |
| Note/CC byte decoding; Note On/Off, held keys and sustain | C++ for the verified native ranges; original paths for other message types |
| Voice selection/stealing, velocity curve, slot initialization, note start/release | C++ lifecycle; calls remaining H8 parameter routines |
| DSP A/B host address, status, 24-bit read/write and voice address selection | C++, same bus timing, polling and timeout call targets |
| Oscillator/filter/envelope/LFO/FX parameter calculation and periodic voice maintenance | H8 |
| Channel/part dispatch, remaining CC/NRPN, bend/pressure, MIDI clock and arpeggiator | H8 |
| Program/performance changes, SysEx, state/Flash serialization and boot | H8 with the existing host adapters |
| Interrupt arbitration, timers and UART | Original H8/Board timing |

The private controller probe reports `native_voice_steps`,
`native_allocation_calls`, `native_voice_start_calls`,
`native_voice_release_calls`, `native_transport_steps` and
`original_h8_step_calls`. The last field counts calls to the original core,
including interrupt handling; it is neither an instruction count nor a CPU
measurement. Bus-recording probes explicitly enable `original_h8_pages`
(page number = guest PC >> 8). Page counts add diagnostic work; the vector is empty
in unrecorded timing probes and normal plugin instances. Switching off/on
starts a new counter segment, so toggle reports cover the last enabled segment.

Completion requires implementing the remaining responsibilities, then
validating real state/preset/clock/arp/voice-stealing passages and measuring CPU
against H8. Zero original-core calls in one short passage would establish only
that passage, not complete feature coverage. Windows builds and local checks
do not validate macOS, a real VST3 host or the tester's Ryzen 3 3100.


## Native KS playback controller

Production KS selects C++ from reset. H8 is compiled only into comparison
executables via the separate `ksH8Reference` test object; `ksLib` has no H8
implementation or link. Production Setup shows C++ without a comparison switch.
Legacy project/controller preferences resolve to C++ in production. K-Station's
separate controller is unchanged.

Comparison executables retain the original H8/C++/hybrid choices, reversible
worker-boundary switches and legacy preference tests. Their original H8 runs
through a test-only synchronized adapter, so its host CPU is not comparable to
historical direct-core runs. Use a frozen production binary for performance.

C++ Device instances execute reset, initial DSP setup and warm-up through fixed
compatibility control flow, with interpreter entry locked before reset. The
register/flags/stack/continuation model is still present in independent KS
storage. Removing the H8 dependency does not finish the semantic port.

The native implementation covers normal playback controller code for verified
KS OS 2.1.02: MIDI/system/realtime/SysEx parsing and part routing, all CC/NRPN,
bend/pressure, notes/sustain/voice allocation and stealing, synthesis parameter
conversion and DSP feedback, clock/arpeggiator, program/performance editing,
state and user Flash writes. DSP firmware continues running in the original
DSP emulators with the same JIT configuration, clocks, serial ordering, slice
targets, resampling and buffers.

This is a **native compatibility implementation**, not an event-batched
OsTIrus-style architecture. It retains the register/RAM ABI and original guest
clock grid to preserve exact commands/audio and allow a quiescent switch back
to H8 in comparison tests. Independent KS storage now holds that frame; the
original H8 object exists only inside the test adapter.
Fixed C++ control flow replaces opcode decoding; there is no new JIT or tape.
Native fetches account for bus clocks without rereading instruction bytes.
Ordinary controller RAM/stack accesses bypass the device map while preserving
byte order and bus clocks. MMIO, DSP, Flash and wave accesses use the Board's
actual callbacks. The virtual TPU0 timer advances arithmetically; original H8
keeps its per-clock timer loop as the reference. Windows timers are untouched.

A complete ROM fingerprint and comparison of the active executable SRAM guard
handoff. Writes to that code and states outside the ported playback code fail
explicitly; there is **no automatic H8 fallback**. MCU OS flashing/bootloader
entry is outside this mode. Loading firmware through the plugin recreates the
engine and uses the selected controller from reset. Original/private ROM and data files remain
required and are never embedded in the source. The experiment stays off by
default; verified scenarios are not a claim of every possible patch/wave,
malformed SysEx stream, host or platform.

The worker-owned `Board::enableNativeController(bool)` API enables/disables at
quiescent boundaries. It is mutually exclusive with recorded playback. Explicit
disable re-enables the interpreter and restores the mapped memory/fetch path;
subsequent enable validates the current image again. Device control requests
are acknowledged by the worker; the audio callback never takes a controller
lock or performs the handoff. Switching ends a running diagnostic recording
with `stop_reason=controller_changed`. The report's `session.controller_backend`
retains the recorded mode and the top-level field shows the current mode; start
a separate session after switching. Diagnostic backend strings distinguish all modes.

Private comparisons:

```powershell
cmake --build build-nmake --target ksRuntimeController_test ksControllerTimers_test ksControllerMemory_test ksControllerProbe_test
& build-nmake/source/novation/silverK/buildStandalone/silverPlugin/ksRuntimeController_test.exe output/perf-dsp-threads/private-KSOS2102.SYX
python tools/run_live_controller_comparison.py --executable build-nmake/source/novation/silverK/buildStandalone/silverPlugin/ksControllerProbe_test.exe --ks-rom output/perf-dsp-threads/private-KSOS2102.SYX --flash "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-init-flash.bin" --wave "$env:USERPROFILE/Documents/Silver-K/ROMs/ks-wave-zero.bin" --controller native --repeats 3 --seconds 4
```

The runtime differential test checks every ported boundary with varied full
32-bit registers/flags against original H8. It alternates instruction-read-back
and production clock-only fetch, independently decoding the reference fetch
length and comparing all remaining data/IO accesses, their clocks and in-flight
PC, registers, flags, stack bookkeeping and final RAM. Interrupt entries cover
priorities/masks/ties. Additional ROM-free tests compare native RAM/ROM-overlay
accesses with the mapped reference, MMIO side effects and original timer loops.
The no-argument private runtime CTest skips with code 77; normal public CI cannot
supply private firmware and does not establish runtime ROM equivalence.

The runner compares full logical DSP port tapes byte-for-byte and hashes all
24 integer audio streams, state, user Flash and MIDI output, plus guest/serial
counters. Add `--scenarios controls state clock steal` for the extended live
passages, or `--dsp-threads 2 --toggle --repeats 1 --seconds 2 --scenarios pedal`
for a separate compatibility/switching check. `state` actually stores a Single
preset into the private in-memory Flash and requires a completed page write;
input files are not modified. `clock` drives the arpeggiator at 90/180 BPM with
start/stop/continue. `steal` sends 24 notes with two-voice unison across four
parts. `controls` sends all CC numbers plus explicit NRPN, bend and pressure.
`fx` uses four parts/16 notes with delay, reverb and chorus sends, changes those
sends during playback, exercises sustain and releases the notes into FX tails.
Use `--controller native --scenarios fx --seconds 4 --repeats 1` for complete
bus/audio/state correctness; use separate unrecorded repeats for CPU. The
recorded-controller comparison runner also accepts `--scenarios fx --seconds 4`.
Recorded replay estimates fixed-passage controller cost; it does not implement
live MIDI/voice decisions or establish a future live-controller saving.

The first live event-controller stage is opt-in with
`SILVER_KS_DSP_TRANSPORT=events` and the C++ controller selected. The native compatibility transport remains the default; production has no
KS H8 backend. H8 comparisons are test-only. In the comparison runner,
pass `--controller native --dsp-transport events`; the runner explicitly sets
the transport and rejects a capture with no live jobs. HDI readiness observations,
address requests, 24-bit writes and reply reads use native protocol jobs. They
retain each original port byte, cycle and in-flight PC. A job executes only if
it fits before the next UART/timer/ADC event and the existing slice limit;
IRQs that can preempt the job or shorter intervals use the existing native
compatibility path. Pending mode-2 IRQs masked by the current EXR remain pending;
these jobs do not change the mask. Busy replies drive timed live retries with
the original countdown/overflow flags and timeout continuation. Each retry still
reads the actual DSP port at the original cycle; no response is cached or skipped.
There is no interpreter fallback or recorded traffic. Materialized compatibility
registers/flags preserve interrupt preemption and the H8 comparison switch.
`transport_jobs` and `transport_job_boundaries` count executed work, including
preparation unless metrics were reset; they are not CPU measurements. Voice,
parameter, MIDI and other control logic still use the native compatibility port.

Native reports must show `original_h8_step_calls=0`,
`h8_interpreted_steps_after_boot=0` and
`h8_interpreted_steps_since_handoff=0`, with positive `native_runtime_steps`.
The interpreter counters count calls (including interrupt arbitration), not ISA
instruction counts. The legacy `h8_*_cycles` fields are the shared controller
clock in native mode; growing cycles are not interpreted execution. Explicit
toggle tests allow H8 in the disabled window; the last-handoff counter must
still remain zero. Resetting diagnostics preserves controller state. Counter
segments after a toggle describe only the last enabled controller object.
The fixture's coverage assertion sums handler calls from both native segments,
so a Note On before the switch and release after it are both counted. This does
not change per-instance exported counters or the command/audio/state checks.
Recorded replay has no live state/Flash/MIDI-output snapshot comparison; those
digest fields are null there. CPU means whole-process user+kernel seconds for
equal rendered work on Windows and macOS, excluding bootstrap/preparation and
report IO. Windows uses `GetProcessTimes`; macOS uses
`getrusage(RUSAGE_SELF)` (all threads, not just the coordinator). Other platforms
still report no CPU measurement. Runner metadata identifies the OS, architecture
and measurement availability. Only unrecorded runs are used for CPU comparisons.
Recording and unrecorded timing runs remain separate; the replay saving from
an earlier experiment is not the native live controller's saving.

Add `--reference-executable <archived-original-fixture.exe>` to compare with a
frozen original-H8 build rather than the current instrumented H8 control.
Metadata records both executable SHA-256 values and the runner hash. The frozen
fixture must run the original backend and support the same passage, inputs,
render length and trace format. Older fixtures can lack state/Flash/MIDI-output
snapshots; the summary explicitly records
`state_flash_midi_snapshots_compared=false` in that case. Complete bus tapes,
audio and guest/serial counters must still match. Verify those extra snapshots
separately with the current H8 control. A frozen baseline avoids attributing
new per-step diagnostic overhead to a controller CPU saving.

For macOS, use the same runner with the arm64 `ksControllerProbe_test` executable
and locally supplied private inputs. Pass `--controller native`; the runner's
default remains the historical hybrid. Use `--scenarios single multi midi pedal
controls state clock steal --seconds 2 --repeats 1` for broad correctness, then
four-second repeated unrecorded pairs for timing. Do not run a compiler or other
DSP test concurrently with timing captures. A same-binary H8 comparison includes
its current interpreter guard/counter overhead; it does not replace a frozen
pre-instrumentation baseline when attributing a CPU saving.

Consult the latest diary entry for actual platform results. Real DAW/driver
listening/deadline behavior and the tester's Ryzen 3 3100 remain separate checks.

### OsTIrus DSP threads and CPU cores (2.2.25 source review)

TI/TI2 use one `DSPThread` per DSP. The inspected creation/execution/priority
path does not assign physical CPU IDs or hard affinity: the OS scheduler chooses
cores and can migrate the threads. On macOS `ThreadTools` sets QoS and a Mach
time-constraint policy; those scheduling hints are not a fixed DSP-to-core map.
Snow is a single-DSP case. This finding is for tag 2.2.25, not a runtime trace or
a claim about every later release or host-imposed affinity.

Sources: [DSP worker creation](https://github.com/dsp56300/gearmulator/blob/2.2.25/source/axel/virusLib/dspSingle.cpp),
[pinned DSP thread](https://github.com/dsp56300/dsp56300/blob/e643fa89f0c11147963b315c0bd8afb283bba468/source/dsp56kEmu/dspthread.cpp),
[pinned priorities and macOS policy](https://github.com/dsp56300/dsp56300/blob/e643fa89f0c11147963b315c0bd8afb283bba468/source/dsp56kBase/threadtools.cpp).
Silver-K's opt-in parallel DSP executors likewise have no fixed core IDs;
the macOS coordinator and helper request audio priority and participate in the host audio workgroup
where available. The Windows-only KS P-core CPU Sets experiment targets the
coordinator, not a verified placement of both DSP helpers.


### KS control jobs: 2026-10-15 isolated Mac investigation

The original investigation enabled part-state/parameter-table and scaling
jobs with `SILVER_KS_CONTROL_JOBS=1`. In the current staged runtime, that flag
selects only the remaining part-lookup jobs; key search and complete signed
coefficient tasks run independently of it. The older register-shift batch is
comparison-only. `SILVER_KS_DSP_TRANSPORT=events` still selects the transport
experiment. Omitting flags keeps current native tasks, not the historical
C++ binary. The code is C++17; Windows execution remains separate. Global
compatibility-frame removal is incomplete. See [migration status](KS_NATIVE_CONTROLLER.md).

The live comparison runner accepts `--controller native --dsp-transport events
--control-jobs on`. It clears inherited profile/audio-capture flags. Frequency
profiling (a separate CMake build with `SILVER_KS_CONTROLLER_PROFILING=ON`,
then `SILVER_KS_CONTROLLER_PROFILE=1`) counts dispatched boundaries,
excluding batched jobs, and is reset before the passage. Never include its
overhead in CPU results. The standalone probe also accepts bounded 2–30 second
passages and optional `SILVER_KS_AUDIO_CAPTURE=/private/path/new.pcm`; storage
is preallocated and file writing occurs after the measured render. Compare
those bytes for correctness, and omit capture/recording/profile for CPU runs.

Ordinary builds default `SILVER_KS_CONTROLLER_PROFILING=OFF`; the boundary
profile branch and vector are compiled out, including in the CPU matrices.

Additional controller timing checks (no private ROM needed for the first):

```sh
cmake --build <build-dir> --target ksControllerTiming_test ksControllerUart_test
<build-dir>/silverPlugin/ksControllerTiming_test
<build-dir>/silverPlugin/ksControllerUart_test <KSOS2102.SYX> <ks-init-flash.bin> <ks-wave-zero.bin> <fresh-private-output-dir>
```

The UART test compares H8 with C++ at controller budgets 1..128, validates the
exact receive cycle after every delivered post-boot byte, and checks full
port/slice files, all native audio words, IO/registers/memory and state across
H8 round trips. Run it once with flags omitted, then in another fresh directory
with `SILVER_KS_DSP_TRANSPORT=events` and `SILVER_KS_CONTROL_JOBS=1`. The explicit
post-boot counter excludes bootstrap bytes. This synthetic fixture currently
has zero bootstrap UART bytes. These instrumented checks are not CPU evidence.
Windows uses the corresponding `.exe` targets; this iteration's Windows run
remains pending.

## KS cold-reset compatibility verification — 2026-10-16

Fresh C++ Device instances now enter the C++ compatibility runtime at reset,
not after original-H8 warm-up. Native probe modes reject any boot interpreter
calls; report `h8_interpreted_steps_boot` separately from post-boot/toggle
counts. The register/continuation/flags/stack ABI still remains. H8 selection
is available only in the separate comparison executables.
This is not the completed semantic controller replacement.

```sh
cmake --build <build-dir> --target ksControllerBoot_test ksRuntimeController_test ksControllerTiming_test
<build-dir>/silverPlugin/ksControllerBoot_test <KS-OS> <Flash> <Wave> <fresh-private-output> single
# Repeat with a different fresh output and variant: irregular, multi, mapped.
```

The standalone CMake layout puts these executables under silverPlugin. The
unit skips with 77 without private inputs and refuses to reuse output paths.
It compares two independently initialized Boards from cycle zero. Every
coordinator observation checks cycles, continuation/register/flag/IRQ/bus
state and all IO, followed by full memory/Flash/MIDI/musical state, complete
DSP tapes and 24,000 audible 24-stream frames. The `mapped` variant modifies
only synthetic bank endpoints to exercise the no-placeholder path; it does
not admit original sampled-sound fidelity. These instrumented comparisons
are correctness evidence and must not be used as CPU benchmarks.

Reset operation checks add 4,128 original-H8 comparisons, including rare
checksum/update branches and arbitrary partial frames. The fetch-clock test
now uses actual original read8 calls before its independent observation,
covering 11,520 cases including signed PC / 24-bit physical bus addressing.
The previous 10,880-case test compared clockMem calls directly and did not
prove that additional signed/physical distinction. Keep historical results
separate. No change is made to the original H8 oracle or DSP JIT.

The Desktop build CI now compiles the reset/runtime/UART fixtures on Windows
x64 too. Timing/timer/memory checks run without ROMs; private-input fixtures
skip explicitly with 77 when no ROMs are supplied. A successful build of these
fixtures does not establish Windows firmware/audio/CPU equivalence. No private
ROM or capture is uploaded to CI. Inspect the actual run before claiming pass.

### Test-only original H8 and arithmetic guards (2026-10-16)

Build `ksControlArithmetic_test` and `ksProductionController_test` in addition to
`ksRuntimeController_test`, `ksControllerBoot_test`, `ksControllerTiming_test`
and `ksControllerMemory_test`. CTest names are `ks_control_arithmetic` and
`ks_production_controller`; both run without private ROM inputs. The production
guard deliberately does not link `ksH8Reference`. It checks C++ default, missing
reference/locked-entry rejection without state mutation, and the absence of the
old generic memory-map allocation. An `nm` audit of the actual `ksLib` archive
must also contain no H8 core/decoder symbols. Whole-plugin H8 symbol checks are
not meaningful because K-Station still legitimately includes its own core.

The arithmetic oracle covers 283,113,088 cases and links the original core only
inside its executable. `-fwrapv` applies solely to that original-reference test
on Clang/GCC; no DSP/JIT flags change. The production code defines wrap with
unsigned arithmetic and representable signed conversion. Memory/fetch timing
oracles continue to call original H8 accessors directly.

`ksResetVector_test` is a further ROM-free admission test. It invokes the
original H8 boot directly for all 16,777,216 reset targets with varied upper
vector bytes and prior state, comparing initialization with independent KS
storage. It deliberately does not install the synchronized step adapter.
CTest name: `ks_reset_vector`. This added test does not change production code
or the measured binaries; see the dated journal entry and delivery audit.


### Native key-call correctness admission (2026-10-16)

`ksKeySearchCalls_test` calls the actual runtime/note path and directly executes
original H8 independently. CTest `ks_key_search_calls` skips with 77 without the
private KS OS. This target is deliberately created after the comparison-object
scan: it must have no synchronized H8 adapter or partial-frame factory linked.
The public `ks_keyboard_properties` test still runs without firmware. Actual
Windows private-firmware execution remains separate from a successful CI build.

The controller probe adds `native_key_search_calls`, `native_key_search_returns`
and `native_key_search_test_partial`. Calls/returns count actual native tasks,
not CPU savings or guest instructions. Resetting diagnostics leaves an active
task intact, so a task admitted before reset can return after it. Every fresh
native, non-toggle musical capture rejects a nonzero test-partial counter.
Comparison-only H8 handoffs may recover an existing arbitrary partial frame;
normal plugin builds have no implementation of that recovery hook.

The native task is independent of registers/flags/stack/PC. Its temporary
caller projection still exposes that old ABI to not-yet-converted callers;
this does not complete global CPU-model removal. CPU comparison is explicitly
deferred until the complete semantic KS controller is ready. Test ID
`ks-native-keycalls-20261016` records actual audio/tape/IRQ and allocation scope.


### Native coefficient-call correctness admission (2026-10-16)

Build `ksParameterScaleCalls_test`; CTest `ks_parameter_scale_calls` skips with
77 without a private KS OS argument. It calls the real runtime independently
of directly executed original H8 and deliberately links no synchronized adapter
or partial-state recovery factory. `ks_parameter_scale_properties` remains the
ROM-free arithmetic/time-budget admission.

Probe counters `native_parameter_scale_calls`, `native_parameter_scale_returns`
and `native_parameter_scale_test_partial` count actual complete native tasks
and comparison-only recoveries. Fresh native/non-toggle captures require zero
partial entries. Diagnostic reset leaves active tasks intact; a task can return
after the counters were reset. These counts are not CPU measurements.

The old register-driven scaling batch is comparison-only and is no longer a
`ControllerJobs::candidate`. Production guards reject its direct service and
arbitrary partial coefficient recovery. `ParameterScaleCalls` still projects
the native task onto old callers/IRQ saves; it is transitional, not completion
of the register/flag/stack/PC removal. The dated journal entry and private test
ID `ks-native-parameter-calls-20261016` state actual test scopes and limitations.
CPU comparison stays deferred until the complete semantic controller is ready.


### Keyboard/priority/modulation/sweep admission (2026-10-16)

Test ID `ks-native-key-state-families-20261016` records integrated native routes
and exact private command/audio checks; see dated optimization log for scopes.
New public properties/private-ROM call fixtures are registered after the H8
reference target scan and deliberately assert `!controllerReferenceAvailable()`.
The public call tests skip with 77 without a private OS argument. New probe
prefixes `native_key_activity`, `native_note_on_state`, `native_key_priority`,
`native_oscillator_modulation`, `native_global_modulation`, `native_voice_sweep`
count jobs, returns and comparison-only partial entries. Fresh native musical
runs reject partial recovery and check balanced completed contexts. An in-flight
task survives diagnostic reset; counts are not elapsed time or CPU measurements.
Pure native tasks retain no CPU model; projection bridges remain transitional.
CPU comparisons remain deferred until the entire KS compatibility model is gone.


### Pitch/refresh/Note Off/timer admission (2026-10-16)

`ks-native-pitch-timers-20261016` freezes integrated source identity, independent
original-H8 calls and focused private command/audio evidence. New probe prefixes
`native_oscillator_pitch`, `native_parameter_pitch`, `native_parameter_modulation`,
`native_voice_refresh`, `native_global_setup`, `native_note_off_state` count jobs,
returns and test-partial admissions. They measure ownership, not CPU savings.
Inline tasks retire at caller admission without an added guest boundary. Timer
call/property fixtures verify all five actions separately; timer metrics are
not yet exported by the musical probe. Runtime arbitrary-state tests discard
transient scheduled jobs before each unrelated synthetic frame. The old caller/
IRQ projection remains until downstream lifecycle conversion. CPU comparison
continues to wait for complete KS model removal. Mac evidence does not validate
Windows; no fresh AU/VST3/Standalone validation in this checkpoint.


### Shared inline packet/job-lifetime admission (2026-10-16)

`ks-native-control-packets-fixed-20261016` records corrected integrated evidence.
`retireReturns` preflight is logical completion with no guest effects; call it
for all inline owners before selecting a new family whose root may share the
old caller point. Multi-word intermediate awaits must not be retired as terminal.
Probe prefixes `native_parameter_scalar/time/curve/voice_scale`,
`native_global_scalar/time/shape/range`, `native_part_note`, `native_note_program`
and `native_pedal` expose calls/returns/partials; the newer families also expose
`initial_active` and `active`. Pedal adds `native_pedal_handoffs`. Check
calls+initial_active = returns+handoffs+active. Functional pedal/Note On/Off handler
entry counters remain tied to their original entries separately from job counts.
Checked sustain entering its direct3530 continuation must retain the existing
job; fixture verifies balance after each native step. The previous failed Multi
report is retained as failure evidence, not a final pass. Public/private test
scope and corrected IRQ coverage are detailed in the dated log. CPU comparisons
remain deferred until the broad KS CPU compatibility model is removed.
