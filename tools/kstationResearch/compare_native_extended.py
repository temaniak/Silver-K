"""Streaming, functional-only Original/native board comparison for eight cases.

Run only with frozen functional trace executables. No PCM, Flash, MIDI or timing
normalization is applied. RAM excludes precisely the Original run's audited
context-write addresses, after rejecting context/global aliases.
"""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

from compare_completion import FIELDS, digest

SCENARIOS = ['sysexpatch', 'sysexreply', 'nrpnboundary', 'expression',
             'programstress', 'uipanel', 'uimenu', 'uistore']
FIRMWARE_SHA256 = '2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
CHUNK = 1 << 20
MEMORY_SIZE = 1 << 24
# Function-entry observations from Original.remaining.json, not synthetic input
# counts or Native owner counters. "window" is the two-second capture only.
UI_ENTRIES = {
    0x3c10: 'main_key_dequeue', 0x3c48: 'previous_key', 0x3c80: 'next_key',
    0x3cb8: 'parameter_key', 0x3dc4: 'write_key', 0x3d4e: 'program_key',
    0x3d82: 'compare_key', 0x3e26: 'menu_key', 0x3e36: 'global_key',
    0x3e74: 'midi_key', 0x3eb4: 'modulation_key', 0x3ede: 'arp_key',
    0x4054: 'panel_button', 0x4062: 'panel_relative', 0x406e: 'panel_absolute',
    0x44dc: 'menu_select', 0x44fa: 'menu_next', 0x450e: 'menu_previous',
    0x451e: 'menu_selection_write', 0x4554: 'menu_display', 0x45aa: 'menu_edit',
    0x4904: 'write_initialize', 0x49ee: 'write_finalize',
    0x4d3a: 'program_store', 0x4e10: 'global_store',
    0x3c6e: 'previous_menu_repeat', 0x3ca6: 'next_menu_repeat',
    0x481a: 'program_edit', 0x0ba0: 'serial_tx_data_write',
}
KEY_HANDLERS = {0x3c48, 0x3c80, 0x3cb8, 0x3dc4, 0x3d4e, 0x3d82,
                0x3e26, 0x3e36, 0x3e74, 0x3eb4, 0x3ede}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def save(path, value):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def raw_compare(original, native):
    hashes = [hashlib.sha256(), hashlib.sha256()]
    first, offset, lengths = None, 0, [0, 0]
    with original.open('rb') as a, native.open('rb') as b:
        while True:
            x, y = a.read(CHUNK), b.read(CHUNK)
            hashes[0].update(x)
            hashes[1].update(y)
            lengths[0] += len(x)
            lengths[1] += len(y)
            if first is None and x != y:
                first = offset + next((i for i, (u, v) in enumerate(zip(x, y))
                                       if u != v), min(len(x), len(y)))
            if not x and not y:
                break
            offset += max(len(x), len(y))
    return {'byte_exact': first is None, 'original_bytes': lengths[0],
            'native_bytes': lengths[1], 'first_different_byte': first,
            'original_sha256': hashes[0].hexdigest(),
            'native_sha256': hashes[1].hexdigest()}


