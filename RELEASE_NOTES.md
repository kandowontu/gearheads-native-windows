# Gearheads Native Windows Port 1.0.1

Release date: 2026-08-20

This maintenance release repairs the native audio path and restores the
audited per-toy behavior, interaction, animation, and callback timing while
preserving the self-contained Windows executable.

## Audio repairs

- Replaced the single-channel `PlaySound` path with a 64-voice DirectSound
  mixer, so launches, impacts, abilities, scoring, and interface cues can
  overlap.
- Replaced the unsupported MCI `repeat` command with notified playback and an
  explicit restart at track completion. Legacy MCI path handling now uses a
  compatible short path for the content-addressed cache.
- Wired cue selection to the recovered script and `[sound]` tables: light,
  medium, and heavy collisions; both scoring sides; all crack phases;
  teleporter entry and exit; selection, countdown, go, logo, menu, perfect,
  tournament score, game-over, win, and loss cues are now reachable.
- Corrected result audio so one-player and tournament losses no longer play the
  win sting. Frontend music is restored after a duel and level music remains
  selected from the level's recovered track list.
- Added F9 sound-effects and F10 music toggles with persistent settings and
  on-screen confirmation.
- Added `%LOCALAPPDATA%\Gearheads Native\audio.log` diagnostics and graceful
  fallback when an effects device or MIDI mapper is unavailable.
- Added validation for all 47 PCM effects, all 19 MIDI references, recovered
  cue mappings, and the DirectSound system-library import.

## Existing 1.0 guarantees

- One self-contained 64-bit `Gearheads.exe`; no original CD, VHD, installer,
  executable, DLL, archive, or external asset folder is required.
- Alt+Enter borderless fullscreen with 4:3 letterboxing.
- Correct gameplay sprite palette and orientation, per-toy launch timing, and
  synchronized double-buffered presentation without black-frame flicker.

## Gameplay behavior repairs

- Separated each object's behavior byte, collision layer, animation state,
  animation clock, and attachment pointer so contacts cannot corrupt ability
  timers or animation playback.
- Ported the distinct `+60` contact filters, `+64` contact effects, `+68` tick
  callbacks, and Bomby expiry callback for all twelve selectable toys plus
  Small Fry and Rocket.
- Restored directional animation-state selection and negative SCRIPT frame
  aliases, with animation clocks reset only when the resolved state changes.
- Restored same-tick Bomby chain blasts and Krush reversals by running all toy
  callbacks before the common position pass.
- Restored Clucketta's collision/rest/egg cycle, delayed Small Fry hatching,
  Handy attachment offsets, persistent Orbit direction, and Rocket's docked
  powerup/release/warning lifecycle.
- Added executable tests for animation mapping and Presto/Krush phase timing;
  the complete 26-test suite also revalidates embedded assets, audio,
  fullscreen switching, and the self-contained PE import surface.

The executable is unsigned. Verify it with the supplied SHA-256 values before
bypassing any SmartScreen warning.
