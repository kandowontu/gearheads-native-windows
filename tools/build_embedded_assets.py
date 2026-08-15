#!/usr/bin/env python3
"""Build the deterministic asset pack and Windows resources for Gearheads."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path


MAGIC = b"GHPACK1\0"


def build_pack(asset_root: Path) -> bytes:
    files = sorted(path for path in asset_root.rglob("*") if path.is_file())
    records = bytearray()
    for path in files:
        relative = path.relative_to(asset_root).as_posix()
        encoded_path = relative.encode("utf-8")
        if len(encoded_path) > 0xFFFF:
            raise ValueError(f"asset path is too long: {relative}")
        data = path.read_bytes()
        records += struct.pack("<HQ", len(encoded_path), len(data))
        records += encoded_path
        records += data

    digest = hashlib.sha256(records).digest()
    return MAGIC + struct.pack("<II", 1, len(files)) + digest + records


def rc_path(path: Path) -> str:
    return path.resolve().as_posix().replace('"', '""')


def build_resource(pack: Path, manifest: Path, version: str) -> str:
    parts = [int(part) for part in version.split(".")]
    if len(parts) != 3:
        raise ValueError("version must have three numeric components")
    major, minor, patch = parts
    dotted = f"{major}.{minor}.{patch}.0"
    return f'''#include <windows.h>

101 RCDATA "{rc_path(pack)}"
1 24 "{rc_path(manifest)}"

1 VERSIONINFO
 FILEVERSION {major},{minor},{patch},0
 PRODUCTVERSION {major},{minor},{patch},0
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "Gearheads Native contributors\\0"
            VALUE "FileDescription", "Gearheads - Native Windows Port\\0"
            VALUE "FileVersion", "{dotted}\\0"
            VALUE "InternalName", "Gearheads\\0"
            VALUE "LegalCopyright", "Original game copyright belongs to its respective rights holders\\0"
            VALUE "OriginalFilename", "Gearheads.exe\\0"
            VALUE "ProductName", "Gearheads Native Windows Port\\0"
            VALUE "ProductVersion", "{dotted}\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--resource", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    args.pack.parent.mkdir(parents=True, exist_ok=True)
    args.resource.parent.mkdir(parents=True, exist_ok=True)
    args.pack.write_bytes(build_pack(args.assets))
    args.resource.write_text(
        build_resource(args.pack, args.manifest, args.version), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
