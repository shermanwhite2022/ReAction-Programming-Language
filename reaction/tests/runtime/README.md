# Run time tests (development scripts)

Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026

These are the scripts used while developing `rc`. Each one compiles a small program with `rc`,
assembles it with `asm6502`, and runs the result in a Python 6502 emulator, checking registers,
memory, screen output or Atari CIO calls (`iorun.py` simulates the Atari's CIO).

They are kept for reference, but they do **not** run from a fresh checkout yet:

- they import `fptest` (the 6502 emulator, `from fptest import *`), which is not in this repository;
- they have the paths of the original development machine written in (`/home/claude/...`,
  `/tmp/t6`, `/tmp/t7`).

To revive them: add the emulator module, replace the hard coded paths with the locations of
`reaction/rc` and `asm6502/asm6502`, and make the scratch directories configurable.

What does run from a fresh checkout is `make check` in `reaction/` (see `../audit/run.py`).
