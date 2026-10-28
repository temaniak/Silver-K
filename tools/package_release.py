#!/usr/bin/env python3
"""Package built products, the zero Wave ROM, source and release documentation."""
import argparse
from contextlib import nullcontext
import hashlib
import json
import plistlib
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from check_plugin_identity import check_identity
from check_macos_compatibility import check_bundle

VERSION = '0.4.4a'
PLUGIN_VERSION = '0.4.4'
ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--products', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--platform', choices=['Windows-x64', 'macOS-arm64', 'macOS-universal'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', default='', help='Optional archive label for a separate experimental build')
    parser.add_argument('--test-guide', type=Path, help='Optional tester instructions to include')
    parser.add_argument('--wave-builder', type=Path, help='Companion application bundle or executable; otherwise discover beside silverPlugin')
    parser.add_argument('--staging-directory', type=Path, help='Keep a new staging directory instead of using/cleaning a system temporary folder')
    args = parser.parse_args()
    macos = args.platform.startswith('macOS')
    architectures = ['arm64', 'x86_64'] if args.platform == 'macOS-universal' else ['arm64']
    macos_compatibility = {}
    if args.variant and not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]{0,79}', args.variant):
        parser.error('Variant must be a short alphanumeric/hyphen label')
    if args.test_guide and not args.test_guide.is_file():
        parser.error('Test guide must exist')
    plugin_identity = check_identity(args.products)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    name = f'Silver-K-{VERSION}-{args.platform}'
    if args.variant:
        name += '-' + args.variant
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / (name + '.zip')
    if archive.exists():
        raise RuntimeError(f'Refusing to replace existing release archive: {archive}')
    if args.staging_directory:
        args.staging_directory.mkdir(parents=True, exist_ok=False)
    staging = nullcontext(str(args.staging_directory.resolve())) if args.staging_directory else tempfile.TemporaryDirectory(prefix='silver-package-')
    with staging as temp:
        stage = Path(temp) / name
        stage.mkdir()
        def copy(src, relative):
            dest = stage / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            if src.is_dir():
                shutil.copytree(src, dest, symlinks=True)
            else:
                shutil.copy2(src, dest)
            return dest
        if macos:
            products = [('Standalone', 'Silver-K.app', 'Standalone'),
                        ('VST3', 'Silver-K.vst3', 'Plugins/VST3'),
                        ('AU', 'Silver-K.component', 'Plugins/AU')]
        else:
            products = [('Standalone', 'Silver-K.exe', 'Standalone'),
                        ('VST3', 'Silver-K.vst3', 'Plugins/VST3')]
        for source, filename, target in products:
            built = args.products / source / filename
            if macos:
                info = plistlib.loads((built / 'Contents/Info.plist').read_bytes())
                if info.get('CFBundleShortVersionString') != PLUGIN_VERSION:
                    raise RuntimeError(f'Incorrect bundle version: {built}')
                macos_compatibility[filename] = check_bundle(built, architectures)
                subprocess.run(['codesign', '--verify', '--deep', '--strict', str(built)], check=True)
            copy(built, target + '/' + filename)
        builder_name = 'Silver-K Wave ROM Builder' + ('.app' if args.platform.startswith('macOS') else '.exe')
        builder = args.wave_builder or (args.products.parents[2] / 'waveRomBuilder' /
                  'silverWaveRomBuilder_artefacts' / args.products.name / builder_name)
        if not builder.exists():
            raise RuntimeError('Build silverWaveRomBuilder before packaging, or provide --wave-builder')
        if args.platform.startswith('macOS'):
            macos_compatibility[builder_name] = check_bundle(builder, architectures)
            subprocess.run(['codesign', '--verify', '--deep', '--strict', str(builder)], check=True)
        copy(builder, 'Tools/' + builder_name)
        copy(ROOT / 'doc/WAVE_ROM_BUILDER.md', 'Tools/WAVE_ROM_BUILDER.md')
        for folder in ('ROMs', 'Patches', 'Skins'):
            (stage / 'Silver-K' / folder).mkdir(parents=True)
        copy(ROOT / 'assets/skins/Silver Flat', 'Silver-K/Skins/Silver Flat')
        waves = stage / 'Silver-K/ROMs/ks-wave-zero.bin'
        waves.write_bytes(bytes(129024))
        wave_manifest = dict(origin='Silver-K synthetic zero bank', size=129024,
                             sha256=digest(waves), placeholder=True, contains_firmware=False,
                             contains_sampled_waves=False, mapped_base='0x860000',
                             generator='tools/package_release.py')
        (stage / 'Silver-K/ROMs/ks-wave-zero.manifest.json').write_text(json.dumps(wave_manifest, indent=2)+'\n')
        for src, dest in [('doc/INSTALL.md', 'INSTALL.md'),
                          ('doc/USER_GUIDE.md', 'USER_GUIDE.md'),
                          ('doc/SKINS.md', 'SKINS.md'),
                          (f'doc/releases/{VERSION}.md', 'RELEASE_NOTES.md'),
                          ('LICENSE.md', 'LICENSE.md'), ('THIRD_PARTY.md', 'THIRD_PARTY.md')]:
            copy(ROOT / src, dest)
        if args.test_guide:
            copy(args.test_guide, 'TEST_KS_CONTROLLER_RU.md')
        for dep in ['juce', 'dsp', 'asmjit']:
            copy(args.dependencies / f'silver_{dep}-src/LICENSE.md', f'Licenses/{dep}/LICENSE.md')
        for filename in ['LICENSE-BSD.txt', 'LICENSE-LGPL.txt']:
            copy(ROOT / 'source/3rdparty/libresample' / filename, 'Licenses/libresample/' + filename)
        # Include the actual local source, including current uncommitted C++ changes.
        # Original user assets and unrelated editor work are outside these roots.
        source_files = subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines()
        source_archive = stage / 'Silver-K-source.zip'
        roots = ('source/','cmake/','assets/','tools/','doc/','.github/')
        top = {'CMakeLists.txt','README.md','LICENSE.md','THIRD_PARTY.md','.gitignore'}
        public_docs = {'doc/BUILD.md', 'doc/INSTALL.md', 'doc/USER_GUIDE.md',
                       'doc/SKINS.md', 'doc/WAVE_ROM_BUILDER.md',
                       'doc/releases/0.4a.md', f'doc/releases/{VERSION}.md'}
        with zipfile.ZipFile(source_archive,'w',zipfile.ZIP_DEFLATED) as source_zip:
            for relative in sorted(set(source_files)):
                if relative.startswith('doc/') and relative not in public_docs:
                    continue
                path = ROOT / relative
                if (relative.startswith(roots) or relative in top) and path.is_file():
                    if path.suffix.lower() in {'.bin','.syx','.mid','.midi','.os','.fxb','.fxp','.wav','.skwave'} and relative!='assets/ks-init-flash.bin':
                        raise RuntimeError(f'Private/binary asset in source snapshot: {relative}')
                    source_zip.write(path,relative)
        dirty = bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())
        sources = f'''Silver-K corresponding source is included in Silver-K-source.zip.
Snapshot SHA-256: {digest(source_archive)}
Base Git commit: {commit}; local changes included: {dirty}.
Upstream baseline (the bundled snapshot also includes local changes):
https://github.com/temaniak/Silver-K/tree/{commit}
https://github.com/temaniak/Silver-K/archive/{commit}.tar.gz

Build instructions and local compatibility patches are in that source tree.
Required dependency sources (built locally, not prebuilt binaries):
JUCE: https://github.com/dsp56300/JUCE/archive/2a8699d723fe9279e1148275485ddb0e18957f56.tar.gz
DSP56300: https://github.com/dsp56300/dsp56300/archive/44353be2f0156cac970c6ab4f467ea1998b40605.tar.gz
AsmJit: https://github.com/dsp56300/asmjit/archive/3577608cab0bc509f856ebf6e41b2f9d9f71acc4.tar.gz

Bundled framework, H8S, libresample and ptypes source/notices are in Silver-K's source tree.
No original firmware or factory sounds are included in this binary archive.
'''
        (stage / 'SOURCES.txt').write_text(sources, encoding='utf-8')
        manifest = dict(version=VERSION, channel='experimental' if args.variant else 'release', variant=args.variant or None, plugin_version=PLUGIN_VERSION, platform=args.platform,
                        source_commit=commit, source_dirty=dirty, source_snapshot_sha256=digest(source_archive), plugin_identity=plugin_identity, firmware_included=False, factory_presets_included=False,
                        ks_data_required=True, synthetic_ks_data_included=False, synthetic_wave_rom=wave_manifest,
                        wave_rom_builder_included=True, wave_rom_builder_version='0.1.0',
                        bundled_skins=['Silver Flat'], skin_schema_version=1,
                        macos_signing='ad-hoc; not notarized' if args.platform.startswith('macOS') else None)
        if macos:
            manifest['macos_compatibility'] = macos_compatibility
        (stage / 'release-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
        files = sorted(p for p in stage.rglob('*') if p.is_file() and not p.is_symlink())
        checksums = ''.join(f'{digest(p)}  {p.relative_to(stage).as_posix()}\n' for p in files)
        (stage / 'SHA256SUMS.txt').write_text(checksums, encoding='utf-8')
        if args.platform.startswith('macOS'):
            subprocess.run(['ditto', '-c', '-k', '--keepParent', str(stage), str(archive)], check=True)
        else:
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as package:
                for p in sorted(stage.rglob('*')):
                    package.write(p, p.relative_to(stage.parent).as_posix())
        with zipfile.ZipFile(archive) as package:
            if package.testzip() is not None:
                raise RuntimeError('Archive CRC check failed')
            if name+'/Silver-K/ROMs/ks-init-flash.bin' in package.namelist():
                raise RuntimeError('Test Init Flash must not be distributed as KS All Data')
            if package.read(name+'/Silver-K/ROMs/ks-wave-zero.bin') != bytes(129024):
                raise RuntimeError('Packaged wave substitute is not a zero bank')
        checksum = digest(archive)
        archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n', encoding='utf-8')
        print(f'Packaged {archive} ({archive.stat().st_size} bytes), SHA-256 {checksum}')


if __name__ == '__main__':
    main()
