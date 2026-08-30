# Gearheads Native Windows Port 1.1.1

Release date: 2026-08-30

Version 1.1.1 is a disassembly-backed gameplay-parity update. It corrects
sprite anchoring, collision geometry, erratic-toy timing, long-frame handling,
and the original scoreless-match deadline while preserving the configurable
controls, cheat menu, selected-toy HUD, audio repairs, and self-contained
runtime introduced in earlier releases.

## Rendering and collision parity

- Anchored gameplay toys, powerups, and obstacles with every frame's recovered
  signed XR origin instead of geometrically centering each bitmap.
- Rebuilt forward and mirrored collision rectangles from the same origin
  anchor used for drawing.
- Reproduced the original `(width-1)`/`(height-1)` extents and Win16
  `MulDiv` rounding.
- Kept the toybox preview bitmap-centered, matching its separate original
  rendering path.

## Timing and match-rule parity

- Restored the original 550 ms maximum elapsed-frame step.
- Changed erratic toys from a guaranteed turn every ten ticks to the recovered
  independent `Random()%10 == 0` check on every simulation tick.
- Restored the 300-second scoreless-match deadline and reset it after scoring.
- On deadline expiry, remove arrows, cracks, teleporters, glue, rocks, bugs,
  blocks, and walls while deliberately retaining mud and oil.
- Raise scores below 19 to 19 so the original 21-point, win-by-two finish
  proceeds as sudden death.

## Cheat menu

- Added Infinite Match Clock, which disables the restored scoreless-match
  deadline for the current session.
- Retained live ON/OFF state, Reset All Cheats, and default-off startup
  behavior.

## Packaging and verification

- The release remains one self-contained 64-bit `Gearheads.exe`; it requires
  no original CD, VHD, installer, executable, DLL, archive, or adjacent assets.
- Expanded the automated suite from 29 to 30 tests with exact sprite-origin,
  mirrored collision, sudden-death, erratic-timing, and cheat coverage.
- Revalidated embedded assets, sound references and loops, configurable
  controls, fullscreen toggling, and the native PE import table.
- The executable is unsigned. Verify it with the supplied SHA-256 values before
  bypassing any SmartScreen warning.
