#!/usr/bin/env python3
"""Package the standalone Wave ROM Builder without changing a Silver-K release."""
import argparse
import hashlib
import json
import plistlib
import shutil
import subprocess
import zipfile
from pathlib import Path
from check_macos_compatibility import check_bundle

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.1.0'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--application', type=Path, required=True)
    parser.add_argument('--juce-source', type=Path, required=True)
    parser.add_argument('--platform', choices=['macOS-arm64', 'macOS-universal', 'Windows-x64'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compatibility = None
    name = f'Silver-K-Wave-ROM-Builder-{VERSION}-{args.platform}'
    stage = args.output / name
    archive = args.output / (name + '.zip')
    if stage.exists() or archive.exists():
        raise RuntimeError('Choose a new output directory; existing packages are preserved')
    if not (args.juce_source / 'LICENSE.md').is_file():
        raise RuntimeError('JUCE source license is missing')
    if args.platform.startswith('macOS'):
        info = plistlib.loads((args.application / 'Contents/Info.plist').read_bytes())
        if info['CFBundleShortVersionString'] != VERSION:
            raise RuntimeError('Application version mismatch')
        architectures = ['arm64', 'x86_64'] if args.platform == 'macOS-universal' else ['arm64']
        compatibility = check_bundle(args.application, architectures)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(args.application)], check=True)
    else:
        if args.application.read_bytes()[:2] != b'MZ':
            raise RuntimeError('Expected a Windows executable')
    stage.mkdir(parents=True)
    if args.application.is_dir():
        shutil.copytree(args.application, stage / args.application.name, symlinks=True)
    else:
        shutil.copy2(args.application, stage / args.application.name)
    for source, target in [('doc/WAVE_ROM_BUILDER.md', 'README.md'), ('LICENSE.md', 'LICENSE.md')]:
        shutil.copy2(ROOT / source, stage / target)
    shutil.copy2(args.juce_source / 'LICENSE.md', stage / 'JUCE-LICENSE.md')
    source = stage / 'Wave-ROM-Builder-source.zip'
    with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as output:
        files = list((ROOT / 'source/novation/waveRomBuilder').rglob('*'))
        files += [ROOT / 'cmake/JuceCompatibility.cmake', ROOT / 'cmake/MacOSDefaults.cmake',
                  ROOT / 'doc/WAVE_ROM_BUILDER.md', ROOT / 'LICENSE.md']
        for path in sorted(files):
            if path.is_file():
                if path.suffix not in {'.cpp', '.h', '.txt', '.md', '.cmake'}:
                    raise RuntimeError(f'Unexpected source artifact: {path.name}')
                output.write(path, path.relative_to(ROOT))
    # Include the exact dependency source too; the application needs no DSP/JIT
    # or original instrument assets. Do not include local build directories.
    dependency = stage / 'JUCE-source.zip'
    with zipfile.ZipFile(dependency, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(args.juce_source.rglob('*')):
            relative = path.relative_to(args.juce_source)
            if any(part in {'.git', 'CMakeFiles', '__pycache__'} for part in relative.parts):
                continue
            if path.is_file():
                output.write(path, Path('JUCE') / relative)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    manifest = dict(application='Silver-K Wave ROM Builder', version=VERSION, platform=args.platform,
                    base_commit=revision, local_changes=True, source_sha256=sha(source),
                    juce_source_sha256=sha(dependency), firmware_included=False,
                    sampled_wave_bank_included=False, signing='ad-hoc; not notarized' if args.platform.startswith('macOS') else None)
    if compatibility:
        manifest['macos_compatibility'] = compatibility
    (stage / 'BUILD-IDENTITY.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (stage / 'SOURCES.txt').write_text(
        'Application source: Wave-ROM-Builder-source.zip\n'
        'Pinned JUCE source: JUCE-source.zip\n'
        'JUCE revision: 2a8699d723fe9279e1148275485ddb0e18957f56\n'
        'Extract both sources; use -DJUCE_SOURCE=/absolute/path/to/JUCE when configuring.\n'
        'Build instructions are in README.md. No instrument firmware or samples are included.\n')
    checks = ''.join(f'{sha(p)}  {p.relative_to(stage).as_posix()}\n' for p in sorted(stage.rglob('*')) if p.is_file() and not p.is_symlink())
    (stage / 'SHA256SUMS.txt').write_text(checks)
    if args.platform.startswith('macOS'):
        subprocess.run(['ditto', '-c', '-k', '--keepParent', str(stage), str(archive)], check=True)
    else:
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
            for path in sorted(stage.rglob('*')):
                output.write(path, path.relative_to(stage.parent))
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None:
            raise RuntimeError('Package CRC check failed')
    archive.with_suffix('.zip.sha256').write_text(f'{sha(archive)}  {archive.name}\n')
    print(f'Packaged {archive} ({archive.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
