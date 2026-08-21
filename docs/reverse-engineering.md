# Reverse-engineering record

## Preserved inputs

| Input | Bytes | SHA-256 |
| --- | ---: | --- |
| `Gearheads.iso` | 55,048,192 | `6358df5a4e1bd0e51b1f97d560062f8c743b68f237df41abc511027971934f3c` |
| `Gearheads.vhd` | 35,652,096 | `acbddcdcf0b09ad7c2e7211b4ac25f14e64526279ea74ae30612227e7e1f4c51` |
| `GEAR_EN.EXE` | 272,896 | `fe73024c250026a521299879c00127dd726c53011255f0db36c93bb0990f5991` |
| `GEARAGE.DLL` | 3,206,860 | `ddffb058cb52ea0231e096c0a69fc9be7a572e1e1c0cd286e0b2c815a113c1` |

The installed executable and DLL are byte-identical to their CD copies.

## Executable structure

`GEAR_EN.EXE` is a Windows 3.x NE executable targeting Windows 3.10.  It has 16
code segments and one automatic data segment.  Its imports are `KERNEL`, `GDI`,
`USER`, `WIN87EM`, `WING`, `MMSYSTEM`, `CTL3DV2`, and `WAVEMIX`.

The reproducible analyzer expands non-additive NE relocation chains so every
call site is annotated rather than only the head of each fixup chain.  It emits
complete linear coverage, falling back to `db` for bytes Capstone cannot decode.

Code segment 4 is intentionally mixed-mode.  Each function enters through a
16-bit wrapper, uses DPMI interrupts `31h/000Bh` and `31h/000Ch` to set the code
descriptor D bit, then executes a 32-bit body.  It contains rectangular copies
and the forward/reverse, clipped/unclipped sparse sprite blitters.  Segment 4
offset `023c` is the unclipped forward `xr` blitter from which the complete
stream format and palette-effect lookup were recovered.

The segment-3 wrapper supplies the blitter with the address of the bottom
destination scanline and a pitch that walks toward the top of the logical
image. Consequently XR stream row zero is the sprite bottom; top-down native
images must reverse the decoded row coordinate.

## Mapped subsystems

| Location | Recovered purpose |
| --- | --- |
| segment 4 | 32-bit framebuffer copies and sparse/effect sprite blitters |
| segment 5 `0542` | BMP/DIB loading |
| segment 5 `1684` | Standalone `xr` loader and header validation |
| segment 11 `1102` | Seven-layer palette blend-table generator |
| segment 12 `07cb` | Runtime construction of an `xr` rectangle sprite |
| segment 13 `1126` | Whole-file `AR` archive loader |
| segment 13 `1446` | Binary search of the trailing `AR` index |
| segment 13 `15ea` | Named sprite lookup returning an in-archive `xr` pointer |
| segment 13 `1ca0` | Object animation/script parser |

The script parser resolves values beginning with `@` through the loaded archive
index and treats other sprite references as standalone `xr` files.  Runtime
animation entries retain the raw sprite pointer, frame/command bytes, and two
parameters.  Gameplay reads the `xr` width, height, and signed origins directly
from header bytes 4 through 7.

## Recovered game data

- `SCREENS` contains 35 complete UI-flow sections, including original help
  text, duel setup, both toy-selection flows, tournament progression, score
  presentation, and staff screen.
- `SCRIPT` contains 34 object sections: UI digits/gauges, 12 playable toys,
  secondary objects, all obstacles, powerups, and arrows.
- `GEARHEAD.INI` contains 105 level sections.  Fifty are numbered tournament
  levels, twelve are named bonus duels, and the remaining sections are design
  variants for the kitchen, garden, pond, and factory playfields.
- The INI comments identify obstacle codes 51-59 and 73-78 and document every
  level-section field.
- `ANIM.DAT` contains the timed attract/demo command sequences.

## Embedded configuration descriptors

The automatic data segment contains a 51-entry table at `DS:216e`.  Every
eight-byte entry is a tuple of four 16-bit values: destination offset populated
by the original INI loader, name-string offset, integer count (or ASCII `S` for
a string), and default-string offset.  This is direct compiler data, not a
behavioral guess.  Segment 13 indexes the table from `216e` in eight-byte
strides and reads the name at `+2`, default at `+6`, and passes the entry to the
configuration parser; those accesses also guard against accidentally rotating
the descriptor fields.

