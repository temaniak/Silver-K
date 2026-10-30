#!/usr/bin/env python3
"""Instrument existing H8 differential UI fixtures, including request-bearing states."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[2]
FIXTURES = ("ServicesDirect", "ServicesForeground", "KeyMenu", "KeyDisplayCurrent", "MenuEditDirect")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    rom = args.rom.resolve(strict=True)
    flags = (["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
             if args.sanitize else ["-O2", "-DNDEBUG"])
    with tempfile.TemporaryDirectory(prefix="ui-request-") as temp:
        for fixture in FIXTURES:
            name = "kstationControllerUi" + fixture + "_test.cpp"
            source = (HERE / name).read_text()
            source = '#include "kstationControllerUiServices.h"\n#include "kstationControllerUiRequestEquivalence_test.h"\n' + source
            if fixture == "ServicesForeground":
                needle = "auto u=ui->routeForeground("
                replacement = "ui_request_equivalence_test::check(*ui);auto u=ui->routeForeground("
            else:
                needle = "auto route=runtime.routeDirect("
                replacement = "ui_request_equivalence_test::check(runtime);auto route=runtime.routeDirect("
            if source.count(needle) != 1:
                raise RuntimeError(f"Unexpected route sites in {name}")
            source = source.replace(needle, replacement)
            # Sample the state after routing too, when an external request may have appeared.
            if fixture == "ServicesForeground":
                needle = "if(u==kstation::ControllerUiServices::Route::External)"
                replacement = "ui_request_equivalence_test::check(*ui);" + needle
            else:
                needle = "if(route==kstation::ControllerUiServices::Route::External)"
                if needle not in source:
                    needle = "if(route!=kstation::ControllerUiServices::Route::Ready)"
                replacement = "ui_request_equivalence_test::check(runtime);" + needle
            if source.count(needle) != 1:
                raise RuntimeError(f"Unexpected route result in {name}")
            source = source.replace(needle, replacement)
            file = Path(temp) / name
            file.write_text(source)
            binary = Path(temp) / fixture
            subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", *flags,
                "-DK_RESEARCH_NATIVE_TRACE", "-I"+str(HERE), "-I"+str(SOURCE / "cpu"),
                "-I"+str(SOURCE / "framework"), str(file), "-o", str(binary)], check=True)
            print(f"Running {fixture}", flush=True)
            subprocess.run([str(binary), str(rom)], check=True)

if __name__ == "__main__":
    main()
