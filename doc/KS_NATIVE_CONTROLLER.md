# KS controller migration status

Scope: KS OS 2.1.02 only, authenticated decoded firmware SHA-256
`7acd2d1a8a56f44522b12327437a27826b64fc49a18851816423546fd6ea085c`.
K-Station OS 2.1.01 is a separate controller and is unchanged.

## Fidelity contract

The target is a live C++ controller with original KS state, Single/four-part
Performance, note ownership/release/stealing, sustain, all supported MIDI/NRPN,
parameter/voice calculations, clock/arp, effects control, SysEx, program and
Performance loading/saving, Flash writes and retained state. Preserve the
original controller/DSP clocks, interrupt acceptance boundaries, 128-clock
coordinator slices, B/A DSP catch-up order, serial service and sample-rate
adapter. DSP firmware and JIT are outside this conversion.

Acceptance compares actual ordered HDI reads/writes, values, responses, cycles
and in-flight context, all 24 native audio streams, controller state, Flash,
MIDI output and DSP/serial counters. Timed jobs must yield before an event can
preempt them; neither polling elimination nor recorded response playback is
an accepted implementation. No allocation, I/O, locking or waiting is added to
rendering. H8 remains an independent offline oracle. Passing finite tests is
not proof for every possible state or input.

## Present implementation versus target

The full semantic conversion is **not complete**. Current live C++ playback
executes fixed, verified C++ control flow without calling the H8 interpreter
from reset, but retains a compatibility register/flag/stack/continuation model
and address-based memory storage. The frame is now independent of H8 types;
that separation is not the remaining semantic conversion. The
13,766 compatibility boundaries are not a native event-controller design.
Fresh C++ instances now execute reset/bootstrap and warm-up in C++ too, with
zero H8 interpreter calls. This reset path still uses the same compatibility
register/PC ABI; it is not a semantic typed initializer. Removing the H8 link
or renaming that frame would not finish the migration.

Native jobs and legacy diagnostic flags:

- The former `SILVER_KS_DSP_TRANSPORT=events` flag remains accepted for
  diagnostic compatibility. Address/word/read exchange now always uses typed
  native tasks. All old HDI batch implementations are comparison-only; the
  production batch candidate is always false.
- `SILVER_KS_CONTROL_JOBS=1` remains a diagnostic compatibility flag. Part
  parameter/control/voice selection and the live 20-byte control-mirror copy
  now always run through CPU-independent tasks, including explicitly supplied
  part indices. The former part-lookup batch and partial phases are test-only;
  the production batch candidate is always false. Signed coefficient scaling
  also uses its own native task. Temporary caller/IRQ projections remain.

The legacy control-job flag defaults off and no longer selects an alternative
part-lookup algorithm. The retained diagnostic backend names are
`native-cpp-control-jobs` and, with HDI jobs, `native-cpp-events-control`.
No installed bundle is replaced by this research build.

Clock-only native boundaries also aggregate aligned low-code fetch timing as
three clocks per 16-bit word. This changes no opcode decoding or DSP JIT;
the original byte-clock path remains for all other addresses/lengths/modes.

The C++ paths also snapshot pending UART bytes once per coordinator slice,
rather than attempting two empty atomic queues after every compatibility
boundary. A real pop retains the original priority/order/clock. Board input
is coordinator-only and cannot be fed concurrently with `advance()`; host
threads use Device's event queue. The original H8 tick/queues are unchanged.

For a separate diagnostic build, configure CMake with
`-DSILVER_KS_CONTROLLER_PROFILING=ON`, then set
`SILVER_KS_CONTROLLER_PROFILE=1` to enable boundary-frequency accounting.
Ordinary builds default to OFF and omit both storage and per-boundary profile
checks. Profiling allocates before playback and resets at the passage. It
excludes batched job boundaries and adds overhead; never use it for CPU comparison.
The probe can optionally preallocate `SILVER_KS_AUDIO_CAPTURE` and write all
raw native frames after rendering. That instrumented run is for correctness,
not performance. Probe duration is bounded to 2–30 seconds.

## Earlier CPU measurement — before semantic model removal, 2026-10-16

Final AU total-process CPU, 48 kHz/256, effects, 16 held keys total,
three alternating pairs of 16-second windows, sequential DSPs and profiling
compiled OFF (100% = one CPU):

| AU, FX | Frozen accepted event-transport AU | Final candidate | Relative change |
| --- | ---: | ---: | ---: |
| single-fx | 47.097706% | 46.607250% | -1.041% |
| multi-fx | 46.911813% | 46.225413% | -1.463% |

