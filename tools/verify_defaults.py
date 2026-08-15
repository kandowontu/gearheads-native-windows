#!/usr/bin/env python3
"""Validate the corrected executable-defaults artifact and field alignment."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


TOY_NAMES = [
    "roach", "bomby", "cluck", "zappa", "kanga", "bigal", "destr", "stick",
    "goril", "skull", "magnt", "handy", "small", "roket", "buggy", "block",
    "wall1", "wall2", "wall3", "wall4",
]

FIELD_NAMES = [
    "mass",
    "horizontal_speed",
    "movement_mode",
    "vim_decay",
    "primary_extra",
    "secondary_extra",
    "collision_front_percent",
    "collision_top_percent",
    "collision_back_percent",
    "collision_bottom_percent",
    "handy_attach_x",
    "handy_attach_y",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_runtime(path: Path) -> tuple[dict[str, list[int]], dict[str, str]]:
    numeric: dict[str, list[int]] = {}
    strings: dict[str, str] = {}
    section = ""
    for source_line in path.read_text(encoding="utf-8").splitlines():
        line = source_line.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].casefold()
            continue
        name, separator, value = line.partition("=")
        require(bool(separator), "malformed runtime-defaults.ini line")
        if section == "integers":
            numeric[name.casefold()] = [int(part, 10) for part in value.split()]
        elif section == "strings":
            strings[name.casefold()] = value.replace("\\\\", "\\")
    return numeric, strings


def verify(path: Path, runtime_path: Path | None = None) -> None:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(document.get("format") == 2, "defaults format must be 2")
    table = document["configuration_table"]
    require(table["offset"] == 0x216E, "descriptor table must begin at DS:216e")
    require(table["bytes"] == 51 * 8, "descriptor table must contain 51 entries")
    require(
        table["layout"] == [
            "destination_offset", "name_offset", "count_or_S", "default_offset"
        ],
        "descriptor words are not in their proven order",
    )
    entries = table["entries"]
    require(len(entries) == 51, "expected 51 configuration descriptors")
    by_name = {entry["name"]: entry for entry in entries}
    require(len(by_name) == len(entries), "configuration names must be unique")

    for index, name in enumerate(TOY_NAMES, start=15):
        entry = by_name[name]
        require(entry["value_count"] == 12, f"{name} must have twelve parameters")
        overlap = entry["script_record_overlap"]
        require(overlap is not None, f"{name} must overlap its script-type record")
        require(overlap["record_index"] == index, f"{name} record index is shifted")
        require(overlap["record_name"] == name, f"{name} record name is shifted")
        require(overlap["record_relative_offset"] == 0x0A, f"{name} parameters are misaligned")

    require(by_name["roach"]["value"] == [8, 120, 2, 3, 0, 0, 8, 40, 75, 100, 4, 5],
            "canonical Ziggy parameters changed")
    require(by_name["wall4"]["destination_offset"] == 0x3392,
            "last twelve-word descriptor is rotated")
    require(by_name["GaugeTime"]["value"] == [6000], "GaugeTime changed")
    require(by_name["FrameStep"]["value"] == [55], "FrameStep changed")
    require(by_name["Winningscore"]["value"] == [21], "Winningscore changed")

    layout = document["twelve_word_parameter_layout"]
    require([field["index"] for field in layout] == list(range(12)), "field indexes changed")
    require([field["name"] for field in layout] == FIELD_NAMES, "field names changed")

    if runtime_path is not None:
        numeric, strings = parse_runtime(runtime_path)
        expected_numeric = {
            entry["name"].casefold(): entry["value"]
            for entry in entries
            if entry["value_type"] == "integers"
        }
        expected_strings = {
            entry["name"].casefold(): entry["value"]
            for entry in entries
            if entry["value_type"] == "string"
        }
        require(numeric == expected_numeric, "native numeric defaults differ from defaults.json")
        require(strings == expected_strings, "native string defaults differ from defaults.json")

    suffix = " plus the native runtime projection" if runtime_path is not None else ""
    print(f"validated 51 corrected descriptors and all 12 toy parameter fields{suffix}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("defaults", type=Path)
    parser.add_argument("runtime_defaults", type=Path, nargs="?")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    verify(args.defaults, args.runtime_defaults)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"error: {error}")
