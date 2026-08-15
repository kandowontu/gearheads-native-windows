# Credits

## Original Gearheads staff

The following names are transcribed from the original game's shipped Staff
screen and retained in its displayed order. The screen does not assign roles,
so this project does not invent them.

- Ephraim Cohen
- Daniel Reznick
- Susan Brand
- Frank Lantz
- Mark Nilsen
- Katie Haser
- Brian Loube
- David Zung
- Lynn Keller
- Stacy Koumbis
- Michael Sweet
- Eric Zimmerman
- Jason Strougo
- Karen Sideman
- Kyle Willberg
- Henry Kaufman
- Mark Voelpel
- David Barosin

The shipped presentation identifies Philips Media and R/GA Interactive. Their
names and marks are acknowledged as part of the historical record; this port
is not affiliated with or endorsed by either organization.

## Native Windows port

- Project direction, preservation media, play testing, and acceptance:
  the project maintainer
- Binary and resource-format analysis, deterministic asset conversion, native
  engine implementation, regression tests, documentation, and release
  packaging: developed in collaboration with OpenAI Codex

The native implementation uses Windows GDI, Windows Imaging Component, COM,
and Windows multimedia APIs. Its reproducible build uses CMake, Ninja, Python,
and a C++20 Windows toolchain. No third-party runtime DLL is bundled.

## Preservation acknowledgement

This port was possible because a working installation and CD image survived
long enough for their program behavior and data formats to be studied. The
source repository keeps the raw media separate and documents the conversion so
the resulting runtime can be verified independently.

Thank you to the original staff for a distinctive game worth preserving.