This is a small local improvement; earlier trials are not pooled. All 12 AU
windows match output/tails and programs, with zero lost frames or over-budget
callbacks. Nine complete H8 tapes (8,085,451 events), 14 full raw-audio
comparisons (6,075,618 frames per side x 24 streams), UI/state switches and
focused UART/IRQ/arithmetic/fetch-clock checks pass. This does not prove all
possible presets/inputs or physical-device equivalence. Inputs use the
authenticated OS, synthetic Flash and zero Wave ROM; sampled banks are untested.

The exact measured AU and separately built VST3/Standalone are in the ignored
local `build/optimized-mac-ks-control-jobs-2026-10-16/` package, with source
identity/checksums. VST3/Standalone have not been separately hosted this round.
The shared Optimized checkout and installed plugins are unchanged. This is
an isolated verified stage, **not completion of the removal gates below**.

## Processor-independent tasks under admission — 2026-10-16

`ksParameterScale.h` implements signed coefficient calculation and a resumable
completion-clock task. `ksKeyboard.h` implements strict adjacent-held-key
selection and a resumable task with original live-byte observation timing.
These task states contain no CPU registers, flags, emulated stack or PC.
Direct original-H8 tests admit their values, per-boundary durations and, for
keyboard search, live read order/cycles under mid-search state changes. See
`ks-native-control-core-20261016` in the optimization journal for counts and
limitations. The key-search task is now connected to actual playback through
`KeySearchCalls`; the coefficient task is now connected through
`ParameterScaleCalls` too. The native key
algorithm owns its live observations and completion time. The temporary bridge
still supplies the old callers/IRQ saves with their flags, register projection,
return stack and trace PC. Old key-search instruction bodies are test-only;
fresh native playback must report no partial-test recovery. The actual-runtime
fixture admits 926,866 full calls with zero H8 work and no recovery factory.
The complete tape/audio matrix and limitations are recorded under
`ks-native-keycalls-20261016`. The shared compatibility model remains, so the
full removal gates below still apply. CPU comparison is deferred until all
those removal gates are satisfied.
The coefficient task is admitted by `ks-native-parameter-calls-20261016`:
1,049,756 actual runtime calls, every 16-bit input/eight masks and exact
per-boundary projections; all complete DSP tapes/audio and callbacks pass.
Its 26 old body boundaries and retired register-shift batch are test-only.
Both bridges still expose the old caller/IRQ/return convention, so neither
connection alone completes the requested global model removal.


`DspWordTask` now owns live 24-bit writes and post-write address advancement,
including independent readiness observations, 16-bit timeout/retry state,
yielding for the actual fault service, byte order and exact admission clocks.
It has no CPU registers, flags, stack or PC. `DspWordCalls` is the temporary
caller projection; it still reads the old caller values/return stack and
publishes their IRQ/trace view. The 34 old body boundaries and six old word
batch entry points are comparison-only. Normal playback uses neither partial
recovery nor the retired batch. In-flight tasks survive metric resets.

Admission `ks-native-dsp-word-calls-20261016`: 336,480 outer calls and 56
interleaved child calls against direct original H8, all addresses, both DSPs,
all byte positions, masks, readiness delays, full timeouts/real GPIO fault
service, 560 IRQ save/RTE/resume points and eight live tasks. The full musical
matrix observes 117,278 native writes and zero test recovery; all nine complete
DSP tapes and 14 raw-audio/state timelines match. Allocation and sanitizer
scope, fresh Mac products/callbacks and remaining limitations are in the
optimization journal. Address selection/read reply tasks and all remaining
callers still need conversion before deleting the shared compatibility model.


`DspAddressTask` now owns address requests and optional read replies, both live
readiness phases, their retry/timeout state, HF3 sequencing, actual byte reads
and A/B read-port selection. Its native state contains no CPU register/flag/
stack/PC or instruction addresses. `DspAddressCalls` still supplies the old
caller projection, so the global removal remains unfinished. All 68 retired
body boundaries and all historical HDI batch arithmetic, port access and IRQ/
deadline guards now compile only in the comparison object. The normal batch
ABI is a closed-by-default test hook and never a production candidate.

Admission `ks-native-dsp-address-calls-20261016`: 819,920 complete outer calls
and 84 interleaved child calls against direct original H8, every address, both
DSPs/selectors, reply byte positions, masks, dual-phase timeouts, actual GPIO
fault service, IRQs and eight live tasks. All nine full DSP tapes and 14 raw
musical timelines match, with zero test recovery or retired-batch work. Single
and Multi also match full tape and raw audio without either legacy experimental
flag. Task/bridge allocation and sanitizer checks pass. This stage rebuilds
engine tests; products/callbacks were last verified at the preceding word-write
checkpoint and require fresh final validation. The outer bootstrap and caller/cache/reset/parser/voice/parameter/
clock/state paths still need semantic
conversion; no CPU comparison or push has been performed.


