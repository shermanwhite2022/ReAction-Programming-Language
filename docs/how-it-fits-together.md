# How the pieces fit together

    prog.act --rc--> prog.a --asm6502--> prog.xex / .prg / .bin / disk image
                (ReAction! compiler)  (6502 macro assembler)

- **rc** (in `reaction/`) is a cross compiler that runs on Linux. It reads ReAction! source
  (a superset of Action!), optimizes and prunes unused library code, and writes 6502 assembler
  source in `asm6502` syntax.
- **asm6502** (in `asm6502/`) assembles that source and writes the executable for the target
  (`-t atari800`, `c64`, `appleii`, `prodos`), or a disk image.
- The platform differences in the run time library (`reaction/includes/*.inc`) are handled with
  the assembler's conditional assembly (`.if TARGET == TARGET_C64`) and `.include`.
- `rc` finds its library in `includes/` next to the `rc` binary. The assembler source it writes
  has `.include "includes/..."` lines; with dead code removal on (the default) the used parts are
  copied in, so `asm6502` needs no `-I`. With `rc -k` the includes are kept and the assembler
  needs `-I <directory containing includes/>`.

The assembler is usable on its own; `rc` is the only thing in this repository that depends on it.
