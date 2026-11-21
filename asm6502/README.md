# asm6502 - 6502 macro assembler

Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026

C + flex/bison. Written as the back end of the ReAction! compiler (`../reaction`), and usable on its own for Apple II, C64 and Atari 800 programs. Features: symbol table / two-pass assembly, macros (with named parameters), conditional assembly, `.include`, a listing file, expressions with shifts and modulo, string escapes, default load addresses, and output files for the targets (including D64, ATR, DOS 3.3 and ProDOS disk images). The full manual is `manual/asm6502-manual.pdf`.

## Build and run
    make              # needs gcc, bison (3.0+), flex; builds ./asm6502 (objects go in build/)
    make test         # python3 tests/run_tests.py
    ./asm6502 [-t target] [-o out] [-D name[=value]] [-I dir] [-l file] [-n name] [-p] [-s] [-d] source.asm

- `-t target` is `c64`, `appleii`, `atari800` or `prodos` (aliases `apple2`, `atari`). See "Output formats". The same thing can be set in the source with `.target name`; the command line wins.
- `-o file` names the output file. With a target and no `-o` the file is named after the source (`game.s` gives `game.prg`, `game.bin`, `game.xex` or, for `prodos`, the disk image `game.po`). With no target and no `-o` a hex dump is printed. If the name ends in `.d64`, `.atr`, `.dsk` (or `.do`) or `.po` the output is a disk image with the program on it (see "Disk images").
- `-n name` is the name of the program on a disk image (default: the name of the source file). It is an error without a disk image output.
- `-D name[=value]` defines a symbol before assembly starts, for `.if` and `.ifdef` (`-DDEBUG` is 1; the value is decimal, `$hex` or `0xhex`; `-D name` also works). It can be used more than once.
- `-I dir` adds a directory where `.include` looks for files (see below). It can be used more than once.
- `-l file` writes a listing (see "Listing file").
- `-p` with no target: put the 2 byte load address in front of the bytes. Ignored when there is a target.
- `-s` prints the symbol table. `-d` is debug mode.
- Errors are reported as `file:line: error: ...` (plus `(in macro name)` inside a macro), and nothing is written if there are any.

## Output formats
| target | file | contents |
|---|---|---|
| `c64` | `.prg` | load address (2 bytes, low first), then the program |
| `appleii` | `.bin` | DOS 3.3 binary: load address (2), length (2), then the program |
| `atari800` | `.xex` | Atari DOS load file: `ff ff`, then for each block of memory that was written: start (2), end (2), data. `.run addr` adds a segment that sets RUNAD ($02e0) so the program starts. Without `.run` the file loads but doesn't start, and a note is printed. |
| `prodos` | `.po` | A ProDOS disk image (see below). With any other output name the file is just the program's bytes, with no header: ProDOS keeps the type (BIN) and load address in the disk directory, not in the file. |

- For `c64` and `appleii` the program is the bytes from the lowest to the highest address written, with any gap filled with zeros. For `atari800` each block is its own segment, so gaps cost nothing. `.alloc` doesn't write anything, `.bzero`/`.wzero` do.
- Addresses must be 16 bit for all three targets (the assembler itself allows up to the `.mem` size, 1M by default).
- `.run` is only used for `atari800`. It can be a label defined later.
- **Default load address.** If the source has no `.org`, the program starts at an address that is free on that machine and needs no loader: `c64` $C000 (the 4K above BASIC, `SYS 49152`), `appleii` and `prodos` $6000 (above the hi-res pages, below DOS), `atari800` $2000 (above DOS and the screen). With no target it is $0000 as before. `.target` before any code (or `-t`) picks the default for that target; once code has been assembled, or after any `.org` (even `.org $0000`), the address is left alone. The command line `-t` decides over `.target`, as always.

## Disk images
    asm6502 -t c64      -o game.d64 game.s      # Commodore 1541, 35 tracks
    asm6502 -t atari800 -o game.atr game.s      # Atari DOS 2.0S, single density
    asm6502 -t appleii  -o game.dsk game.s      # Apple DOS 3.3 (DOS sector order; .do is the same)
    asm6502 -t prodos   -o game.po  game.s      # ProDOS (block order)
    asm6502 -t c64 -o game.d64 -n "MY GAME" game.s

