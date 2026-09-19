# Gearheads Native Windows Port 1.1.2

Release date: 2026-09-19

Version 1.1.2 adds native XInput controller support to menus and gameplay. It
retains the disassembly-backed sprite geometry, timing, match rules,
configurable keyboard controls, cheat menu, selected-toy HUD, audio repairs,
and self-contained runtime from version 1.1.1.

## XInput controllers

- Added hot-plug polling for up to four XInput devices and active gameplay
  support for the first two connected controllers.
- Added D-pad and left-stick navigation for menus, lanes, and toy selection.
- Added LB/RB as alternate previous/next toy controls.
- Added A for menu confirmation and toy release, Start for menu confirmation,
  and B or Back for returning from menus and leaving a duel.
- Assigned controllers to human-controlled sides from left to right. Controller
  1 therefore controls the right-side human in one-player games; controllers 1
  and 2 control the left and right sides in two-player games.
- Used edge-triggered button and stick input so held controls do not repeat
  releases or skip multiple menu entries.
- Suspended controller polling while the game is unfocused to prevent
  background input.

## Integration and compatibility

- Kept configurable keyboard bindings fully independent from the fixed
  controller layout.
- Added the controller layout to the in-game Controls screen, README, and
  packaged instructions.
- Loaded `xinput1_4.dll`, `xinput1_3.dll`, or `xinput9_1_0.dll`
  dynamically from the Windows system directory. The game still starts when
  no controller or XInput runtime is available and gains no new redistributable
  dependency.

## Packaging and verification

- The release remains one self-contained 64-bit `Gearheads.exe`; it requires
  no original CD, VHD, installer, executable, DLL, archive, or adjacent assets.
- Expanded the automated suite from 30 to 31 tests with controller mapping,
  analog dead-zone, edge-detection, and player-assignment coverage.
- Revalidated embedded assets, sound references and loops, keyboard controls,
  fullscreen toggling, and the native PE import table.
- The executable is unsigned. Verify it with the supplied SHA-256 values before
  bypassing any SmartScreen warning.
