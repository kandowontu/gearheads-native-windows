GEARHEADS NATIVE WINDOWS PORT 1.0.1
==================================

This is a self-contained 64-bit Windows build. Run Gearheads.exe. You do not
need the original CD, installer, game executable, DLLs, disk images, archives,
or a separate assets folder.

System requirements
-------------------
64-bit Windows 10 or Windows 11.

First launch
------------
Gearheads.exe contains all converted runtime data. On first launch it creates
a content-addressed cache under:

  %LOCALAPPDATA%\Gearheads Native\Cache

You may delete that cache at any time; the EXE will recreate it. Champion-table
updates are stored separately under %LOCALAPPDATA%\Gearheads Native.

Controls
--------
Menus: Arrow keys, Enter or Space. Escape or Backspace goes back.
Right player: Up/Down lane, Left/Right toy, Enter or Space release.
Left player: W/S lane, A/D toy, F release.
Escape leaves a duel. Any key leaves an attract demonstration.
Alt+Enter toggles borderless fullscreen.
F9 toggles sound effects. F10 toggles music. These choices are saved.

Audio diagnostics
-----------------
If an audio device or MIDI mapper cannot be opened, the game continues without
that audio channel and writes details to:

  %LOCALAPPDATA%\Gearheads Native\audio.log

Verification
------------
Gearheads.exe is unsigned. Compare its SHA-256 digest with SHA256SUMS.txt if
Windows displays a SmartScreen warning.

See CREDITS.txt, RELEASE-NOTES.txt, and THIRD-PARTY-NOTICES.txt for the complete
release record.
