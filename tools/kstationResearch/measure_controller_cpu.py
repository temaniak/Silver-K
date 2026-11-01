"""Alternating plain-Release whole-process CPU measurements, separate from traces.

The executable supplies user+system RUSAGE_SELF over delivered audio frames.
This synthetic paced host is not a DAW or physical audio-driver measurement.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess

from compare_completion import FIELDS


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('firmware', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--variant', action='append', nargs=3, metavar=('LABEL', 'BINARY', 'MODE'), required=True)
    p.add_argument('--scenarios', nargs='+', default=['idle', 'held8', 'notes', 'clockburst'])
    p.add_argument('--seconds', type=float, default=8)
    p.add_argument('--warm-seconds', type=float, default=2)
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--deadline-gate', action='store_true')
    args = p.parse_args()
    assert args.seconds > 0 and args.repeats > 0 and args.warm_seconds >= 0
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    variants = [(label, Path(binary).resolve(), mode) for label, binary, mode in args.variant]
    assert len(set(x[0] for x in variants)) == len(variants)
    paths = {str(path): digest(path) for path in {args.firmware.resolve(), *(v[1] for v in variants)}}
    assert digest(args.firmware) == '2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
    result = {'measurement': 'RUSAGE_SELF user+system / delivered audio duration; all process threads',
              'paced_synthetic_host': True, 'deadline_gate': args.deadline_gate,
              'seconds': args.seconds, 'warm_seconds': args.warm_seconds,
              'repeats': args.repeats, 'block': 256, 'identities': paths,
              'runner_sha256': digest(Path(__file__)), 'runs': [], 'passed': False}
    output = args.output / 'summary.json'
    save(output, result)
    fields = [field for field in FIELDS if field != 'memory_fnv'] + ['peak', 'rms']
    try:
        for scenario in args.scenarios:
            expected = None
            native_memory = None
            for repeat in range(args.repeats):
                order = variants[repeat % len(variants):] + variants[:repeat % len(variants)]
                for label, binary, mode in order:
                    for path, checksum in paths.items():
                        assert digest(Path(path)) == checksum, f'Artifact changed: {path}'
                    prefix = args.output / f'{scenario}-{repeat}-{label}'
                    command = [str(binary), str(args.firmware.resolve()), scenario,
                               str(args.seconds), 'off', 'paced', '256', str(prefix), str(args.warm_seconds)]
                    with prefix.with_suffix('.log').open('w') as log:
                        code = subprocess.run(command, env=dict(os.environ, K_STAGE1_MODE=mode),
                                              stdout=log, stderr=subprocess.STDOUT).returncode
                    row = {'scenario': scenario, 'repeat': repeat, 'variant': label,
                           'command': command, 'returncode': code, 'passed': False}
                    result['runs'].append(row)
                    save(output, result)
                    assert code == 0, f'{prefix.name} failed; log retained'
                    report = json.loads(prefix.with_suffix('.json').read_text())
                    row['report'] = report
                    assert report['controller_mode'] == mode
                    assert report['dropped_frames'] == 0
                    assert report['process_cpu_seconds'] is not None
                    assert report['cpu_percent_one_core'] > 0
                    row['paced_duration_passed'] = report['wall_seconds'] <= report['audio_seconds'] + 0.05
                    if mode == 'native':
                        assert report['interpreted_steps'] == 0
                        assert report['cumulative_interpreted_steps'] == 0
                        if native_memory is None:
                            native_memory = report['memory_fnv']
                        assert report['memory_fnv'] == native_memory, 'Native full-RAM checksum differs'
                    current = {field: report[field] for field in fields}
                    if expected is None:
                        expected = current
                    row['different_functional_fields'] = [field for field in fields if current[field] != expected[field]]
                    assert not row['different_functional_fields'], row['different_functional_fields']
                    if args.deadline_gate:
                        row['deadline_passed'] = (report['block_compute_over_budget'] == 0
                            and report['late_block_completions'] == 0 and row['paced_duration_passed'])
                    else:
                        assert report['block_compute_over_budget'] is None
                        assert report['max_block_compute_seconds'] is None
                    row['passed'] = True
                    save(output, result)
                    print(scenario, repeat, label, f"{report['cpu_percent_one_core']:.3f}% one CPU",
                          f"deadline={row.get('deadline_passed', 'separate series')}", flush=True)
        result['statistics'] = {}
        for scenario in args.scenarios:
            values = {}
            for label, _, _ in variants:
                runs = [r for r in result['runs'] if r['scenario'] == scenario and r['variant'] == label]
                numbers = [r['report']['cpu_percent_one_core'] for r in runs]
                values[label] = {'median_percent_one_core': statistics.median(numbers),
                                 'min': min(numbers), 'max': max(numbers), 'runs': len(numbers)}
            result['statistics'][scenario] = values
        result['passed'] = all(r['passed'] for r in result['runs'])
        result['paced_duration_failures'] = sum(not r['paced_duration_passed'] for r in result['runs'])
        if args.deadline_gate:
            result['all_deadlines_passed'] = all(r['deadline_passed'] for r in result['runs'])
    except (AssertionError, OSError, KeyError, ValueError) as error:
        result['error'] = str(error)
    finally:
        save(output, result)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
