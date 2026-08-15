#!/usr/bin/env python3
"""Inspect, extract, and linearly disassemble 16-bit Windows NE binaries."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import BinaryIO


RESOURCE_TYPE_NAMES = {
    1: "CURSOR",
    2: "BITMAP",
    3: "ICON",
    4: "MENU",
    5: "DIALOG",
    6: "STRING",
    7: "FONTDIR",
    8: "FONT",
    9: "ACCELERATOR",
    10: "RCDATA",
    11: "MESSAGETABLE",
    12: "GROUP_CURSOR",
    14: "GROUP_ICON",
    16: "VERSION",
}

RELOCATION_SOURCE_NAMES = {
    0x00: "low-byte",
    0x02: "selector16",
    0x03: "pointer16:16",
    0x05: "offset16",
    0x0B: "pointer16:32",
    0x0D: "offset32",
}


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def pascal_string(data: bytes, offset: int) -> str:
    if not 0 <= offset < len(data):
        return f"<invalid-string-offset-0x{offset:x}>"
    size = data[offset]
    end = offset + 1 + size
    if end > len(data):
        return f"<truncated-string-offset-0x{offset:x}>"
    return data[offset + 1 : end].decode("cp1252", errors="replace")


@dataclasses.dataclass(frozen=True)
class NeHeader:
    offset: int
    linker_version: int
    linker_revision: int
    entry_table_offset: int
    entry_table_size: int
    crc: int
    flags: int
    automatic_data_segment: int
    initial_heap: int
    initial_stack: int
    initial_ip: int
    initial_cs: int
    initial_sp: int
    initial_ss: int
    segment_count: int
    module_reference_count: int
    nonresident_name_size: int
    segment_table_offset: int
    resource_table_offset: int
    resident_name_table_offset: int
    module_reference_table_offset: int
    imported_name_table_offset: int
    nonresident_name_table_offset: int
    movable_entry_count: int
    alignment_shift: int
    resource_segment_count: int
    target_os: int
    other_flags: int
    expected_windows_version: int


@dataclasses.dataclass(frozen=True)
class Segment:
    number: int
    file_offset: int
    data_length: int
    flags: int
    minimum_allocation: int

    @property
    def is_data(self) -> bool:
        return bool(self.flags & 0x0001)

    @property
    def has_relocations(self) -> bool:
        return bool(self.flags & 0x0100)


@dataclasses.dataclass(frozen=True)
class EntryPoint:
    ordinal: int
    flags: int
    segment: int | None
    offset: int | None
    movable: bool


@dataclasses.dataclass(frozen=True)
class Relocation:
    source_type: int
    flags: int
    source_offset: int
    target_1: int
    target_2: int
    description: str


@dataclasses.dataclass(frozen=True)
class Resource:
    type_name: str
    type_id: int | None
    name: str
    numeric_id: int | None
    file_offset: int
    length: int
    flags: int


def read_header(data: bytes) -> NeHeader:
    if data[:2] != b"MZ" or len(data) < 0x40:
        raise ValueError("file does not contain an MZ header")
    ne_offset = u32(data, 0x3C)
    if ne_offset + 0x40 > len(data) or data[ne_offset : ne_offset + 2] != b"NE":
        raise ValueError("file does not contain a 16-bit Windows NE header")
    return NeHeader(
        offset=ne_offset,
        linker_version=data[ne_offset + 2],
        linker_revision=data[ne_offset + 3],
        entry_table_offset=u16(data, ne_offset + 0x04),
        entry_table_size=u16(data, ne_offset + 0x06),
        crc=u32(data, ne_offset + 0x08),
        flags=u16(data, ne_offset + 0x0C),
        automatic_data_segment=u16(data, ne_offset + 0x0E),
        initial_heap=u16(data, ne_offset + 0x10),
        initial_stack=u16(data, ne_offset + 0x12),
        initial_ip=u16(data, ne_offset + 0x14),
        initial_cs=u16(data, ne_offset + 0x16),
        initial_sp=u16(data, ne_offset + 0x18),
        initial_ss=u16(data, ne_offset + 0x1A),
        segment_count=u16(data, ne_offset + 0x1C),
        module_reference_count=u16(data, ne_offset + 0x1E),
        nonresident_name_size=u16(data, ne_offset + 0x20),
        segment_table_offset=u16(data, ne_offset + 0x22),
        resource_table_offset=u16(data, ne_offset + 0x24),
        resident_name_table_offset=u16(data, ne_offset + 0x26),
        module_reference_table_offset=u16(data, ne_offset + 0x28),
        imported_name_table_offset=u16(data, ne_offset + 0x2A),
        nonresident_name_table_offset=u32(data, ne_offset + 0x2C),
        movable_entry_count=u16(data, ne_offset + 0x30),
        alignment_shift=u16(data, ne_offset + 0x32),
        resource_segment_count=u16(data, ne_offset + 0x34),
        target_os=data[ne_offset + 0x36],
        other_flags=data[ne_offset + 0x37],
        expected_windows_version=u16(data, ne_offset + 0x3E),
    )


def read_segments(data: bytes, header: NeHeader) -> list[Segment]:
    result: list[Segment] = []
    table = header.offset + header.segment_table_offset
    for index in range(header.segment_count):
        offset = table + index * 8
        if offset + 8 > len(data):
            raise ValueError("truncated segment table")
        sector, raw_length, flags, raw_minimum = struct.unpack_from("<HHHH", data, offset)
        result.append(
            Segment(
                number=index + 1,
                file_offset=sector << header.alignment_shift,
                data_length=raw_length or 0x10000,
                flags=flags,
                minimum_allocation=raw_minimum or 0x10000,
            )
        )
    return result


def read_name_table(data: bytes, start: int, limit: int | None = None) -> dict[int, str]:
    names: dict[int, str] = {}
    cursor = start
    end = len(data) if limit is None else min(len(data), start + limit)
    while cursor < end:
        size = data[cursor]
        cursor += 1
        if size == 0:
            break
        if cursor + size + 2 > end:
            raise ValueError("truncated NE name table")
        name = data[cursor : cursor + size].decode("cp1252", errors="replace")
        cursor += size
        ordinal = u16(data, cursor)
        cursor += 2
        names[ordinal] = name
    return names


def read_module_references(data: bytes, header: NeHeader) -> list[str]:
    table = header.offset + header.module_reference_table_offset
    import_base = header.offset + header.imported_name_table_offset
    modules: list[str] = []
    for index in range(header.module_reference_count):
        offset = table + index * 2
        if offset + 2 > len(data):
            raise ValueError("truncated module-reference table")
        modules.append(pascal_string(data, import_base + u16(data, offset)))
    return modules


def read_entries(data: bytes, header: NeHeader) -> list[EntryPoint]:
    entries: list[EntryPoint] = []
    cursor = header.offset + header.entry_table_offset
    end = cursor + header.entry_table_size
    ordinal = 1
    while cursor < end:
        count = data[cursor]
        cursor += 1
        if count == 0:
            break
        if cursor >= end:
            raise ValueError("truncated entry-table bundle")
        segment_indicator = data[cursor]
        cursor += 1
        if segment_indicator == 0:
            for _ in range(count):
                entries.append(EntryPoint(ordinal, 0, None, None, False))
                ordinal += 1
            continue
        if segment_indicator == 0xFF:
            for _ in range(count):
                if cursor + 6 > end:
                    raise ValueError("truncated movable entry")
                flags = data[cursor]
                interrupt = data[cursor + 1 : cursor + 3]
                segment = data[cursor + 3]
                offset = u16(data, cursor + 4)
                if interrupt not in {b"\xcd\x3f", b"\x3f\xcd"}:
                    # Some linkers do not emit the conventional INT 3F marker.
                    pass
                entries.append(EntryPoint(ordinal, flags, segment, offset, True))
                cursor += 6
                ordinal += 1
        else:
            for _ in range(count):
                if cursor + 3 > end:
                    raise ValueError("truncated fixed entry")
                flags = data[cursor]
                offset = u16(data, cursor + 1)
                entries.append(EntryPoint(ordinal, flags, segment_indicator, offset, False))
                cursor += 3
                ordinal += 1
    return entries


def resource_identifier(data: bytes, table_start: int, raw: int) -> tuple[str, int | None]:
    if raw & 0x8000:
        numeric = raw & 0x7FFF
        return str(numeric), numeric
    return pascal_string(data, table_start + raw), None


def read_resources(data: bytes, header: NeHeader) -> list[Resource]:
    table_start = header.offset + header.resource_table_offset
    if table_start + 2 > len(data):
        raise ValueError("truncated resource table")
    alignment_shift = u16(data, table_start)
    cursor = table_start + 2
    resources: list[Resource] = []
    while True:
        if cursor + 2 > len(data):
            raise ValueError("unterminated resource table")
        raw_type = u16(data, cursor)
        cursor += 2
        if raw_type == 0:
            break
        if cursor + 6 > len(data):
            raise ValueError("truncated resource-type record")
        count = u16(data, cursor)
        cursor += 6  # count plus reserved dword
        type_label, type_id = resource_identifier(data, table_start, raw_type)
        if type_id is not None:
            type_label = RESOURCE_TYPE_NAMES.get(type_id, f"TYPE_{type_id}")
        for _ in range(count):
            if cursor + 12 > len(data):
                raise ValueError("truncated resource-name record")
            raw_offset, raw_length, flags, raw_id, _handle, _usage = struct.unpack_from(
                "<HHHHHH", data, cursor
            )
            cursor += 12
            name, numeric_id = resource_identifier(data, table_start, raw_id)
            resources.append(
                Resource(
                    type_name=type_label,
                    type_id=type_id,
                    name=name,
                    numeric_id=numeric_id,
                    file_offset=raw_offset << alignment_shift,
                    length=raw_length << alignment_shift,
                    flags=flags,
                )
            )
    return resources


def imported_name(data: bytes, header: NeHeader, offset: int) -> str:
    return pascal_string(data, header.offset + header.imported_name_table_offset + offset)


def read_relocations(
    data: bytes, header: NeHeader, segment: Segment, modules: list[str]
) -> list[Relocation]:
    if not segment.has_relocations:
        return []
    cursor = segment.file_offset + segment.data_length
    if cursor + 2 > len(data):
        raise ValueError(f"segment {segment.number} has a truncated relocation table")
    count = u16(data, cursor)
    cursor += 2
    result: list[Relocation] = []
    for _ in range(count):
        if cursor + 8 > len(data):
            raise ValueError(f"segment {segment.number} has truncated relocation records")
        source_type, flags, source_offset, target_1, target_2 = struct.unpack_from(
            "<BBHHH", data, cursor
        )
        cursor += 8
        target_type = flags & 0x03
        if target_type == 0:
            if target_1 == 0x00FF:
                description = f"internal ordinal {target_2}"
            else:
                description = f"internal {target_1:04x}:{target_2:04x}"
        elif target_type in {1, 2}:
            module = modules[target_1 - 1] if 1 <= target_1 <= len(modules) else f"module#{target_1}"
            if target_type == 1:
                description = f"import {module}.ordinal_{target_2}"
            else:
                description = f"import {module}.{imported_name(data, header, target_2)}"
        else:
            description = f"OS fixup {target_1:04x}:{target_2:04x}"
        if flags & 0x04:
            description += " additive"
        source_offsets = [source_offset]
        if not flags & 0x04:
            # Non-additive NE fixups are threaded: the word at each source
            # location contains the next source location, ending in 0xffff.
            # Expanding the chain is essential because one relocation-table
            # record can describe dozens of call sites.
            seen = {source_offset}
            current = source_offset
            while current != 0xFFFF:
                link_offset = segment.file_offset + current
                if link_offset + 2 > segment.file_offset + segment.data_length:
                    raise ValueError(
                        f"segment {segment.number} relocation chain leaves the segment at 0x{current:04x}"
                    )
                following = u16(data, link_offset)
                if following == 0xFFFF:
                    break
                if following in seen:
                    raise ValueError(
                        f"segment {segment.number} relocation chain loops at 0x{following:04x}"
                    )
                seen.add(following)
                source_offsets.append(following)
                current = following
        for expanded_offset in source_offsets:
            result.append(
                Relocation(
                    source_type=source_type,
                    flags=flags,
                    source_offset=expanded_offset,
                    target_1=target_1,
                    target_2=target_2,
                    description=description,
                )
            )
    return result


def binary_summary(path: Path, data: bytes) -> tuple[dict, list[Segment], list[EntryPoint], list[Resource]]:
    header = read_header(data)
    segments = read_segments(data, header)
    modules = read_module_references(data, header)
    entries = read_entries(data, header)
    resident_names = read_name_table(data, header.offset + header.resident_name_table_offset)
    nonresident_names = read_name_table(
        data, header.nonresident_name_table_offset, header.nonresident_name_size
    )
    names = dict(nonresident_names)
    names.update(resident_names)
    resources = read_resources(data, header)
    segment_json = []
    relocations_by_segment: dict[int, list[Relocation]] = {}
    for segment in segments:
        relocations = read_relocations(data, header, segment, modules)
        relocations_by_segment[segment.number] = relocations
        segment_json.append(
            {
                "number": segment.number,
                "kind": "data" if segment.is_data else "code",
                "file_offset": segment.file_offset,
                "data_length": segment.data_length,
                "minimum_allocation": segment.minimum_allocation,
                "flags": f"0x{segment.flags:04x}",
                "relocation_count": len(relocations),
            }
        )
    entry_json = [
        {
            "ordinal": entry.ordinal,
            "name": names.get(entry.ordinal),
            "flags": f"0x{entry.flags:02x}",
            "segment": entry.segment,
            "offset": entry.offset,
            "movable": entry.movable,
        }
        for entry in entries
        if entry.segment is not None or entry.ordinal in names
    ]
    resource_json = [dataclasses.asdict(resource) for resource in resources]
    summary = {
        "file": str(path),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "header": {
            **dataclasses.asdict(header),
            "flags": f"0x{header.flags:04x}",
            "other_flags": f"0x{header.other_flags:02x}",
            "initial_cs_ip": f"{header.initial_cs:04x}:{header.initial_ip:04x}",
            "initial_ss_sp": f"{header.initial_ss:04x}:{header.initial_sp:04x}",
        },
        "modules": modules,
        "names": {str(ordinal): name for ordinal, name in sorted(names.items())},
        "entries": entry_json,
        "segments": segment_json,
        "resources": resource_json,
    }
    summary["_relocations"] = relocations_by_segment
    return summary, segments, entries, resources


def printable_summary(summary: dict) -> dict:
    result = dict(summary)
    result.pop("_relocations", None)
    return result


def resource_filename(resource: Resource, index: int) -> str:
    safe_type = "".join(c if c.isalnum() or c in "-_" else "_" for c in resource.type_name)
    safe_name = "".join(c if c.isalnum() or c in "-_" else "_" for c in resource.name)
    return f"{index:04d}_{safe_type}_{safe_name}.bin"


def extract_resources(data: bytes, resources: list[Resource], destination: Path) -> list[dict]:
    destination.mkdir(parents=True, exist_ok=True)
    manifest: list[dict] = []
    for index, resource in enumerate(resources):
        end = resource.file_offset + resource.length
        if end > len(data):
            raise ValueError(f"resource {index} extends beyond end of file")
        payload = data[resource.file_offset:end]
        filename = resource_filename(resource, index)
        (destination / filename).write_bytes(payload)
        manifest.append(
            {
                **dataclasses.asdict(resource),
                "file": filename,
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        )
    (destination / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest


def write_segment_outputs(
    data: bytes,
    summary: dict,
    segments: list[Segment],
    entries: list[EntryPoint],
    destination: Path,
) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    names = {int(ordinal): name for ordinal, name in summary["names"].items()}
    labels_by_location: dict[tuple[int, int], list[str]] = {}
    for entry in entries:
        if entry.segment is None or entry.offset is None:
            continue
        default_name = f"ordinal_{entry.ordinal:04d}"
        labels_by_location.setdefault((entry.segment, entry.offset), []).append(
            names.get(entry.ordinal, default_name)
        )
    labels_by_location.setdefault(
        (summary["header"]["initial_cs"], summary["header"]["initial_ip"]), []
    ).append("program_entry")

    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, CS_MODE_32, Cs
    except ImportError as error:
        raise ValueError("Capstone is required for disassembly: pip install capstone") from error

    disassemblers = {
        16: Cs(CS_ARCH_X86, CS_MODE_16),
        32: Cs(CS_ARCH_X86, CS_MODE_32),
    }
    for disassembler in disassemblers.values():
        disassembler.skipdata = True
    relocations_by_segment: dict[int, list[Relocation]] = summary["_relocations"]
    for segment in segments:
        payload = data[segment.file_offset : segment.file_offset + segment.data_length]
        stem = f"segment_{segment.number:03d}_{'data' if segment.is_data else 'code'}"
        (destination / f"{stem}.bin").write_bytes(payload)
        if segment.is_data:
            continue
        relocation_map: dict[int, list[Relocation]] = {}
        for relocation in relocations_by_segment[segment.number]:
            relocation_map.setdefault(relocation.source_offset, []).append(relocation)
        lines = [
            f"; NE segment {segment.number}",
            f"; file offset: 0x{segment.file_offset:x}",
            f"; length: 0x{segment.data_length:x}",
            f"; flags: 0x{segment.flags:04x}",
            "",
        ]

        # Gearheads contains a hand-written mixed-mode segment.  Each routine
        # starts with a 16-bit DPMI thunk that sets the descriptor's D bit and
        # then continues with a 32-bit body.  Treating the entire NE as 16-bit
        # loses instruction boundaries in the sprite blitters, so identify
        # these compiler-generated thunks and disassemble each region in its
        # actual mode.
        thunk_signature = bytes.fromhex(
            "57 83 ec 08 8c cb 8c d0 8e c0 8b fc b8 0b 00 cd 31"
        )
        body_signature = bytes.fromhex("66 55 0f b7 ec")
        thunk_starts: list[int] = []
        search_at = 0
        while True:
            found = payload.find(thunk_signature, search_at)
            if found < 0:
                break
            thunk_starts.append(found)
            search_at = found + 1
        regions: list[tuple[int, int, int, str]] = []
        if thunk_starts:
            if thunk_starts[0] > 0:
                regions.append((0, thunk_starts[0], 16, "16-bit code"))
            for index, thunk_start in enumerate(thunk_starts):
                next_thunk = (
                    thunk_starts[index + 1]
                    if index + 1 < len(thunk_starts)
                    else len(payload)
                )
                body_start = payload.find(
                    body_signature, thunk_start, min(next_thunk, thunk_start + 0x80)
                )
                if body_start < 0:
                    regions.append((thunk_start, next_thunk, 16, "16-bit code"))
                    continue
                regions.append(
                    (thunk_start, body_start, 16, "16-bit DPMI mode-switch thunk")
                )
                regions.append((body_start, next_thunk, 32, "32-bit routine body"))
        else:
            regions.append((0, len(payload), 16, "16-bit code"))

        covered = 0
        for region_start, region_end, mode, description in regions:
            if region_start > covered:
                for address in range(covered, region_start):
                    labels = labels_by_location.get((segment.number, address), [])
                    lines.extend(f"{label}:" for label in labels)
                    lines.append(f"seg{segment.number:03d}_{address:04x}: db 0x{payload[address]:02x}")
            lines.extend(["", f"; {description}", f"bits {mode}", ""])
            region_covered = region_start
            for instruction in disassemblers[mode].disasm(
                payload[region_start:region_end], region_start
            ):
                if instruction.address > region_covered:
                    for address in range(region_covered, instruction.address):
                        labels = labels_by_location.get((segment.number, address), [])
                        lines.extend(f"{label}:" for label in labels)
                        lines.append(
                            f"seg{segment.number:03d}_{address:04x}: db 0x{payload[address]:02x}"
                        )
                labels = labels_by_location.get((segment.number, instruction.address), [])
                lines.extend(f"{label}:" for label in labels)
                instruction_relocations: list[tuple[int, Relocation]] = []
                for source_offset in range(
                    instruction.address, instruction.address + instruction.size
                ):
                    instruction_relocations.extend(
                        (source_offset, relocation)
                        for relocation in relocation_map.get(source_offset, [])
                    )
                for source_offset, relocation in instruction_relocations:
                    source_name = RELOCATION_SOURCE_NAMES.get(
                        relocation.source_type & 0x0F,
                        f"source-0x{relocation.source_type:02x}",
                    )
                    lines.append(
                        f"; relocation +0x{source_offset - instruction.address:x} "
                        f"{source_name}: {relocation.description}"
                    )
                encoding = " ".join(f"{byte:02x}" for byte in instruction.bytes)
                operation = f"{instruction.mnemonic} {instruction.op_str}".rstrip()
                lines.append(
                    f"seg{segment.number:03d}_{instruction.address:04x}: "
                    f"{encoding:<24} {operation}"
                )
                region_covered = instruction.address + instruction.size
            for address in range(region_covered, region_end):
                labels = labels_by_location.get((segment.number, address), [])
                lines.extend(f"{label}:" for label in labels)
                lines.append(f"seg{segment.number:03d}_{address:04x}: db 0x{payload[address]:02x}")
            covered = region_end
        for address in range(covered, len(payload)):
            labels = labels_by_location.get((segment.number, address), [])
            lines.extend(f"{label}:" for label in labels)
            lines.append(f"seg{segment.number:03d}_{address:04x}: db 0x{payload[address]:02x}")
        (destination / f"{stem}.asm").write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="NE executable or DLL")
    parser.add_argument("--json", type=Path, help="write machine-readable analysis")
    parser.add_argument("--extract-resources", type=Path, metavar="DIR")
    parser.add_argument("--disassemble", type=Path, metavar="DIR")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    data = args.binary.read_bytes()
    summary, segments, entries, resources = binary_summary(args.binary, data)
    output = json.dumps(printable_summary(summary), indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    if args.extract_resources:
        extract_resources(data, resources, args.extract_resources)
    if args.disassemble:
        write_segment_outputs(data, summary, segments, entries, args.disassemble)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
