# 6502 tools

Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026

Tools for programming 6502 retro machines (Atari 800, Commodore 64, Apple II) from a modern
Linux machine:

| Tool | What it is | Directory |
|---|---|---|
| **asm6502** | A 6502 macro assembler with conditional assembly, includes, listings, and output for the Atari 800 (`.xex`), C64 (`.prg`), Apple II DOS 3.3 (`.bin`) and ProDOS, plus D64/ATR/DOS 3.3/ProDOS disk images. | [`asm6502/`](asm6502) |
| **ReAction! (`rc`)** | A cross compiler for ReAction!, an extended, object-oriented Action!: classes, `CASE`, floating point, `TRY`/`CATCH`, an I/O library, and dead code removal. It writes `asm6502` source. | [`reaction/`](reaction) |

    prog.act --rc--> prog.a --asm6502--> prog.xex / .prg / .bin / disk image

Programs written for Action! on the Atari should compile unchanged, apart from the differences
listed in the ReAction! manual. See [`docs/how-it-fits-together.md`](docs/how-it-fits-together.md).

## Build

You need `gcc`, `make`, `bison` (3.0+), `flex`, and `yacc` (bison's `-y` mode is fine).
`python3` is needed for the tests.

    make              # builds asm6502/asm6502 and reaction/rc
    make test         # assembler test suite, then compile-and-assemble checks for ReAction!
    sudo make install # PREFIX=/usr/local by default; also PREFIX=... and DESTDIR=...

Each tool also builds on its own: `make -C asm6502`, `make -C reaction`.

## Try it

    cd reaction
    ./rc examples/boing.act -o boing.a
    ../asm6502/asm6502 -t atari800 -o boing.xex boing.a
    atari800 boing.xex            # needs an Atari OS ROM (see below)

## Layout

    asm6502/        the assembler
      src/          sources (C, flex, bison)
      tests/        test suite (make test)
      manual/       PDF manual
      contrib/      user contributions
    reaction/       the ReAction! compiler
      src/          sources (C, flex, yacc)
      includes/     the run time library (.ACT in ReAction!, .inc in assembler)
      examples/     example programs (Atari: Boing-style demo, Breakout, Lander ...)
      tests/        compile checks (make check); run time scripts, see tests/runtime/README.md
      manual/       PDF manual
      contrib/      user contributions
      ACTION_COMPAT.md   audit of compatibility with Action! 3.6
    docs/           notes on how the tools fit together

## Status and notes

- The ReAction! manual says its programs were tested on an emulated 6502 with stand-ins for each
  machine's OS and hardware, and not yet on real machines. Treat timing, sound and graphics
  details as unconfirmed.
- Atari OS ROM images are copyrighted and are not included. To run programs in the `atari800`
  emulator, use a real OS ROM (the emulator's built-in replacement OS can misbehave).
- Generated files (`build/`, parser and lexer output, `.xex`/`.prg`/disk images) are not tracked.

## Contributing

Put examples, libraries and helper scripts under `asm6502/contrib/` or `reaction/contrib/`
(each has a README with guidelines).

## License

<!-- TODO: choose a license and add a LICENSE file at the top of the repository. -->