`DspFaultTask` now owns the actual timeout latch/display pulse, both live
control observations and its three-tick delay. Semantic IO callbacks and fixed
admission/release intervals contain no CPU state or device addresses. Its 159
body clocks plus the temporary caller's 17-clock return match the original
176-clock service. `DspFaultCalls` alone retains the old caller/IRQ/trace/stack
projection. The 18 old wrapper/pulse boundaries are comparison-only.

Admission `ks-native-dsp-fault-calls-20261016`: 525,056 complete outer calls
plus 224 children against direct original H8, every latch/initial-flag
combination and every control-byte pair, 704 IRQ/RTE points and eight live
tasks. Forced word/address timeouts execute all 96 complete native fault jobs.
Healthy musical scenarios observe no faults, so they do not stand in for that
explicit error-path proof. Full tapes, 14 raw musical timelines and two default
Single/Multi cases match. Allocation/sanitizer scope and product-validation
limits remain in the journal. Outer bootstrap initialization and the broader controller/caller model
still need conversion.


`DspBootStreamTask` now owns the separate firmware byte stream: native word
count/data cursor/latched byte, actual source observations after TX readiness,
retries/timeouts and exact durations. Its source is a native byte-index
callback, with no CPU fields or addresses. The old 20 body boundaries are
comparison-only; `DspBootStreamCalls` still provides the temporary old-caller
projection. Sources are bounded to mapped three-clock ROM/Flash/SRAM spans.

Admission `ks-native-dsp-boot-stream-20261016`: 132,336 outer calls plus 112
children, full 16-bit count edges including 65,536 words, changing source
bytes, 1,024 IRQ/RTE points and eight live contexts match direct H8. Each cold
and musical start proves two complete native firmware streams (1,871 and
2,840 words), zero recovery and zero native interpreter work. All full tape/
audio/default-path checks pass. Private raw evidence is retained in
`/private/tmp/ks-native-dsp-boot-stream-20261016`; product/callback validation
still requires a fresh final pass. The parent header, delay, boot-ready and frequency-table setup now runs in
`DspBootSetupTask`, independent of CPU fields. Its temporary caller/IRQ
projection remains. The outer bootstrap/reset and broader model still require
conversion.


## Remaining removal gates

1. The C++ compatibility reset path now passes independent full-boot command,
   state and timing comparison. Replace this path with semantic typed native
   initialization jobs as part of the compatibility-frame removal.
   The admitted image resets at `0x2ec`, copies code/data from Flash into SRAM,
   executes a copied overlay trampoline at `0xffec00`, then returns through
   `0x180`/`0x36e`. A saved mutable startup image is not a native initialization
   algorithm and must not replace this work.
2. Lift normal foreground, periodic voice/parameter, parser/clock/arp, state
   and Flash lifecycles out of the instruction-boundary compatibility ABI.
   Retain exact data-dependent durations and interruption/yield state.
3. Replace the generic CPU register/flag/stack/address-space frame with typed
   controller state and explicit bounded job storage; eliminate the large
   generic device-map allocation. Do not retain a hidden interpreter fallback.
4. Production KS no longer links or includes H8. `ksH8Reference` is a separate
   test object and supplies the independent original core to comparison
   executables only. Keep that separation while completing the semantic frame
   conversion. K-Station still requires its own H8 implementation.
5. Build and repeat the comparisons separately on Windows x64, then validate
   the actual host/plugin formats. The control-job source uses portable C++17;
   macOS results alone do not establish Windows correctness or CPU savings.

See `OPTIMIZATION_LOG.md` for measured results and source/build identities.
The isolated checkout is intentionally independent of concurrent preset-manager
work; no browser/library/patch-manager changes belong to this migration.

## Reset compatibility follow-up — 2026-10-16

Fresh C++ Device instances select the C++ reset path before boot. The reset
vector, SRAM initialization/copy/checksum, overlay trampoline, startup DSP
downloads, initial synth state and warm-up no longer execute H8 instructions.
The original H8 path remains separately selectable as the reference while
the semantic conversion is incomplete; this is not yet a production target
with H8 limited to tests. DSP firmware/JIT, clocks, slices and serial order
are unchanged. Mid-session H8/C++ switching retains the existing state ABI.

