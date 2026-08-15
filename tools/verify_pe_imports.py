#!/usr/bin/env python3
"""Reject non-system DLL dependencies in the native Windows executable."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess


SYSTEM_DLLS = {
    "gdi32.dll",
    "kernel32.dll",
    "ole32.dll",
    "shell32.dll",
    "user32.dll",
    "winmm.dll",
}


def verify(executable: Path, objdump: Path) -> list[str]:
    completed = subprocess.run(
        [str(objdump), "-p", str(executable)],
        check=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    imports = [
        match.group(1).strip()
        for line in completed.stdout.splitlines()
        if (match := re.search(r"DLL Name:\s*(\S+)", line))
    ]
    if not imports:
        raise ValueError("objdump reported no DLL imports")
    forbidden = [
        name
        for name in imports
        if name.lower() not in SYSTEM_DLLS and not name.lower().startswith("api-ms-win-crt-")
    ]
    if forbidden:
        raise ValueError(f"non-system runtime DLL imports: {forbidden!r}")
    return imports


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("objdump", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    imports = verify(args.executable, args.objdump)
    print(f"verified {len(imports)} Windows-system DLL imports: {', '.join(imports)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}")
