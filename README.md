# Gearheads Native Windows Port 1.0.1

Gearheads Native is a preservation-oriented, native Windows reimplementation
of the 1996 toy-battle game. Version 1.0.1 runs without the original CD, disk
image, installer, executable, DLLs, or separately supplied game assets.

The release is a single 64-bit `Gearheads.exe`. Its verified converted runtime
data is embedded as a Windows resource. On first launch the executable
materializes that data into a content-addressed cache under
`%LOCALAPPDATA%\Gearheads Native\Cache`; the cache can be deleted safely and is
recreated from the EXE. Champion-table updates are stored separately under
`%LOCALAPPDATA%\Gearheads Native`.

## Download and run

1. Extract the v1.0.1 release ZIP, or copy `Gearheads.exe` by itself.
2. Run `Gearheads.exe`. No installation or compatibility mode is required.
3. If Windows SmartScreen warns about the unsigned executable, inspect its
   SHA-256 value against `SHA256SUMS.txt` before choosing to run it.

The supported release target is 64-bit Windows 10 or Windows 11. MIDI playback
uses Windows' installed MIDI mapper, so its sound can vary by system. Sound
effects use the Windows DirectSound mixer and can overlap instead of cutting
one another off. Audio initialization and device errors are recorded in
`%LOCALAPPDATA%\Gearheads Native\audio.log`; an unavailable audio device does
not prevent the game from running.

## Controls

- Menus: Arrow keys select; Enter or Space confirms; Escape or Backspace goes
  back.
- Right player, and the human in one-player games: Up/Down selects a lane,
  Left/Right selects a toy, and Enter or Space releases it.
- Left player: W/S selects a lane, A/D selects a toy, and F releases it.
- Escape leaves a duel. Any key leaves an attract-mode demonstration.
- Alt+Enter toggles borderless fullscreen. The original 4:3 picture is scaled
  with black letterboxing rather than distorted.
- F9 toggles sound effects; F10 toggles music. Both choices are saved under
  `%LOCALAPPDATA%\Gearheads Native`, and an on-screen notice confirms changes.

## What version 1.0.1 preserves

The native engine reads all 35 recovered screen sections, 105 level sections,
34 object scripts, 51 executable defaults, and 14 timed `ANIM.DAT`
demonstrations. Its embedded set contains 689 converted sprites, 17 boards, 11
UI images, 47 sounds, 19 MIDI tracks, and the original game font.

Gameplay runs at the recovered 55 ms simulation step. The port implements the
original fixed-point movement, winding decay, collision rectangles,
mass-weighted response, friction and ice, toy abilities, powerups, board
surfaces, teleporters, moving obstacles, match rules, toybox selection,
computer players, 50-level tournament, bonuses, lives, score multipliers,
champion table, and scripted attract mode.

The XR sprite converter uses the separately recovered gameplay palette rather
than the palette embedded in the toybox and background DIBs. It converts the
original bottom-to-top scanlines to top-to-bottom PNG rows and preserves every
sprite's signed origin. Launch gauges and computer release behavior use the
individual timing values recovered from the game.

## Build from source

Prerequisites are CMake 3.20 or later, Ninja, Python 3, and a Windows C++20
toolchain with a resource compiler. The tested GNU build statically links the
compiler runtimes.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build -C Release --output-on-failure
```

The build creates `dist\Gearheads.exe`. `tools\build_embedded_assets.py`
deterministically packs the converted `assets` tree into the PE resource; the
finished EXE does not search for an adjacent asset folder.

To produce the local v1.0.1 package:

```powershell
python tools\package_release.py `
  --exe dist\Gearheads.exe `
  --output release `
  --version 1.0.1
```

## Preservation and reverse engineering

The preservation inputs are deliberately excluded from Git. Given a full
installation extracted from the supplied VHD, the deterministic conversion is:

```powershell
python tools\extract_assets.py `
  --game-root preservation\extracted\vhd\GEARHEAD `
  --windows-root preservation\extracted\vhd\WINDOWS `
  --output assets
python tools\verify_assets.py assets
```

`assets\manifest.json` records every converted file and SHA-256 digest. The
verifier rejects missing, extra, modified, or duplicate files and rejects
original/container formats including EXE, DLL, ISO, VHD, AR, and BMP.

The recovered executable analysis can be reproduced with:

```powershell
python tools\ne_analyze.py preservation\extracted\vhd\GEARHEAD\GEAR_EN.EXE `
  --json analysis\generated\gear_en.ne.json `
  --extract-resources analysis\generated\gear_en.resources `
  --disassemble analysis\generated\gear_en.segments
```

See [docs/formats.md](docs/formats.md) for recovered formats and
[docs/reverse-engineering.md](docs/reverse-engineering.md) for the executable
map and behavioral findings.

## Verification

The test suite validates recovered data counts, every WAV and MIDI reference,
all WAV headers and sample payloads, all generated asset digests, XR palette
and scanline conversion, embedded-pack extraction, fixed-step physics, match
rules, dispatch tables, surfaces, audio cue mappings, powerups, dynamic obstacles,
tournament behavior, all 281 attract events, champion persistence, fullscreen
switching, and the final PE import table. The import audit permits only Windows
system libraries; MinGW does not need to be installed on the target PC.

The renderer uses persistent logical and presentation back buffers and
explicitly synchronizes GDI with direct DIB writes to prevent black-frame
flicker.

## Credits and status

See [CREDITS.md](CREDITS.md) for the complete recovered original staff list and
native-port acknowledgements, [RELEASE_NOTES.md](RELEASE_NOTES.md) for v1.0.1
details, and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for provenance and
rights information.

This is an unofficial preservation project. It is not affiliated with or
endorsed by the original developers, publisher, or current rights holders.
