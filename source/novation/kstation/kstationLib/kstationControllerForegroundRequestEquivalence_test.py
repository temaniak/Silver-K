#!/usr/bin/env python3
"""Run existing H8 differential fixtures with request-payload equivalence checks."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[2]
FIXTURES = ("Runtime", "Pump", "Parser", "Feedback", "Expression", "VoiceRefresh")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--fixture", choices=FIXTURES, action="append")
    args = parser.parse_args()
    rom = args.rom.resolve(strict=True)
    flags = (["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
             if args.sanitize else ["-O2", "-DNDEBUG"])
    with tempfile.TemporaryDirectory(prefix="foreground-request-") as directory:
        directory = Path(directory)
        for fixture in args.fixture or FIXTURES:
            name = "kstationControllerForeground" + fixture + "_test.cpp"
            source = (HERE / name).read_text()
            marker = "auto req=foreground.request();"
            if source.count(marker) != 1:
                raise RuntimeError(f"Unexpected request sites in {name}")
            source = source.replace("int main(",
                '#include "kstationControllerForegroundRequestEquivalence_test.h"\nint main(', 1)
            source = source.replace(marker, marker +
                "request_equivalence_test::check(foreground,req);")
            opening = "int main(int argc,char**argv){"
            if source.count(opening) != 1:
                raise RuntimeError(f"Unexpected main in {name}")
            source = source.replace(opening, opening +
                "{kstation::ControllerForegroundRuntime idle;"
                "request_equivalence_test::check(idle,idle.request());}", 1)
            temp_source = directory / name
            temp_source.write_text(source)
            binary = directory / fixture
            subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", *flags, "-DK_RESEARCH_NATIVE_TRACE",
                "-I" + str(HERE), "-I" + str(SOURCE / "cpu"),
                "-I" + str(SOURCE / "framework"), str(temp_source), "-o", str(binary)], check=True)
            print(f"Running {fixture}", flush=True)
            subprocess.run([str(binary), str(rom)], check=True)


if __name__ == "__main__":
    main()
