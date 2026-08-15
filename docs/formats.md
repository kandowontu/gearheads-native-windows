# Recovered data formats

## `AR` sprite archive

All integers are little-endian.

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `char[2]` | ASCII `ar` |
| 2 | `uint16` | Entry count |
| 4 | `uint32` | Offset of the trailing index |
| 8 | bytes | Concatenated `xr` records |
| index | entries | `count` records, 16 bytes each |

Each index record is an eight-byte NUL-padded ASCII name, a `uint32` record
offset, and a `uint32` record size.  The index occupies the exact end of the
file and is sorted case-insensitively for binary search.

## `XR` sparse sprite

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `char[2]` | ASCII `xr` |
| 2 | `uint16` | Complete record size |
| 4 | `uint8` | Width, at most 127 |
| 5 | `uint8` | Height |
| 6 | `int8` | Horizontal origin/hotspot |
| 7 | `int8` | Vertical origin/hotspot |
| 8 | `uint16` | Flags; zero in all shipped records |
| 10 | bytes | Sparse scanline stream |

The stream keeps an absolute end-X coordinate and current row.  A span starts
with an X byte and a signed length byte.  If the signed X is less than the
previous end-X, the decoder advances one row.  A negative X at a row boundary
is an absolute row jump (`row = -x`), followed by the real unsigned X.

Stream row zero is the sprite's **bottom** scanline. The original segment-3
WinG wrapper anchors the segment-4 blitter at the bottom destination row and
advances it upward as the XR row counter increases. Converted top-down PNGs
therefore write stream row `r` to image row `height - 1 - r`.

- Positive length: that many literal 8-bit palette indices follow.
- Negative length: `-length` pairs of `(palette index, effect factor)` follow.
- Zero length: end of sprite.

Effect factors are 0 through 6.  The original engine builds seven lookup-table
layers and computes, per RGB component:

```text
output = source + ((destination - source) * (factor + 1) >> 3)
```

The native assets preserve the same intent as straight RGBA, with source alpha
`(7 - factor) / 8`.  Literal spans are opaque; omitted pixels are transparent.
All 689 records decode exactly through their terminator without trailing data.

XR records contain *realized WinG framebuffer indices*: every shipped sprite
index is in the range 10 through 225 because Windows reserves physical palette
slots 0 through 9. They do not use the palette embedded in a background DIB.
Segment 11 constructs a separate 216-color gameplay palette in those slots.
For physical index `p`, let `i = p - 10`; then:

```text
red   = (i % 6) * 51
green = ((i / 6) % 6) * 51
blue  = (i / 36) * 51
```

Integer division is used. Resolving XR bytes through the background DIB palette
(directly or after subtracting ten) produces incorrect gameplay colors even
though the independently rendered toybox bitmap remains correct.

## Executable text resources

`GEAR_EN.EXE` contains two named custom resources of numeric type 258:
`SCRIPT` and `SCREENS`.  Meaningful bytes are XORed with `0x80`, terminated by
DOS EOF (`0x1a` after decoding), and padded with zeroes to the NE resource
alignment.  Decoding produces CRLF INI-like text.

`SCRIPT` maps game object states to sprite records and sound resources.
`SCREENS` is the complete data-driven front end: screen transitions, keyboard
navigation, image maps, text, buttons, toy descriptions, and tournament flow.

The native ordered parser retains repeated keys instead of collapsing them as
a conventional INI library would.  The shipped resource contains 35 sections;
its UTF-8 conversion and every referenced `@section` transition are validated
by `screen_data_test`.

## Embedded default table

`assets/data/defaults.json` is generated from the executable's automatic data
segment.  Its source is a 51-entry array at `DS:2170`; each eight-byte record is
four little-endian words: name pointer, integer count or ASCII `S`, default
text pointer, and destination offset.  The JSON retains those offsets, the raw
default text, parsed values, and the adjacent 36 records of stride `0x7a`.

## Level data

`GEARHEAD.INI` contains 105 `LEVEL...` sections.  Alongside background, music,
stage, friction, ice, powerup rate, and toy-box properties, obstacle keys have
the grammar `K<two-digit type>n<ordinal>` and three integer values: facing,
X, and Y.  The documented types are conveyors 51–53, ice crack 54,
transporter 55, mud variants 56–59, buggy 73, block 74, and moving walls
75–78.  `level_data_test` validates the complete section count, all five
shipped background themes, stage rectangles, music paths, and obstacle parse.

## DLL resources

`GEARAGE.DLL` is a resource-only 16-bit NE DLL with a minimal loader stub.

- 16 BITMAP resources are Windows DIBs without a 14-byte BMP file header.
- 47 WAVE resources are RIFF/WAVE streams padded to the NE resource boundary.
  The real stream ends at `8 + riff_size`.

The converter adds a BMP header for decoding and trims each WAVE to its RIFF
declared size.
