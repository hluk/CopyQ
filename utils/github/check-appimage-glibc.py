#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Reject AppImage payloads requiring glibc newer than the build baseline."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("appdir", type=Path)
    parser.add_argument("max_glibc", help="Maximum allowed glibc version, e.g. 2.39")
    args = parser.parse_args()
    if not re.fullmatch(r"\d+(?:\.\d+)+", args.max_glibc):
        parser.error("max_glibc must be a dotted version number")
    baseline = tuple(map(int, args.max_glibc.split(".")))
    checked = 0
    failed = False
    seen = set()
    for path in sorted(args.appdir.rglob("*")):
        if not path.is_file() or path.resolve() in seen:
            continue
        seen.add(path.resolve())
        with path.open("rb") as binary:
            if binary.read(4) != b"\x7fELF":
                continue
        checked += 1
        versions = subprocess.check_output(
            ["readelf", "--version-info", "--wide", str(path)],
            text=True,
            env={**os.environ, "LC_ALL": "C"},
        )
        required = {
            tuple(map(int, version.split(".")))
            for version in re.findall(r"Name: GLIBC_(\d+(?:\.\d+)+)\b", versions)
        }
        if required and max(required) > baseline:
            version = ".".join(map(str, max(required)))
            print(f"{path}: requires GLIBC_{version}, exceeds GLIBC_{args.max_glibc}")
            failed = True
    if not checked:
        print(f"No ELF files found in {args.appdir}", file=sys.stderr)
        return 1
    print(f"Checked {checked} ELF files against GLIBC_{args.max_glibc}")
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
