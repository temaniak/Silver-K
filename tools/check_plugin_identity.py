#!/usr/bin/env python3
"""Check exported plugin metadata without creating a synth or loading user assets."""
import ctypes
import plistlib
import sys
import uuid
from pathlib import Path

MANUFACTURER = 'NK Stuff'
BUNDLE_ID = 'com.theusualsuspects.silverk'
VST3_CLASSES = {
    'Audio Module Class': 'ABCDEF019182FAEB5475735054736B76',
    'Component Controller Class': 'ABCDEF011234ABCD5475735054736B76',
    'Plugin Compatibility Class': 'ABCDEF01C0DEF00D5475735054736B76',
}


class FactoryInfo(ctypes.Structure):
    _fields_ = [('vendor', ctypes.c_char * 64), ('url', ctypes.c_char * 256),
                ('email', ctypes.c_char * 128), ('flags', ctypes.c_int32)]


class ClassInfo(ctypes.Structure):
    _fields_ = [('cid', ctypes.c_ubyte * 16), ('cardinality', ctypes.c_int32),
                ('category', ctypes.c_char * 32), ('name', ctypes.c_char * 64)]


def check_identity(products):
    products = Path(products)
    windows = sys.platform == 'win32'
    binary = products / 'VST3/Silver-K.vst3/Contents' / (
        'x86_64-win/Silver-K.vst3' if windows else 'MacOS/Silver-K')
    library = (ctypes.WinDLL if windows else ctypes.CDLL)(str(binary.resolve()))
    call = ctypes.WINFUNCTYPE if windows else ctypes.CFUNCTYPE
    library.GetPluginFactory.restype = ctypes.c_void_p
    factory = library.GetPluginFactory()
    if not factory:
        raise RuntimeError('VST3 factory is null')
    vtable = ctypes.cast(factory, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    try:
        info = FactoryInfo()
        if call(ctypes.c_int32, ctypes.c_void_p, ctypes.POINTER(FactoryInfo))(vtable[3])(factory, ctypes.byref(info)) != 0:
            raise RuntimeError('Cannot read VST3 factory metadata')
        vendor = info.vendor.decode()
        if vendor != MANUFACTURER:
            raise RuntimeError(f'Unexpected VST3 vendor: {vendor}')
        count = call(ctypes.c_int32, ctypes.c_void_p)(vtable[4])(factory)
        classes = {}
        for index in range(count):
            entry = ClassInfo()
            if call(ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ClassInfo))(vtable[5])(factory, index, ctypes.byref(entry)) != 0:
                raise RuntimeError('Cannot read VST3 class metadata')
            raw = bytes(entry.cid)
            cid = uuid.UUID(bytes_le=raw).hex.upper() if windows else raw.hex().upper()
            category = entry.category.decode()
            if entry.name.decode() != 'Silver-K' or category in classes:
                raise RuntimeError('Unexpected VST3 class name or duplicate category')
            classes[category] = cid
        if classes != VST3_CLASSES:
            raise RuntimeError(f'VST3 class IDs changed: {classes}')
    finally:
        call(ctypes.c_uint32, ctypes.c_void_p)(vtable[2])(factory)
    identity = dict(manufacturer=vendor, vst3_classes=classes)
    if not windows:
        for relative in ('AU/Silver-K.component', 'VST3/Silver-K.vst3', 'Standalone/Silver-K.app'):
            info = plistlib.loads((products / relative / 'Contents/Info.plist').read_bytes())
            if info.get('CFBundleIdentifier') != BUNDLE_ID:
                raise RuntimeError(f'Bundle ID changed: {relative}')
            if relative.startswith('AU/'):
                component, = info['AudioComponents']
                expected = dict(name=MANUFACTURER + ': Silver-K', manufacturer='TusP', subtype='Tskv', type='aumu')
                if any(component.get(key) != value for key, value in expected.items()):
                    raise RuntimeError(f'AU identity changed: {component}')
                identity['au'] = expected
        identity['bundle_id'] = BUNDLE_ID
    return identity


if __name__ == '__main__':
    import argparse
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('products', type=Path)
    print(json.dumps(check_identity(parser.parse_args().products), indent=2))
