#!/usr/bin/env python3
"""Verify every recovered audio asset is named by recovered runtime data."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        raise ValueError("expected the asset root")
    assets = Path(sys.argv[1])
    script = (assets / "data" / "script.ini").read_text(encoding="utf-8")
    settings = (assets / "data" / "gearhead.ini").read_text(encoding="utf-8")

    script_wavs = {
        match.group(1).lower() + ".wav"
        for match in re.finditer(r"(?im)^s\d+\s*=\s*@([a-z0-9_]+)\s*$", script)
    }
    sound_section = settings.split("[sound]", 1)[1].split("[", 1)[0]
    alias_wavs = {
        match.group(1).lower() + ".wav"
        for match in re.finditer(r"(?im)^\w+\s*=\s*@([a-z0-9_]+)\s*$", sound_section)
    }
    recovered_wavs = {path.name.lower() for path in (assets / "sounds").glob("*.wav")}
    referenced_wavs = script_wavs | alias_wavs
    if recovered_wavs != referenced_wavs:
        raise ValueError(
            f"WAV reference mismatch: unreferenced={sorted(recovered_wavs - referenced_wavs)!r}, "
            f"missing={sorted(referenced_wavs - recovered_wavs)!r}"
        )

    level_midis: set[str] = set()
    for line in settings.splitlines():
        if not line.lower().startswith("song="):
            continue
        names = line.split("=", 1)[1].split(";")
        if names and names[0].strip().isdigit():
            names = names[1:]
        level_midis.update(
            Path(name.strip().replace("\\", "/")).name.lower()
            for name in names
            if name.strip()
        )
    referenced_midis = level_midis | {"open.mid"}
    recovered_midis = {path.name.lower() for path in (assets / "music").glob("*.mid")}
    if recovered_midis != referenced_midis:
        raise ValueError(
            f"MIDI reference mismatch: unreferenced={sorted(recovered_midis - referenced_midis)!r}, "
            f"missing={sorted(referenced_midis - recovered_midis)!r}"
        )

    print(
        f"validated recovered references for {len(recovered_wavs)} effects and "
        f"{len(recovered_midis)} MIDI tracks"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")
