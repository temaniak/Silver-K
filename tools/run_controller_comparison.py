#!/usr/bin/env python3
"""Capture original KS H8/DSP traffic and test a checked recorded C++ controller.

This is a finite scenario replay prototype, not a live MIDI replacement. Raw
bus tapes may contain private firmware/presets and remain local/ignored. Never
changes Windows policy, JIT, clocks or placement. Runs one process at a time.
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
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'ks-rom', 'flash', 'wave'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('output/perf-controller'))
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--scenarios', nargs='+', choices=['single', 'multi', 'fx'], default=['single', 'multi'])
    parser.add_argument('--seconds', type=float, default=2., help='Same bounded 2..4 s render for each mode')
    parser.add_argument('--dsp-threads', choices=['1', '2'], default='1')
    args = parser.parse_args()
    if args.repeats < 1 or not 2 <= args.seconds <= 4 or any(not p.is_file() for p in [args.executable, args.ks_rom, args.flash, args.wave]):
        parser.error('Positive repeats and existing executable/private input files required')
    root = args.output.resolve() / time.strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True, exist_ok=False)
    exe = args.executable.resolve()
    metadata = dict(executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                    started_utc=datetime.now(timezone.utc).isoformat(), dsp_threads=args.dsp_threads,
                    platform=platform.platform(), seconds=args.seconds)
    (root / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    rows = []
    print('Local results:', root, flush=True)

    def run(mode, scenario, tag, tape):
        folder = root / tag
        folder.mkdir()
        env = os.environ.copy()
        for name in ('SILVER_WORKER_WAIT', 'SILVER_KS_WORKER_CORES', 'SILVER_KS_DSP_THREADS'):
            env.pop(name, None)
        env['SILVER_KS_DSP_THREADS'] = args.dsp_threads if mode != 'boot' else '1'
        report = folder / 'report.json'
        print('Running', tag, flush=True)
        with (folder / 'fixture.log').open('w', encoding='utf-8') as log:
            result = subprocess.run([str(exe), str(args.ks_rom.resolve()), str(args.flash.resolve()),
                                     str(args.wave.resolve()), str(report), mode, scenario, str(tape), str(args.seconds)],
                                    env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        row = dict(test_id=tag, returncode=result.returncode, mode=mode, scenario=scenario,
                   completed_utc=datetime.now(timezone.utc).isoformat())
        if report.is_file():
            row['report'] = json.loads(report.read_text(encoding='utf-8-sig'))
        rows.append(row)
        (root / 'index.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
        if result.returncode or 'report' not in row:
            raise RuntimeError(f'{tag} failed; inspect retained fixture.log; no performance conclusion')
        print(json.dumps(row), flush=True)
        return row['report']

    run('boot', 'single', 'boot-capture', root / 'boot.kst')
    summaries = []
    for scenario in args.scenarios:
        tape = root / (scenario + '.kst')
        reference = run('record', scenario, scenario + '-capture', tape)
        for repeat in range(args.repeats):
            for mode in (['h8', 'replay'] if repeat % 2 == 0 else ['replay', 'h8']):
                current = run(mode, scenario, f'{scenario}-r{repeat+1}-{mode}', tape)
                for key in ['frames', 'audio_hash', 'peak', 'controller_end_cycles', 'dsp_a_cycles', 'dsp_b_cycles',
                            'dsp_a_instructions', 'dsp_b_instructions', 'serial_depth', 'serial_underruns', 'serial_overflows']:
                    if current[key] != reference[key]:
                        raise ValueError(f'{scenario}/{mode}: {key} differs; comparison invalid')
                if mode == 'replay' and (current['checked_ports'] != reference['trace_events'] or current['h8_start_cycles'] != current['h8_end_cycles']):
                    raise ValueError('Prototype did not validate all ports or executed original H8')
        case = [r['report'] for r in rows if r['scenario'] == scenario and r['mode'] in ['h8', 'replay']]
        summary = dict(scenario=scenario, exact_audio_and_dsp_counters=True, frames=reference['frames'],
                       audio_hash=reference['audio_hash'], trace_events=reference['trace_events'],
                       trace_slices=reference['trace_slices'], modes={},
                       tape_sha256=hashlib.sha256(tape.read_bytes()).hexdigest(), tape_bytes=tape.stat().st_size)
        for mode in ['h8', 'replay']:
            group = [r for r in case if r['mode'] == mode]
            summary['modes'][mode] = dict(runs=len(group),
                cpu_seconds=[r['cpu_seconds'] for r in group], wall_seconds=[r['wall_seconds'] for r in group],
                median_cpu_seconds=statistics.median(r['cpu_seconds'] for r in group),
                median_wall_seconds=statistics.median(r['wall_seconds'] for r in group))
        before, after = summary['modes']['h8'], summary['modes']['replay']
        if os.name == 'nt' or platform.system() == 'Darwin':
            summary['cpu_change_percent'] = (after['median_cpu_seconds'] / before['median_cpu_seconds'] - 1) * 100
        summary['wall_change_percent'] = (after['median_wall_seconds'] / before['median_wall_seconds'] - 1) * 100
        summaries.append(summary)
        (root / 'summary.json').write_text(json.dumps(summaries, indent=2), encoding='utf-8')
        print(json.dumps(summary, indent=2), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