All 4,128 reset boundary cases pass with arbitrary values/flags and mapped
or clock-only fetches. The actual H8 read8 is now the fetch-clock oracle,
including signed trampoline PC and 24-bit physical address wrap (11,520
cases). The earlier 10,880-case byte-clock oracle did not distinguish this
signed-PC/physical-bus case; it must not be described as proving that case.

Four full cold timelines pass: Single, Performance, irregular slice budgets
and a synthetic nonzero Wave mapping. Each compares controller/IRQ/IO state
through reset, complete DSP-port/slice tapes, full memory/Flash/state/MIDI and
24,000 audible frames x 24 streams after warm-up. Each C++ run executes zero
H8 instructions. The mapping variant is not an authenticated sampled-wave
bank and does not establish sampled-sound accuracy. Final musical/plugin
rechecks and identities are recorded in OPTIMIZATION_LOG.md. Local test ID:
`perf-ks-native-reset-20261016`. The register/PC/flags/stack model still remains.

The final follow-up passes all 13 cold/musical/host/CPU stages. AU whole-process
medians (100% = one CPU, same alternating three-pair method as above):

| AU, FX | Frozen accepted event-transport AU | C++ reset + current jobs candidate | Relative change |
| --- | ---: | ---: | ---: |
| single-fx | 44.863262% | 44.445200% | -0.932% |
| multi-fx | 44.716487% | 43.866525% | -1.901% |

This new series has exact output/tails and zero lost/over-budget frames in all
12 windows. It measures the combined candidate against the earlier accepted
event-transport AU; it is not a measurement of reset alone. The exact tested
AU and source snapshot are in ignored
`build/optimized-mac-ks-native-reset-2026-10-16/`. This is still an intermediate
compatibility implementation. H8 has not been restricted to test targets.

## Independent KS storage and test-only H8 — 2026-10-16

This follow-up removes the H8 base class and the 16,777,216-entry generic device
map from KS production. `ksLib` has no `h8s` link and no original-core types or
opcode-decoder symbols. It uses one explicit Board bus. The remaining temporary
compatibility frame is **16,777,472 bytes**; the 128 MiB pointer-map allocation
per KS instance is eliminated. The original K-Station engine is unaffected.
This is a storage/dependency separation, not removal of the register model.

The original unmodified H8 is compiled by `ksH8Reference_test.cpp` into a separate
object linked only to comparison executables. The adapter synchronizes state at
core/Board boundaries; actual H8 decode, arithmetic, clock charging and interrupt
arbitration remain in that original core. Reference host CPU is no longer a
historically comparable benchmark because this adapter adds synchronization.
CPU comparisons must use frozen production plugins instead.

Production chooses C++ before reset, including when loading a legacy project
with an old H8/hybrid choice. Setup reflects that active choice and provides no
H8 selector in production. Comparison executables retain the original switch
and legacy preference/restore tests. Missing or locked reference execution fails
without changing clocks or incrementing interpreter counts. The ROM fingerprint
still rejects unsupported controller images; there is no silent fallback.

Control add/subtract operations are now pure fixed-width C++ computations,
independent of H8 methods. Unsigned intermediates define 32-bit wrap. They pass
283,113,088 independent original-H8 comparisons: all byte operand/status pairs,
carry masking and sticky zero, full word operands against signed/half-carry/wrap
edges, and wide edge/random cases. The test-only original signed subtract is
compiled with defined wrap; no DSP/JIT compile options change. Native bus/timing
fixtures call the original H8 accessors directly, rather than reusing the new
adapter as their timing oracle.

Validation report: `perf-ks-reference-isolation-20261016` (private/ignored).
All 16 frozen musical/plugin/CPU stages pass. A subsequent independent original
boot test covers 16,777,216 reset targets; two actual AU legacy-project runs
also pass. Details, the modest Multi CPU saving and mixed Single results are
recorded in the journal. The implementation remains portable C++17, but Mac results do not
establish Windows ROM/audio correctness or CPU gains. Full typed lifecycle,
foreground/IRQ continuation and register/stack elimination remain outstanding.


Native setup admission `ks-native-dsp-boot-setup-20261016`: all 65,536 headers,
112 complete native table/deadline cases, 721 actual full original-H8 calls
(106,048,286 steps), 206 selected IRQ points, 735 metric resets and 14 nested
calls pass. Both 512-word table recurrences, live ready/fault handling and
all original accesses/clocks match. Every cold start proves two complete native
setups and streams with zero test recovery. Four cold timelines, nine full
tapes, 14 raw-audio timelines and two default cases pass. The compatibility
register/flag/stack/PC model still exists globally; this is not the full port.