The table includes the default left/right toy boxes, 20 twelve-integer toy and
obstacle parameter vectors, ten six-integer board-object vectors, and all 19
global settings.  The latter include `Winningscore=21`, `FrameStep=55`,
`MaxFrameStep=550`, `SuddenDeath=300`, and the original keyboard scan codes.
`tools/recover_game_data.py` extracts and validates the complete table as
`assets/data/defaults.json`, including every original data-segment offset.
The asset converter also emits `assets/data/runtime-defaults.ini`, a compact
projection of all 51 values that the native C++ runtime parses without a JSON
dependency.  The test suite compares every projected integer and string back
to `defaults.json`; the values are generated artifacts rather than constants
retyped into the port.

The same data segment contains 35 initialized script-type records at `DS:2354`
(types 0 through 34), each
`0x7a` bytes.  The analyzer records when a configuration destination overlaps
one of those records, but deliberately does not assign semantic field names
until each access has been proven from code.

### Type callback dispatch

The initializer at segment 10 `0496-08a7` installs five far callbacks in every
type record.  Call sites prove their roles independently: `+60` is the
pair-contact filter (segment 14 `0b5f/0b8f`), `+64` is the contact effect
(`0c19` onward), `+68` is the per-tick callback (segment 10 `1546`), `+6c` is
the expiry/off-board callback (`207d` and `265f`), and `+70` prepares the
current render/collision rectangle (`148d`).  `src/original_dispatch.cpp`
preserves all 35 recovered records and all 22 specialized callback writes as a
checked native table.  Its test also retains an apparent original initializer
typo: DS:`28b5` is cleared twice, so Oily's `+23` state-class byte remains at
the default value 2.

The ordinary toy defaults are contact-filter `1dcc` (false), contact-effect
`1de4` (true), tick `2638`, expiry `1dfc`, and rectangle preparation `0d10`.
Specializations cover all twelve toys plus Small Fry and Rocket, the moving bug
and block, all four walls, every board surface, the transporter, crack, rock,
and powerup.  This table is now the inventory for replacing callbacks one at a
time without inferring behavior from sprite names.

The private byte at object offset `+32` is behavior state, while `+33` is the
collision layer, `+34` is the resolved animation state, `+35` is that state's
frame clock, and `+40` is the per-contact attachment pointer. These fields are
not interchangeable: the original callbacks freely update behavior timers
while animation selection resets only `+35`. The native runtime therefore
stores them independently and clears attachment pointers at the beginning of
each collision pass.

Segment 13's animation-state decoder maps `w/e/d/z/f/x/y` to bases
`0/3/6/9/12/15/18`; `d` and `u` suffixes add one and two when the corresponding
directional state exists. A negative SCRIPT resource copies an earlier
ten-byte frame record. Resolving those aliases is required for wind-down,
death, and action durations; treating them as non-image commands shortens
several toy state machines.

Bomby's override is especially complete.  Segment 10 `2832-2869` forces a
last positive winding value below `DecayTime` to one before calling the common
toy tick.  Its expiry callback at `1e8c-2038` stops all motion, selects state 6,
plays the special sound, and after the `d` animation tests every live physical
object against `(Blast Radius * 16)^2`.  The comparison is strict; an object on
the radius is not hit.  A hit caps winding at one, except another Bomby receives
`-100` to chain-trigger, while original type 21 (Disasteroid) is explicitly
immune.  The native duel ports those rules using the recovered default radius
83, all thirteen ripped explosion frames, and `tb_spc1.wav`.

All twelve fields are now structurally identified.  The executable's hidden
`GEARCON` developer dialog labels words 0 through 4 `Mass`, `Speed`, `P`, `Vim`,
and `Extra`.  The object-construction path at segment 10 `18fb` proves `P` is
the movement mode.  Segment 10 `2668` subtracts Vim from the object's winding
value each update, so it is the winding-energy decay.  Words 4 and 5 are
toy-specific extras: the same dialog labels them contextually as values such as
Bomb `Blast Radius`, Cluck `Rest`/`Egg %`, Zap `Energy Drain`, Kanga `Punch
Distance`, Disasteroid `Recover Time`, Presto `Jump Time`, Krush `Jump`/`Radius`,
and Handy `Energy Added`.

The adjacent `RECTCON` developer dialog is titled `Collision Box Size
(Percent)` and labels words 6 through 9 `Front`, `Top`, `Back`, and `Bottom`.
Its final two controls are labelled `X Handy Y`; segment 10 `0ec6-0f53` applies
those signed values while Handy positions an attached toy, proving words 10 and
11 are the per-type Handy attachment offsets.  The complete ordered layout is
therefore mass, speed, movement mode, Vim decay, two type-specific extras, four
collision-box percentages, and Handy X/Y attachment offsets.

Segment 14 `0e26-0f7b` supplies the executable-level geometry proof for those
four collision fields.  For the current sprite it reads the `xr` width, height,
and signed X/Y origin, converts `(width-1)` and `(height-1)` to 1/16-pixel
units, and uses `MulDiv(..., percent, 100)` to create forward and mirrored
rectangles.  The native duel now uses the same four percentage extents against
the current animation frame instead of its former invented circular radius.

