# ReAction! compiler (`rc`)

Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026

ReAction! is an extended, object-oriented [Action!](https://en.wikipedia.org/wiki/Action!_(programming_language))
that compiles on a Linux machine to 6502 assembly for the Atari 800, Commodore 64, Apple II
(DOS 3.3) and Apple II with ProDOS. The assembly goes to [`asm6502`](../asm6502), which
produces the executable. ReAction! is not affiliated with the authors or the owners of Action!.

Added to Action!: `CASE`/`ESAC`, classes (inheritance with `EXTENDS`, virtual methods,
constructors, `NEW`), `FLOAT` with math functions, fixed-size array fields in records,
`TRY`/`CATCH`/`YRT`, `STRING` and `ZTRING`, `**`, whole-record assignment, `SIZEOF`,
`SYSMEM`, `INCLUDE`/`SET`/`DEFINE`, and an I/O library (CIO on the Atari; C64 and Apple II
approximations). There is no recursion: every variable has a fixed place in memory.
The compiler removes unused routines and variables, and variables that start as zero are not
stored in the program file.

Full documentation: [`manual/ReAction-manual.pdf`](manual/ReAction-manual.pdf).
Compatibility with Action! 3.6: [`ACTION_COMPAT.md`](ACTION_COMPAT.md).

## Build and run

    make                  # needs gcc, yacc (or bison), flex; builds ./rc (objects go in build/)
    make check            # compiles every program in examples/ and tests/audit/ and assembles it

    ./rc prog.act -o prog.a
    ../asm6502/asm6502 -t atari800 -o prog.xex prog.a      # also c64, appleii, prodos

`rc [-d] [-k] [-v] [-Z] [-O0] [-S stack] [-I dir] [-o prog.a] prog.act`. Common options:
`-S addr` sets the run time stack top, `-k` keeps unused code, `-Z` stores zero-initialized
data in the file, `-v` reports what dead code removal dropped, `-I dir` adds an `INCLUDE`
directory. A compile error prints a line starting `ERR!` and exits non-zero.
See chapter 2 of the manual for all options and the memory map for each machine.

`rc` finds the library in `includes/` next to the `rc` binary, so keep them together (`make install`
from the top of the repository does this). The assembly that `rc` writes includes
`includes/*.inc`; with dead code removal (the default) the needed parts are copied in, and
`asm6502` needs `-I <directory containing includes/>` only for `rc -k` output.

## Layout

    src/          compiler sources: reaction.l, reaction.y, emit.c (code generator),
                  peephole.c (optimizer), prune.c (dead code removal), storage.c, ...
    includes/     the run time library: IO.ACT and friends (ReAction!), *.inc (assembler)
    examples/     example programs
    tests/audit/  programs that must compile; tests/audit/errors/ must be rejected
    tests/runtime/  development scripts that run programs on an emulated 6502 (see its README)
    manual/       the manual (PDF)
    contrib/      user contributions
