# NativeGlobalJob integration

Public source: ksNativeGlobalJob.h, ksNativeGlobalJob_test.cpp and
ksNativeGlobalJobOriginal_test.cpp. Independent H8 and allocation/sanitizer
proofs are retained privately under ks-native-global-graph-20261016. Original
firmware and raw captures are not part of this source document.

Scope: one complete normal global refresh request (original96d6 entry, typed caller completion after Final or optional Effect). This is not yet a per-parameter entry API. Full fixed child graph: Setup controls lookup and part lookup, voice-A route; sevenScalar, FirstTime, threeShape, SecondTime, Range, sixMix, Table, Matrix, ClampedTime, Pair, fourTailScalar, TailTime, twoFinishScalar, Final, optionalEffect. Matrix includes SquareRoot and live part route/mode children; Effect includes signed coefficient and both DSP transports. Inline blocks advance at zero added clocks; only explicit callable task returns charge original return intervals. NativeCoefficientJob and NativeDspRoutingJob are parent APIs.

NativeGlobalContext holds musical data only: DSP transport context, patch resource reference, selected control and part handles, mode218 latch. There are no CPU registers, flags, PC, guest stack, opcodes, execution tokens, boundary address tables or interpreter calls. Typed stage discriminants and admitted musical read/write events are the continuation state. Musical data resource references below use firmware data offsets as adapter keys; they are not execution addresses or a CPU register bank.

IO contract:
- nextClocks(IO&) queries memoryWriteClocks(dataReference), read-only. advanceBefore(available,IO&) admits one original observation interval; insufficient budget performs no reads/writes/state changes. Returns NativeDspJob::Progress; completed reports only whole refresh completion. IRQ scheduler pauses this job, charges static IRQ intervals externally and invalidates its data-bus pairing history; it never manufactures CPU frames.
- readGlobal8/16/32(reference,at) return live data and apply effects at event offsets inside current admitted interval. writeGlobal8/16(reference,value,at) publish ordered data. Low-memory aligned word reads/writes are one3-clock pair; long operations have first word at at−3, second at at. SetupMirror and Matrix/Table postincrement reads call at=cost−2. Byte publications use event offset cost. memoryWriteClocks(reference) preserves original paired-store data timing, including intervening native IRQ writers. This pairing belongs in bus/event IO; never reconstruct architectural stack state.
- NativeDspRoutingJob adapter readRoute/writeRoute/readDsp/writeDsp/readFault/writeFault must refer to the same backing protocol data as global resource writes. Global reset of primary174a8/alternate174aa and status17527 must update the corresponding domain DSP mirror. readRoute observes live mirrors; context is a transport cache, never an alternative source of truth.

Named data resources:
- CurrentPart17526, BasePart167ba, CurrentVoice174a7; Controls(part)=174ae+20*part; PartParameters(part)=160d2+334*part; patchReference stored at Controls+0.
- GlobalControlsMirror=Controls+9..16; PedalConfiguration=PartParameters+313. Patch read fields are named by synth parameter field numbers in existing Task classes; Setup copies patch179..186 and pedal209. Mode218 is latched when SecondTime reads it and retained through following Range/Mix/Matrix blocks.
- Master live patch168fc (master89,level91,route124,inverse90,status208), old status17527.
- TimingRate1512e, ShapeBase15132, FirstTimeTable159f4, ClampedTimeTable15a7c, PairTable152fa, MatrixSourceRows15282; row selector patch217 clamps5. MatrixConfig17546..58, GeneratedCoefficients1752e..38, TailCoefficients1753a..44. Matrix uses six seed recurrence words, six source-tail words and twelve DSP uploads.
- Effect reads the original selected patch after Final; flags123,amount248,time247,toggle246. It returns typed completion after coefficient and B/A DSP words; no extra inline return interval.

Proof: full20484 complete requests /59968247 independent H8 boundaries /2868 IRQ pauses /29328441 live source mutations; includes four65537-poll fault-child graphs. Both allocation/sanitizer probes420 /2513441 boundaries /160 IRQ /549911 mutations; allocation0 during admitted stepping. Exact watched musical data+MMIO byte values/order/clocks, original caller continuation/return, final global data. Pure native source has no architectural CPU dependency. Sanitizers instrument own graph/header/test; frozen original dependencies are not instrumented. This does not assert whole controller completion, audio/Windows equality or CPU savings; parent must integrate, run exact command/audio suites and only then measure CPU.