- The image is a new, empty disk with the program as its only file, so it can be put in a drive or an emulator, or the file can be copied off with a disk utility. The extension has to fit the target (`-t c64` with `.atr` is an error), and a disk image needs a target.
- What the file is: on a D64 a PRG (load address, then the program); on an ATR the `.xex` load file (with `.run`, if you gave it); on a DOS 3.3 disk a binary file (type B: address, length, program); on a ProDOS disk a BIN file ($06) with the load address as its aux type and the program's bytes as the contents.
- Names. Without `-n` the name comes from the source file (`1st-try.s` is `1ST-TRY` on a D64, `1ST.TRY` on DOS 3.3, `P1ST.TRY` on ProDOS, `P1ST_TRY.XEX` on an Atari disk), made upper case, with characters the file system doesn't allow replaced and cut to fit. With `-n` the name is checked and refused if it can't be used as it is (it is made upper case): D64 1-16 characters (`$20`-`$5f`); DOS 3.3 1-30 characters, no comma or colon; ProDOS up to 15 letters, digits and periods, first a letter; Atari DOS 2 `NAME.EXT`, 8 + 3 letters, digits and `_`, first a letter (no extension if you give none).
- The disks are **not bootable**: there are no boot sectors, no DOS on them (DOS 3.3 tracks 0-2 are kept free for it), no ProDOS system files. Atari's `AUTORUN.SYS` convention needs a DOS on the disk, so `-n AUTORUN.SYS` only makes the name right. The disk names are `ASM6502`; all dates are zero.
- Layout details: D64 allocates from track 1 sector 0 with interleave 10 and never uses track 18; ATR files take the lowest free sectors (from 4) and the VTOC and directory are at 360-368 as in DOS 2.0S; DOS 3.3 files go on tracks 3-16 and 18-34 (sectors 15 down to 0) with a track/sector list; ProDOS files are a seedling (up to 512 bytes) or a sapling (an index block and the data blocks, up to 256 of them).
- Everything a 64K program can be fits on all four disks, so "does not fit" is only a safety check. The images are checked by independent readers in `tests/images.py` (they follow the directories and check every free-sector map and link chain), not by a real DOS, so loading one into a real drive or emulator is still worth doing once.