The native toy catalog also obtains mass, horizontal speed, movement mode, Vim
decay, extras, collision percentages, and Handy offsets from this runtime table.
Segment 10 `18fb-1911` proves that construction multiplies horizontal speed by
the facing sign and by four in the original 1/16-pixel coordinate system; the
port converts that quantity to pixels per original 55 ms `FrameStep`.  Segment
10 `2668-2673` proves the per-update subtraction of each type's Vim value from
its winding energy, which the native fixed-step loop now preserves.

Segment 10 `005a-042c` builds each object's desired motion.  It subtracts
`DecayTime` from winding, uses Win16 `MulDiv` to taper Speed over
`SlowingTime`, and then multiplies the result by four in the 1/16-pixel system.
Modes 0, 1, and 2 select straight, quadrant-diagonal, and erratic 64-heading
motion.  Segment 12 `0104-0151` reveals that headings use two signed 64-word
sine/cosine tables scaled by 1024; the native engine preserves the tables,
including the original cosine table's one-unit asymmetries.  Segment 14
`06bb-0717` approaches desired velocity with
`(current*16 + (desired-current)*friction) >> 4`, using the level's 1-15
friction value, and zeros motion at the decay cutoff.  These fixed-point paths
are covered by the native physics test.

The two-body response at segment 14 `0062-03fb` first projects both velocities
onto the center-to-center normal and its tangent.  Separating pairs are left
alone.  Ordinary pairs receive the standard elastic normal response weighted
by each object's `+38` value; segment 10 `0a07-0a10` proves that this value is a
copy of the recovered type Mass.  Tangential velocity is preserved.  Type flag
bits `0x08/0x10` select the motion-blocking branch used by obstacles.  Finally,
the runtime helper at segment 16 `0882-08be` selects x87 truncate-toward-zero
before writing the four 16-bit response components.  The native solver ports
all of those branches and rounding semantics.

The caller at segment 10 `14f5-15d5` establishes the surrounding order:
collision detection runs first, segment 14 `03fc-0730` integrates contact
groups second, each object's callback updates desired motion and winding third,
and only then are actual velocities added to positions.  Non-ice contacts are
joined through object links `+06/+08`; the group pass mass-weights current and
desired velocities, applies friction with signed divide-toward-zero semantics,
and assigns one velocity to the entire connected component.  On ice, ordinary
toys skip that friction/group pass and use the elastic two-body solver instead.
The native update loop now preserves this branch, ordering, connected-component
behavior, and the special motion-blocking zero-velocity result.

The menu's `>numtoys#1` through `#12` actions label their setting `Toybox
Size`; the following `~whattoys1#15` through `#26` and `~whattoys2#15` through
`#26` actions select that many distinct original toy types for each player's
roster.  This is separate from the recovered `Maxtoys=59` default.  Segment 10
`08e1-08ed` and `24b0-24c6` compare `Maxtoys` with the global count of live
script objects, proving it is the engine-wide allocation ceiling rather than a
per-player launch limit.  The native duel now preserves that distinction and
cycles each player only through the chosen roster.

Segment 12 `02f6-0681` recovers the launch-gauge state machine. The shipped
values are `GaugeTime=6000`, `DecayTime=1200`, `Resetimeonpick=1`, and
`FrameStep=55`. A match begins at full gauge; a successful launch samples that
value as the toy's winding energy and resets the gauge to `DecayTime/2`, or 600
milliseconds. The release branch requires the gauge to be strictly greater
than `DecayTime`, so holding or repeatedly pressing the launch key cannot emit
an unlimited stream of toys. Failed attempts leave the gauge unchanged and
play the original `notoy.wav`; selecting another toy performs the configured
reset. Gauge accumulation uses elapsed milliseconds and caps at 6000.

The same segment's render path centers each level's gauge anchor rectangle but
positions a frame with the XR header origin: X is `center-origin_x`, while Y is
`center-height+origin_y`. This matters because the 36 winding frames have
different dimensions and origins. The extractor now emits all 689 recovered
origin pairs as `data/sprite-origins.ini`, and the native image cache attaches
them to decoded PNGs. The adjacent toybox path centers the selected toy's first
`w` sprite by bitmap dimensions and mirrors player two. Gameplay scores use the
original `digwh00` through `digwh09` artwork, and both lane selectors use the
original arrow sprite mirrored toward the board; no replacement text or black
HUD strip is drawn over the playfield.

