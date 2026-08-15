#!/usr/bin/env python3
"""Verify the self-contained converted Gearheads runtime asset tree."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


BANNED_SUFFIXES = {".ar", ".bmp", ".dll", ".exe", ".iso", ".vhd"}
BANNED_NAMES = {"gear_en.exe", "gearage.dll", "gearheads.iso", "gearheads.vhd"}


def verify(root: Path) -> tuple[int, int]:
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        raise ValueError(f"manifest is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    records = manifest.get("files")
    if not isinstance(records, list):
        raise ValueError("manifest files field is not a list")

    expected: set[str] = set()
    total_bytes = 0
    for record in records:
        relative = record.get("path")
        if not isinstance(relative, str) or not relative or relative in expected:
            raise ValueError(f"invalid or duplicate manifest path: {relative!r}")
        path = Path(relative)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"unsafe manifest path: {relative}")
        if path.suffix.lower() in BANNED_SUFFIXES or path.name.lower() in BANNED_NAMES:
            raise ValueError(f"original/runtime-forbidden format is present: {relative}")
        expected.add(path.as_posix())
        full_path = root / path
        if not full_path.is_file():
            raise ValueError(f"manifest file is missing: {relative}")
        payload = full_path.read_bytes()
        if len(payload) != record.get("bytes"):
            raise ValueError(f"size mismatch: {relative}")
        digest = hashlib.sha256(payload).hexdigest()
        if digest != record.get("sha256"):
            raise ValueError(f"SHA-256 mismatch: {relative}")
        total_bytes += len(payload)

    actual = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path != manifest_path
    }
    missing_from_manifest = actual - expected
    missing_from_tree = expected - actual
    if missing_from_manifest:
        raise ValueError(f"unmanifested files: {sorted(missing_from_manifest)!r}")
    if missing_from_tree:
        raise ValueError(f"missing files: {sorted(missing_from_tree)!r}")
    return len(expected), total_bytes


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_root", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    count, total_bytes = verify(args.asset_root)
    print(
        f"verified {count} converted files ({total_bytes} bytes); "
        "no original binary/container formats present"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"error: {error}")
