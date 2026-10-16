#!/usr/bin/env python3
"""Private KS live C++ controller stages vs original H8; no tape playback.

--controller hybrid retains original callees. --controller native runs all
post-boot control flow in C++, with a locked H8 interpreter and verified code
image. Both retain original DSP timing; neither is recorded tape playback.
Compare full DSP bus tapes, all audio, controller state/Flash and MIDI output.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import platform
from pathlib import Path
import statistics
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'ks-rom', 'flash', 'wave'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--reference-executable', type=Path, help='Optional frozen original-H8 fixture (records its identity; legacy state snapshots may be unavailable)')
    p.add_argument('--output', type=Path, default=Path('output/perf-live-controller'))
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--scenarios', nargs='+', choices=['single', 'multi', 'midi', 'pedal', 'controls', 'state', 'clock', 'steal', 'fx'], default=['single', 'multi', 'midi', 'pedal'])
    p.add_argument('--seconds', type=float, default=4., help='Same bounded 2..4 s work for bus/audio/CPU')
    p.add_argument('--dsp-threads', choices=['1', '2'], default='1')
    p.add_argument('--controller', choices=['hybrid', 'native'], default='hybrid', help='Staged hybrid or full native post-boot execution')
    p.add_argument('--dsp-transport', choices=['compat', 'events'], default='compat', help='Native HDI compatibility boundaries or timed live protocol jobs')
    p.add_argument('--toggle', action='store_true', help='Disable at 0.65 s and re-enable at 0.95 s')
    args = p.parse_args()
    if args.dsp_transport == 'events' and args.controller != 'native':
        p.error('Event DSP transport requires the full native controller')
    if args.repeats < 1 or not 2 <= args.seconds <= 4 or any(not v.is_file() for v in [args.executable, args.ks_rom, args.flash, args.wave]):
        p.error('Positive repeats and existing executable/private inputs required')
    if args.reference_executable and not args.reference_executable.is_file():
        p.error('Reference executable must exist')
    root = args.output.resolve() / time.strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True, exist_ok=False)
    exe = args.executable.resolve()
    reference = args.reference_executable.resolve() if args.reference_executable else exe
    (root / 'metadata.json').write_text(json.dumps(dict(
        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
        reference_executable_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
        runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        started_utc=datetime.now(timezone.utc).isoformat(), platform=platform.platform(), architecture=platform.machine(),
        cpu_measurement=('whole-process user+kernel; render only' if os.name == 'nt' or platform.system() == 'Darwin' else 'unavailable'),
        controller=args.controller, dsp_transport=args.dsp_transport, dsp_threads=args.dsp_threads, toggle=args.toggle, seconds=args.seconds,
        comparison=('original H8 vs full native post-boot C++ control flow; no H8 fallback' if args.controller == 'native' else 'original H8 vs staged hybrid C++ controller')), indent=2))
    print('Local results:', root, flush=True)
    rows, summaries = [], []

    def run(mode, scenario, tag):
        folder = root / tag
        folder.mkdir()
        env = os.environ.copy()
        for name in ('SILVER_WORKER_WAIT', 'SILVER_KS_WORKER_CORES', 'SILVER_KS_DSP_THREADS', 'SILVER_KS_CONTROLLER', 'SILVER_KS_DSP_TRANSPORT'):
            env.pop(name, None)
        env['SILVER_KS_DSP_THREADS'] = args.dsp_threads
        env['SILVER_KS_DSP_TRANSPORT'] = args.dsp_transport
        report, tape = folder / 'report.json', folder / 'bus.kst'
        print('Running', tag, flush=True)
        with (folder / 'fixture.log').open('w', encoding='utf-8') as log:
            selected = reference if mode in ('h8', 'record') else exe
            r = subprocess.run([str(selected), str(args.ks_rom.resolve()), str(args.flash.resolve()),
                                str(args.wave.resolve()), str(report), mode, scenario, str(tape), str(args.seconds)],
                               stdout=log, stderr=subprocess.STDOUT, env=env, timeout=120)
        row = dict(test_id=tag, returncode=r.returncode, mode=mode, scenario=scenario)
        if report.is_file():
            row['report'] = json.loads(report.read_text(encoding='utf-8-sig'))
        rows.append(row)
        (root / 'index.json').write_text(json.dumps(rows, indent=2))
        if r.returncode or 'report' not in row:
            raise RuntimeError(f'{tag} failed; inspect retained log; comparison invalid')
        return row['report']

    keys = ['frames', 'audio_hash', 'peak', 'h8_start_cycles', 'h8_end_cycles',
            'controller_end_cycles', 'dsp_a_cycles', 'dsp_b_cycles', 'dsp_a_instructions',
            'dsp_b_instructions', 'serial_depth', 'serial_underruns', 'serial_overflows']
    snapshots = ['state_digest', 'flash_digest', 'flash_revision', 'midi_tx_digest', 'midi_tx_bytes', 'midi_received']

    def equal(a, b, fields):
        for key in fields:
            if a[key] != b[key]:
                raise ValueError(f'{key} differs ({a[key]} / {b[key]}); no equivalence claim')

    for scenario in args.scenarios:
        candidate = ('native' if args.controller == 'native' else 'live') + ('-toggle' if args.toggle else '')
        before = run('record', scenario, scenario + '-h8-bus')
        after = run(candidate + '-record', scenario, scenario + '-live-bus')
        if before.get('controller_backend') != 'original-h8':
            raise ValueError('Reference must execute original H8')
        snapshot_available = all(key in before for key in snapshots)
        if not snapshot_available and not args.reference_executable:
            raise ValueError('Current H8 control must provide state/Flash/MIDI snapshots')
        compared = keys + (snapshots if snapshot_available else [])
        equal(before, after, compared + ['trace_events', 'trace_slices', 'trace_digest', 'bus'])
        original_tape = root / (scenario + '-h8-bus') / 'bus.kst'
        native_tape = root / (scenario + '-live-bus') / 'bus.kst'
        # Explicit bytes, including cycle/PC/read responses/slice clocks, not
        # merely a matching aggregate transaction count.
        if original_tape.read_bytes() != native_tape.read_bytes():
            raise ValueError('Full H8/native bus tapes differ')
        for repeat in range(args.repeats):
            for mode in (['h8', candidate] if repeat % 2 == 0 else [candidate, 'h8']):
                current = run(mode, scenario, f'{scenario}-r{repeat+1}-{mode}')
                equal(before, current, compared)
                if mode == candidate:
                    if current.get('native_midi_messages', 0) == 0:
                        raise ValueError('C++ mode handled no MIDI')
                    if args.controller == 'native':
                        if not current.get('native_runtime_steps', 0) or current.get('original_h8_step_calls', -1) != 0 or current.get('h8_interpreted_steps_since_handoff', -1) != 0:
                            raise ValueError('Native mode did not establish H8-free execution')
                        if not args.toggle and current.get('h8_interpreted_steps_after_boot', -1) != 0:
                            raise ValueError('H8 executed during native post-boot preparation/playback')
                        if args.dsp_transport == 'events' and (current.get('transport_jobs', 0) == 0 or current.get('transport_job_boundaries', 0) == 0):
                            raise ValueError('Native event transport did not execute live protocol jobs')
        summary = dict(scenario=scenario, full_bus_bytes_identical=True, state_flash_midi_snapshots_compared=snapshot_available,
                       audio_and_guest_counters_identical=True, frames=before['frames'],
                       audio_hash=before['audio_hash'], trace_events=before['trace_events'],
                       tape_sha256=hashlib.sha256(original_tape.read_bytes()).hexdigest(),
                       native_steps=after['native_midi_steps'], native_messages=after['native_midi_messages'],
                       note_on=after['native_note_on'], note_off=after['native_note_off'], cc=after['native_cc'], modes={},
                       note_steps=after['native_note_steps'], note_on_calls=after['native_note_on_calls'],
                       note_off_calls=after['native_note_off_calls'], sustain_calls=after['native_sustain_calls'],
                       sustain_release_calls=after['native_sustain_release_calls'],
                       voice_steps=after['native_voice_steps'], allocation_calls=after['native_allocation_calls'],
                       voice_start_calls=after['native_voice_start_calls'], voice_release_calls=after['native_voice_release_calls'],
                       transport_steps=after['native_transport_steps'], original_h8_step_calls=after['original_h8_step_calls'],
                       original_h8_pages=after['original_h8_pages'], controller_backend=after['controller_backend'],
                       native_runtime_steps=after.get('native_runtime_steps'),
                       native_interrupt_entries=after.get('native_interrupt_entries'),
                       h8_interpreted_steps_after_boot=after.get('h8_interpreted_steps_after_boot'),
                       h8_interpreted_steps_since_handoff=after.get('h8_interpreted_steps_since_handoff'),
                       dsp_transport=args.dsp_transport, transport_jobs=after.get('transport_jobs', 0),
                       transport_job_boundaries=after.get('transport_job_boundaries', 0))
        for mode in ['h8', candidate]:
            group = [r['report'] for r in rows if r['scenario'] == scenario and r['mode'] == mode]
            summary['modes'][mode] = dict(runs=len(group),
                cpu_seconds=[r['cpu_seconds'] for r in group], wall_seconds=[r['wall_seconds'] for r in group],
                median_cpu_seconds=statistics.median(r['cpu_seconds'] for r in group),
                median_wall_seconds=statistics.median(r['wall_seconds'] for r in group))
        if os.name == 'nt' or platform.system() == 'Darwin':
            summary['cpu_change_percent'] = (summary['modes'][candidate]['median_cpu_seconds'] /
                                             summary['modes']['h8']['median_cpu_seconds'] - 1) * 100
        summaries.append(summary)
        (root / 'summary.json').write_text(json.dumps(summaries, indent=2))
        print(json.dumps(summary, indent=2), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
