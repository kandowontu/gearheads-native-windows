# Gearheads Native Windows Port 1.1.0

Release date: 2026-08-21

Version 1.1 adds optional gameplay cheats, fully configurable keyboard
controls, and a selected-toy HUD for bonus boards when their normally fixed
rosters are expanded. It retains the audited gameplay, audio, palette,
orientation, timing, fullscreen, and self-contained-runtime repairs from
version 1.0.1.

## Configurable controls

- Replaced the static Controls page with an interactive ten-action binding
  screen covering lane selection, toy selection, and release for both sides.
- Added single-key capture with Escape cancellation and live binding labels.
- Conflicting assignments swap automatically instead of activating two
  gameplay actions at once.
- Added Reset Default Controls, including the original Enter/Space dual release
  binding for the right player.
- Saved custom bindings to `%LOCALAPPDATA%\Gearheads Native\controls.ini` and
  restored them on later launches.
- Kept menu navigation, Escape, Alt+Enter, F9, and F10 fixed so custom gameplay
  bindings cannot lock the player out of navigation, fullscreen, or audio
  controls.

## Cheat menu

- Added a hidden, session-only menu opened with Ctrl+Alt+F1 on the main menu.
  All cheat settings start disabled on every launch.
- Added Never Lose vs Computer, which blocks an AI match-winning point without
  changing ordinary scoring.
- Added Infinite Toy Wind-Up for human-owned toys.
- Added Instant Full Launch, allowing immediate releases at maximum winding
  while leaving computer timing unchanged.
- Added All Toys Everywhere, expanding both rosters to all twelve selectable
  toys even on tournament levels with fixed rosters.
- Added Powerup Party, enabling powerups on every board with faster spawn and
  effect timing.
- Added Reset All Cheats and live ON/OFF indicators.

## Bonus-level HUD

- Added compact P1/P2 selected-toy badges when a board has no original toybox
  rectangle but a cheat or future mode supplies multiple selectable toys.
- Scaled the original gameplay sprites into the badges without changing their
  recovered palette or orientation.
- Tested badge placement against all twelve original bonus boards so the HUD
  avoids their recovered wind-up gauge positions.
- Preserved the original hidden preview when a bonus level still has its normal
  forced single-toy roster.

## Packaging and verification

- The release remains one self-contained 64-bit `Gearheads.exe`; it requires no
  original CD, VHD, installer, executable, DLL, archive, or adjacent assets.
- Expanded the automated suite to 29 tests, adding cheat-rule, control-binding
  persistence/conflict, and all-bonus HUD-layout coverage while retaining the
  embedded-asset, PE-import, and fullscreen checks.
- The executable is unsigned. Verify it with the supplied SHA-256 values before
  bypassing any SmartScreen warning.
