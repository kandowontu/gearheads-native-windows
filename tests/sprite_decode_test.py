#!/usr/bin/env python3
"""Regression checks for the recovered XR gameplay RGB cube."""

from __future__ import annotations

import json
from pathlib import Path
import struct
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from extract_assets import decode_sprite, sprite_color  # noqa: E402


def xr(width: int, height: int, stream: bytes) -> bytes:
    size = 10 + len(stream)
    return b"xr" + struct.pack("<HBBBBH", size, width, height, 0, 0, 0) + stream


def main() -> int:
    assert sprite_color(10) == (0, 0, 0)
    assert sprite_color(15) == (255, 0, 0)
    assert sprite_color(16) == (0, 51, 0)
    assert sprite_color(45) == (255, 255, 0)
    assert sprite_color(46) == (0, 0, 51)
    assert sprite_color(225) == (255, 255, 255)

    # Dominant Clucketta shades provide an independent control against the
    # original indexed TOYBOX bitmap.
    assert sprite_color(20) == (204, 51, 0)
    assert sprite_color(26) == (204, 102, 0)
    assert sprite_color(27) == (255, 102, 0)

    literal, _ = decode_sprite(xr(2, 1, bytes((0, 2, 10, 225, 2, 0))))
    assert literal.getpixel((0, 0)) == (0, 0, 0, 255)
    assert literal.getpixel((1, 0)) == (255, 255, 255, 255)

    # XR row zero is the bottom scanline. The original blitter starts there
    # and advances upward through the WinG destination surface.
    vertical, metadata = decode_sprite(
        xr(1, 2, bytes((0, 1, 10, 0, 1, 225, 1, 0)))
    )
    assert vertical.getpixel((0, 0)) == (255, 255, 255, 255)
    assert vertical.getpixel((0, 1)) == (0, 0, 0, 255)
    assert metadata["stream_row_order"] == "bottom-to-top"

    effect, metadata = decode_sprite(
        xr(2, 1, bytes((0, 0xFE, 10, 0, 225, 6, 2, 0)))
    )
    assert effect.getpixel((0, 0)) == (0, 0, 0, 223)
    assert effect.getpixel((1, 0)) == (255, 255, 255, 32)
    assert metadata["effect_factors"] == {"0": 1, "6": 1}

    try:
        decode_sprite(xr(1, 1, bytes((0, 1, 9, 1, 0))))
    except ValueError as error:
        assert "unrealized palette index 9" in str(error)
    else:
        raise AssertionError("XR palette index 9 should have been rejected")

    asset_root = Path(__file__).resolve().parents[1] / "assets"
    manifest = json.loads((asset_root / "manifest.json").read_text(encoding="utf-8"))
    origins = {}
    for line in (asset_root / "data" / "sprite-origins.ini").read_text(
        encoding="ascii"
    ).splitlines():
        path, coordinates = line.split("=", 1)
        origins[path] = tuple(int(value) for value in coordinates.split())
    assert len(origins) == len(manifest["sprites"])
    for sprite in manifest["sprites"]:
        assert origins[sprite["path"]] == (sprite["origin_x"], sprite["origin_y"])
    assert origins["sprites/ck/ck_wh01.png"] == (34, 47)
    assert origins["sprites/wuk/wuk1w01.png"] == (7, 10)

    print("XR gameplay RGB cube and sprite origins verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
