# Gearheads Native Windows Port 1.0.0

Release date: 2026-08-14

This is the first complete native Windows release.

## Release highlights

- One self-contained 64-bit Windows executable with all converted runtime data
  embedded; no original CD, VHD, installer, EXE, DLL, archive, or external
  asset folder is required.
- Borderless fullscreen toggled with Alt+Enter, with aspect-ratio-preserving
  letterboxing.
- Native implementations of the game screens, two-player duels, computer
  opponents, tournament progression, champion persistence, attract mode,
  physics, obstacles, powerups, and per-toy behavior.
- Correct gameplay sprite palette, upright sprite orientation, recovered sprite
  origins, and individual launch timing behavior.
- Double-buffered presentation and GDI synchronization to eliminate the
  reported black flicker.
- Embedded 1.0.0 Windows version information, modern application manifest, and
  a PE import audit requiring only Windows system libraries.
- Reproducible asset, data, gameplay, packaging, and smoke-test coverage.

## Runtime notes

The EXE materializes its embedded converted data under
`%LOCALAPPDATA%\Gearheads Native\Cache` on first launch. This cache is not an
installation dependency: deleting it causes the same EXE to recreate it.
Champion data is kept separately so clearing the cache does not erase scores.

The executable is unsigned. Verify it with the SHA-256 values supplied in the
release before bypassing any SmartScreen warning.
