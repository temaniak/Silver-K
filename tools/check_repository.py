#!/usr/bin/env python3
"""Audit tracked source paths and file signatures before publication."""
from pathlib import Path
import hashlib, subprocess, sys
root=Path(__file__).resolve().parents[1]
files=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
errors=[]
for name in filter(None,files):
    p=Path(name);data=(root/p).read_bytes()
    if (p.name=='AGENTS.md' or name.startswith(('.codex/','.agents/','private/','reports/','design-stand/qa/'))
        or name in {'design-stand/design-qa.md','doc/KS_CONTROLLER_TEST_WINDOWS_RU.md',
                    'doc/KS_NATIVE_CONTROLLER.md','doc/OPTIMIZATION_LOG.md','doc/PERFORMANCE_DIAGNOSTICS.md',
                    'doc/STABLE_BUILD_20261016_RU.md','doc/STABLE_PERFORMANCE_20261016_RU.md',
                    'doc/WINDOWS_STABLE_HANDOFF_20261016_RU.md','doc/silver_k.md'}
        or (name.startswith('design-stand/public/assets/') and p.name.endswith('-notes.md'))
        or name.startswith('doc/releases/0.2')):
        errors.append(name+': internal report or AI working context must remain local')
    if name=='assets/ks-init-flash.bin':
        if len(data)!=524288 or any(v!=255 for v in data[:0x1fc00]) or any(v!=255 for v in data[0x5e800:]):
            errors.append(name+': unexpected flash size or data in reserved/firmware regions')
        continue
    if p.suffix.lower() in {'.wav','.aif','.aiff','.mp3','.flac','.bin','.rom','.os','.syx','.mid','.midi','.fxb','.fxp','.state','.skwave','.zip','.gz','.7z','.dmg','.pkg','.o','.obj','.a','.lib','.so','.dylib','.dll','.exe'}:
        errors.append(name+': forbidden artifact/data extension')
    if any(s.lower() in {'output','artifacts','temp','roms','_deps','cmakefiles'} or s.lower()=='build' or s.lower().startswith(('build-','cmake-build')) or s.lower().endswith(('.app','.vst3','.component','.dsym')) for s in p.parts[:-1]):
        errors.append(name+': forbidden artifact directory')
    if data.startswith((b'RIFF',b'RF64',b'FORM',b'\x7fELF',b'MZ',b'!<arch>\n',b'PK\x03\x04',b'\xcf\xfa\xed\xfe',b'\xfe\xed\xfa\xcf',b'\xca\xfe\xba\xbe')):
        errors.append(name+': binary/audio/archive signature')
    if p.suffix.lower()=='.png':
        if not data.startswith(b'\x89PNG\r\n\x1a\n'):errors.append(name+': invalid PNG asset')
    else:
        try:data.decode('utf-8')
        except UnicodeDecodeError:errors.append(name+': unexpected binary file')
if errors:
    print('\n'.join(errors));sys.exit(1)
print(f'Audited {len(list(filter(None,files)))} tracked files: no builds, audio or ROM; one synthetic flash asset.')
print('Init flash SHA-256:',hashlib.sha256((root/'assets/ks-init-flash.bin').read_bytes()).hexdigest())
