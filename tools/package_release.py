#!/usr/bin/env python3
"""Create a deterministic local Gearheads release package."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path


ZIP_TIME = (2026, 8, 15, 12, 0, 0)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def add_file(archive: zipfile.ZipFile, source: Path, name: str) -> None:
    info = zipfile.ZipInfo(name, ZIP_TIME)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, source.read_bytes(), compresslevel=9)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    project = Path(__file__).resolve().parent.parent
    release_name = f"Gearheads-Native-Windows-v{args.version}"
    staging = args.output / release_name
    archive_path = args.output / f"{release_name}.zip"
    standalone = args.output / "Gearheads.exe"

    args.output.mkdir(parents=True, exist_ok=True)
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir()

    copies = {
        args.exe: staging / "Gearheads.exe",
        project / "packaging/README.txt": staging / "README.txt",
        project / "CREDITS.md": staging / "CREDITS.txt",
        project / "RELEASE_NOTES.md": staging / "RELEASE-NOTES.txt",
        project / "THIRD_PARTY_NOTICES.md": staging / "THIRD-PARTY-NOTICES.txt",
    }
    for source, destination in copies.items():
        shutil.copy2(source, destination)
    shutil.copy2(args.exe, standalone)

    exe_digest = sha256(staging / "Gearheads.exe")
    (staging / "SHA256SUMS.txt").write_text(
        f"{exe_digest}  Gearheads.exe\n", encoding="ascii"
    )

    with zipfile.ZipFile(archive_path, "w") as archive:
        for source in sorted(staging.iterdir(), key=lambda path: path.name.casefold()):
            add_file(archive, source, f"{release_name}/{source.name}")

    generic_archive = args.output / "Gearheads-Native-Windows.zip"
    shutil.copy2(archive_path, generic_archive)
    sums = (
        f"{sha256(standalone)}  Gearheads.exe\n"
        f"{sha256(archive_path)}  {archive_path.name}\n"
        f"{sha256(generic_archive)}  {generic_archive.name}\n"
    )
    (args.output / "SHA256SUMS.txt").write_text(sums, encoding="ascii")
    print(f"Created {archive_path}")
    print(f"Executable SHA-256: {exe_digest}")


if __name__ == "__main__":
    main()
