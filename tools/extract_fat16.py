#!/usr/bin/env python3
"""Extract a FAT12/FAT16 partition from a raw disk or fixed VHD image.

The extractor deliberately avoids mounting the source image. It reads the MBR,
walks FAT cluster chains, understands long-file-name entries, and writes a copy
of the directory tree to a caller-provided destination.
"""

from __future__ import annotations

import argparse
import dataclasses
import os
from pathlib import Path
import struct
import sys


@dataclasses.dataclass(frozen=True)
class Geometry:
    partition_offset: int
    bytes_per_sector: int
    sectors_per_cluster: int
    reserved_sectors: int
    fat_count: int
    root_entries: int
    sectors_per_fat: int
    total_sectors: int

    @property
    def cluster_size(self) -> int:
        return self.bytes_per_sector * self.sectors_per_cluster

    @property
    def fat_offset(self) -> int:
        return self.partition_offset + self.reserved_sectors * self.bytes_per_sector

    @property
    def root_offset(self) -> int:
        return self.fat_offset + self.fat_count * self.sectors_per_fat * self.bytes_per_sector

    @property
    def root_size(self) -> int:
        return self.root_entries * 32

    @property
    def data_offset(self) -> int:
        root_sectors = (self.root_size + self.bytes_per_sector - 1) // self.bytes_per_sector
        return self.root_offset + root_sectors * self.bytes_per_sector

    @property
    def cluster_count(self) -> int:
        root_sectors = (self.root_size + self.bytes_per_sector - 1) // self.bytes_per_sector
        data_sectors = self.total_sectors - (
            self.reserved_sectors + self.fat_count * self.sectors_per_fat + root_sectors
        )
        return data_sectors // self.sectors_per_cluster


@dataclasses.dataclass(frozen=True)
class Entry:
    name: str
    attributes: int
    first_cluster: int
    size: int

    @property
    def is_directory(self) -> bool:
        return bool(self.attributes & 0x10)


def read_at(handle, offset: int, size: int) -> bytes:
    handle.seek(offset)
    data = handle.read(size)
    if len(data) != size:
        raise ValueError(f"short read at offset {offset}: wanted {size}, got {len(data)}")
    return data


def partition_offset(handle, partition_number: int) -> int:
    mbr = read_at(handle, 0, 512)
    if mbr[510:512] != b"\x55\xaa":
        raise ValueError("image does not contain an MBR signature")
    if not 1 <= partition_number <= 4:
        raise ValueError("partition number must be between 1 and 4")
    entry = mbr[446 + (partition_number - 1) * 16 : 462 + (partition_number - 1) * 16]
    partition_type = entry[4]
    start_lba, sector_count = struct.unpack_from("<II", entry, 8)
    if partition_type == 0 or sector_count == 0:
        raise ValueError(f"partition {partition_number} is empty")
    if partition_type not in {0x01, 0x04, 0x06, 0x0E}:
        raise ValueError(f"partition {partition_number} has unsupported type 0x{partition_type:02x}")
    return start_lba * 512


def read_geometry(handle, offset: int) -> Geometry:
    boot = read_at(handle, offset, 512)
    if boot[510:512] != b"\x55\xaa":
        raise ValueError("partition does not contain a DOS boot-sector signature")
    bytes_per_sector = struct.unpack_from("<H", boot, 11)[0]
    sectors_per_cluster = boot[13]
    reserved_sectors = struct.unpack_from("<H", boot, 14)[0]
    fat_count = boot[16]
    root_entries = struct.unpack_from("<H", boot, 17)[0]
    total_sectors_16 = struct.unpack_from("<H", boot, 19)[0]
    sectors_per_fat = struct.unpack_from("<H", boot, 22)[0]
    total_sectors_32 = struct.unpack_from("<I", boot, 32)[0]
    total_sectors = total_sectors_16 or total_sectors_32
    if bytes_per_sector not in {512, 1024, 2048, 4096}:
        raise ValueError(f"invalid bytes-per-sector value {bytes_per_sector}")
    if sectors_per_cluster == 0 or sectors_per_cluster & (sectors_per_cluster - 1):
        raise ValueError(f"invalid sectors-per-cluster value {sectors_per_cluster}")
    if not all((reserved_sectors, fat_count, root_entries, sectors_per_fat, total_sectors)):
        raise ValueError("partition is not FAT12/FAT16")
    geometry = Geometry(
        partition_offset=offset,
        bytes_per_sector=bytes_per_sector,
        sectors_per_cluster=sectors_per_cluster,
        reserved_sectors=reserved_sectors,
        fat_count=fat_count,
        root_entries=root_entries,
        sectors_per_fat=sectors_per_fat,
        total_sectors=total_sectors,
    )
    if geometry.cluster_count >= 65525:
        raise ValueError("partition is FAT32, which this extractor does not support")
    return geometry