The DSP reset/control pulse, calibrated two settling intervals, both explicit
port selectors and the two-image startup orchestration now have CPU-independent
state in `ksDspStartupTasks.h`. Forty-five old bodies are test-only; temporary
caller/IRQ projections still exist. Admission `ks-native-dsp-startup-20261016`:
1,642 actual calls / 70,356,926 direct original steps, all control bytes/flags,
90 selected IRQ points, 602 metric resets and four nested jobs pass. Four cold
variants and focused Multi/FX full tape/raw audio cases match; no H8/recovery.
Global register/flag/stack/PC elimination and final validation remain pending.


`DspRoutingTask` now owns DSP command preparation, both signed relative address
windows, B voice offsets and A part voice/base clamping. All 51 old bodies are
comparison-only. The address mirror comparison remains observational: the
original still sends the address even on a cache hit. The temporary bridge
publishes the typed request to the native wire tasks and retains caller/IRQ
projections. Admission `ks-native-dsp-routing-20261016`: 524,544 native property
cases and 529,238 complete actual calls / 13,293,680 original steps pass, including
all 16-bit relative/read addresses and every A voice/base pair. Focused cold,
Multi/FX tape/raw-audio and allocation/sanitizer checks pass. Global controller
model removal remains pending; no CPU comparison or push.


`PartLookupTask` owns part parameter/control and voice-slot indexing; its
result is a native byte offset. `PartMirrorTask` copies 20 live control bytes,
latching each at its scheduled observation and asking the bus for publication
latency when a neighboring byte store can share a transfer. An intervening
writer breaks that pair. The 29 old bodies and former control batch are
comparison-only. Admission `ks-native-part-control-20261016`: 1,536 public
properties and 655,518 actual calls / 16,913,534 original steps, all index/flag
combinations, live mutations/self-overlap, 68 IRQ points and ten nested calls
match. Focused cold/Multi/FX and allocation/sanitizer checks pass. No CPU
comparison or push; global controller model elimination remains pending.


`VelocityCurveTask` now owns repeated quantized velocity products, inversion,
square/mixed curves and live threshold selection, including full byte wraps.
Its 43 former boundaries are comparison-only; the caller/IRQ projection still
exists. Admission `ks-native-velocity-curve-20261016`: 393,216 independent
formula/deadline cases and 525,722 complete calls / 114,953,148 direct original
steps pass, with all velocity/curve pairs at powers 1/2/3, all velocity/power
pairs at selected curves, all flags, mutable thresholds, 298 IRQ points and
14 nested calls. Four cold variants and focused Multi/FX full tapes/audio,
plus Multi with both legacy flags enabled, match. No retired control/HDI
batch executes. Allocation/sanitizer checks pass. Global register/flag/stack/PC
removal, final product tests and CPU comparison/push remain pending.


`VoiceSelectionTask` now owns free/oldest/owner reuse/note-retrigger policy,
unsigned age comparisons, ties, active-age bits, 14/256-slot scans, live index
observations and release requests. `VoiceGateOffTask` emits the original zero
command. The 129 former selection/gate bodies are comparison-only. Native
selection ends at the slot-population handoff; the population handoff is now native too, while remaining voice
startup/parameter work still needs conversion. Admission
`ks-native-voice-selection-20261016`: 8,192 independent random policies and
72 original result/own-clock admissions; 95,226 actual calls / 71,766,674 direct
original steps, 4,398 selected IRQ points and ten nested calls pass. Four cold
variants and focused Multi/FX/steal/pedal tapes/raw audio match. No CPU comparison
or push; the global register/flag/stack/PC model remains.


`VoicePopulationTask` now owns selected-slot initialization, original/transpose
notes, held/unison/velocity fields, packed age, current part/control snapshot,
extra patch state and live DSP pitch/glide-distance setup. It composes the
native voice/part lookup, velocity, mirror copy, B selection and pitch-read
jobs. Its 39 old bodies are comparison-only. Child pitch uses the first/high
reply byte, and the slot's transposed note is reread at the original late point.
Admission `ks-native-voice-population-20261016`: 840 original result/own-clock
scenarios and 65,536 full-byte arithmetic/live-read cases; 294,058 actual calls /
76,413,262 direct original steps, all note/transpose and note/pitch pairs, powers,
flags, parts/slot bounds/tagged patch bindings, 1,610 IRQ points and 12 nested
calls pass. Four cold variants and focused Multi/FX/steal/pedal tapes/audio match.
The complete selection/population/velocity helper region now uses native tasks,
but remaining voice startup, parameter/MIDI/clock/state/reset/caller work and
the global compatibility frame still require conversion before CPU comparison
and publication.
