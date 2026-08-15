#!/usr/bin/env python3
"""Recover the executable's embedded Gearheads configuration descriptor table.

The Win16 executable stores defaults as text plus an array of eight-byte
descriptors: destination offset, name pointer, value count/type, and default
pointer.  This tool turns that compiler data into a portable JSON artifact.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

import ne_analyze


RECORD_STRIDE = 0x7A
RECORD_COUNT = 36


def render_runtime_defaults(result: dict) -> str:
    """Render every recovered descriptor in a tiny, dependency-free format.

    The native executable deliberately does not carry a JSON library.  This
    companion representation remains generated from the same authoritative
    recovery result as defaults.json and is simple enough for a strict native
    parser.  Numeric descriptors retain their complete vectors; strings are
    emitted separately and escaped only where the line format requires it.
    """
    numeric: list[tuple[str, list[int]]] = []
    strings: list[tuple[str, str]] = []
    entries = result["configuration_table"]["entries"]
    for entry in entries:
        if entry["value_type"] == "integers":
            numeric.append((entry["name"], entry["value"]))
        elif entry["value_type"] == "string":
            value = entry["value"]
            if any(character in value for character in "\r\n"):
                raise ValueError(f"{entry['name']} cannot be represented in runtime defaults")
            strings.append((entry["name"], value.replace("\\", "\\\\")))
        else:
            raise ValueError(f"unsupported recovered value type: {entry['value_type']}")

    lines = [
        "; Generated from the recovered GEAR_EN.EXE configuration table.",
        "; This file is runtime data, not a copy of the original executable.",
        "format=1",
        f"source_executable_sha256={result['source_executable_sha256']}",
        "",
        "[integers]",
    ]
    lines.extend(f"{name}={' '.join(str(value) for value in values)}" for name, values in numeric)
    lines.extend(["", "[strings]"])
    lines.extend(f"{name}={value}" for name, value in strings)
    return "\n".join(lines) + "\n"


def c_string(data: bytes, offset: int) -> str:
    if not 0 <= offset < len(data):
        raise ValueError(f"string offset 0x{offset:04x} is outside the data segment")
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"string at 0x{offset:04x} is not terminated")
    return data[offset:end].decode("cp1252")


def is_descriptor(data: bytes, offset: int) -> bool:
    if offset < 0 or offset + 8 > len(data):
        return False
    destination, name_offset, count, default_offset = struct.unpack_from("<HHHH", data, offset)
    if count not in {*range(1, 13), ord("S")}:
        return False
    if not 0 <= destination < len(data):
        return False
    try:
        name = c_string(data, name_offset)
        default = c_string(data, default_offset)
    except (UnicodeDecodeError, ValueError):
        return False
    return bool(name and default and all(character.isprintable() for character in name + default))


def locate_data(executable: Path) -> tuple[int, bytes]:
    payload = executable.read_bytes()
    header = ne_analyze.read_header(payload)
    segments = ne_analyze.read_segments(payload, header)
    number = header.automatic_data_segment
    if not 1 <= number <= len(segments):
        raise ValueError("NE automatic data segment number is invalid")
    segment = segments[number - 1]
    if not segment.is_data:
        raise ValueError("NE automatic data segment is not marked as data")
    return number, payload[segment.file_offset : segment.file_offset + segment.data_length]


def recover(executable: Path) -> dict:
    segment_number, data = locate_data(executable)
    roach_name = data.find(b"roach\0")
    roach_default = data.find(b"8 120 2 3 0 0 8 40 75 100 4 5\0")
    if roach_name < 0 or roach_default < 0:
        raise ValueError("could not locate the canonical roach defaults")
    # The searchable three-word signature starts at byte two of the entry,
    # immediately after its destination word.
    signature = struct.pack("<HHH", roach_name, 12, roach_default)
    table_roach_name = data.find(signature)
    if table_roach_name < 2:
        raise ValueError("could not locate the configuration descriptor table")

    table_start = table_roach_name - 2
    while is_descriptor(data, table_start - 8):
        table_start -= 8

    record_base = data.find(b"\0\0art\0")
    if record_base < 0:
        raise ValueError("could not locate the 36-entry script-type record array")
    records = []
    for index in range(RECORD_COUNT):
        offset = record_base + index * RECORD_STRIDE
        if offset + 10 > len(data):
            raise ValueError("script-type record array is truncated")
        record_id = struct.unpack_from("<H", data, offset)[0]
        records.append(
            {
                "index": index,
                "offset": offset,
                "id": record_id,
                "name": c_string(data, offset + 2),
            }
        )

    descriptors = []
    cursor = table_start
    while is_descriptor(data, cursor):
        destination, name_offset, count, default_offset = struct.unpack_from(
            "<HHHH", data, cursor
        )
        name = c_string(data, name_offset)
        raw_default = c_string(data, default_offset)
        value: str | list[int]
        if count == ord("S"):
            value = raw_default
            value_type = "string"
        else:
            try:
                value = [int(part, 10) for part in raw_default.split()]
            except ValueError as error:
                raise ValueError(f"{name} has a non-integer default") from error
            if len(value) != count:
                raise ValueError(f"{name} declares {count} values but contains {len(value)}")
            value_type = "integers"

        overlap = None
        if record_base <= destination < record_base + RECORD_COUNT * RECORD_STRIDE:
            record_index = (destination - record_base) // RECORD_STRIDE
            overlap = {
                "record_index": record_index,
                "record_name": records[record_index]["name"],
                "record_relative_offset": (destination - record_base) % RECORD_STRIDE,
            }
        if count == 12:
            group = "toy_and_obstacle_parameters"
        elif count == 6:
            group = "board_object_parameters"
        else:
            group = "global_settings"
        descriptors.append(
            {
                "name": name,
                "group": group,
                "descriptor_offset": cursor,
                "name_offset": name_offset,
                "value_type": value_type,
                "value_count": count if count != ord("S") else None,
                "default_offset": default_offset,
                "raw_default": raw_default,
                "value": value,
                "destination_offset": destination,
                "script_record_overlap": overlap,
            }
        )
        cursor += 8

    expected_names = {"LeftBoxs", "RightBoxs", "roach", "wall4", "SuddenDeath", "Anim", "LeftShiftCtrl"}
    actual_names = {descriptor["name"] for descriptor in descriptors}
    if not expected_names <= actual_names or len(descriptors) != 51:
        raise ValueError(
            f"configuration table validation failed: recovered {len(descriptors)} descriptors"
        )

    return {
        "format": 2,
        "source_executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "data_segment": segment_number,
        "configuration_table": {
            "offset": table_start,
            "bytes": cursor - table_start,
            "descriptor_size": 8,
            "layout": ["destination_offset", "name_offset", "count_or_S", "default_offset"],
            "entries": descriptors,
        },
        "twelve_word_parameter_layout": [
            {"index": 0, "name": "mass", "evidence": "segment 6:3147 and segment 10 object logic"},
            {"index": 1, "name": "horizontal_speed", "evidence": "segment 10:18fb-1911"},
            {"index": 2, "name": "movement_mode", "evidence": "segment 10:1921-197d"},
            {
                "index": 3,
                "name": "vim_decay",
                "evidence": "GEARCON labels Vim; segment 10:2668-2673 subtracts it from winding",
            },
            {
                "index": 4,
                "name": "primary_extra",
                "evidence": "GEARCON labels Extra and toy-specific controls; segment 10 special callbacks",
            },
            {
                "index": 5,
                "name": "secondary_extra",
                "evidence": "GEARCON toy-specific controls; segment 10 special callbacks",
            },
            {
                "index": 6,
                "name": "collision_front_percent",
                "evidence": "RECTCON Collision Box Size dialog, control Front",
            },
            {
                "index": 7,
                "name": "collision_top_percent",
                "evidence": "RECTCON Collision Box Size dialog, control Top",
            },
            {
                "index": 8,
                "name": "collision_back_percent",
                "evidence": "RECTCON Collision Box Size dialog, control Back",
            },
            {
                "index": 9,
                "name": "collision_bottom_percent",
                "evidence": "RECTCON Collision Box Size dialog, control Bottom",
            },
            {
                "index": 10,
                "name": "handy_attach_x",
                "evidence": "RECTCON X Handy Y controls and segment 10:0ec6-0f53",
            },
            {
                "index": 11,
                "name": "handy_attach_y",
                "evidence": "RECTCON X Handy Y controls and segment 10:0ec6-0f53",
            },
        ],
        "script_type_records": {
            "offset": record_base,
            "stride": RECORD_STRIDE,
            "count": RECORD_COUNT,
            "records": records,
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    result = recover(args.executable)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as error:
        raise SystemExit(f"error: {error}")
