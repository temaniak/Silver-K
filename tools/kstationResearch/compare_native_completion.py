"""Exact native-board proof against the authenticated original firmware tape.

PCM, Flash and MIDI are never normalized. Trace comparison excludes only CPU
architecture fields unavailable in a CPU-independent backend. RAM excludes only
addresses actually written as original call/IRQ context by an audited exact run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

from compare_completion import SCENARIOS, FIELDS, compare, digest


def trace_compare(archive, member, candidate, width, fields):
    record = struct.Struct('<' + 'Q' * width)
    count = 0
    with archive.open(member) as a, candidate.open('rb') as b:
        while True:
            x, y = a.read(record.size), b.read(record.size)
            if not x and not y:
                return {'exact': True, 'records': count, 'fields_compared': fields}
            if len(x) != record.size or len(y) != record.size:
                return {'exact': False, 'record': count, 'reason': 'length', 'fields_compared': fields}
            u, v = record.unpack(x), record.unpack(y)
            if any(u[k] != v[k] for k in fields):
                return {'exact': False, 'record': count, 'original': u, 'native': v, 'fields_compared': fields}
            count += 1


def memory_compare(archive, member, candidate, audit):
    assert audit['observed_phases'] > 0
    assert audit['bytes_outside_onchip'] == 0 and not audit['global_overlap_sites']
    excluded = set(audit['written_addresses'])
    assert excluded and all(0xfffe92 <= at < 0xffff10 for at in excluded)
    a, b = bytearray(archive.read(member)), bytearray(candidate.read_bytes())
    assert len(a) == len(b) == 1 << 24
    ignored_differences = [at for at in excluded if a[at] != b[at]]
    for at in excluded:
        a[at] = b[at] = 0
    differences = [at for at, (x, y) in enumerate(zip(a, b)) if x != y]
    return {'exact': not differences, 'bytes': len(a), 'excluded_original_context_addresses': sorted(excluded),
            'context_bytes_different': len(ignored_differences), 'semantic_difference_count': len(differences),
            'first_semantic_differences': differences[:32], 'original_semantic_sha256': hashlib.sha256(a).hexdigest(),
            'native_semantic_sha256': hashlib.sha256(b).hexdigest()}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('binary', type=Path)
    p.add_argument('firmware', type=Path)
    p.add_argument('reference_archive', type=Path)
    p.add_argument('stack_audits', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--scenarios', nargs='+', choices=SCENARIOS, default=SCENARIOS)
    a = p.parse_args()
    assert digest(a.firmware) == '2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
    assert digest(a.reference_archive) == '11df4f490835384f31a706e365e38ea3cda78595818b8d8699a8117dc7769295'
    identity = digest(a.binary)
    a.output.mkdir(exist_ok=False)
    results = []
    with zipfile.ZipFile(a.reference_archive) as archive:
        for case in a.scenarios:
            prefix = a.output / case
            with prefix.with_suffix('.log').open('w') as log:
                code = subprocess.run([str(a.binary), str(a.firmware), case, '2', 'capture', 'offline', '256', str(prefix), '1'],
                    env=dict(os.environ, K_STAGE1_MODE='native', K_RESEARCH_FAILURE_TAPE=str(prefix)),
                    stdout=log, stderr=subprocess.STDOUT).returncode
            assert code == 0, f'{case}: native execution failed; evidence retained'
            report = json.loads(prefix.with_suffix('.json').read_text())
            original = json.loads(archive.read(f'correctness-final/{case}-original.json'))
            assert report['cpu_measured'] is False and report['interpreted_steps'] == 0
            assert report['implementation'] == 'native-controller-development'
            audit_path = a.stack_audits / f'{case}-native.stack.json'
            audit = json.loads(audit_path.read_text())
            raw = {ext: compare(archive, f'correctness-final/{case}-original.{ext}', prefix.with_suffix('.' + ext))
                for ext in ['pcm', 'flash', 'midi']}
            memory = memory_compare(archive, f'correctness-final/{case}-original.memory', prefix.with_suffix('.memory'), audit)
            traces = {ext: trace_compare(archive, f'correctness-final/{case}-original.{ext}', prefix.with_suffix('.' + ext), width, fields)
                for ext, width, fields in [('ports', 5, [0, 2, 3, 4]), ('slices', 17, [0, 1, 5, 6])]}
            fields = [f for f in FIELDS if f != 'memory_fnv']
            different_fields = [f for f in fields if report[f] != original[f]]
            passed = not different_fields and memory['exact'] and all(v['exact'] for v in traces.values()) and all(v['byte_exact'] for v in raw.values())
            results.append({'scenario': case, 'passed': passed, 'different_report_fields': different_fields,
                'raw': raw, 'memory': memory, 'traces': traces, 'stack_audit_sha256': digest(audit_path), 'run': report})
            (a.output / 'summary.json').write_text(json.dumps(results, indent=2))
            assert digest(a.binary) == identity, 'Binary changed during series'
            assert passed, f'{case}: exact comparison failed; evidence retained'
            print(case, 'PASS exact PCM/Flash/MIDI/semantic RAM/IO clocks/order/128-clock boundaries', flush=True)
    (a.output / 'identity.json').write_text(json.dumps({'binary_sha256': identity, 'cpu_measured': False,
        'reference_archive_sha256': digest(a.reference_archive), 'architecture_fields_excluded': {
            'ports': ['program_counter'], 'slices': ['program_counter', 'CCR', 'EXR', 'register_bank', 'CPU_bus_histories']},
        'sound_normalization': None, 'timing_normalization': None}, indent=2))


if __name__ == '__main__':
    main()