class FatVolume:
    def __init__(self, handle, geometry: Geometry):
        self.handle = handle
        self.geometry = geometry
        self.fat_bits = 12 if geometry.cluster_count < 4085 else 16
        fat_size = geometry.sectors_per_fat * geometry.bytes_per_sector
        self.fat = read_at(handle, geometry.fat_offset, fat_size)

    def next_cluster(self, cluster: int) -> int | None:
        if self.fat_bits == 16:
            value = struct.unpack_from("<H", self.fat, cluster * 2)[0]
            if value >= 0xFFF8:
                return None
            if value == 0xFFF7:
                raise ValueError(f"bad cluster encountered after {cluster}")
        else:
            offset = cluster + cluster // 2
            pair = struct.unpack_from("<H", self.fat, offset)[0]
            value = pair >> 4 if cluster & 1 else pair & 0x0FFF
            if value >= 0x0FF8:
                return None
            if value == 0x0FF7:
                raise ValueError(f"bad cluster encountered after {cluster}")
        if value < 2:
            raise ValueError(f"invalid cluster-chain value {value} after {cluster}")
        return value

    def cluster_bytes(self, cluster: int) -> bytes:
        if not 2 <= cluster < self.geometry.cluster_count + 2:
            raise ValueError(f"cluster {cluster} lies outside the data area")
        offset = self.geometry.data_offset + (cluster - 2) * self.geometry.cluster_size
        return read_at(self.handle, offset, self.geometry.cluster_size)

    def chain_bytes(self, first_cluster: int, size: int | None = None) -> bytes:
        if first_cluster == 0:
            return b""
        chunks: list[bytes] = []
        seen: set[int] = set()
        cluster: int | None = first_cluster
        while cluster is not None:
            if cluster in seen:
                raise ValueError(f"cluster-chain loop at {cluster}")
            seen.add(cluster)
            chunks.append(self.cluster_bytes(cluster))
            cluster = self.next_cluster(cluster)
        data = b"".join(chunks)
        return data if size is None else data[:size]

    def root_directory_bytes(self) -> bytes:
        return read_at(self.handle, self.geometry.root_offset, self.geometry.root_size)


def decode_short_name(raw: bytes) -> str:
    stem = raw[0:8].decode("cp437", errors="replace").rstrip(" ")
    suffix = raw[8:11].decode("cp437", errors="replace").rstrip(" ")
    if raw[0] == 0x05:
        stem = chr(0xE5) + stem[1:]
    return f"{stem}.{suffix}" if suffix else stem


def decode_lfn_piece(raw: bytes) -> str:
    encoded = raw[1:11] + raw[14:26] + raw[28:32]
    chars: list[str] = []
    for index in range(0, len(encoded), 2):
        codepoint = struct.unpack_from("<H", encoded, index)[0]
        if codepoint in {0x0000, 0xFFFF}:
            break
        chars.append(chr(codepoint))
    return "".join(chars)


def safe_name(name: str) -> str:
    if name in {"", ".", ".."}:
        return name
    if any(character in name for character in ("/", "\\", "\x00")):
        raise ValueError(f"unsafe path component {name!r}")
    return name


def directory_entries(data: bytes) -> list[Entry]:
    result: list[Entry] = []
    lfn_pieces: dict[int, str] = {}
    for offset in range(0, len(data) - 31, 32):
        raw = data[offset : offset + 32]
        if raw[0] == 0x00:
            break
        if raw[0] == 0xE5:
            lfn_pieces.clear()
            continue
        attributes = raw[11]
        if attributes == 0x0F:
            order = raw[0] & 0x1F
            if order:
                lfn_pieces[order] = decode_lfn_piece(raw)
            continue
        if attributes & 0x08:
            lfn_pieces.clear()
            continue
        long_name = "".join(lfn_pieces[index] for index in sorted(lfn_pieces)) if lfn_pieces else ""
        lfn_pieces.clear()
        name = safe_name(long_name or decode_short_name(raw[0:11]))
        first_cluster = struct.unpack_from("<H", raw, 26)[0]
        size = struct.unpack_from("<I", raw, 28)[0]
        result.append(Entry(name=name, attributes=attributes, first_cluster=first_cluster, size=size))
    return result


def extract_directory(volume: FatVolume, directory_data: bytes, destination: Path) -> tuple[int, int]:
    directory_count = 0
    file_count = 0
    for entry in directory_entries(directory_data):
        if entry.name in {".", ".."}:
            continue
        output_path = destination / entry.name
        if entry.is_directory:
            output_path.mkdir(parents=False, exist_ok=True)
            child_data = volume.chain_bytes(entry.first_cluster)
            child_dirs, child_files = extract_directory(volume, child_data, output_path)
            directory_count += 1 + child_dirs
            file_count += child_files
        else:
            output_path.write_bytes(volume.chain_bytes(entry.first_cluster, entry.size))
            file_count += 1
    return directory_count, file_count


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="raw disk or fixed VHD image")
    parser.add_argument("destination", type=Path, help="empty or existing extraction directory")
    parser.add_argument("--partition", type=int, default=1, help="MBR partition number (default: 1)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    with args.image.open("rb") as handle:
        offset = partition_offset(handle, args.partition)
        geometry = read_geometry(handle, offset)
        volume = FatVolume(handle, geometry)
        directories, files = extract_directory(volume, volume.root_directory_bytes(), args.destination)
    print(
        f"Extracted {files} files in {directories} directories from "
        f"FAT{volume.fat_bits} partition {args.partition} "
        f"({geometry.cluster_count} clusters, {geometry.cluster_size}-byte clusters)."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