## Source syntax
- A name in the first column is a label (optional trailing colon). Instructions and directives must be indented. Comments start with `;` or `//`.
- Names, mnemonics and directives are not case sensitive. Mnemonics (`lda`, `bit`, ...) and the single letters `a`, `x`, `y` can't be used as symbol names.
- Numbers: `255`, `$ff`, `0xff`, `&377` (octal), `%11111111`, and a character in single quotes: `'A'` is 65 (see below).
- Expressions use `+ - * / %`, the shifts `<< >>`, the bitwise operators `& | ^`, comparisons, `&& || !`, byte operators, parentheses, and `*` for the address of the current statement. From tightest to loosest: `!`, `<expr` (low byte), `>expr` (high byte), unary `-` and `+`, `* / %`, `+ -`, `<< >>`, `&`, `^`, `|`, `< > <= >=`, `== !=`, `&&`, `||`. Comparisons and logic give 1 or 0. `<` and `>` bind tighter than `+`, so `<a+1` is `(<a)+1` (and `<a>>4` is `(<a)>>4`). The bitwise operators bind tighter than comparisons (as in Python, not C): `a & 1 == 1` is `(a & 1) == 1`.
- `%` is modulo (the sign follows the left side, as in C: `-7 % 3` is -1), and a zero divisor is an error as for `/`. The shifts work on 32 bit values: `>>` is arithmetic (it keeps the sign), and the count has to be 0 to 31. Shifts bind looser than `+`, so `1+1<<2` is 8 and `1<<2+1` is 8; and tighter than `&`, so `3&1<<1` is `3&(1<<1)`. `%` is also the binary prefix (`%101`), told apart the same way as `&`: after a value (a name, number, character or `)`) it is modulo, so `a %2` and `7%%11` are modulo, and where a value is expected `%101` is binary.
- `*` is the program counter at the start of the statement or item it is in: `bne *` loops on itself, `.word *+2` is the address after the word, `.org *+16` skips ahead. It is never a forward reference. Where a value is expected `*` is the address; after a value it is multiplication (`* * 2` is the address times 2).
- Parentheses group in expressions (`(a+1)*2`, `#(1+2)*3`, `.if (x&3)==1`). The one place they don't is the **start of an instruction operand**: there `(` always begins an indirect operand (`lda (z),y`, `lda (z,x)`, `jmp (v)`), so `lda (1+2)*3` is a syntax error. Write `lda +(1+2)*3` (unary plus) or `lda 0+(1+2)*3` for that, or put the group inside: `lda ((1+2)*3),y`. After an instruction, `#(...)` is fine.
- `&` is octal where a value is expected (`.byte &17` is 15) and AND after a value (`.byte 5&17`, `.byte (a)&17`, `.byte v &17` are all 5 AND 17). `&&` is always logical AND. A name, a number, a character or a `)` counts as a value; `*` does not, so write `* & 3` with a space (`*&3` reads `&3` as octal).
- **Escapes** in strings and characters: `\n` (10), `\r` (13), `\t` (9), `\0` (0), `\a` (7), `\e` (27), `\\`, `\'`, `\"`, and `\xHH` (two hex digits, any case). Examples: `.byte "Hello\r\0"`, `lda #'\''`, `lda #'\x9b'` (the Atari's end of line), `.string "say \"hi\""`. Anything else after a backslash (`\q`, `\b`, `\f`, `\v`), a `\x` without two hex digits, or a backslash at the very end of a string, is an error. A `\0` or `\x00` inside a string is kept: strings are counted by length, so `.strmode 1` gives the full length. `\x80`-`\xff` produce those byte values. Escapes work in macro arguments too (an escaped quote or comma does not end the argument). **This changes old behaviour:** a backslash is now an escape character, so `"a\b"` is an error where it used to be three characters and `'\'` is no longer a character (write `'\\'`).
- A single character in single quotes, like `'A'` or `' '`, is a number (its ASCII code) wherever a number is expected: `lda #'A'`, `.byte 'a'-32`, `.if c == ';'`. Double quotes, and single quotes around more than one character, are strings as before. `.string 'A'` and `.title 'A'` still work. This changes one old behaviour: `.byte 'A'` used to store the string `'A'` followed by its terminating zero, and is now the single byte 65 (use `"A"` for the string).
- Implemented directives: `.org/.origin/.start`, `.byte/.data`, `.word/.int` (16 bit, low byte first), `.string`, `.strmode n` (0 zero terminated, 1 length byte first, 2 literal; also spelled `.smode`), `.alloc n`, `.bzero n`, `.wzero n`, `.const name value`, `.map name @ addr`, `.mem size`, `.title "text"`, `.target name`, `.run addr` (also `.entry`), `.end`, `.macro/.endm`, `.include`, and the conditional directives below. Strings in `.byte`/`.word` lists are padded with zeros to the element size.
- Parsed but reports "not supported yet": `.union`, `.group`, `.custom` (the code is there, switched off), custom `.directives`, `.float`, `_sizeof()`. `.proc` is lexed but has no grammar rule.

## Conditional assembly
    .if expr            ; assembled when expr is not zero
    .elif expr          ; (any number of these) tried when every block above was false
    .else
    .endif
    .ifdef name         ; assembled when name (a symbol or a macro) is defined above this line
    .ifndef name

- The condition is worked out when the line is read, so every symbol in it has to be defined above it. A forward reference (or a typo) is an error: "forward references are not allowed in .if". `.ifdef` also only sees what is above it, so it gives the same answer in both passes (a label or macro defined later is not defined yet). It sees macro names too (not case sensitive), so `.ifndef mymacro` / `.macro mymacro ...` / `.endif` defines a macro only if there isn't one.
- The text of a block that is not assembled is thrown away by the lexer, line by line, so it can contain anything (even syntax errors or macros that don't exist). It is only scanned for `.if`/`.ifdef`/`.ifndef` (to match nesting), `.elif`, `.else` and `.endif`. Line numbers stay right.
- Blocks nest to any depth. Each block has to be closed in the file or the macro expansion where it was opened: a macro can contain a complete `.if ... .endif`, but a block can't be opened in one macro and closed in another or after it. Conditionals inside macros see the arguments after substitution (`.if @1 == 3`).
- Predefined symbols: `TARGET` is the output target chosen so far (`TARGET_NONE` 0, `TARGET_C64` 1, `TARGET_APPLEII` 2, `TARGET_ATARI800` 3, `TARGET_PRODOS` 4), so `.if TARGET == TARGET_C64` works with `-t c64` or `.target c64` (a `.target` line changes it from that line on). They and `-D` symbols are constants that can't be redefined in the source, and are not listed by `-s`.
- A label on the same line as `.if`/`.else`/`.endif` gets the address at that point, but after a skipped `.else`/`.elif` it ends up attached to the next statement (and two labels in a row are a syntax error), so put labels on their own lines.
- The parser gets only `.if expr` and `.elif expr` (to evaluate them); `.else`, `.endif`, `.ifdef`, `.ifndef` and the skipping are in `6502parse.l`. The skip state works because the parser has already read the newline at the end of the `.if` line when it runs the action, so the lexer is at the start of the next line.

## Including files
    .include "defs.inc"     ; also 'defs.inc'

- The named file is read as if its text were at that point. Everything is shared: labels, constants, macros, the program counter and string mode (an `.org` in an included file stays in force afterwards), `.target`, `.title`. `.end` in an included file ends the whole assembly.
- A relative name is looked for next to the file that contains the `.include` (so a file can include its neighbours no matter where the assembler is run from), then in each `-I` directory in the order given. An absolute name is used as it is. A directory is not a file.
- Files can include files, up to 16 deep (a file that includes itself reaches the limit and reports an error). There is no automatic guard against including a file twice: use `.ifndef`/`.const` at the top of the file (`.ifndef GUARD` / `.const GUARD 1` ... `.endif`), which works because `.ifdef` only sees what is above it.
- Conditionals and macro definitions have to be closed in the file where they start: an `.if` that is still open at the end of an included file, an `.endif` in an included file for an `.if` in the includer, and an unfinished `.macro` are errors. A macro can't contain an `.include` (a macro body that has one reports an error when the macro is called). An `.include` in a block that isn't assembled is not looked at, so the file doesn't have to exist.
- Errors give the file and line where they happen, followed by `included from file:line` for each file above it. A symbol defined twice names the file of the first definition when it's another file.
- It's done in the lexer (6502parse.l): the file gets its own flex buffer, on a stack with the file name, line number and conditional depth to go back to. The parser never sees `.include`. Both passes read every file again.
- The file name is read in full from the line, so it can't be built from a macro argument or symbol.

## Listing file
    asm6502 -l game.lst game.s

Writes the source with the address and bytes next to each line, then the symbol table. Nothing is written if there are errors.

    line  addr  bytes        source
        1                     ; listing test
        7  2000  a9 41        start	lda #'A'
        8                     	.include "sub/inc.s"
           ; ---- sub/inc.s
        1  2002  c8           inc	iny
           ; ---- end of sub/inc.s
        9  2005  ea ea        	_two

- A line that makes more than 4 bytes continues on rows with just the address and bytes. A write of more than 16 bytes (a big `.bzero`) shows the first 16 and `... N more bytes`.
- Lines in a block that was skipped are listed with no bytes. A macro call shows the bytes of the whole expansion; the macro body is not shown again. An included file is listed where its `.include` is (once for each time it is included), with the line numbers of that file.
- It is built in `list.c`: pass 2 notes every write to memory with its file and line, and the files are read again to write the listing, so they must still be there and unchanged.
- Lines with a label or `.alloc` and no bytes show no address. Use `-s` (or the symbol table at the end of the listing) for label addresses.

## Macros
    .macro wait count       ; definition: the lines up to .endm. The names after the macro name are the parameters
        ldx #@count         ; @count (or @1) is the first argument
    @loop   dex             ; any other @name is a label local to one expansion
        bne @loop
    .endm

        _wait 3             ; call: _name followed by the arguments
    lbl _wait $10           ; a label on the call line labels the first line of the expansion

- Macros must be defined before they are used. Names are not case sensitive. Macros can call other macros (up to 16 deep, so a macro that calls itself is an error).
- Arguments are plain text, separated by commas. Commas inside parentheses or quotes don't split, so `_ldi ($10),y` is two arguments, `($10)` and `y`. The text is dropped in as written, so an argument can be an expression, `#5`, `($10)`, a register, anything.
- **Named parameters.** The names go after the macro name, separated by commas (`.macro add16 dst, src`), and `@dst` in the body is replaced by the text of that argument, exactly like `@1`. The names are not case sensitive, are whole words (`@src` does not touch `@srcx`), may contain letters, digits and `_`, and have to be different. Numbers and names can be mixed (`@1` is the same as the first name). A parameter name wins over a local label of the same name. Using a parameter that wasn't passed is an error (`macro 'add16' uses @src but was called with 1 argument`); extra arguments are ignored, and so are missing ones that the body doesn't use. A macro with no names after it works as before.
- `@0` is the number of arguments passed. Using `@N` for an argument that wasn't passed is an error.
- Any `@name` that is not a parameter becomes `name__mN` (N counts expansions), so each expansion gets its own copy of the label. Avoid ordinary labels that end in `__m` and a number.
- `@` substitution is not done inside comments or strings.
- It is done in the lexer (6502parse.l): the body is stored as text, and a call pushes the expanded text as a nested input buffer, so a macro body can contain anything a line of source can. Both passes expand the same way, so labels inside macros get the same values in each.
- Error line numbers inside an expansion are the line of the call.

## How the two passes work
- Pass 0 defines every label/constant and only has to get instruction sizes right. Pass 1 generates the code. Symbols are in `symtab.c`.
- A symbol used before it is defined (in the current pass) is a forward reference. Instructions using one always get the absolute form, so sizes are the same in both passes. As a result `lda later_zp_var` with a forward reference costs one byte more than strictly needed. A label that still ends up at a different address in pass 1 is reported as a phase error.
- `.org`, `.alloc`, `.bzero`, `.wzero`, `.const`, `.mem`, `.map`, `.if` and `.elif` don't accept forward references.
- `asmOp()` in `asm.c` picks zero page vs absolute (jmp/jsr and anything without a zero page form always go absolute), works out branch offsets, and range checks operands.

## Files
The sources are in `src/`: `asm.c` opcode table + encoding/emit (and the shift/escape helpers), `symtab.c` symbols (including the predefined ones), `output.c` output files for the targets, `image.c` disk images (D64, ATR, DOS 3.3, ProDOS), `6502parse.y` grammar, `6502parse.l` lexer + macros + conditional assembly + includes, `list.c` listing file, `main.c` driver, `utils.c` helpers, `asm.h`/`asmtypes.h`/`utils.h` headers, `tests/run_tests.py` (checks all 151 official opcode/mode pairs against a map built from the opcode bit patterns, plus label, string, escape, data, macro, conditional assembly, `.include`, expression, listing, output format, disk image and error cases), `tests/images.py` (independent readers for the four disk image formats, used by the tests) and `tests/labels.s`.

## Fixed since the first snapshot
Token names now match between lexer and grammar; lexer bugs fixed (`.include` was lexed as `.int`, `.map` was missing, typos in the `.bzero` and `.endm` patterns, invalid `\s` classes, opcodes captured with leading whitespace, float value stored in the wrong union member); the opcode table order and `ldx zp,y` entry fixed; instruction length for zp,y and indirect fixed; the binary search no longer misses `adc`; `>expr` now gives the high byte; labels get the address of the instruction they label (not the address after it); statements without a trailing newline and comment-only lines parse.

## Not done yet
Unions/groups/custom types and `_sizeof` (the `.custom` code is kept but switched off), floats, `.proc`, and bootable disk images (no DOS or boot sectors are written; DOS 3.3 and ProDOS images are also only in the 140K 35 track form).

## Repository layout
`src/` sources, `tests/` test suite, `manual/` the PDF manual, `contrib/` user contributions (see its README).
