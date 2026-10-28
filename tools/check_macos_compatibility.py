#!/usr/bin/env python3
"""Verify the architectures and actual deployment targets of a macOS bundle."""
import argparse
import json
import plistlib
import re
import subprocess
from pathlib import Path


def version(value):
    parts = tuple(int(part) for part in value.split('.'))
    if not 1 <= len(parts) <= 3:
        raise ValueError(f'Invalid macOS version: {value}')
    return parts + (0,) * (3 - len(parts))


def check_bundle(bundle, architectures, maximum_macos='12.0'):
    bundle = Path(bundle)
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    binary = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
    actual = set(subprocess.check_output(['lipo', '-archs', str(binary)], text=True).split())
    expected = set(architectures)
    if actual != expected:
        raise RuntimeError(f'{bundle}: architectures {sorted(actual)}, expected {sorted(expected)}')
    limit = version(maximum_macos)
    for minimum in [info.get('LSMinimumSystemVersion'),
                    *info.get('LSMinimumSystemVersionByArchitecture', {}).values()]:
        if minimum and version(minimum) > limit:
            raise RuntimeError(f'{bundle}: Info.plist requires macOS {minimum}, expected <= {maximum_macos}')
    result = {}
    for arch in sorted(actual):
        output = subprocess.check_output(
            ['xcrun', 'vtool', '-arch', arch, '-show-build', str(binary)], text=True)
        platform = re.search(r'^\s*platform\s+(\S+)', output, re.MULTILINE)
        minimum = re.search(r'^\s*minos\s+(\S+)', output, re.MULTILINE)
        # Older linkers use LC_VERSION_MIN_MACOSX instead of LC_BUILD_VERSION.
        if not minimum and 'LC_VERSION_MIN_MACOSX' in output:
            minimum = re.search(r'^\s*version\s+(\S+)', output, re.MULTILINE)
        if not minimum or (platform and platform[1] != 'MACOS'):
            raise RuntimeError(f'{bundle} ({arch}): missing macOS deployment target')
        if version(minimum[1]) > limit:
            raise RuntimeError(f'{bundle} ({arch}): binary requires macOS {minimum[1]}, expected <= {maximum_macos}')
        result[arch] = dict(minimum_macos=minimum[1])
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--architectures', nargs='+', default=['arm64', 'x86_64'])
    parser.add_argument('--maximum-macos', default='12.0')
    args = parser.parse_args()
    print(json.dumps(check_bundle(args.bundle, args.architectures, args.maximum_macos), indent=2))