def trace_compare(original, native, width, fields):
    record = struct.Struct('<' + 'Q' * width)
    selected = struct.Struct('<' + 'Q' * len(fields))
    raw_hashes = [hashlib.sha256(), hashlib.sha256()]
    selected_hashes = [hashlib.sha256(), hashlib.sha256()]
    counts, malformed = [0, 0], []

    def records(path, side):
        with path.open('rb') as source:
            while True:
                chunk = source.read(record.size * 8192)
                if not chunk:
                    return
                raw_hashes[side].update(chunk)
                aligned = len(chunk) - len(chunk) % record.size
                for row in record.iter_unpack(chunk[:aligned]):
                    counts[side] += 1
                    selected_hashes[side].update(selected.pack(*(row[k] for k in fields)))
                    yield row
                if aligned != len(chunk):
                    malformed.append({'side': ['original', 'native'][side],
                                      'record': counts[side],
                                      'trailing_bytes': len(chunk) - aligned})

    missing = object()
    first = None
    for index, (x, y) in enumerate(itertools.zip_longest(
            records(original, 0), records(native, 1), fillvalue=missing)):
        if first is None and (x is missing or y is missing):
            first = {'record': index, 'byte_offset': index * record.size,
                     'reason': 'record_count',
                     'original': None if x is missing else x,
                     'native': None if y is missing else y}
        elif first is None and any(x[k] != y[k] for k in fields):
            first = {'record': index, 'byte_offset': index * record.size,
                     'reason': 'selected_fields', 'original': x, 'native': y,
                     'different_fields': [k for k in fields if x[k] != y[k]]}
    return {'exact': first is None and not malformed, 'fields_compared': fields,
            'original_records': counts[0], 'native_records': counts[1],
            'first_difference': first, 'malformed_records': malformed,
            'original_raw_sha256': raw_hashes[0].hexdigest(),
            'native_raw_sha256': raw_hashes[1].hexdigest(),
            'original_selected_sha256': selected_hashes[0].hexdigest(),
            'native_selected_sha256': selected_hashes[1].hexdigest()}


def context_addresses(audit):
    require(audit['observed_phases'] > 0, 'Original stack audit observed no phases')
    require(audit['bytes_outside_onchip'] == 0,
            'Original context escaped on-chip RAM; refusing RAM exclusions')
    require(not audit['global_overlap_sites'],
            'Original context aliases live globals; refusing RAM exclusions')
    written = audit['written_addresses']
    require(written and all(type(at) is int and 0xfffe92 <= at < 0xffff10
                            for at in written),
            'Original context addresses must all be within FFFE92..FFFF10')
    return sorted(set(written))


def memory_compare(original, native, audit):
    excluded = context_addresses(audit)
    require(original.stat().st_size == native.stat().st_size == MEMORY_SIZE,
            'Both RAM files must contain all 16 MiB')
    raw_hashes = [hashlib.sha256(), hashlib.sha256()]
    semantic_hashes = [hashlib.sha256(), hashlib.sha256()]
    different, context_different, first = 0, 0, []
    with original.open('rb') as a, native.open('rb') as b:
        for offset in range(0, MEMORY_SIZE, CHUNK):
            x, y = bytearray(a.read(CHUNK)), bytearray(b.read(CHUNK))
            require(len(x) == len(y) == CHUNK, 'RAM changed during comparison')
            raw_hashes[0].update(x)
            raw_hashes[1].update(y)
            for at in excluded:
                if offset <= at < offset + len(x):
                    i = at - offset
                    context_different += x[i] != y[i]
                    x[i] = y[i] = 0
            semantic_hashes[0].update(x)
            semantic_hashes[1].update(y)
            if x != y:
                for i, (u, v) in enumerate(zip(x, y)):
                    if u != v:
                        different += 1
                        if len(first) < 32:
                            first.append({'address': offset + i, 'original': u, 'native': v})
    return {'exact': different == 0, 'bytes': MEMORY_SIZE,
            'excluded_original_context_addresses': excluded,
            'context_bytes_different': context_different,
            'semantic_difference_count': different, 'first_semantic_differences': first,
            'original_raw_sha256': raw_hashes[0].hexdigest(),
            'native_raw_sha256': raw_hashes[1].hexdigest(),
            'original_semantic_sha256': semantic_hashes[0].hexdigest(),
            'native_semantic_sha256': semantic_hashes[1].hexdigest()}


def ui_witnesses(coverage):
    rows = {entry['address']: entry for entry in coverage.get('observed_entries', coverage['entries'])}
    observed = []
    for address, name in UI_ENTRIES.items():
        row = rows.get(address, {})
        if row.get('total', 0) or row.get('window', 0):
            observed.append({'address': address, 'address_hex': f'{address:04X}',
                             'entry': name, 'capture_entries': row.get('window', 0),
                             'cumulative_entries': row.get('total', 0)})
    return {'source': 'Original.remaining.json per-phase observed PC entries',
            'capture_key_handler_entries': sum(rows.get(at, {}).get('window', 0)
                                               for at in KEY_HANDLERS),
            'entries': observed,
            'input_model': 'synthetic keyboard queue words / rotary timer counts / level-ready TX IRQ'}


