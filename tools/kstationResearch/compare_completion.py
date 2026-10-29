"""Bounded functional comparisons against the authenticated stage-4 H8 tape.

CPU measurement is disabled in the completion executables. The frozen original
data is streamed directly from its existing archive; no second ROM/raw corpus
is extracted and no equality tolerance or output normalization is applied.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

SCENARIOS = ['idle', 'held1', 'held8', 'notes', 'sweep', 'arp', 'clock', 'irq',
             'sustain', 'program', 'irq2', 'scierror', 'txirq', 'txclock',
             'txqueue', 'clockext', 'clockjitter', 'clockgap', 'clockburst', 'rxrunning']
FIELDS = ['frames', 'pcm_fnv', 'memory_fnv', 'flash_fnv', 'h8_instructions',
          'h8_cycles', 'dsp_instructions', 'dsp_cycles', 'hdi_reads', 'hdi_writes',
          'midi_received', 'midi_out_fnv', 'midi_out_count', 'dropped_frames',
          'before_patch', 'after_patch', 'bank', 'program', 'before_clock_source',
          'after_clock_source', 'before_clock_route', 'after_clock_route']


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def compare(archive, member, candidate):
    original_hash, candidate_hash = hashlib.sha256(), hashlib.sha256()
    first = None
    offset = 0
    with archive.open(member) as original, candidate.open('rb') as current:
        while True:
            a, b = original.read(1048576), current.read(1048576)
            original_hash.update(a)
            candidate_hash.update(b)
            if first is None and a != b:
                first = offset + next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            offset += len(a)
            if not a and not b:
                break
    return {'byte_exact': first is None,
            'bytes': candidate.stat().st_size,
            'reference_bytes': archive.getinfo(member).file_size,
            'first_different_byte': first,
            'reference_member': member,
            'reference_sha256': original_hash.hexdigest(),
            'candidate_sha256': candidate_hash.hexdigest()}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('build', type=Path)
    p.add_argument('firmware', type=Path)
    p.add_argument('reference_archive', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--scenarios', nargs='+', choices=SCENARIOS, default=SCENARIOS)
    p.add_argument('--mode', default='development', choices=['development', 'timer_rx', 'original'])
    args = p.parse_args()
    assert digest(args.firmware) == '2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
    assert digest(args.reference_archive) == '11df4f490835384f31a706e365e38ea3cda78595818b8d8699a8117dc7769295'
    args.output.mkdir(exist_ok=False)
    binary = args.build / 'kstation_completion_candidate_trace'
    identity = digest(binary)
    results = []
    with zipfile.ZipFile(args.reference_archive) as archive:
        for scenario in args.scenarios:
            prefix = args.output / f'{scenario}-native'
            env = dict(os.environ, K_STAGE1_MODE=args.mode)
            with prefix.with_suffix('.log').open('w') as log:
                code = subprocess.run([str(binary), str(args.firmware), scenario, '2', 'capture', 'offline',
                                       '256', str(prefix), '1'], stdout=log, stderr=subprocess.STDOUT, env=env).returncode
            assert code == 0, f'{scenario}: execution failed; preserve log'
            current = json.loads(prefix.with_suffix('.json').read_text())
            reference = json.loads(archive.read(f'correctness-final/{scenario}-original.json'))
            differences = [field for field in FIELDS if current[field] != reference[field]]
            raw = {extension: compare(archive, f'correctness-final/{scenario}-original.{extension}',
                                     prefix.with_suffix('.' + extension))
                   for extension in ['pcm', 'memory', 'flash', 'midi', 'ports', 'slices']}
            result = {'scenario': scenario, 'passed': not differences and all(v['byte_exact'] for v in raw.values()),
                      'report_differences': differences, 'raw': raw, 'run': current}
            results.append(result)
            (args.output / 'summary.json').write_text(json.dumps(results, indent=2))
            assert current['cpu_measured'] is False, 'Functional-only configuration required'
            assert digest(binary) == identity, 'Executable changed during functional series'
            assert result['passed'], f'{scenario}: exact comparison failed; preserve raw data'
            if args.mode == 'development':
                assert current['native_adc_steps'] > 0 and current['native_foreground_steps'] > 0
                assert current['native_dsp_transfer_steps'] > 0
                assert current['interpreted_steps'] > 0, 'Update readiness gate before claiming a complete controller'
            print(scenario, 'PASS exact PCM/RAM/Flash/MIDI/IO/slices; remaining H8', current['interpreted_steps'], flush=True)
    (args.output / 'identity.json').write_text(json.dumps({'binary_sha256': identity, 'mode': args.mode,
        'cpu_measured': False, 'reference_archive_sha256': digest(args.reference_archive)}, indent=2))


if __name__ == '__main__':
    main()