Segment 6 `6994-6b4b` is the computer-player difficulty dispatcher. It takes a
`rand()%10` roll on every call and selects low/medium/high routines in these
exact proportions: difficulty 1 is 10/10 low; 2-4 blend in 1/10, 2/10, and
3/10 medium; 5 is 10/10 medium; 6-8 blend in 3/10, 6/10, and 9/10 high; and 9
is 10/10 high. The three routines retain independent cursor clocks. They turn
`timeGetTime()` into 60 Hz ticks with `(milliseconds*6)/100` and accept a
cursor step only when the unsigned tick difference is strictly greater than
40. The low routine at `2198-2284` launches only above winding 4800 (or 5300
when the alternate-rules word is set), then picks an inclusive random step
from -1 through 1 and walks original type numbers 15..26 until it finds an
available toy. The native AI cadence and numeric roster walk are direct ports
of these branches; the earlier hand-authored per-difficulty seconds interval
has been removed.

Segment 12 `1011-1078` is the complete duel winner predicate.  It first checks
each score against `Winningscore`, then requires that score to exceed the other
player's score plus one.  With the recovered default `Winningscore=21`, the
result is therefore first to 21 with a lead of at least two, exactly as the
help screen says.  The native match rule, overtime behavior, win sound, and
return to the one- or two-player post-duel screen now use this recovered path.

The level-hacking notes in `GEARHEAD.INI` define obstacle codes 51-59 and
73-78.  Their displayed resources tie 51/52/53 to the `htrd`/`utrd`/`dtrd`
conveyor records, 54-59 to crack/transporter/mud/oil/glue/rock, 73 to the
garden bug, and 74-78 to block plus wall records 1-4.  The native obstacle
catalog now tests that complete mapping.  Fixed walls use the same recovered
per-record Front/Top/Back/Bottom percentage geometry as toys.  On
ice they enter segment 14's motion-blocking reflection branch; on non-ice they
stop the connected contact group.  Initializer writes at segment 10
`0794-07c7` prove that only wall records 31-34 receive flag `0x10`; block
record 30 remains an ordinary heavy movable object. The native engine ports the
three conveyors, crack, transporter, mud, oil, glue, rock, garden bug, moving
block, and four wall state machines, with regression tests for their recovered
contact and steering rules.

Mud has one naming trap: script record 11 is `mudd`, while the six-word
configuration descriptor that writes to record 11 offset `+0x12` is named
`hole`. The runtime therefore resolves obstacle code 56 through the `hole`
descriptor. Looking up `mudd` as a configuration key is invalid and used to
surface as a level-load error in the native port.

## Palette realization

The original sparse XR streams store realized WinG palette indices. Every
decoded source byte across all 689 archive sprites lies in `10..225`, with
physical indices `0..9` reserved by Windows. Crucially, these are not indices
into a background bitmap's DIB table. Segment 11 `1408-1579` creates a fresh
256-entry logical palette, fills entries `10..225` with a 6x6x6 RGB cube, marks
them `PC_RESERVED`, and realizes it. For `i = physical_index - 10`, red is
`(i % 6)*51`, green is `((i/6) % 6)*51`, and blue is `(i/36)*51`.

This distinction is visible directly in Clucketta's data: common XR indices
26, 27, and 20 become `(204,102,0)`, `(255,102,0)`, and `(204,51,0)`, the same
dominant orange/red colors in the original indexed toybox portrait. Resolving
the bytes through the background DIB palette—either directly or shifted by
ten—gives the wrong gameplay colors. The generated-cube mapping is recorded in
the asset manifest and covered at cube boundaries by `sprite_decode_test.py`.

## Tournament, demonstrations, and scores

Segments 7 and 15 recover the encoded tournament starts (`101`, `413`, and
`1025`), three lives, bonus levels divisible by three below level 37, the
level multiplier, perfect-game doubling, and score formula
`(human-computer)*multiplier`. The native tournament uses each numbered level's
roster and trailing AI digit, restores the human box after bonus overrides, and
supports the original champion-name flow.

The score table at DS:`6a72` has seven 18-byte records: a 16-bit score followed
by a 16-byte name. Routine `n1_scores` maps names to even placeholders
`#00,#02,...` and right-aligned scores to odd placeholders. The port parses the
shipped defaults, preserves the 15-character input limit, and stores later
results independently of the immutable runtime assets.

`ANIM.DAT` declares 14 sequences and 281 non-terminator events. Each event
contains a timestamp, player, and five `(type,winding)` lane pairs. The native
startup restores the original Philips → R/GA → demonstration → Gearheads-logo
chain and schedules those records directly. Four shipped `07000` tokens are
the otherwise consistent malformed spelling of `0,7000` and are handled as a
documented source-data quirk.

The source program and its binary containers are analysis inputs only.  Runtime
code uses the converted assets under `assets/`.