def run(binary, firmware, scenario, mode, prefix):
    command = [str(binary), str(firmware), scenario, '2', 'capture', 'offline',
               '256', str(prefix), '2']
    result = {'command': command, 'controller_mode': mode, 'returncode': None}
    with prefix.with_suffix('.log').open('w') as log:
        result['returncode'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
            env=dict(os.environ, K_STAGE1_MODE=mode,
                     K_RESEARCH_FAILURE_TAPE=str(prefix))).returncode
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('oracle_binary', type=Path)
    parser.add_argument('native_binary', type=Path)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--scenarios', nargs='+', choices=SCENARIOS, default=SCENARIOS)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    paths = {name: getattr(args, name).resolve()
             for name in ('oracle_binary', 'native_binary', 'firmware')}
    summary = {'passed': False, 'exit_code': None, 'cpu_measured': False,
               'capture_seconds': 2, 'warm_seconds': 2, 'block': 256,
               'scenarios_requested': args.scenarios, 'results': []}
    summary_path = args.output / 'summary.json'
    identities = {}

    def check_identity():
        for name, path in paths.items():
            observed = digest(path)
            if observed != identities[name]['sha256']:
                summary.setdefault('identity_violations', []).append({
                    'artifact': name, 'expected_sha256': identities[name]['sha256'],
                    'observed_sha256': observed})
                raise ValueError(f'{name} changed during functional series')

    try:
        identities = {name: {'path': str(path), 'sha256': digest(path),
                             'bytes': path.stat().st_size} for name, path in paths.items()}
        identity = {'artifacts': identities, 'cpu_measured': False,
                    'runner_sha256': digest(Path(__file__)),
                    'report_fields_compared': [field for field in FIELDS if field != 'memory_fnv'],
                    'expected_firmware_sha256': FIRMWARE_SHA256,
                    'sound_normalization': None, 'timing_normalization': None,
                    'architecture_fields_excluded': {'ports': [1],
                        'slices': [2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]},
                    'ram_exclusion_source': 'each scenario Original.stack.json written_addresses'}
        save(args.output / 'identity.json', identity)
        save(summary_path, summary)
        require(len(args.scenarios) == len(set(args.scenarios)),
                'Duplicate scenarios would overwrite retained artifacts')
        require(identities['firmware']['sha256'] == FIRMWARE_SHA256,
                'Firmware SHA256 differs from the pinned decoded K image')
        for scenario in args.scenarios:
            case = {'scenario': scenario, 'passed': False, 'errors': [], 'runs': {}}
            summary['results'].append(case)
            prefixes = {side: args.output / f'{scenario}-{side}'
                        for side in ('original', 'native')}
            for side, binary in [('original', paths['oracle_binary']),
                                 ('native', paths['native_binary'])]:
                check_identity()
                try:
                    case['runs'][side] = run(binary, paths['firmware'], scenario, side, prefixes[side])
                    if case['runs'][side]['returncode'] != 0:
                        case['errors'].append(f'{side} execution failed; raw failure tape retained')
                except OSError as error:
                    case['runs'][side] = {'returncode': None, 'error': str(error)}
                    case['errors'].append(f'{side} could not execute: {error}')
                check_identity()
                save(summary_path, summary)
            try:
                reports = {side: json.loads(prefix.with_suffix('.json').read_text())
                           for side, prefix in prefixes.items()}
                case['reports'] = reports
                for side, report in reports.items():
                    require(report['cpu_measured'] is False, f'{side} measured CPU')
                    require(report['controller_mode'] == side, f'{side} selected the wrong runtime')
                    for field in ('process_cpu_seconds', 'wall_seconds', 'cpu_percent_one_core'):
                        require(report.get(field) is None, f'{side} emitted CPU measurement {field}')
                native = reports['native']
                require(native['interpreted_steps'] == 0 and native['cumulative_interpreted_steps'] == 0,
                        'Native backend executed H8 instructions in boot/warm/capture')
                require(native['implementation'] in ('native-controller-development', 'native-controller'),
                        'Native controller implementation identity mismatch')
                if native['implementation'] == 'native-controller':
                    require(native['full_native_ready'] is True, 'Native capability flag missing')
                fields = [field for field in FIELDS if field != 'memory_fnv']
                case['fields_compared'] = fields
                case['different_report_fields'] = [
                    {'field': field, 'original': reports['original'][field], 'native': native[field]}
                    for field in fields if reports['original'][field] != native[field]]
                case['raw'] = {ext: raw_compare(prefixes['original'].with_suffix('.' + ext),
                                               prefixes['native'].with_suffix('.' + ext))
                               for ext in ('pcm', 'flash', 'midi')}
                audit_path = prefixes['original'].with_suffix('.stack.json')
                audit = json.loads(audit_path.read_text())
                case['original_stack_audit'] = audit
                case['original_stack_audit_sha256'] = digest(audit_path)
                case['memory'] = memory_compare(prefixes['original'].with_suffix('.memory'),
                                                prefixes['native'].with_suffix('.memory'), audit)
                case['traces'] = {ext: trace_compare(prefixes['original'].with_suffix('.' + ext),
                    prefixes['native'].with_suffix('.' + ext), width, fields)
                    for ext, width, fields in [('ports', 5, [0, 2, 3, 4]),
                                               ('slices', 17, [0, 1, 5, 6])]}
                coverage = json.loads(prefixes['original'].with_suffix('.remaining.json').read_text())
                case['actual_ui_entry_witnesses'] = ui_witnesses(coverage)
                if scenario.startswith('ui'):
                    require(case['actual_ui_entry_witnesses']['capture_key_handler_entries'] > 0,
                            'Original capture observed no actual UI key-handler entry')
                    require(reports['original']['synthetic_tx_ready_requests'] > 0 and
                            reports['original']['synthetic_tx_ready_requests'] == native['synthetic_tx_ready_requests'],
                            'Synthetic TX stimulus differs or was not exercised')
                    visits = {row['address']: row.get('window', 0) for row in coverage['observed_entries']}
                    require(visits.get(0x0ba0, 0) > 0, 'Original capture never wrote serial TDR')
                    if scenario == 'uimenu':
                        for address in (0x3d82, 0x44dc, 0x45aa, 0x3c6e, 0x3ca6, 0x481a):
                            require(visits.get(address, 0) > 0,
                                    f'Original capture missed required menu service {address:04X}')
                case['passed'] = (not case['errors'] and not case['different_report_fields']
                    and all(row['byte_exact'] for row in case['raw'].values())
                    and case['memory']['exact']
                    and all(row['exact'] for row in case['traces'].values()))
            except (OSError, ValueError, KeyError, TypeError) as error:
                case['errors'].append(str(error))
            case['artifact_index'] = {
                path.name: {'bytes': path.stat().st_size, 'sha256': digest(path)}
                for prefix in prefixes.values() for path in sorted(args.output.glob(prefix.name + '.*'))
                if path.is_file()}
            check_identity()
            save(summary_path, summary)
            print(scenario, 'PASS' if case['passed'] else 'FAIL',
                  'exact functional comparison; CPU not measured', flush=True)
        summary['passed'] = all(case['passed'] for case in summary['results'])
        summary['exit_code'] = 0 if summary['passed'] else 1
        check_identity()
    except KeyboardInterrupt:
        summary['error'] = 'Interrupted; completed and partial artifacts retained'
        summary['exit_code'] = 130
    except (OSError, ValueError, KeyError, TypeError) as error:
        summary['error'] = str(error)
        summary['passed'] = False
        summary['exit_code'] = 1
    finally:
        save(summary_path, summary)
    return summary['exit_code']


if __name__ == '__main__':
    sys.exit(main())
