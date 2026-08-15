#!/usr/bin/env python3
"""Convert the installed Gearheads data into standalone native-port assets.

The source executable and DLL are read only as containers.  The output never
contains either binary, an AR archive, or a CD/VHD image.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct

from PIL import Image

import ne_analyze
import recover_game_data


# XR records are blitted as physical WinG framebuffer indices. Segment 11
# constructs their gameplay palette independently of every DIB: entries
# 10..225 are a 6x6x6 RGB cube, with red changing fastest, then green, then
# blue. Windows keeps the first ten physical entries for system colors.
SPRITE_PALETTE_BASE = 10
SPRITE_PALETTE_LAST = 225


@dataclass(frozen=True)
class ArchiveEntry:
    name: str
    offset: int
    size: int
    payload: bytes


def signed_byte(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def safe_name(value: str) -> str:
    return "".join(character.lower() if character.isalnum() else "_" for character in value)


def sprite_color(color_index: int) -> tuple[int, int, int]:
    if not SPRITE_PALETTE_BASE <= color_index <= SPRITE_PALETTE_LAST:
        raise ValueError(f"xr uses unrealized palette index {color_index}")
    cube_index = color_index - SPRITE_PALETTE_BASE
    return (
        (cube_index % 6) * 51,
        ((cube_index // 6) % 6) * 51,
        (cube_index // 36) * 51,
    )


def parse_archive(path: Path) -> list[ArchiveEntry]:
    data = path.read_bytes()
    if len(data) < 8 or data[:2] != b"ar":
        raise ValueError(f"{path} is not a Gearheads AR archive")
    count = struct.unpack_from("<H", data, 2)[0]
    table_offset = struct.unpack_from("<I", data, 4)[0]
    if table_offset + count * 16 != len(data):
        raise ValueError(f"{path} has an invalid trailing index")
    entries: list[ArchiveEntry] = []
    previous_name = ""
    for index in range(count):
        cursor = table_offset + index * 16
        raw_name = data[cursor : cursor + 8].split(b"\0", 1)[0]
        name = raw_name.decode("ascii")
        offset, size = struct.unpack_from("<II", data, cursor + 8)
        if offset < 8 or offset + size > table_offset:
            raise ValueError(f"{path}:{name} points outside the archive data")
        if previous_name and name.casefold() < previous_name.casefold():
            raise ValueError(f"{path} index is not sorted at {name}")
        previous_name = name
        entries.append(ArchiveEntry(name, offset, size, data[offset : offset + size]))
    return entries


def decode_sprite(payload: bytes) -> tuple[Image.Image, dict]:
    if len(payload) < 12 or payload[:2] != b"xr":
        raise ValueError("sprite does not have an xr header")
    declared_size = struct.unpack_from("<H", payload, 2)[0]
    if declared_size != len(payload):
        raise ValueError(f"xr size is {declared_size}, expected {len(payload)}")
    width, height = payload[4], payload[5]
    origin_x, origin_y = signed_byte(payload[6]), signed_byte(payload[7])
    flags = struct.unpack_from("<H", payload, 8)[0]
    if not width or not height:
        raise ValueError("xr has a zero dimension")

    rgba = bytearray(width * height * 4)
    cursor = 10
    row = 0
    previous_end = 0
    literal_pixels = 0
    effect_pixels = 0
    spans = 0
    factors: dict[int, int] = {}
    terminated = False
    while cursor < len(payload):
        raw_x = payload[cursor]
        cursor += 1
        x = signed_byte(raw_x)
        if x < signed_byte(previous_end):
            row += 1
            if x < 0:
                row = -x
                if cursor >= len(payload):
                    raise ValueError("xr ends after a row-jump marker")
                x = payload[cursor]
                cursor += 1
        if x < 0:
            raise ValueError(f"xr has a negative span x coordinate {x}")
        if cursor >= len(payload):
            raise ValueError("xr ends before a span length")
        count = signed_byte(payload[cursor])
        cursor += 1
        if count == 0:
            terminated = True
            break
        span_length = abs(count)
        if row >= height or x + span_length > width:
            raise ValueError(
                f"xr span ({x}, {row}, {span_length}) exceeds {width}x{height}"
            )
        previous_end = x + span_length
        spans += 1
        if count > 0:
            end = cursor + count
            if end > len(payload):
                raise ValueError("xr literal span is truncated")
            for column, color_index in enumerate(payload[cursor:end], x):
                red, green, blue = sprite_color(color_index)
                # The original WinG wrapper anchors the XR stream at the
                # sprite's bottom scanline and advances toward the top.
                destination = ((height - 1 - row) * width + column) * 4
                rgba[destination : destination + 4] = bytes((red, green, blue, 255))
            cursor = end
            literal_pixels += count
        else:
            end = cursor + span_length * 2
            if end > len(payload):
                raise ValueError("xr effect span is truncated")
            for column in range(x, x + span_length):
                color_index = payload[cursor]
                factor = payload[cursor + 1]
                cursor += 2
                if factor > 6:
                    raise ValueError(f"xr uses unknown effect factor {factor}")
                # The original builds seven 256x256 palette-compositing tables.
                # Factor 0 keeps 7/8 of the source color; factor 6 keeps 1/8.
                alpha = (255 * (7 - factor) + 4) // 8
                red, green, blue = sprite_color(color_index)
                destination = ((height - 1 - row) * width + column) * 4
                rgba[destination : destination + 4] = bytes((red, green, blue, alpha))
                factors[factor] = factors.get(factor, 0) + 1
            effect_pixels += span_length
    if not terminated:
        raise ValueError("xr does not contain its zero-length terminator")
    if cursor != len(payload):
        raise ValueError(f"xr has {len(payload) - cursor} bytes after its terminator")

    image = Image.frombytes("RGBA", (width, height), bytes(rgba))
    metadata = {
        "width": width,
        "height": height,
        "origin_x": origin_x,
        "origin_y": origin_y,
        "flags": flags,
        "spans": spans,
        "literal_pixels": literal_pixels,
        "effect_pixels": effect_pixels,
        "effect_factors": {str(key): factors[key] for key in sorted(factors)},
        "stream_row_order": "bottom-to-top",
    }
    return image, metadata


def dib_layout(data: bytes) -> tuple[int, int, int, int, int]:
    if len(data) < 40:
        raise ValueError("truncated DIB")
    header_size = struct.unpack_from("<I", data, 0)[0]
    if header_size < 40 or header_size > len(data):
        raise ValueError(f"unsupported DIB header size {header_size}")
    width, height, planes, bits_per_pixel = struct.unpack_from("<iiHH", data, 4)
    compression = struct.unpack_from("<I", data, 16)[0]
    colors_used = struct.unpack_from("<I", data, 32)[0]
    if width <= 0 or height == 0 or planes != 1:
        raise ValueError("unsupported DIB dimensions or plane count")
    if compression != 0:
        raise ValueError(f"unsupported compressed DIB type {compression}")
    palette_entries = colors_used or ((1 << bits_per_pixel) if bits_per_pixel <= 8 else 0)
    pixel_offset = header_size + palette_entries * 4
    if pixel_offset > len(data):
        raise ValueError("DIB palette extends beyond the resource")
    return width, height, bits_per_pixel, palette_entries, pixel_offset


def dib_palette(data: bytes) -> list[tuple[int, int, int]]:
    _width, _height, bits_per_pixel, entries, _pixel_offset = dib_layout(data)
    if bits_per_pixel != 8 or entries != 256:
        raise ValueError("the canonical Gearheads palette must contain 256 colors")
    header_size = struct.unpack_from("<I", data, 0)[0]
    palette: list[tuple[int, int, int]] = []
    for index in range(entries):
        blue, green, red, _reserved = data[header_size + index * 4 : header_size + index * 4 + 4]
        palette.append((red, green, blue))
    return palette


def decode_dib(data: bytes) -> Image.Image:
    _width, _height, _bits, _entries, pixel_offset = dib_layout(data)
    bitmap = struct.pack("<2sIHHI", b"BM", len(data) + 14, 0, 0, pixel_offset + 14) + data
    with Image.open(io.BytesIO(bitmap)) as image:
        image.load()
        return image.convert("RGBA")


def read_ne_resources(path: Path) -> list[tuple[ne_analyze.Resource, bytes]]:
    data = path.read_bytes()
    _summary, _segments, _entries, resources = ne_analyze.binary_summary(path, data)
    return [
        (resource, data[resource.file_offset : resource.file_offset + resource.length])
        for resource in resources
    ]


def decode_xor_text(payload: bytes) -> str:
    meaningful = payload.rstrip(b"\0")
    decoded = bytes(value ^ 0x80 for value in meaningful)
    if decoded.endswith(b"\x1a"):
        decoded = decoded[:-1]
    return decoded.decode("cp1252").replace("\r\n", "\n")


def write_text_copy(source: Path, destination: Path) -> None:
    text = source.read_bytes().decode("cp1252").replace("\r\n", "\n")
    destination.write_text(text, encoding="utf-8", newline="\n")


def source_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_manifest(output: Path, conversion: dict) -> None:
    files = []
    for path in sorted(output.rglob("*")):
        if not path.is_file() or path.name == "manifest.json":
            continue
        payload = path.read_bytes()
        files.append(
            {
                "path": path.relative_to(output).as_posix(),
                "bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        )
    manifest = {**conversion, "files": files}
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def extract(game_root: Path, windows_root: Path, output: Path) -> None:
    executable = game_root / "GEAR_EN.EXE"
    resource_dll = game_root / "GEARAGE.DLL"
    required = [
        executable,
        resource_dll,
        game_root / "ANIM.DAT",
        game_root / "GEAR.TTF",
        windows_root / "GEARHEAD.INI",
    ]
    for path in required:
        if not path.is_file():
            raise ValueError(f"required source file is missing: {path}")

    if output.exists():
        shutil.rmtree(output)
    for directory in ["backgrounds", "data", "fonts", "music", "sounds", "sprites", "ui"]:
        (output / directory).mkdir(parents=True, exist_ok=True)

    dll_resources = read_ne_resources(resource_dll)
    sprite_records = []
    archive_paths = sorted((game_root / "ART").glob("*.AR"))
    for archive_path in archive_paths:
        archive_name = archive_path.stem.lower()
        archive_output = output / "sprites" / archive_name
        archive_output.mkdir(parents=True, exist_ok=True)
        for entry in parse_archive(archive_path):
            image, metadata = decode_sprite(entry.payload)
            relative = Path("sprites") / archive_name / f"{entry.name.lower()}.png"
            image.save(output / relative, format="PNG", optimize=True)
            sprite_records.append(
                {
                    "archive": archive_path.name,
                    "name": entry.name,
                    "path": relative.as_posix(),
                    "source_offset": entry.offset,
                    "source_bytes": entry.size,
                    "source_sha256": hashlib.sha256(entry.payload).hexdigest(),
                    **metadata,
                }
            )

    origin_lines = [
        f"{record['path']}={record['origin_x']} {record['origin_y']}"
        for record in sorted(sprite_records, key=lambda record: record["path"])
    ]
    (output / "data" / "sprite-origins.ini").write_text(
        "\n".join(origin_lines) + "\n", encoding="ascii", newline="\n"
    )

    for bitmap in sorted((game_root / "BG").glob("*.BMP")):
        with Image.open(bitmap) as image:
            image.load()
            image.convert("RGBA").save(
                output / "backgrounds" / f"{bitmap.stem.lower()}.png",
                format="PNG",
                optimize=True,
            )

    wave_records = []
    bitmap_records = []
    for resource, payload in dll_resources:
        name = safe_name(resource.name)
        if resource.type_name == "BITMAP":
            image = decode_dib(payload)
            subdirectory = "backgrounds" if resource.name.upper().startswith("BG") else "ui"
            relative = Path(subdirectory) / f"{name}.png"
            image.save(output / relative, format="PNG", optimize=True)
            bitmap_records.append(
                {
                    "resource": resource.name,
                    "path": relative.as_posix(),
                    "width": image.width,
                    "height": image.height,
                }
            )
        elif resource.type_name == "WAVE":
            if len(payload) < 12 or payload[:4] != b"RIFF" or payload[8:12] != b"WAVE":
                raise ValueError(f"WAVE resource {resource.name} does not contain RIFF/WAVE data")
            riff_size = struct.unpack_from("<I", payload, 4)[0] + 8
            if riff_size > len(payload):
                raise ValueError(f"WAVE resource {resource.name} is truncated")
            relative = Path("sounds") / f"{name}.wav"
            (output / relative).write_bytes(payload[:riff_size])
            wave_records.append(
                {"resource": resource.name, "path": relative.as_posix(), "bytes": riff_size}
            )

    exe_resources = read_ne_resources(executable)
    decoded_custom = {}
    for resource, payload in exe_resources:
        if resource.type_name == "TYPE_258" and resource.name in {"SCRIPT", "SCREENS"}:
            relative = Path("data") / f"{resource.name.lower()}.ini"
            (output / relative).write_text(
                decode_xor_text(payload), encoding="utf-8", newline="\n"
            )
            decoded_custom[resource.name.lower()] = relative.as_posix()
    if set(decoded_custom) != {"script", "screens"}:
        raise ValueError("GEAR_EN.EXE does not contain both SCRIPT and SCREENS resources")

    write_text_copy(game_root / "ANIM.DAT", output / "data" / "anim.dat")
    write_text_copy(windows_root / "GEARHEAD.INI", output / "data" / "gearhead.ini")
    recovered_defaults = recover_game_data.recover(executable)
    (output / "data" / "defaults.json").write_text(
        json.dumps(recovered_defaults, indent=2) + "\n", encoding="utf-8"
    )
    (output / "data" / "runtime-defaults.ini").write_text(
        recover_game_data.render_runtime_defaults(recovered_defaults),
        encoding="utf-8",
        newline="\n",
    )
    shutil.copyfile(game_root / "GEAR.TTF", output / "fonts" / "gear.ttf")
    for midi in sorted((game_root / "WAV").glob("*.MID")):
        shutil.copyfile(midi, output / "music" / midi.name.lower())

    build_manifest(
        output,
        {
            "format": 1,
            "source": {
                "gear_en_exe_sha256": source_hash(executable),
                "gearage_dll_sha256": source_hash(resource_dll),
            },
            "conversion": {
                "sprite_effect_alpha": "alpha = round(255 * (7 - factor) / 8)",
                "sprite_palette_source": "GEAR_EN.EXE segment 11 generated 6x6x6 RGB cube",
                "sprite_palette_mapping": "XR index p in 10..225 maps cube index p-10; red changes fastest",
            },
            "sprites": sprite_records,
            "resource_bitmaps": bitmap_records,
            "resource_waves": wave_records,
            "decoded_custom_resources": decoded_custom,
        },
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--windows-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    extract(args.game_root, args.windows_root, args.output)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as error:
        raise SystemExit(f"error: {error}")
