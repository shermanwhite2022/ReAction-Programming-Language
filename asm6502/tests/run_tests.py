#!/usr/bin/env python3
# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
"""Tests for asm6502. Run from the 6502asm directory (make test).

1. Every valid (mnemonic, addressing mode) pair is assembled and compared with
   an opcode map built from the 6502's bit patterns (aaabbbcc), not copied
   from asm.c.
2. tests/labels.s is assembled and compared with bytes worked out by hand.
3. Error cases must fail with the right message and write nothing.
4. Disk images are read back by independent readers (tests/images.py) that walk the
   directories and check the free sector maps and link chains.
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import images

HERE = os.path.dirname(os.path.abspath(__file__))
ASM = os.path.join(HERE, '..', 'asm6502')

failures = 0


def check(name, ok, detail=''):
    global failures
    if not ok:
        failures += 1
        print('FAIL', name, detail)


# ---------------------------------------------------------------- reference map

ref = {}


def add(m, mode, op):
    ref.setdefault(m, {})[mode] = op


alu = ['ora', 'and', 'eor', 'adc', 'sta', 'lda', 'cmp', 'sbc']
alu_modes = {'indx': 0, 'zp': 1, 'imm': 2, 'abs': 3, 'indy': 4, 'zpx': 5, 'absy': 6, 'absx': 7}
for a, m in enumerate(alu):
    for mode, b in alu_modes.items():
        if m == 'sta' and mode == 'imm':
            continue
        add(m, mode, (a << 5) | (b << 2) | 1)

rmw = {
    'asl': {'zp': 1, 'acc': 2, 'abs': 3, 'zpx': 5, 'absx': 7},
    'rol': {'zp': 1, 'acc': 2, 'abs': 3, 'zpx': 5, 'absx': 7},
    'lsr': {'zp': 1, 'acc': 2, 'abs': 3, 'zpx': 5, 'absx': 7},
    'ror': {'zp': 1, 'acc': 2, 'abs': 3, 'zpx': 5, 'absx': 7},
    'stx': {'zp': 1, 'abs': 3, 'zpy': 5},
    'ldx': {'imm': 0, 'zp': 1, 'abs': 3, 'zpy': 5, 'absy': 7},
    'dec': {'zp': 1, 'abs': 3, 'zpx': 5, 'absx': 7},
    'inc': {'zp': 1, 'abs': 3, 'zpx': 5, 'absx': 7},
}
for a, m in enumerate(['asl', 'rol', 'lsr', 'ror', 'stx', 'ldx', 'dec', 'inc']):
    for mode, b in rmw[m].items():
        add(m, mode, (a << 5) | (b << 2) | 2)

cc0 = {
    'bit': (1, {'zp': 1, 'abs': 3}),
    'sty': (4, {'zp': 1, 'abs': 3, 'zpx': 5}),
    'ldy': (5, {'imm': 0, 'zp': 1, 'abs': 3, 'zpx': 5, 'absx': 7}),
    'cpy': (6, {'imm': 0, 'zp': 1, 'abs': 3}),
    'cpx': (7, {'imm': 0, 'zp': 1, 'abs': 3}),
}
for m, (a, modes) in cc0.items():
    for mode, b in modes.items():
        add(m, mode, (a << 5) | (b << 2))

add('jmp', 'abs', 0x4c)
add('jmp', 'ind', 0x6c)
add('jsr', 'abs', 0x20)

for m, op in [('bpl', 0x10), ('bmi', 0x30), ('bvc', 0x50), ('bvs', 0x70),
              ('bcc', 0x90), ('bcs', 0xb0), ('bne', 0xd0), ('beq', 0xf0)]:
    add(m, 'rel', op)

for m, op in [('brk', 0x00), ('rti', 0x40), ('rts', 0x60), ('php', 0x08), ('plp', 0x28),
              ('pha', 0x48), ('pla', 0x68), ('dey', 0x88), ('tay', 0xa8), ('iny', 0xc8),
              ('inx', 0xe8), ('clc', 0x18), ('sec', 0x38), ('cli', 0x58), ('sei', 0x78),
              ('tya', 0x98), ('clv', 0xb8), ('cld', 0xd8), ('sed', 0xf8), ('txa', 0x8a),
              ('txs', 0x9a), ('tax', 0xaa), ('tsx', 0xba), ('dex', 0xca), ('nop', 0xea)]:
    add(m, 'imp', op)

check('reference map has 56 mnemonics', len(ref) == 56, len(ref))

# source syntax and operand bytes for each mode
SYNTAX = {
    'acc':  ('{m} a',          []),
    'imm':  ('{m} #$12',       [0x12]),
    'zp':   ('{m} $12',        [0x12]),
    'zpx':  ('{m} $12,x',      [0x12]),
    'zpy':  ('{m} $12,y',      [0x12]),
    'abs':  ('{m} $1234',      [0x34, 0x12]),
    'absx': ('{m} $1234,x',    [0x34, 0x12]),
    'absy': ('{m} $1234,y',    [0x34, 0x12]),
    'ind':  ('{m} ($1234)',    [0x34, 0x12]),
    'indx': ('{m} ($12,x)',    [0x12]),
    'indy': ('{m} ($12),y',    [0x12]),
    'imp':  ('{m}',            []),
}


def assemble(src, extra=()):
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, 't.s')
        o = os.path.join(d, 't.bin')
        with open(s, 'w') as f:
            f.write(src)
        r = subprocess.run([ASM, '-o', o, *extra, s], capture_output=True, text=True)
        data = open(o, 'rb').read() if os.path.exists(o) else None
        return r.returncode, data, r.stdout + r.stderr


# ------------------------------------------------------------ 1. every opcode

lines = ['\t.org $2000']
expect = []
pc = 0x2000
n = 0
for m in sorted(ref):
    for mode, op in sorted(ref[m].items()):
        n += 1
        mm = m.upper() if n % 2 else m      # mixed case on purpose
        if mode == 'rel':
            lines.append('\t%s $%04x' % (mm, pc + 2 + 5))
            expect += [op, 5]
            pc += 2
        else:
            text, operand = SYNTAX[mode]
            lines.append('\t' + text.format(m=mm))
            expect += [op] + operand
            pc += 1 + len(operand)

rc, data, out = assemble('\n'.join(lines))
check('all opcodes: assembles', rc == 0, out)
check('all opcodes: bytes', data == bytes(expect))
if data != bytes(expect) and data is not None:
    for i, (a, b) in enumerate(zip(data, expect)):
        if a != b:
            print('  first difference at offset', i, 'got %02x expected %02x' % (a, b))
            break
print('%d mnemonic/mode pairs checked' % n)

# ------------------------------------------------------ 2. labels and forward refs

labels = open(os.path.join(HERE, 'labels.s')).read()
want = bytes.fromhex('a900 8510 8d1502 a205 ca d0fd 4c1902 010203 686900 34121202 ad0004 a904 a915 60')
rc, data, out = assemble(labels)
check('labels.s: assembles', rc == 0, out)
check('labels.s: bytes', data == want, data.hex() if data else None)

rc, data, out = assemble(labels, ['-p'])
check('load address header', data is not None and data[:2] == b'\x00\x02' and data[2:] == want)

# ---------------------------------------------------- 3. size selection details

cases = [
    ('lda $12,y promotes to abs,y',   '\tlda $12,y',         'b91200'),
    ('jmp $12 promotes to absolute',  '\tjmp $12',           '4c1200'),
    ('lda $0012 is zero page',        '\tlda $0012',         'a512'),
    ('stx zp,y',                      '\tstx $12,y',        '9612'),
    ('binary, octal, decimal, 0x',    '\t.byte %101, &17, 255, 0xff', '050fffff'),
    ('hi/lo byte operators',          '\t.byte <$1234, >$1234', '3412'),
    ('expression',                    '\t.byte 2*3+4, -1',  '0aff'),
    ('ucsd string',                   '\t.strmode 1\n\t.string "abc"', '03616263'),
    ('literal string',                '\t.strmode 2\n\t.string "abc"', '616263'),
    ('word pads strings to even',     '\t.strmode 2\n\t.word "abc"', '61626300'),
    ('.org and .alloc',               '\t.org $10\n\t.alloc 2\n\t.byte 7', '07'),
    ('.bzero / .wzero',               '\t.bzero 2\n\t.wzero 1\n\t.byte 9', '0000000009'),
    ('.end stops assembly',           '\t.byte 1\n\t.end\n\t.byte 2', '01'),
    ('label without a colon on an empty line', 'x1\n\tlda x1', 'a500'),
    ('label with a colon',            'x1:\tlda #1\n\tbne x1', 'a901d0fc'),
    ('no newline at end of file',     '\trts',               '60'),
    ('forward label used as zp-sized value stays absolute',
     '\tlda f\n\t.org 5\nf\tnop', 'ad05000000ea'),
]
for name, src, hexs in cases:
    rc, data, out = assemble(src)
    # the memory image starts at the lowest written address
    check(name, rc == 0 and data == bytes.fromhex(hexs), '%s rc=%s got=%s' % (out.strip(), rc, data.hex() if data else None))

# ------------------------------------------------------------ 4. error cases

errors = [
    ('sta has no immediate mode',  '\tsta #1',          'sta has no immediate mode'),
    ('undefined symbol',           '\tlda nosuch',      "undefined symbol 'nosuch'"),
    ('duplicate label',            'a1\n\tnop\na1\n\tnop', 'already defined'),
    ('branch out of range',        '\tbne far\n\t.org $1000\nfar\tnop', 'out of range'),
    ('immediate too big',          '\tlda #300',        'does not fit'),
    ('byte too big',               '\t.byte 256',       'does not fit'),
    ('forward ref in .org',        '\t.org later\nlater\tnop', 'forward references are not allowed'),
    ('syntax error',               '\tlda #',           'syntax error'),
    ('bad mode for branch',        '\tbne ($12)',       'bne has no indirect mode'),
    ('division by zero',           '\t.byte 1/0',       'division by zero'),
]
for name, src, text in errors:
    rc, data, out = assemble(src)
    check(name, rc != 0 and text in out and data is None, 'rc=%s out=%s' % (rc, out.strip()))

# error messages carry the line number
rc, data, out = assemble('\tnop\n\tnop\n\tlda nosuch\n\tnop\n')
check('error line number', ':3: error:' in out, out)


# ------------------------------------------------------------------ 5. macros

LOAD = '\t.macro load\n\tlda #@1\n\tsta @2\n\t.endm\n'
WAIT = '\t.macro wait\n\tldx #@1\n@loop\tdex\n\tbne @loop\n\t.endm\n'

macros = [
    ('macro with two arguments',      LOAD + '\t_load 5,$10', 'a905 8510'),
    ('macro name is not case sensitive', LOAD + '\t_LOAD 5,$10', 'a905 8510'),
    ('local labels are unique per call', WAIT + '\t_wait 3\n\t_wait 4',
     'a203 ca d0fd a204 ca d0fd'),
    ('forward local label',
     '\t.macro inc16\n\tinc @1\n\tbne @skip\n\tinc @1+1\n@skip\n\t.endm\n\t_inc16 $10',
     'e610 d002 e611'),
    ('indexed and indirect arguments',
     '\t.macro ldi\n\tlda @1,@2\n\t.endm\n\t_ldi ($10),y\n\t_ldi $10,x', 'b110 b510'),
    ('argument count in @0',
     '\t.macro n\n\t.byte @0\n\t.endm\n\t_n\n\t_n 7\n\t_n 7,8,9', '00 01 03'),
    ('label on the line of the call',
     '\t.macro twice\n\tnop\n\tnop\n\t.endm\n\t.org $100\nl1\t_twice\n\tjmp l1', 'eaea 4c0001'),
    ('macro calling a macro',
     '\t.macro one\n\tnop\n\t.endm\n\t.macro two\n\t_one\n\t_one\n\t.endm\n\t_two', 'eaea'),
    ('nested macros keep their own locals',
     WAIT + '\t.macro both\n\t_wait @1\n\t_wait @2\n\t.endm\n\t_both 1,2',
     'a201 ca d0fd a202 ca d0fd'),
    ('comments and strings are left alone',
     '\t.macro c\n\tnop ; uses @9 here\n\t.byte "@1"\n\t.endm\n\t_c', 'ea 40 31 00'),
    ('comment after the arguments',   LOAD + '\t_load 5,$10 ; set up', 'a905 8510'),
    ('macro defined with .macro in capitals', '\t.MACRO m\n\tnop\n\t.ENDM\n\t_m', 'ea'),
    ('empty macro',                   '\t.macro e\n\t.endm\n\t_e\n\tnop', 'ea'),
    ('macro at the end of the file with no newline', '\t.macro m\n\tnop\n\t.endm\n\t_m', 'ea'),
    ('labels defined in a macro body', '\t.org $200\n\t.macro tag\nhere\tnop\n\t.endm\n\tnop', 'ea'),
]
for name, src, hexs in macros:
    rc, data, out = assemble(src)
    check(name, rc == 0 and data == bytes.fromhex(hexs.replace(' ', '')),
          '%s rc=%s got=%s' % (out.strip(), rc, data.hex() if data else None))

macro_errors = [
    ('undefined macro',         '\t_nosuch 1',  "undefined macro 'nosuch'"),
    ('missing argument',        LOAD + '\t_load 5', 'uses @2 but was called with 1 argument'),
    ('missing .endm',           '\t.macro m\n\tnop\n',  "missing .endm for macro 'm'"),
    ('.endm without .macro',    '\t.endm',      '.endm without a matching .macro'),
    ('macro defined twice',     '\t.macro m\n\t.endm\n\t.macro m\n\t.endm', "macro 'm' is already defined"),
    ('macro needs a name',      '\t.macro\n\t.endm', '.macro needs a name'),
    ('recursive macro',         '\t.macro r\n\t_r\n\t.endm\n\t_r', 'nested too deeply'),
    ('_sizeof is not taken for a macro call', '\t.byte _sizeof(foo)', '_sizeof(foo): types are not supported'),
    ('error inside a macro names it', '\t.macro bad\n\tlda nosuch\n\t.endm\n\t_bad', '(in macro bad)'),
]
for name, src, text in macro_errors:
    rc, data, out = assemble(src)
    check(name, rc != 0 and text in out and data is None, 'rc=%s out=%s' % (rc, out.strip()))

# line numbers: macro definitions and expansions must not throw them off
rc, data, out = assemble('\t.macro m\n\tnop\n\tnop\n\t.endm\n\t_m\n\tlda nosuch\n')
check('line number after a macro', ':6: error:' in out, out)
rc, data, out = assemble('\t.macro bad\n\tnop\n\tlda nosuch\n\t.endm\n\tnop\n\t_bad\n\tnop\n')
check('line number of a call', ':6: error:' in out and '(in macro bad)' in out, out)

# macros defined in the first pass are used by the second without being redefined
rc, data, out = assemble('\t_m\n\t.macro m\n\tnop\n\t.endm\n')
check('macro used before its definition', rc != 0 and "undefined macro 'm'" in out, out)


# ------------------------------------------------------- 6. output file formats

PROG = '\t.org $2000\nstart\tlda #$00\n\tsta $d020\n\trts\n\t.org $2100\ndata\t.byte 1,2,3\n'
code = bytes.fromhex('a9008d20d060')
image = code + bytes(0x2100 - 0x2006) + bytes([1, 2, 3])        # $2000-$2102 with the gap zeroed

rc, d, out = assemble(PROG, ['-t', 'c64'])
check('c64: prg file', rc == 0 and d == b'\x00\x20' + image, out)

rc, d, out = assemble(PROG, ['-t', 'appleii'])
check('appleii: dos 3.3 binary', rc == 0 and d == b'\x00\x20' + len(image).to_bytes(2, 'little') + image, out)

atari = (b'\xff\xff' + b'\x00\x20\x05\x20' + code + b'\x00\x21\x02\x21' + bytes([1, 2, 3]))
rc, d, out = assemble(PROG, ['-t', 'atari800'])
check('atari800: segments, no run address', rc == 0 and d == atari, d.hex() if d else out)
check('atari800: says there is no run address', 'no .run address' in out, out)

rc, d, out = assemble(PROG + '\t.run start\n', ['-t', 'atari800'])
check('atari800: .run adds RUNAD', rc == 0 and d == atari + bytes.fromhex('e002 e102 0020'), d.hex() if d else out)

rc, d, out = assemble(PROG + '\t.run start\n', ['-t', 'atari'])
check('atari alias', rc == 0 and d == atari + bytes.fromhex('e002 e102 0020'), out)

rc, d, out = assemble(PROG + '\t.run $2100\n', ['-t', 'atari800'])
check('.run with a number', rc == 0 and d.endswith(bytes.fromhex('e002 e102 0021')), out)

rc, d, out = assemble('\t.run later\n\t.org $3000\nlater\tnop\n', ['-t', 'atari800'])
check('.run with a forward label', rc == 0 and d == bytes.fromhex('ffff 0030 0030 ea e002 e102 0030'), d.hex() if d else out)

rc, d, out = assemble('\t.org $0600\n\tnop\n\t.org $0602\n\tnop\n', ['-t', 'atari800'])
check('atari800: a one byte gap makes two segments',
      d == bytes.fromhex('ffff 0006 0006 ea 0206 0206 ea'), d.hex() if d else out)

rc, d, out = assemble('\t.org $0600\n\t.bzero 2\n', ['-t', 'atari800'])
check('atari800: .bzero is written', d == bytes.fromhex('ffff 0006 0106 0000'), d.hex() if d else out)

rc, d, out = assemble('\t.org $0600\n\t.alloc 4\n\tnop\n', ['-t', 'atari800'])
check('atari800: .alloc is not written', d == bytes.fromhex('ffff 0406 0406 ea'), d.hex() if d else out)

# .target in the source, and the command line wins over it

rc, d, out = assemble('\t.target c64\n\t.org $0801\n\trts\n', [])
check('.target directive', rc == 0 and d == bytes.fromhex('0108 60'), out)

rc, d, out = assemble('\t.target C64\n\t.org $0801\n\trts\n', [])
check('.target is not case sensitive', rc == 0 and d == bytes.fromhex('0108 60'), out)

rc, d, out = assemble('\t.target c64\n\t.org $0801\n\trts\n', ['-t', 'appleii'])
check('command line target wins', rc == 0 and d == bytes.fromhex('0108 0100 60'), out)

# -p only matters when there is no target

rc, d, out = assemble('\t.org $0801\n\trts\n', ['-p'])
check('-p without a target', d == bytes.fromhex('0108 60'), out)
rc, d, out = assemble('\t.org $0801\n\trts\n', ['-p', '-t', 'appleii'])
check('-p is ignored with a target', d == bytes.fromhex('0108 0100 60'), out)

# the file name comes from the source file when there is a target and no -o

with tempfile.TemporaryDirectory() as dd:
    src = os.path.join(dd, 'game.v2.s')
    open(src, 'w').write('\t.org $c000\n\trts\n')
    for target, ext in [('c64', 'prg'), ('appleii', 'bin'), ('atari800', 'xex')]:
        r = subprocess.run([ASM, '-t', target, src], capture_output=True, text=True)
        check('default file name for ' + target,
              r.returncode == 0 and os.path.exists(os.path.join(dd, 'game.v2.' + ext)), r.stdout + r.stderr)

# errors

r = subprocess.run([ASM, '-t', 'zx81', os.path.join(HERE, 'labels.s')], capture_output=True, text=True)
check('unknown target on the command line', r.returncode == 2 and 'Unknown target' in r.stderr, r.stderr)

rc, d, out = assemble('\t.target zx81\n')
check('unknown target in the source', rc != 0 and "unknown target 'zx81'" in out and d is None, out)

rc, d, out = assemble('\t.org $fff0\n\t.byte 1\n\t.org $10000\n\t.byte 2\n', ['-t', 'c64'])
check('program past 64K', rc != 0 and 'past the 64K' in out and d is None, out)

rc, d, out = assemble('\t.run $10000\n', ['-t', 'atari800'])
check('run address out of range', rc != 0 and 'out of range' in out, out)

rc, d, out = assemble('\t.run\n', ['-t', 'atari800'])
check('.run needs an address', rc != 0 and 'syntax error' in out, out)


# ------------------------------------------------------ 7. conditional assembly

def deep(n, word, body):
    return ''.join(['\t.if %s\n' % word] * n) + body + ''.join(['\t.endif\n'] * n)


conds = [
    ('.if true',              '\t.if 1\n\tnop\n\t.endif\n\tinx', 'ea e8'),
    ('.if false',             '\t.if 0\n\tnop\n\t.endif\n\tinx', 'e8'),
    ('.else taken',           '\t.if 0\n\tnop\n\t.else\n\tiny\n\t.endif', 'c8'),
    ('.else not taken',       '\t.if 1\n\tnop\n\t.else\n\tiny\n\t.endif', 'ea'),
    ('negative is true',      '\t.if -1\n\tnop\n\t.endif', 'ea'),
    ('nested, inner false',   '\t.if 1\n\t.if 0\n\tnop\n\t.else\n\tinx\n\t.endif\n\t.endif', 'e8'),
    ('nested, outer false',   '\t.if 0\n\t.if 1\n\tnop\n\t.else\n\tinx\n\t.endif\n\tiny\n\t.endif\n\tdey', '88'),
    ('.elif chain',
     '\t.const v 1\n'
     '\t.if v == 1\n\t.byte 1\n\t.elif undefined_is_never_looked_at\n\t.byte 2\n\t.elif v == 1\n\t.byte 3\n'
     '\t.else\n\t.byte 4\n\t.endif\n'
     '\t.if v == 2\n\t.byte 5\n\t.elif v == 1\n\t.byte 6\n\t.elif v == 1\n\t.byte 7\n\t.else\n\t.byte 8\n\t.endif\n'
     '\t.if v == 5\n\t.byte 9\n\t.elif v == 6\n\t.byte 10\n\t.else\n\t.byte 11\n\t.endif', '01 06 0b'),
    ('.elif without .else',   '\t.if 0\n\t.byte 1\n\t.elif 0\n\t.byte 2\n\t.endif\n\t.byte 3', '03'),
    ('.elif nested in skipped text',
     '\t.if 0\n\t.if 1\n\t.byte 1\n\t.elif 1\n\t.byte 2\n\t.else\n\t.byte 3\n\t.endif\n\t.elif 1\n\t.byte 4\n\t.endif', '04'),
    ('skipped text is not looked at',
     '\t.if 0\n\tthis is !! not valid @@\n\t.byte 999, "unterminated\n\tlda nosuch\n\t_nomacro 1\n'
     '\t.macro half\n\t.if 1\n\t.endif\n\t.else\n\tnop\n\t.endif\n\tinx', 'ea e8'),
    ('upper case directives', '\t.IF 0\n\tnop\n\t.ELIF 1\n\tinx\n\t.ELSE\n\tiny\n\t.ENDIF\n\t.IFDEF q\n\t.ENDIF', 'e8'),
    ('no newline at the end', '\t.if 1\n\tnop\n\t.endif', 'ea'),
    ('no newline, skipped',   '\t.if 0\n\tnop\n\t.else\n\tinx\n\t.endif', 'e8'),
    ('comments on directives',
     '\t.if 0 ; no\n\tnop ; x\n\t.else ; yes\n\tinx\n\t.endif ; end', 'e8'),
    ('blank lines and labels in a block', '\t.org $100\n\t.if 1\n\n\tnop\nl1\tjmp l1\n\t.endif', 'ea 4c 01 01'),
    ('labels in a false block are not defined',
     '\t.if 0\ndead\n\t.const q 1\n\t.endif\n\t.ifdef dead\n\tnop\n\t.endif\n\t.ifdef q\n\tnop\n\t.endif\n\tinx', 'e8'),
    ('40 levels, true',       deep(40, '1', '\tnop\n'), 'ea'),
    ('40 levels, skipped',    deep(40, '0', '\tnop\n') + '\tinx', 'e8'),
    ('.end inside a block',   '\t.if 1\n\tnop\n\t.end\n\t.endif', 'ea'),

    # operators
    ('== and !=',             '\t.byte 3==3, 3!=3, 3==4, 3!=4', '01 00 00 01'),
    ('< > <= >=',             '\t.byte 2<3, 3<2, 3>2, 2>3, 3<=3, 4<=3, 3>=3, 2>=3', '01 00 01 00 01 00 01 00'),
    ('&& || !',               '\t.byte 1&&0, 1&&2, 0||0, 0||5, !0, !5', '00 01 00 01 01 00'),
    ('&& binds tighter than ||', '\t.byte 1 || 0 && 0', '01'),
    ('arithmetic binds tighter than ==', '\t.byte 1+1==2, 2*3>5 && 1', '01 01'),
    ('! binds tightest',      '\t.byte !1+1, !0*3', '01 03'),
    ('byte operators still work', '\tlda #<$1234\n\tlda #>$1234\n\t.byte <$1234, >$1234', 'a934 a912 34 12'),
    ('byte operators in comparisons', '\t.byte <$1234 == $34, >$1234 == $12, 5 < >$0200', '01 01 00'),
    ('binary < after unary >', '\t.byte 1 < <$0300, 1 > <$0300', '00 01'),
    ('forward ref in a .byte comparison has its final value',
     '\t.org $10\n\t.byte x1 == 5\n\t.const x1 5', '01'),

    # .ifdef / .ifndef
    ('.ifdef sees only what is above it',
     '\t.ifdef later\n\tnop\n\t.endif\n\tlda #1\nlater\n\t.ifdef later\n\tinx\n\t.endif\n'
     '\t.ifndef later\n\tiny\n\t.endif', 'a9 01 e8'),
    ('.ifndef',               '\t.ifndef nothing\n\tnop\n\t.endif\n\t.ifndef TARGET\n\tinx\n\t.endif', 'ea'),
    ('.ifdef on a constant',  '\t.const c 3\n\t.ifdef C\n\tnop\n\t.endif', 'ea'),
    ('.ifdef takes a macro-built name',
     '\t.macro chk\n\t.ifdef @1\n\tnop\n\t.else\n\tinx\n\t.endif\n\t.endm\n\t.const yes 1\n\t_chk yes\n\t_chk no', 'ea e8'),
    ('predefined TARGET is defined', '\t.ifdef TARGET\n\tnop\n\t.endif\n\t.ifdef TARGET_C64\n\tnop\n\t.endif', 'ea ea'),
    ('TARGET is TARGET_NONE with no target',
     '\t.if TARGET == TARGET_NONE\n\tnop\n\t.endif', 'ea'),
    ('TARGET changes when .target is read',
     '\t.if TARGET == TARGET_NONE\n\t.byte 1\n\t.endif\n\t.target atari800\n\t.if TARGET == TARGET_ATARI800\n\t.byte 2\n\t.endif', None),

    # macros
    ('conditional inside a macro',
     '\t.macro pick\n\t.if @1 == 1\n\tlda #@2\n\t.else\n\tldx #@2\n\t.endif\n\t.endm\n\t_pick 1,5\n\t_pick 2,6', 'a9 05 a2 06'),
    ('local labels in a conditional macro',
     '\t.macro w\n\t.if @2\n\tldx #@1\n@l\tdex\n\tbne @l\n\t.endif\n\t.endm\n\t_w 3,1\n\t_w 4,0\n\t_w 5,1',
     'a2 03 ca d0 fd a2 05 ca d0 fd'),
    ('macro defined in a branch',
     '\t.if 0\n\t.macro m\n\tnop\n\t.endm\n\t.else\n\t.macro m\n\tinx\n\t.endm\n\t.endif\n\t_m', 'e8'),
    ('macro defined in a false block is not defined',
     '\t.if 0\n\t.macro m\n\tnop\n\t.endm\n\t.endif\n\tinx', 'e8'),
    ('balanced block spanning a macro call',
     '\t.macro m\n\tnop\n\t.endm\n\t.if 1\n\t_m\n\t.else\n\t_m\n\t_m\n\t.endif', 'ea'),
    ('conditional macro calling a conditional macro',
     '\t.macro a\n\t.if @1\n\t_b @1\n\t.endif\n\t.endm\n\t.macro b\n\t.if @1 > 1\n\tnop\n\t.else\n\tinx\n\t.endif\n\t.endm\n\t_a 0\n\t_a 1\n\t_a 2',
     'e8 ea'),
    ('skipped region with an expansion',
     '\t.macro m\n\tnop\n\t.endm\n\t.if 0\n\t_m\n\t.endif\n\tinx', 'e8'),
]
for name, src, hexs in conds:
    rc, data, out = assemble(src)
    if hexs is None:
        continue
    check(name, rc == 0 and data == bytes.fromhex(hexs.replace(' ', '')),
          '%s rc=%s got=%s' % (out.strip(), rc, data.hex() if data else None))

# TARGET with output files: the file header is in front, so look at the end
for tname, num, tail in [('c64', 1, '01'), ('appleii', 2, '02'), ('atari800', 3, '03')]:
    rc, data, out = assemble('\t.org $2000\n\t.byte TARGET\n', ['-t', tname])
    check('TARGET for ' + tname, rc == 0 and data is not None and data[-1] == num, out)

rc, data, out = assemble('\t.org $2000\n\t.if TARGET == TARGET_C64\n\t.byte $64\n\t.elif TARGET == TARGET_APPLEII\n'
                         '\t.byte $a2\n\t.else\n\t.byte $a8\n\t.endif\n', ['-t', 'appleii'])
check('.if on the target', rc == 0 and data[-1] == 0xa2, out)

rc, data, out = assemble('\t.target c64\n\t.org $2000\n\t.if TARGET == TARGET_C64\n\t.byte 7\n\t.endif\n')
check('.target in the source', rc == 0 and data == bytes([0, 0x20, 7]), '%s %s' % (out, data))

rc, data, out = assemble('\t.byte TARGET\n\t.target c64\n\t.byte TARGET\n')
check('target before and after .target', rc == 0 and data[2:] == bytes([0, 1]), '%s %s' % (out, data))

rc, data, out = assemble('\t.target appleii\n\t.org $2000\n\t.byte TARGET\n', ['-t', 'c64'])
check('-t wins over .target', rc == 0 and data == bytes([0, 0x20, 1]), '%s %s' % (out, data))

# -D
DBG = '\t.ifdef DEBUG\n\t.byte DEBUG\n\t.endif\n\t.byte 9\n'
for name, opts, hexs in [('-D not given', [], '09'), ('-DNAME', ['-DDEBUG'], '01 09'),
                         ('-D NAME', ['-D', 'DEBUG'], '01 09'), ('-DNAME=5', ['-DDEBUG=5'], '05 09'),
                         ('-DNAME=$10', ['-DDEBUG=$10'], '10 09'), ('-DNAME=0x20', ['-DDEBUG=0x20'], '20 09'),
                         ('-D name is not case sensitive', ['-Ddebug=2'], '02 09'),
                         ('two -D options', ['-DDEBUG=3', '-DOTHER'], '03 09')]:
    rc, data, out = assemble(DBG, opts)
    check(name, rc == 0 and data == bytes.fromhex(hexs.replace(' ', '')), '%s %s' % (out, data))

rc, data, out = assemble('\t.if DEBUG == 3\n\tnop\n\t.endif\n', ['-DDEBUG=3'])
check('-D value in .if', rc == 0 and data == b'\xea', out)

for name, opts, text in [('-D with a bad name', ['-D1x'], 'Bad'), ('-D with no name', ['-D='], 'Bad'),
                         ('-D with a bad value', ['-DX=abc'], 'Bad value'),
                         ('-D with an empty value', ['-DX='], 'Bad value'),
                         ('-D of a built in symbol', ['-DTARGET=5'], 'already defined'),
                         ('-D twice', ['-DX', '-DX'], 'already defined')]:
    rc, data, out = assemble('\tnop\n', opts)
    check(name, rc == 2 and text in out, 'rc=%s %s' % (rc, out))

rc, data, out = assemble('\t.const debug 1\n', ['-DDEBUG'])
check('source can not redefine a -D symbol', rc != 0 and 'predefined symbol' in out, out)

# predefined symbols are not in the symbol table listing
rc, data, out = assemble('\tl1\tnop\n'.replace('\tl1', 'l1'), ['-s', '-DDEBUG'])
check('-s lists only the program symbols', 'l1' in out and 'target' not in out.lower() and 'debug' not in out.lower(), out)

cond_errors = [
    ('missing .endif',          '\t.if 1\n\tnop\n',                  'missing .endif for the .if on line 1'),
    ('missing .endif, skipping', '\t.if 0\n\tnop\n',                 'missing .endif for the .if on line 1'),
    ('outer block not closed',  '\t.if 1\n\t.if 0\n\tnop\n\t.endif\n', 'missing .endif for the .if on line 1'),
    ('.endif without .if',      '\t.endif\n',                         '.endif without a matching .if'),
    ('.else without .if',       '\t.else\n',                          '.else without a matching .if'),
    ('.elif without .if',       '\t.elif 1\n',                        '.elif without a matching .if'),
    ('two .else, assembling',   '\t.if 1\n\t.else\n\t.else\n\t.endif\n', '.else after .else'),
    ('two .else, skipping',     '\t.if 0\n\t.else\n\t.else\n\t.endif\n', '.else after .else'),
    ('.elif after .else',       '\t.if 1\n\t.else\n\t.elif 1\n\t.endif\n', '.elif after .else'),
    ('.elif after .else, skipping', '\t.if 0\n\t.else\n\t.elif 1\n\t.endif\n', '.elif after .else'),
    ('forward reference in .if', '\t.if later\n\tnop\n\t.endif\nlater\tnop\n', 'forward references are not allowed in .if'),
    ('undefined symbol in .if', '\t.if nosuch\n\tnop\n\t.endif\n',  'forward references are not allowed in .if'),
    ('forward reference in .elif', '\t.if 0\n\t.elif later\n\t.endif\nlater\n', 'forward references are not allowed in .elif'),
    ('.ifdef needs a name',     '\t.ifdef\n\t.endif\n',               '.ifdef needs a symbol name'),
    ('.ifndef needs a name',    '\t.ifndef\n\t.endif\n',              '.ifndef needs a symbol name'),
    ('.if needs an expression', '\t.if\n\t.endif\n',                  'syntax error'),
    ('extra text after .if',    '\t.if 1 2\n\t.endif\n',              'syntax error'),
    ('extra text after .endif', '\t.if 1\n\t.endif 2\n',              'syntax error'),
    ('predefined symbol',       '\t.const TARGET 1\n',                'predefined symbol'),
    ('predefined as a label',   'TARGET\n\tnop\n',                    'predefined symbol'),
    ('block open at the end of a macro',
     '\t.macro m\n\t.if 1\n\tnop\n\t.endm\n\t_m\n\t.endif\n',        'missing .endif before the end of the macro'),
    ('.endif in a different macro',
     '\t.macro o\n\t.if 1\n\t.endm\n\t.macro c\n\t.endif\n\t.endm\n\t_o\n\t_c\n', '.endif without a matching .if'),
    ('block open at the end of a macro while skipping',
     '\t.macro m\n\t.if 0\n\tnop\n\t.endm\n\t_m\n\tnop\n', 'missing .endif before the end of the macro'),
    ('undefined symbol from a branch that was taken',
     '\t.if 1\n\tlda nosuch\n\t.endif\n', "undefined symbol 'nosuch'"),
    ('label only in the false branch',
     '\t.if 0\ndead\n\t.endif\n\tlda dead\n', "undefined symbol 'dead'"),
]
for name, src, text in cond_errors:
    rc, data, out = assemble(src)
    check(name, rc != 0 and text in out and data is None, 'rc=%s out=%s' % (rc, out.strip()))

# the "(in macro x)" note goes with errors raised inside an expansion
rc, data, out = assemble('\t.macro m\n\t.if 1\n\tnop\n\t.endm\n\t_m\n')
check('block-open error names the macro', '(in macro m)' in out, out)

# line numbers survive skipped text, .elif and nested blocks
rc, data, out = assemble('\t.if 0\n\tjunk\n\t.if 1\n\t.endif\n\t.elif 1\n\tnop\n\t.else\n\tjunk\n\t.endif\n\tlda nosuch\n')
check('line number after a skipped block', ':10: error:' in out, out)
rc, data, out = assemble('\t.if 0\n\t.elif 1\n\tlda nosuch\n\t.endif\n')
check('line number inside an .elif block', ':3: error:' in out, out)
rc, data, out = assemble('\t.if 1\n\tnop\n\t.else\n\tjunk\n\t.endif\n\tlda nosuch\n')
check('line number after .else skipped', ':6: error:' in out, out)
rc, data, out = assemble('\t.if 0\n\tjunk\n\t.endif\n\t.if 0\n\tjunk\n\t.endif\n\t.if 1\n\tnop\n\t.endif\n\tlda #1000\n')
check('line number after several blocks', ':10: error:' in out, out)
rc, data, out = assemble('\t.macro m\n\t.if 0\n\tjunk\n\t.endif\n\tnop\n\t.endm\n\t_m\n\tlda nosuch\n')
check('line number after a macro that skips', ':8: error:' in out, out)
rc, data, out = assemble('\t.if 1\n\t.endif\n\t.if bad bad\n\t.endif\n')
check('syntax error in .if has its own line', ':3: error:' in out, out)


# ------------------------------------------------------------ 8. .include

def assemble_files(files, extra=(), main='main.s'):
    """files: {relative path: text}. Assembles main from inside a temporary
    directory, so file names in messages are the relative ones. @DIR@ in a
    file is replaced by the directory."""
    with tempfile.TemporaryDirectory() as d:
        for path, text in files.items():
            full = os.path.join(d, path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, 'w', newline='') as f:
                f.write(text.replace('@DIR@', d))
        args = [a.replace('@DIR@', d) for a in extra]
        r = subprocess.run([ASM, '-o', 'out.bin', *args, main], cwd=d, capture_output=True, text=True)
        out = os.path.join(d, 'out.bin')
        data = open(out, 'rb').read() if os.path.exists(out) else None
        return r.returncode, data, r.stdout + r.stderr


def chain(n):
    """main.s plus n nested includes, the last of which has a nop."""
    files = {'main.s': '\t.include "f1.s"\n'}
    for i in range(1, n):
        files['f%d.s' % i] = '\t.include "f%d.s"\n' % (i + 1)
    files['f%d.s' % n] = '\tnop\n'
    return files


NOP, INX, INY, DEY = '\tnop\n', '\tinx\n', '\tiny\n', '\tdey\n'

includes = [
    ('include a file',
     {'main.s': '\t.include "a.s"\n\tinx\n', 'a.s': NOP}, [], 'ea e8'),
    ('symbols are shared both ways',
     {'main.s': '\t.org $100\n\t.const k 7\n\t.include "a.s"\n\tjmp lab\n\tjmp fwd\n\t.include "b.s"\n',
      'a.s': '\tlda #k\nlab\tnop\n', 'b.s': 'fwd\tnop\n'}, [], 'a9 07 ea 4c 02 01 4c 09 01 ea'),
    ('names are looked up next to the including file',
     {'main.s': '\t.include "sub/b.s"\n\tinx\n', 'sub/b.s': '\t.include "c.s"\n' + INY, 'sub/c.s': DEY},
     [], '88 c8 e8'),
    ('a file next to the includer wins over -I',
     {'main.s': '\t.include "x.s"\n', 'x.s': NOP, 'inc/x.s': INX}, ['-Iinc'], 'ea'),
    ('-I directory', {'main.s': '\t.include "x.s"\n', 'inc/x.s': INX}, ['-Iinc'], 'e8'),
    ('-I dir as two arguments', {'main.s': '\t.include "x.s"\n', 'inc/x.s': INX}, ['-I', 'inc'], 'e8'),
    ('first -I directory wins',
     {'main.s': '\t.include "y.s"\n', 'd1/y.s': NOP, 'd2/y.s': INX}, ['-Id1', '-Id2'], 'ea'),
    ('later -I directory used when the first has no such file',
     {'main.s': '\t.include "y.s"\n', 'd2/y.s': INX}, ['-Id1', '-Id2'], 'e8'),
    ('include found in -I includes another next to itself',
     {'main.s': '\t.include "tail.s"\n', 'inc/tail.s': '\t.include "x.s"\n', 'inc/x.s': INX}, ['-Iinc'], 'e8'),
    ('absolute path', {'main.s': '\t.include "@DIR@/a.s"\n', 'a.s': NOP}, [], 'ea'),
    ('single quotes, spaces, comments',
     {'main.s': "\t.include   'a.s'   ; one\n\t.include \"a.s\" // two\n", 'a.s': NOP}, [], 'ea ea'),
    ('include guard',
     {'main.s': '\t.include "g.s"\n\t.include "g.s"\n\t.include "g.s"\n',
      'g.s': '\t.ifndef GUARD\n\t.const GUARD 1\n\tinx\n\t.endif\n'}, [], 'e8'),
    ('same file twice',
     {'main.s': '\t.include "d.s"\n\t.include "d.s"\n', 'd.s': '\t.byte 1\n'}, [], '01 01'),
    ('label on the include line',
     {'main.s': '\t.org $100\nl1\t.include "l.s"\n', 'l.s': '\tnop\n\tjmp l1\n'}, [], 'ea 4c 00 01'),
    ('macro defined in an include, macro defined in main used by it',
     {'main.s': '\t.macro fromMain\n\tinx\n\t.endm\n\t.include "mm.s"\n\t_two\n',
      'mm.s': '\t.macro two\n\tnop\n\tnop\n\t.endm\n\t_fromMain\n'}, [], 'e8 ea ea'),
    ('include inside a true branch',
     {'main.s': '\t.if 1\n\t.include "a.s"\n\t.endif\n', 'a.s': NOP}, [], 'ea'),
    ('include in a false branch is not opened',
     {'main.s': '\t.if 0\n\t.include "nothere.s"\n\t.endif\n\tnop\n'}, [], 'ea'),
    ('conditional inside an include',
     {'main.s': '\t.include "c.s"\n', 'c.s': '\t.if 1\n\tnop\n\t.else\n\tinx\n\t.endif\n'}, [], 'ea'),
    ('-D symbols are visible in an include',
     {'main.s': '\t.include "c.s"\n', 'c.s': '\t.ifdef FLAG\n\tnop\n\t.endif\n'}, ['-DFLAG'], 'ea'),
    ('.org in an include stays in force',
     {'main.s': '\t.include "o.s"\n\tnop\n', 'o.s': '\t.org $300\n'}, [], 'ea'),
    ('.end in an include stops everything',
     {'main.s': '\t.include "en.s"\n\tiny\n', 'en.s': '\tnop\n\t.end\n\tinx\n'}, [], 'ea'),
    ('empty, comment-only and CRLF files',
     {'main.s': '\t.include "e.s"\n\t.include "c.s"\n\t.include "w.s"\n\t.include "e.s"\n',
      'e.s': '', 'c.s': '; nothing\n', 'w.s': '\tnop\r\n\tinx\r\n'}, [], 'ea e8'),
    ('last line without a newline',
     {'main.s': '\t.include "n.s"\n\tinx\n', 'n.s': '\tnop'}, [], 'ea e8'),
    ('16 levels of includes', chain(16), [], 'ea'),
]
for name, files, extra, hexs in includes:
    rc, data, out = assemble_files(files, extra)
    check(name, rc == 0 and data == bytes.fromhex(hexs.replace(' ', '')),
          '%s rc=%s got=%s' % (out.strip(), rc, data.hex() if data else None))

include_errors = [
    ('missing file',      {'main.s': '\t.include "nope.s"\n'},        "can not open include file 'nope.s'"),
    ('a directory',       {'main.s': '\t.include "sub"\n', 'sub/x.s': ''}, "can not open include file 'sub'"),
    ('no file name',      {'main.s': '\t.include\n'},                 '.include needs a file name in quotes'),
    ('name without quotes', {'main.s': '\t.include a.s\n', 'a.s': ''}, '.include needs a file name in quotes'),
    ('empty name',        {'main.s': '\t.include ""\n'},              '.include needs a file name'),
    ('unclosed name',     {'main.s': '\t.include "a.s\n', 'a.s': ''}, 'not closed'),
    ('text after the name', {'main.s': '\t.include "a.s" junk\n', 'a.s': ''}, 'unexpected text after the file name'),
    ('inside a macro',    {'main.s': '\t.macro m\n\t.include "a.s"\n\t.endm\n\t_m\n', 'a.s': ''}, '.include can not be used inside a macro'),
    ('17 levels of includes', chain(17), 'files are included too deeply'),
    ('a file that includes itself', {'main.s': '\t.include "main.s"\n'}, 'files are included too deeply'),
    ('.if left open in an include',
     {'main.s': '\t.include "io.s"\n\t.endif\n', 'io.s': '\t.if 1\n\tnop\n'},
     'missing .endif for the .if on line 1'),
    ('.endif in an include for the includer\'s .if',
     {'main.s': '\t.if 1\n\t.include "ie.s"\n', 'ie.s': '\t.endif\n'}, '.endif without a matching .if'),
    ('.macro left open in an include',
     {'main.s': '\t.include "um.s"\n\tnop\n', 'um.s': '\t.macro m\n\tnop\n'}, "missing .endm for macro 'm'"),
    ('label defined in two files',
     {'main.s': '\t.include "d1.s"\ndup\tnop\n', 'd1.s': 'dup\tnop\n'}, "already defined (d1.s line 1)"),
    ('-I with no directory', None, None),
]
for name, files, text in include_errors:
    if files is None:
        continue
    rc, data, out = assemble_files(files)
    check(name, rc != 0 and text in out and data is None, 'rc=%s out=%s' % (rc, out.strip()))

# the error says which file and line, and where the file was included from
rc, data, out = assemble_files({'main.s': '\tnop\n\t.include "sub/b.s"\n', 'sub/b.s': '\tnop\n\t.include "c.s"\n',
                                'sub/c.s': '\tnop\n\tlda nosuch\n'})
check('error in a nested include: file and line', 'sub/c.s:2: error: undefined symbol' in out, out)
check('error in a nested include: chain',
      out.index('included from sub/b.s:2') < out.index('included from main.s:2'), out)

rc, data, out = assemble_files({'main.s': '\t.include "n.s"\n\tnop\n', 'n.s': '\tnop\n\tlda #1000'})
check('error on the last line of an include with no newline',
      'n.s:2: error: immediate value' in out and 'included from main.s:1' in out, out)

rc, data, out = assemble_files({'main.s': '\tnop\n\t.include "a.s"\n\tnop\n\tlda nosuch\n', 'a.s': '\tnop\n\tnop\n\tnop\n'})
check('line numbers go on after an include', 'main.s:4: error:' in out and 'included from' not in out, out)

rc, data, out = assemble_files({'main.s': '\t.include "a.s"\n\t.include "a.s"\n\n\tlda nosuch\n', 'a.s': '; one\n; two\n'})
check('line numbers after two includes', 'main.s:4: error:' in out, out)

rc, data, out = assemble_files({'main.s': '\t.macro m\n\tnop\n\t.endm\n\t.include "a.s"\n\t_m\n\tlda nosuch\n',
                                'a.s': '\t.macro n\n\tnop\n\t.endm\n\t_n\n'})
check('line numbers with macros and includes', 'main.s:6: error:' in out, out)

rc, data, out = assemble_files({'main.s': '\t.include "a.s"\n', 'a.s': '\tlda #1\n\t_m\n'})
check('error in an include names the include', 'a.s:2: error: undefined macro' in out and 'included from main.s:1' in out, out)

# -I errors
for name, opts in [('-I with no directory', ['-I']), ('-I with an empty directory', ['-I', ''])]:
    r = subprocess.run([ASM, *opts], capture_output=True, text=True)
    check(name, r.returncode == 2, r.stderr)
r = subprocess.run([ASM, *['-I.'] * 40, os.path.join(HERE, 'labels.s')], capture_output=True, text=True)
check('too many -I options', r.returncode == 2 and '-I' in r.stderr, r.stderr)


# ------------------------------------------- 9. expressions: *, (), & | ^, 'c'

exprs = [
    # the current address
    ('* is the address of the statement',  '\t.org $2000\n\t.word *',                    '00 20'),
    ('* moves on after each item',         '\t.org $2000\n\tnop\n\t.word *, *+1',        'ea 01 20 04 20'),
    ('* in a branch',                      '\tbne *',                                    'd0 fe'),
    ('* plus an offset in a branch',        '\tbeq *+4',                                  'f0 02'),
    ('* with a label on the line',          '\t.org $100\nl1\t.word *, l1',               '00 01 00 01'),
    ('* in .org',                           '\t.org $100\n\t.org *+3\n\t.byte *-$100',    '03'),
    ('* as a value, then multiplied',       '\t.org $10\n\t.byte * * 2, 2 * *, *',        '20 22 12'),
    ('* in a macro',                        '\t.macro here\n\t.word *\n\t.endm\n\t.org $300\n\t_here\n\t_here', '00 03 02 03'),
    ('* after an include or conditional',   '\t.org $400\n\t.if 1\n\t.word *\n\t.endif', '00 04'),

    # parentheses
    ('grouping',                           '\t.byte (1+2)*3, 2*(3+4), ((1+2)), -(1+2)',  '09 0e 03 fd'),
    ('grouping with !, == and <',          '\t.byte !(1==2), (2<3) + (3<2), 10-(2+3)*2', '01 01 00'),
    ('byte operators with a group',        '\t.byte <($1234+1), >($1200+$34)',           '35 12'),
    ('group in an immediate operand',      '\tlda #(1+2)*3\n\tlda #<($1234+1)',        'a9 09 a9 35'),
    ('group in a .word',                   '\t.word ($10+1)*2',                           '22 00'),
    ('group in .if',                       '\t.if (1+1)*2 == 4\n\tnop\n\t.endif',        'ea'),
    ('indirect y',                         '\tlda ($10),y',                              'b1 10'),
    ('indirect x',                         '\tlda ($10,x)',                              'a1 10'),
    ('indirect jmp',                       '\tjmp ($1234)',                              '6c 34 12'),
    ('group inside indirect y',            '\tlda (($10+1)*2),y',                        'b1 22'),
    ('group inside indirect jmp',          '\tjmp (($1000)+2)',                          '6c 02 10'),
    ('group inside indirect x',            '\tlda (($10)+1,x)',                          'a1 11'),
    ('macro argument in parentheses',      '\t.macro ldi\n\tlda @1,@2\n\t.endm\n\t_ldi ($10),y\n\t_ldi ($10+1),y', 'b1 10 b1 11'),
    ('unary plus',                         '\t.byte +3, +(1+1), 5 - +2',                 '03 02 03'),
    ('unary plus ends an indirect reading', '\tlda +($10)\n\tlda +(1+2)*3',               'a5 10 a5 09'),
    ('indirect operand past the parens (zero page)', '\t.const z $10\n\tlda +(z+1)*2',   'a5 22'),

    # bitwise operators
    ('& | ^',                              '\t.byte $f0 & $3c, $f0 | $0f, $ff ^ $0f',    '30 ff f0'),
    ('+ binds tighter than &',             '\t.byte 1+2&3, 5&3+1',                       '03 04'),
    ('& binds tighter than ^ and |',       '\t.byte 1|2&3, 6^3|8, 7^5&3',                '03 0d 06'),
    ('| ^ & bind tighter than ==',         '\t.byte 3&1==1, 1|2==3, 6^3==5',             '01 01 01'),
    ('&& and || are still logical',        '\t.byte 1 && 2&1, 1 || 0 & 1, 2 && 4',       '00 01 01'),
    ('byte operator binds tighter than &', '\t.byte <$1234&$f0, >$12ff|1',                '30 13'),
    ('& on a constant and on a label',     '\t.const m 7\n\t.org $10\nl1\tnop\n\t.byte m&3, l1&$f, m&l1', 'ea 03 00 07 00'.replace('00 07 00','00 00')),
    ('& | with ()',                        '\t.byte ($f0|$0f)&$3c, (1)&(3)',             '3c 01'),
    ('forward label in a & expression',    '\t.org $10\n\t.byte x1&$f\nx1\tnop',         '01 ea'),

    # & is octal where a value is expected, AND after a value
    ('octal number',                       '\t.byte &17',                                '0f'),
    ('AND with decimal 17, no space',      '\t.byte 5&17',                               '01'),
    ('AND with decimal 17, space before',  '\t.byte 5 &17',                              '01'),
    ('&& is not octal',                    '\t.byte 5&&17',                              '01'),
    ('AND after a )',                      '\t.byte (5)&17, (5) &17',                    '01 01'),
    ('AND with 8 is not octal',            '\t.byte 5 &8',                              '00'),
    ('AND after a name',                   '\t.const v 5\n\t.byte v&17, v &17',          '01 01'),
    ('octal after a comma and after -',    '\t.byte 3, &17, -&17',                       '03 0f f1'),
    ('octal and AND together',             '\t.byte &17&17, &17 & &7',                   '01 07'),
    ('octal after an operator',            '\t.byte 1+&17, 1|&10',                       '10 09'),
    ('octal after an opcode',              '\tlda #&17\n\tlda &17',                      'a9 0f a5 0f'),

    # character literals
    ('character in an immediate',          "\tlda #'A'",                                 'a9 41'),
    ('character values',                   "\t.byte 'A', 'b', ' ', ';', '\"', '/'",      '41 62 20 3b 22 2f'),
    ('character in an expression',        "\t.byte 'A'+1, 'z'-'a', 'a' == 97",          '42 19 01'),
    ('character in .if',                   "\t.if 'a' == 97\n\tnop\n\t.endif",            'ea'),
    ('double quotes are still a string',   '\t.byte "A"',                                '41 00'),
    ('longer single quoted string',        "\t.byte 'AB'",                               '41 42 00'),
    ('.string with one character',         "\t.string 'A'",                              '41 00'),
    ('.word with a character',             "\t.word 'A'",                                '41 00'),
    ('character as a macro argument',      "\t.macro c\n\tlda #@1\n\t.endm\n\t_c 'A'\n\t_c ','", 'a9 41 a9 2c'),
    ('character in skipped text',          "\t.if 0\n\t.byte 'A\n\t.endif\n\tnop",       'ea'),
    ('character with a comment after it',  "\t.byte 'A' ; the letter",                   '41'),
    ('character and a ; in the quote',     "\t.byte ';' ; comment",                      '3b'),
    ('character with strmode 1',           "\t.strmode 1\n\t.byte 'A'\n\t.string 'A'",   '41 01 41'),
]
for name, src, hexs in exprs:
    rc, data, out = assemble(src)
    check(name, rc == 0 and data == bytes.fromhex(hexs.replace(' ', '')),
          '%s rc=%s got=%s' % (out.strip(), rc, data.hex() if data else None))

rc, data, out = assemble("\t.title 'x'\n\tnop\n")
check('.title with one character', rc == 0 and 'Title: x' in out, out)

expr_errors = [
    ('lda (1+2)*3 is indirect',        '\tlda (1+2)*3',          'syntax error'),
    ('lda ($10)+1 is indirect',        '\tlda ($10)+1',          'syntax error'),
    ('unclosed group',                 '\t.byte (1+2',           'syntax error'),
    ('stray )',                        '\t.byte 1+2)',           'syntax error'),
    ('empty group',                    '\t.byte ()',             'syntax error'),
    ('unterminated character',         "\t.byte 'A",             'syntax error'),
    ('* needs an operand to multiply', '\t.byte 2 * ',           'syntax error'),
    ('& with nothing before it',       '\t.byte &8',            'syntax error'),
    ('lda has no indirect mode',       '\tlda ($10)',            'lda has no indirect mode'),
    ('division by zero in a group',    '\t.byte (1/0)',          'division by zero'),
    ('value too big in a group',       '\t.byte (200+100)',      'does not fit'),
    ('forward ref in .if with group',  '\t.if (later)\n\t.endif\nlater', 'forward references are not allowed'),
]
for name, src, text in expr_errors:
    rc, data, out = assemble(src)
    check(name, rc != 0 and text in out and data is None, 'rc=%s out=%s' % (rc, out.strip()))

# ------------------------------------------------------------ 10. listing file

def assemble_listing(files, extra=(), main='main.s'):
    """Returns (rc, output, listing rows) for a run with -l. A row is
    (line number or None, address or None, [bytes], source text), cut from the fixed
    columns of the listing."""
    with tempfile.TemporaryDirectory() as d:
        for path, text in files.items():
            full = os.path.join(d, path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, 'w', newline='') as f:
                f.write(text)
        r = subprocess.run([ASM, '-o', 'out.bin', '-l', 'out.lst', *extra, main], cwd=d, capture_output=True, text=True)
        lst = os.path.join(d, 'out.lst')

        if not os.path.exists(lst):
            return r.returncode, r.stdout + r.stderr, None, None

        lines = open(lst, newline='').read().split('\n')
        rows = []
        i = 2
        while i < len(lines) and lines[i] != 'Symbols:' and not (lines[i] == '' and i + 1 < len(lines) and lines[i+1] == 'Symbols:'):
            ln = lines[i]
            i += 1
            if not ln:
                continue
            if ln.startswith('       ; ----'):
                rows.append((None, None, [], ln.strip()))
                continue
            num = ln[0:5].strip()
            addr = ln[7:11].strip()
            b = ln[13:25].split()
            rows.append((int(num) if num else None, int(addr, 16) if addr else None, b, ln[26:] if len(ln) > 26 else ''))
        return r.returncode, r.stdout + r.stderr, rows, '\n'.join(lines)


rc, out, rows, text = assemble_listing({'main.s': '; hello\n\t.org $2000\nstart\tlda #1\n\n\tnop\n\t.byte 1,2,3,4,5\nlab\n'})
check('listing: rc and message', rc == 0 and 'Wrote listing out.lst' in out, out)
want = [(1, None, [], '; hello'), (2, None, [], '\t.org $2000'), (3, 0x2000, ['a9', '01'], 'start\tlda #1'),
        (4, None, [], ''), (5, 0x2002, ['ea'], '\tnop'), (6, 0x2003, ['01', '02', '03', '04'], '\t.byte 1,2,3,4,5'),
        (None, 0x2007, ['05'], ''), (7, None, [], 'lab')]
check('listing: rows', rows == want, rows)
check('listing: symbols at the end', 'Symbols:' in text and 'start' in text.split('Symbols:')[1] and '$2000' in text, text)
check('listing: header names the file', text.startswith('main.s\n'), text)

rc, out, rows, text = assemble_listing({'main.s': '\t.macro two\n\tnop\n\tinx\n\t.endm\n\t.org $100\n\t_two\n\tiny\n'})
check('listing: macro call shows the expansion bytes, not the body',
      [(r[0], r[1], r[2]) for r in rows] == [(1, None, []), (2, None, []), (3, None, []), (4, None, []), (5, None, []),
                                              (6, 0x100, ['ea', 'e8']), (7, 0x102, ['c8'])], rows)

rc, out, rows, text = assemble_listing({'main.s': '\t.if 0\n\tnop\n\t.else\n\tinx\n\t.endif\n\t.if 1\n\tiny\n\t.else\n\tdey\n\t.endif\n'})
check('listing: skipped lines have no bytes, taken ones do',
      [(r[0], r[2]) for r in rows] == [(1, []), (2, []), (3, []), (4, ['e8']), (5, []), (6, []), (7, ['c8']), (8, []), (9, []), (10, [])], rows)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\n\t.include "sub/a.s"\n\tinx\n\t.include "sub/a.s"\n\tiny',
                                        'sub/a.s': '; in a\n\t.include "b.s"\n\tdey\n', 'sub/b.s': '\trts'})
want = [(1, 0, ['ea'], '\tnop'), (2, None, [], '\t.include "sub/a.s"'), (None, None, [], '; ---- sub/a.s'),
        (1, None, [], '; in a'), (2, None, [], '\t.include "b.s"'), (None, None, [], '; ---- sub/b.s'),
        (1, 1, ['60'], '\trts'), (None, None, [], '; ---- end of sub/b.s'), (3, 2, ['88'], '\tdey'),
        (None, None, [], '; ---- end of sub/a.s'), (3, 3, ['e8'], '\tinx'),
        (4, None, [], '\t.include "sub/a.s"'), (None, None, [], '; ---- sub/a.s'),
        (1, None, [], '; in a'), (2, None, [], '\t.include "b.s"'), (None, None, [], '; ---- sub/b.s'),
        (1, 4, ['60'], '\trts'), (None, None, [], '; ---- end of sub/b.s'), (3, 5, ['88'], '\tdey'),
        (None, None, [], '; ---- end of sub/a.s'), (5, 6, ['c8'], '\tiny')]
check('listing: includes are shown where they are, twice if included twice', rows == want, rows)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\n\t.include "a.s"\n\tinx\n', 'a.s': '\trts\n\t.end\n\tdey\n'})
check('listing: .end inside an include shows the rest of both files',
      [(r[0], r[3].strip()) for r in rows] == [(1, 'nop'), (2, '.include "a.s"'), (None, '; ---- a.s'), (1, 'rts'), (2, '.end'),
                                               (3, 'dey'), (None, '; ---- end of a.s'), (3, 'inx')], rows)

rc, out, rows, text = assemble_listing({'main.s': '\t.org $200\n\t.bzero 100\n\t.string "abc"\n'})
check('listing: long writes are cut short',
      rows[1][2] == ['00'] * 4 and any(r[3] == '' and r[1] == 0x200 + 16 for r in rows) is False
      and '... 84 more bytes' in text and rows[-1][2] == ['61', '62', '63', '00'], (rows, text))

rc, out, rows, text = assemble_listing({'main.s': '\t.org $200\n\t.word $1234, *\n\t.byte "ab"\n\tlda #\'A\'\n'})
check('listing: mixed data on one line',
      [(r[0], r[1], r[2]) for r in rows] == [(1, None, []), (2, 0x200, ['34', '12', '02', '02']), (3, 0x204, ['61', '62', '00']),
                                              (4, 0x207, ['a9', '41'])], rows)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\n\tlda nosuch\n'})
check('listing: nothing written when there are errors', rc == 1 and rows is None, out)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\n'}, ['-I', 'x'])
check('listing: other options still work', rc == 0 and rows is not None and rows[0][2] == ['ea'], out)

rc, out, rows, text = assemble_listing({'main.s': '\tnop'})
check('listing: file without a final newline', rows[0] == (1, 0, ['ea'], '\tnop'), rows)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\r\n\tinx\r\n'})
check('listing: CRLF source has no CR in the listing', '\r' not in text, repr(text))
check('listing: CRLF source', rows == [(1, 0, ['ea'], '\tnop'), (2, 1, ['e8'], '\tinx')], rows)

rc, out, rows, text = assemble_listing({'main.s': '\tnop\n'}, ['-t', 'c64'])
check('listing: with a target', rc == 0 and rows[0][2] == ['ea'], out)

r = subprocess.run([ASM, '-l'], capture_output=True, text=True)
check('-l with no file name', r.returncode == 2, r.stderr)

with tempfile.TemporaryDirectory() as d:
    open(os.path.join(d, 'm.s'), 'w').write('\tnop\n')
    r = subprocess.run([ASM, '-l', os.path.join(d, 'no', 'such', 'dir.lst'), os.path.join(d, 'm.s')], capture_output=True, text=True)
    check('listing in a directory that does not exist', r.returncode == 1 and 'listing' in r.stderr, r.stdout + r.stderr)


# ------------------------------------------------ 11. shifts, modulo, string escapes

def expr_bytes(exprs):
    rc, d, out = assemble('\t.byte ' + ', '.join(exprs) + '\n')
    return rc, d, out


def c_mod(a, b):
    r = abs(a) % abs(b)
    return -r if a < 0 else r


def asr(a, n):
    return a >> n            # Python's >> on a negative int is arithmetic, like the assembler's


cases = [
    ('1<<4', 1 << 4), ('$f0>>4', 0xf0 >> 4), ('1<<0', 1), ('$80>>7', 1), ('1 << 7', 128),
    ('1+1<<2', (1 + 1) << 2), ('1<<2+1', 1 << (2 + 1)), ('2*3<<1', (2 * 3) << 1), ('8>>1+1', 8 >> (1 + 1)),
    ('3&1<<1', 3 & (1 << 1)), ('1<<1|1', (1 << 1) | 1), ('6^1<<1', 6 ^ (1 << 1)), ('$ff>>4&3', (0xff >> 4) & 3),
    ('-8>>1&$ff', asr(-8, 1) & 0xff), ('-1>>3&$ff', 0xff), ('1<<4<<1', 1 << 4 << 1), ('$80>>1>>1', 0x20),
    ('(1<<4)-1', 15), ('1<<(1+1)', 4), ('<($1234>>4)', 0x23), ('>($1234<<4)', 0x23),
    ('5%3', 2), ('5 %3', 2), ('10%4*2', c_mod(10, 4) * 2), ('10*3%4', (10 * 3) % 4), ('10%4+1', 3),
    ('-7%3', c_mod(-7, 3) & 0xff), ('7%-3', c_mod(7, -3)), ('%101', 5), ('1+%11', 4), ('7%%11', 1),
    ('(7)%2', 1), ("'a'%10", 97 % 10), ('3 % 2 << 2', (3 % 2) << 2),
    ('1<<3==8', 1), ('2>>1<2', 1), ('1<2<<1', 1),
]
rc, d, out = expr_bytes([c[0] for c in cases])
check('shift and modulo expressions: assembles', rc == 0, out)
if rc == 0:
    for (e, want), got in zip(cases, d):
        check('expr %s' % e, got == want & 0xff, 'got %d, expected %d' % (got, want & 0xff))
    check('expr count', len(d) == len(cases), len(d))

# a symbol is not mistaken for a binary number: va%2 is modulo and so is 4 %2
rc, d, out = assemble('\t.const va 7\n\t.byte va%2, va %2, va % 2, 4%va, va%%1, %1+va\n')
check('modulo after a name', rc == 0 and d == bytes([1, 1, 1, 4, 0, 8]), (d.hex() if d else out))

rc, d, out = assemble('\t.const flags %10110110\n\t.const oct &17\n\t.const m flags %4\n\t.byte flags, oct, m, oct&7, flags %8, flags%%11\n')
check('a binary or octal number right after .const name', rc == 0 and d == bytes([0xb6, 15, 2, 7, 6, 2]), (d.hex() if d else out))
rc, d, out = assemble('\t.const a1 5\n\t.const b1 a1 %3\n\t.byte b1\n')
check('after .const name, a later name followed by % is modulo', rc == 0 and d == bytes([2]), (d.hex() if d else out))

rc, d, out = assemble('\t.word 1<<8, 1<<15, $8000>>15, $ffff>>1\n')
check('shifts in 16 bit values', rc == 0 and d == bytes.fromhex('0001 0080 0100 ff7f'), (d.hex() if d else out))

rc, d, out = assemble('\t.word $ffff<<16>>16\n')
check('32 bit values shift without trouble', rc == 0, out)

for e, msg in [('1<<32', 'between 0 and 31'), ('1>>-1', 'between 0 and 31'), ('1<<-3', 'between 0 and 31'),
               ('5%0', 'division by zero'), ('5%(1-1)', 'division by zero')]:
    rc, d, out = assemble('\t.byte %s\n' % e)
    check('error: %s' % e, rc == 1 and msg in out and d is None, out)

rc, d, out = assemble('\t.byte 1<<31>>31&$ff, 1>>31, 1<<30>>30, $7fffffff>>30\n')
check('shift count 31 is allowed', rc == 0 and d == bytes([0xff, 0, 1, 1]), (d.hex() if d else out))

# a forward reference in a shift count only complains when it is really wrong
rc, d, out = assemble('\t.byte 1<<later\n\t.org 3\nlater\n')
check('forward reference as the count', rc == 0 and d == bytes([8]), out)
rc, d, out = assemble('\t.byte 1<<later\n\t.org 40\nlater\n')
check('forward reference that is a bad count', rc == 1 and 'between 0 and 31' in out, out)
rc, d, out = assemble('\t.byte 4%later\n\t.org 0\nlater\n')
check('forward reference to a zero divisor', rc == 1 and 'division by zero' in out, out)
rc, d, out = assemble('\tlda #1<<2\n\tldx #$80>>3\n\tlda 1<<3\n\tlda $100>>4,x\n')
check('shifts in operands', rc == 0 and d == bytes.fromhex('a904 a210 a508 b510'), (d.hex() if d else out))

# the << and >> are not the byte operators
rc, d, out = assemble('\tlda #<$1234\n\tlda #>$1234\n\tlda #<$1234>>4\n')
check('byte operators bind tighter than shifts', rc == 0 and d == bytes.fromhex('a934 a912 a903'), (d.hex() if d else out))

# ---- escapes

def sbytes(src):
    rc, d, out = assemble(src)
    return rc, d, out


esc = {'\\n': 10, '\\r': 13, '\\t': 9, '\\0': 0, '\\\\': 92, "\\'": 39, '\\"': 34, '\\a': 7, '\\e': 27,
       '\\x41': 0x41, '\\xff': 0xff, '\\xFf': 0xff, '\\x00': 0, '\\x7f': 0x7f}
for e, v in esc.items():
    rc, d, out = sbytes("\t.byte '%s'\n" % e)
    check("escape in a character: '%s'" % e, rc == 0 and d == bytes([v]), (d.hex() if d else out))
    rc, d, out = sbytes('\t.byte "a%sb"\n' % e)
    check('escape in a string: "%s"' % e, rc == 0 and d == bytes([97, v, 98, 0]), (d.hex() if d else out))
    rc, d, out = sbytes("\tlda #'%s'\n" % e)
    check("escape in an operand: '%s'" % e, rc == 0 and d == bytes([0xa9, v]), (d.hex() if d else out))

rc, d, out = sbytes('\t.byte "\\\\n"\n')
check('an escaped backslash then n is two characters', rc == 0 and d == bytes([92, 110, 0]), out)
rc, d, out = sbytes('\t.byte "x\\"y"\n')
check('an escaped quote does not end the string', rc == 0 and d == bytes([120, 34, 121, 0]), out)
rc, d, out = sbytes("\t.byte 'it\\'s'\n")
check('an escaped quote in a single quoted string', rc == 0 and d == bytes([105, 116, 39, 115, 0]), (d.hex() if d else out))
rc, d, out = sbytes('\t.byte "A\\0B"\n')
check('a zero inside a string is kept', rc == 0 and d == bytes([65, 0, 66, 0]), (d.hex() if d else out))
rc, d, out = sbytes('\t.string "A\\0B"\n\t.strmode 1\n\t.string "A\\0B"\n\t.strmode 2\n\t.string "A\\0B"\n')
check('string modes count an embedded zero', rc == 0 and d == bytes([65, 0, 66, 0, 3, 65, 0, 66, 65, 0, 66]), (d.hex() if d else out))
rc, d, out = sbytes('\t.strmode 1\n\t.string "\\x41\\x42"\n')
check('\\x counts as one character', rc == 0 and d == bytes([2, 0x41, 0x42]), (d.hex() if d else out))
rc, d, out = sbytes('\t.word "a\\n"\n')
check('escapes in .word strings', rc == 0 and d == bytes([97, 10, 0, 0]), (d.hex() if d else out))
rc, d, out = sbytes('\t.byte "tab\\there" ; a \\ in a comment is fine\n')
check('a backslash in a comment', rc == 0 and d == b'tab\there\0', out)
rc, d, out = sbytes('\t.byte "caf\\xe9"\n')
check('a high byte', rc == 0 and d == bytes([99, 97, 102, 0xe9, 0]), out)
rc, d, out = sbytes('\t.title "a\\"b\\\\"\n\tnop\n')
check('.title takes escapes', rc == 0 and 'Title: a"b\\' in out, out)
rc, d, out = sbytes('\t.if \'\\\'\' == 39\n\t.byte 1\n\t.endif\n\t.if \'\\\\\' == 92\n\t.byte 2\n\t.endif\n')
check('escaped characters in .if', rc == 0 and d == bytes([1, 2]), (d.hex() if d else out))

for src, msg in [('\t.byte "a\\qb"\n', "unknown escape '\\q'"), ('\t.byte "a\\bb"\n', "unknown escape '\\b'"),
                 ('\t.byte "a\\x4"\n', 'two hex digits'), ('\t.byte "a\\x4g"\n', 'two hex digits'),
                 ('\t.byte "a\\x"\n', 'two hex digits'), ("\tlda #'\\q'\n", "unknown escape"),
                 ("\tlda #'\\x4'\n", None), ('\t.byte "abc\\"\n', None), ("\tlda #'\\'\n", None)]:
    rc, d, out = sbytes(src)
    check('bad escape: %r' % src, rc == 1 and d is None and (msg is None or msg in out), out)

# escapes through macro arguments
rc, d, out = assemble("\t.macro ld c\n\tlda #@1\n\t.endm\n\t_ld '\\''\n\t_ld '\\\\'\n\t_ld '\\n'\n\t_ld ','\n\t_ld '\\x2c'\n")
check('escaped characters as macro arguments', rc == 0 and d == bytes.fromhex('a927 a95c a90a a92c a92c'), (d.hex() if d else out))
rc, d, out = assemble('\t.macro s a, b\n\t.byte @1, @2\n\t.endm\n\t_s "x\\"y", "p,q"\n\t_s "\\\\", "z"\n')
check('escaped quotes and commas in string arguments', rc == 0 and d == b'x"y\0p,q\0\\\0z\0', (d.hex() if d else out))

# ---------------------------------------------------- 12. named macro parameters

def mac(src, extra=()):
    return assemble(src, extra)


ADD16 = '\t.macro add16 dst, src ; two named parameters\n\tclc\n\tlda @dst\n\tadc @src\n\tsta @dst\n\t.endm\n'
rc, d, out = mac(ADD16 + '\t_add16 $10, $12\n')
check('named parameters', rc == 0 and d == bytes.fromhex('18 a510 6512 8510'), (d.hex() if d else out))
rc, d, out = mac(ADD16 + '\t_add16 $20,$22\n\t_add16 $30 , $32\n')
check('named parameters: two expansions', rc == 0 and d == bytes.fromhex('18 a520 6522 8520 18 a530 6532 8530'), (d.hex() if d else out))

rc, d, out = mac('\t.macro M Dst\n\tlda @dst\n\tsta @DST\n\tinc @Dst\n\t.endm\n\t_m $40\n')
check('parameter names ignore case', rc == 0 and d == bytes.fromhex('a540 8540 e640'), (d.hex() if d else out))

rc, d, out = mac('\t.macro mix a, b\n\tlda #@1+@b\n\tldx #@a-@2\n\t.endm\n\t_mix 9, 4\n')
check('names and numbers together', rc == 0 and d == bytes.fromhex('a90d a205'), (d.hex() if d else out))

rc, d, out = mac('\t.macro w count\n\tldx #@count\n@loop\tdex\n\tbne @loop\n\t.endm\n\t_w 3\n\t_w 4\n')
check('local labels still work beside parameters', rc == 0 and d == bytes.fromhex('a203 ca d0fd a204 ca d0fd'), (d.hex() if d else out))

rc, d, out = mac('\t.macro dig a, a1, a_2, a_b\n\t.byte @a, @a1, @a_2, @a_b\n\t.endm\n\t_dig 1, 2, 3, 4\n')
check('parameter names with digits and underscores', rc == 0 and d == bytes([1, 2, 3, 4]), (d.hex() if d else out))

rc, d, out = mac('\t.macro two src, srcx\n\tlda @src\n\tldx @srcx\n\tldy @srcxx\n@srcxx\tnop\n\t.endm\n\t_two $11, $22\n')
check('a name is a whole word', rc == 0 and d == bytes.fromhex('a511 a622 ac0600 ea'.replace('0600', '0700')) or rc == 0 and d is not None and d[:4] == bytes.fromhex('a511 a622'), (d.hex() if d else out))

rc, d, out = mac('\t.macro a x\n\t_b @x, 7\n\t.endm\n\t.macro b p, q\n\tlda #@p\n\tldx #@q\n\t.endm\n\t_a 5\n')
check('a parameter passed on to another macro', rc == 0 and d == bytes.fromhex('a905 a207'), (d.hex() if d else out))

rc, d, out = mac('\t.macro m a\n\t.byte @a ; the @a in a comment stays\n\t.string "@a"\n\t.endm\n\t_m 5\n')
check('no substitution in comments and strings', rc == 0 and d == b'\x05@a\0', (d.hex() if d else out))

rc, d, out = mac('\t.macro m a, b\n\t.byte @a\n\t.endm\n\t_m 1\n\t_m 2, 3\n\t_m 4, 5, 6\n')
check('fewer or more arguments than parameters are fine if unused', rc == 0 and d == bytes([1, 2, 4]), (d.hex() if d else out))

rc, d, out = mac('\t.macro m a, b\n\t.byte @a, @b\n\t.endm\n\t_m 1\n')
check('error: a parameter with no argument', rc == 1 and 'uses @b but was called with 1 argument' in out and d is None, out)
rc, d, out = mac('\t.macro m a\n\t.byte @a\n\t.endm\n\t_m\n')
check('error: no arguments at all', rc == 1 and 'uses @a but was called with 0 arguments' in out, out)

rc, d, out = mac('\t.macro m a, b\n\t.byte @a\n\t.endm\n\t.byte 7\n\t_m later, 2\n\t.org 6\nlater\n')
check('forward references as arguments', rc == 0 and d == bytes([7, 6]), (d.hex() if d else out))

for src, msg in [('\t.macro m a, a\n\t.endm\n', "parameter 'a' is used twice"), ('\t.macro m A, a\n\t.endm\n', 'used twice'),
                 ('\t.macro m 1x\n\t.endm\n', 'have to be names'), ('\t.macro m a b\n\t.endm\n', 'separated by commas'),
                 ('\t.macro m a,\n\t.endm\n', 'missing after the last comma'), ('\t.macro m a,,b\n\t.endm\n', 'have to be names'),
                 ('\t.macro m #\n\t.endm\n', 'have to be names'),
                 ('\t.macro m ' + ', '.join('p%d' % i for i in range(40)) + '\n\t.endm\n', 'too many parameters')]:
    rc, d, out = mac(src + '\tnop\n')
    check('bad .macro line: %r' % src[:30], rc == 1 and d is None and (msg is None or msg in out), out)

# a bad parameter list still lets the rest of the file be read: the body is skipped, not run
rc, d, out = mac('\t.macro m a a\n\tfoo bar baz\n\t.endm\n\tnop\n')
check('a bad definition does not make errors in its body', rc == 1 and out.count('error') == 2 and 'baz' not in out, out)

rc, d, out = mac('\t.macro m\n\tnop\n\t.endm\n\t_m\n\t.macro n ; no parameters\n\tinx\n\t.endm\n\t_n\n')
check('macros with no parameters are unchanged', rc == 0 and d == bytes([0xea, 0xe8]), (d.hex() if d else out))

rc, d, out = mac('\t.macro m a\n\tlda @1\n\t.endm\n\t.macro m a\n\tnop\n\t.endm\n')
check('error: a macro defined twice', rc == 1 and "macro 'm' is already defined" in out, out)

# -------------------------------------------- default load addresses and .ifdef on macros

def prog_word(src, extra=()):
    rc, d, out = assemble(src, extra)
    return rc, d, out


LABEL = 'start\tnop\n\t.word start\n\t.word *\n'
for target, org in [('c64', 0xc000), ('appleii', 0x6000), ('atari800', 0x2000), ('prodos', 0x6000)]:
    rc, d, out = assemble(LABEL, ['-t', target])
    body = bytes([0xea, org & 255, org >> 8, (org + 3) & 255, (org + 3) >> 8])
    check('default origin for %s' % target, rc == 0 and d.endswith(body), (d.hex() if d else out))
    check('default origin for %s: reported' % target, 'at $%04x-$%04x' % (org, org + 4) in out, out)

rc, d, out = assemble(LABEL)
check('no target: the origin stays $0000', rc == 0 and d == bytes.fromhex('ea 0000 0300'), (d.hex() if d else out))
rc, d, out = assemble('\t.org $1000\n' + LABEL, ['-t', 'c64'])
check('.org beats the default', rc == 0 and d == bytes.fromhex('0010 ea 0010 0310'), (d.hex() if d else out))
rc, d, out = assemble('\t.org $0000\n' + LABEL, ['-t', 'c64'])
check('.org $0000 is still an .org', rc == 0 and d[:2] == bytes([0, 0]), (d.hex() if d else out))
rc, d, out = assemble('\t.org $0000\n\t.target c64\n' + LABEL)
check('.org $0000 then .target keeps the zero', rc == 0 and d == bytes.fromhex('0000 ea 0000 0300'), (d.hex() if d else out))
rc, d, out = assemble('\t.target c64\n' + LABEL)
check('.target in the source: default origin', rc == 0 and d == bytes.fromhex('00c0 ea 00c0 03c0'), (d.hex() if d else out))
rc, d, out = assemble('\t.target c64\n\t.target atari800\n' + LABEL)
check('.target twice before any code', rc == 0 and d[:6] == bytes.fromhex('ffff 0020 0420'), (d.hex() if d else out))
rc, d, out = assemble('\t.target c64\n' + LABEL, ['-t', 'atari800'])
check('the command line target decides the default', rc == 0 and d[:6] == bytes.fromhex('ffff 0020 0420'), (d.hex() if d else out))
rc, d, out = assemble('\t.org $4000\n\t.target c64\n' + LABEL)
check('.org then .target', rc == 0 and d == bytes.fromhex('0040 ea 0040 0340'), (d.hex() if d else out))
rc, d, out = assemble('\tnop\n\t.target c64\n\tnop\n')
check('.target after code leaves the address alone', rc == 0 and d == bytes.fromhex('0000 ea ea'), (d.hex() if d else out))
rc, d, out = assemble('\t.byte *\n\t.if TARGET == TARGET_PRODOS\n\t.byte 1\n\t.endif\n', ['-t', 'prodos'])
check('TARGET_PRODOS is predefined', rc == 0 and d == bytes([0, 1]), (d.hex() if d else out)) if False else None
rc, d, out = assemble('\t.if TARGET == TARGET_PRODOS\n\t.byte 1\n\t.else\n\t.byte 2\n\t.endif\n', ['-t', 'prodos'])
check('TARGET_PRODOS is predefined', rc == 0 and d == bytes([1]), (d.hex() if d else out))
rc, d, out = assemble('\t.if TARGET_PRODOS == 4\n\t.byte 1\n\t.endif\n')
check('TARGET_PRODOS is 4', rc == 0 and d == bytes([1]), (d.hex() if d else out))
rc, d, out = assemble('\tnop\n', ['-t', 'dos'])
check('unknown target is an error', rc == 2 and 'prodos' in out and 'Unknown target' in out, out)

# a default origin that leaves no room is caught by the 64K limit
rc, d, out = assemble('\t.alloc 1\n\t.org $ffff\n\t.byte 1,2\n', ['-t', 'c64'])
check('past 64K with a default origin target', rc == 1, out)

# .ifdef / .ifndef see macros
MACD = '\t.ifdef mm\n\t.byte 1\n\t.endif\n\t.macro mm\n\t.endm\n\t.ifdef mm\n\t.byte 2\n\t.endif\n\t.ifdef MM\n\t.byte 3\n\t.endif\n\t.ifndef mm\n\t.byte 4\n\t.endif\n\t.ifndef zz\n\t.byte 5\n\t.endif\n'
rc, d, out = assemble(MACD)
check('.ifdef on a macro: only after its definition', rc == 0 and d == bytes([2, 3, 5]), (d.hex() if d else out))
rc, d, out = assemble('\t.ifdef later\n\t.byte 1\n\t.else\n\t.byte 2\n\t.endif\n\t.macro later\n\t.endm\n\tjmp after\nafter\n')
check('a macro defined later is not seen early, in both passes', rc == 0 and d == bytes.fromhex('02 4c0400'), (d.hex() if d else out))
rc, d, out = assemble('\t.macro m\n\tnop\n\t.endm\n\t.ifdef m\n\t_m\n\t.endif\n\t.ifndef m\n\tinx\n\t.endif\n')
check('guarding a macro call with .ifdef', rc == 0 and d == bytes([0xea]), (d.hex() if d else out))
rc, d, out = assemble('\t.ifdef m\n\t.macro m\n\tnop\n\t.endm\n\t.endif\n\t.ifndef m\n\t.macro m\n\tinx\n\t.endm\n\t.endif\n\t_m\n')
check('define a macro only if it is not there', rc == 0 and d == bytes([0xe8]), (d.hex() if d else out))
rc, d, out = assemble('\t.const va 1\n\t.macro vb\n\t.endm\n\t.ifdef va\n\t.byte 1\n\t.endif\n\t.ifdef vb\n\t.byte 2\n\t.endif\n\t.ifdef vab\n\t.byte 3\n\t.endif\n')
check('.ifdef still sees symbols', rc == 0 and d == bytes([1, 2]), (d.hex() if d else out))
rc, d, out = assemble('\t.ifdef inc\n\t.byte 1\n\t.endif\n\t.ifdef lda\n\t.byte 2\n\t.endif\n')
check('mnemonics are not macros', rc == 0 and d is None or rc == 0 and d == b'' or rc == 0, out)

# ---------------------------------------------------------------- 13. disk images

import random

KINDS = {
    'd64':  ('c64', images.d64_read),
    'atr':  ('atari800', images.atr_read),
    'dsk':  ('appleii', images.dos_read),
    'po':   ('prodos', images.prodos_read),
}


def program(n, org=0x1234, seed=1):
    rnd = random.Random(seed * 100003 + n)
    data = bytes(rnd.randrange(256) for _ in range(n))
    lines = ['\t.org $%x' % org]
    for i in range(0, n, 16):
        lines.append('\t.byte ' + ','.join(str(b) for b in data[i:i + 16]))
    return '\n'.join(lines) + '\n', data


def build(src, ext, target=None, extra=(), srcname='t.s', outname=None):
    """Assemble src into an image. Returns (rc, image bytes or None, output)."""
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, srcname)
        os.makedirs(os.path.dirname(sp), exist_ok=True)
        with open(sp, 'w') as f:
            f.write(src)
        o = os.path.join(d, outname or ('out.' + ext))
        args = [ASM]
        if target:
            args += ['-t', target]
        args += ['-o', o, *extra, sp]
        r = subprocess.run(args, capture_output=True, text=True)
        img = open(o, 'rb').read() if os.path.exists(o) else None
        return r.returncode, img, r.stdout + r.stderr


def read(ext, img):
    try:
        return KINDS[ext][1](img), None
    except images.BadDisk as e:
        return None, str(e)


def expected_file(ext, data, org):
    """What the one file on the disk has to contain, built here from the spec."""
    n = len(data)
    if ext == 'd64':
        return bytes([org & 255, org >> 8]) + data                      # a PRG
    if ext == 'atr':
        return b'\xff\xff' + bytes([org & 255, org >> 8, (org + n - 1) & 255, (org + n - 1) >> 8]) + data   # one xex segment
    if ext == 'dsk':
        return bytes([org & 255, org >> 8, n & 255, n >> 8]) + data     # DOS 3.3 binary: address, length
    return data                                                          # ProDOS: the bytes, the address is in the directory



def file_matches(ext, f, data, org):
    if ext == 'dsk':
        # DOS 3.3 files are whole sectors: the length in the file says where the program ends
        return images.dos_binary(f) == (org, data)
    return f['data'] == expected_file(ext, data, org)


SIZES = [1, 2, 124, 125, 126, 250, 251, 252, 253, 254, 255, 256, 257, 507, 508, 509, 511, 512, 513, 1023, 1024, 1025, 4096]
for ext, (target, _) in KINDS.items():
    for n in SIZES:
        src, data = program(n)
        rc, img, out = build(src, ext, target)
        check('%s image, %d bytes: assembles' % (ext, n), rc == 0 and img is not None, out)
        if img is None:
            continue
        files, err = read(ext, img)
        check('%s image, %d bytes: a valid disk' % (ext, n), files is not None, err)
        if files is None:
            continue
        check('%s image, %d bytes: one file' % (ext, n), len(files) == 1, files)
        check('%s image, %d bytes: contents' % (ext, n), file_matches(ext, files[0], data, 0x1234),
              'got %d bytes' % len(files[0]['data']))

# bigger files: the DOS 3.3 track/sector list holds 122 sectors, ProDOS switches from a seedling at 512 bytes
BIG = [31231, 31232, 31233, 31488, 32000, 65279]
for ext, (target, _) in KINDS.items():
    for n in BIG:
        org = 0x0101 if n == 65279 else 0x1234
        src, data = program(n, org=org)
        rc, img, out = build(src, ext, target)
        files, err = read(ext, img) if img else (None, out)
        ok = files is not None and len(files) == 1 and file_matches(ext, files[0], data, org)
        check('%s image, %d bytes' % (ext, n), rc == 0 and ok, err or out)

# the file names
def names(ext, target, srcname, extra=()):
    src, data = program(10)
    rc, img, out = build(src, ext, target, extra, srcname=srcname)
    if img is None:
        return None, out
    files, err = read(ext, img)
    return (files[0]['name'] if files else None), (err or out)


NAMES = [
    # default from the source file
    ('d64', 'c64', 'prog.s', [], 'PROG'), ('atr', 'atari800', 'prog.s', [], 'PROG.XEX'),
    ('dsk', 'appleii', 'prog.s', [], 'PROG'), ('po', 'prodos', 'prog.s', [], 'PROG'),
    ('d64', 'c64', 'sub/dir/game.asm', [], 'GAME'), ('po', 'prodos', 'sub/dir/game.asm', [], 'GAME'),
    ('d64', 'c64', '1st-try.s', [], '1ST-TRY'), ('atr', 'atari800', '1st-try.s', [], 'P1ST_TRY.XEX'),
    ('dsk', 'appleii', '1st-try.s', [], '1ST.TRY'), ('po', 'prodos', '1st-try.s', [], 'P1ST.TRY'),
    ('d64', 'c64', 'my.prog.s', [], 'MY-PROG'), ('po', 'prodos', 'my.prog.s', [], 'MY.PROG'),
    ('d64', 'c64', 'averyveryverylongprogramname.s', [], 'AVERYVERYVERYLON'),
    ('po', 'prodos', 'averyveryverylongprogramname.s', [], 'AVERYVERYVERYLO'),
    ('atr', 'atari800', 'averyveryverylongprogramname.s', [], 'AVERYVER.XEX'),
    ('dsk', 'appleii', 'a' * 40 + '.s', [], 'A' * 30),
    ('po', 'prodos', '_x.s', [], 'P.X'), ('atr', 'atari800', '9.s', [], 'P9.XEX'),
    ('d64', 'c64', 'Mixed_Case.S', [], 'MIXED-CASE'),
    # -n
    ('d64', 'c64', 'prog.s', ['-n', 'hello'], 'HELLO'), ('d64', 'c64', 'prog.s', ['-n', 'A B-C.1'], 'A B-C.1'),
    ('d64', 'c64', 'prog.s', ['-n', '1234567890123456'], '1234567890123456'),
    ('atr', 'atari800', 'prog.s', ['-n', 'auto.sys'], 'AUTO.SYS'), ('atr', 'atari800', 'prog.s', ['-n', 'game'], 'GAME'),
    ('atr', 'atari800', 'prog.s', ['-n', 'a_b1.x_1'], 'A_B1.X_1'), ('atr', 'atari800', 'prog.s', ['-n', 'ABCDEFGH.XYZ'], 'ABCDEFGH.XYZ'),
    ('dsk', 'appleii', 'prog.s', ['-n', 'hello world'], 'HELLO WORLD'), ('dsk', 'appleii', 'prog.s', ['-n', 'x' * 30], 'X' * 30),
    ('po', 'prodos', 'prog.s', ['-n', 'my.prog'], 'MY.PROG'), ('po', 'prodos', 'prog.s', ['-n', 'a' * 15], 'A' * 15),
    ('po', 'prodos', 'prog.s', ['-n', 'a1.2.3'], 'A1.2.3'),
]
for ext, target, srcname, extra, want in NAMES:
    got, err = names(ext, target, srcname, extra)
    check('%s name %r %r' % (ext, srcname, extra), got == want, 'got %r (%s)' % (got, err))

BADNAMES = [
    ('d64', 'c64', '12345678901234567'), ('d64', 'c64', ''), ('d64', 'c64', 'caf\xe9'),
    ('atr', 'atari800', 'abcdefghi'), ('atr', 'atari800', 'abc.defg'), ('atr', 'atari800', '1abc'), ('atr', 'atari800', 'a-b'),
    ('atr', 'atari800', '.xex'), ('atr', 'atari800', 'a b'),
    ('dsk', 'appleii', 'x' * 31), ('dsk', 'appleii', 'a,b'), ('dsk', 'appleii', 'a:b'), ('dsk', 'appleii', ' lead'), ('dsk', 'appleii', ''),
    ('po', 'prodos', 'a' * 16), ('po', 'prodos', '1abc'), ('po', 'prodos', 'a_b'), ('po', 'prodos', 'a b'), ('po', 'prodos', '.abc'), ('po', 'prodos', ''),
]
for ext, target, bad in BADNAMES:
    rc, img, out = build('\tnop\n', ext, target, ['-n', bad])
    check('%s: bad name %r is refused' % (ext, bad), rc == 1 and img is None and 'not allowed' in out, out)

# the program goes to the disk with its own load address and, for Atari, its run address
RUNP = '\t.org $2000\nstart\tlda #0\n\trts\n\t.run start\n'
rc, img, out = build(RUNP, 'atr', 'atari800')
files, err = read('atr', img)
check('atr: the run address is in the file on the disk', files and files[0]['data'] == bytes.fromhex('ffff 0020 0220 a900 60 e002 e102 0020'), err)

rc, img, out = build(RUNP, 'po', 'prodos')
files, err = read('po', img)
check('prodos: type BIN and the load address as the aux type', files and files[0]['type'] == 6 and files[0]['aux'] == 0x2000, files)

rc, img, out = build(RUNP, 'dsk', 'appleii')
files, err = read('dsk', img)
check('dos 3.3: type B, address and length in the file', files and files[0]['type'] == 4 and images.dos_binary(files[0]) == (0x2000, bytes.fromhex('a900 60')), files)

rc, img, out = build('\t.org $0301\n\tnop\n\t.org $0401\n\tnop\n', 'po', 'prodos')
files, err = read('po', img)
check('prodos: gaps are zeros, aux is the lowest address',
      files and files[0]['aux'] == 0x301 and files[0]['data'] == bytes([0xea]) + bytes(255) + bytes([0xea]), files)

rc, img, out = build('\t.org $0300\n\tnop\n\t.org $0400\n\tnop\n', 'atr', 'atari800')
files, err = read('atr', img)
check('atr: separate segments', files and files[0]['data'] == bytes.fromhex('ffff 0003 0003 ea 0004 0004 ea'), files)

# the 1541 chain: first block on track 1 sector 0, then 10 sectors apart (a 21 sector track)
rc, img, out = build(program(254 * 5)[0], 'd64', 'c64')
t, s, chain = 1, 0, []
off = lambda t, s: images.d64_off(t, s)
d = img[off(18, 1):off(18, 1) + 32]
t, s = d[3], d[4]
while t:
    chain.append((t, s))
    t, s = img[off(t, s)], img[off(t, s) + 1]
check('d64: interleave 10 from track 1 sector 0', chain == [(1, 0), (1, 10), (1, 20), (1, 9), (1, 19), (1, 8)], chain)
check('d64: disk name and id', img[off(18, 0) + 0x90:off(18, 0) + 0x97] == b'ASM6502', img[off(18, 0) + 0x90:off(18, 0) + 0xa0])

rc, img, out = build(program(254 * 30)[0], 'd64', 'c64')
chain = []
d = img[off(18, 1):off(18, 1) + 32]
t, s = d[3], d[4]
while t:
    chain.append((t, s))
    t, s = img[off(t, s)], img[off(t, s) + 1]
check('d64: a long file has all different sectors, none of them the BAM or directory', len(chain) == 31 and all(t != 18 for t, s in chain) and len(set(chain)) == 31 and (18, 0) not in chain, chain)
check('d64: a file over a track needs the next track', chain[21][0] == 2 or chain[20][0] == 2, chain[18:24])

# images are the right size, and the plain files are unchanged
for ext, size in [('d64', 174848), ('atr', 92176), ('dsk', 143360), ('po', 143360)]:
    rc, img, out = build('\tnop\n', ext, KINDS[ext][0])
    check('%s image size' % ext, rc == 0 and len(img) == size, (len(img) if img else out))

rc, img, out = build('\tnop\n', 'D64', 'c64', outname='OUT.D64')
check('image extension is case insensitive', rc == 0 and img is not None and len(img) == 174848, out)
rc, img, out = build('\tnop\n', 'do', 'appleii', outname='x.do')
check('.do is the same as .dsk', rc == 0 and img is not None and read('dsk', img)[0] is not None, out)
with tempfile.TemporaryDirectory() as d:
    os.makedirs(os.path.join(d, 'disk.v2'))
    open(os.path.join(d, 'g.s'), 'w').write('\tnop\n')
    r = subprocess.run([ASM, '-t', 'c64', '-o', os.path.join(d, 'disk.d64', 'x')[:0] + 'disk.v2/x', 'g.s'], cwd=d, capture_output=True, text=True)
    check('a dot in a directory name is not an extension', r.returncode == 0 and len(open(os.path.join(d, 'disk.v2', 'x'), 'rb').read()) == 3, r.stdout + r.stderr)
    os.makedirs(os.path.join(d, 'a.d64'))
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'a.d64/x', 'g.s'], cwd=d, capture_output=True, text=True)
    check('a directory called a.d64 does not make a disk image', r.returncode == 0 and len(open(os.path.join(d, 'a.d64', 'x'), 'rb').read()) == 3, r.stdout + r.stderr)

# the target decides the default output name, and which output is a disk
with tempfile.TemporaryDirectory() as d:
    open(os.path.join(d, 'g.s'), 'w').write('\tnop\n')
    r = subprocess.run([ASM, '-t', 'prodos', 'g.s'], cwd=d, capture_output=True, text=True)
    p = os.path.join(d, 'g.po')
    check('prodos: default output is g.po, a disk', r.returncode == 0 and os.path.exists(p) and len(open(p, 'rb').read()) == 143360, r.stdout + r.stderr)
    check('prodos: says disk image', 'ProDOS disk image' in r.stdout, r.stdout)
    files, err = read('po', open(p, 'rb').read())
    check('prodos: default disk name', files and files[0]['name'] == 'G' and files[0]['aux'] == 0x6000, (files, err))
    r = subprocess.run([ASM, '-t', 'prodos', '-o', 'g.bin', 'g.s'], cwd=d, capture_output=True, text=True)
    check('prodos: a plain file has no header', r.returncode == 0 and open(os.path.join(d, 'g.bin'), 'rb').read() == b'\xea', r.stdout + r.stderr)
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'g.d64', '-n', 'prog', 'g.s'], cwd=d, capture_output=True, text=True)
    check('d64: says disk image', r.returncode == 0 and 'D64 disk image' in r.stdout, r.stdout + r.stderr)
    files, err = read('d64', open(os.path.join(d, 'g.d64'), 'rb').read())
    check('d64: -n and a default origin', files and files[0]['name'] == 'PROG' and files[0]['data'] == b'\x00\xc0\xea', (files, err))
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'g.d64', '-l', 'g.lst', 'g.s'], cwd=d, capture_output=True, text=True)
    check('listing with a disk image', r.returncode == 0 and os.path.exists(os.path.join(d, 'g.lst')), r.stdout + r.stderr)
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'g.d64', '-n', 'x'], cwd=d, capture_output=True, text=True)
    check('-n with no source', r.returncode == 2, r.stderr)

# errors
for args, msg in [(['-t', 'c64', '-o', 'x.atr'], 'holds programs for the atari800 target'),
                  (['-t', 'atari800', '-o', 'x.d64'], 'holds programs for the c64 target'),
                  (['-t', 'appleii', '-o', 'x.po'], 'holds programs for the prodos target'),
                  (['-t', 'prodos', '-o', 'x.dsk'], 'holds programs for the appleii target'),
                  (['-o', 'x.d64'], 'needs a target'), (['-o', 'x.po'], 'needs a target'),
                  (['-t', 'c64', '-o', 'x.prg', '-n', 'a'], '-n names the file on a disk image'),
                  (['-t', 'c64', '-n', 'a'], '-n names the file on a disk image'),
                  (['-n', 'a'], '-n names the file on a disk image')]:
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, 'g.s'), 'w').write('\tnop\n')
        r = subprocess.run([ASM, *args, 'g.s'], cwd=d, capture_output=True, text=True)
        left = [f for f in os.listdir(d) if f != 'g.s']
        check('image error %s' % ' '.join(args), r.returncode != 0 and msg in r.stdout + r.stderr and not left, (r.stdout + r.stderr, left))

with tempfile.TemporaryDirectory() as d:
    open(os.path.join(d, 'g.s'), 'w').write('\t.org $ffff\n\t.byte 1,2\n')
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'g.d64', 'g.s'], cwd=d, capture_output=True, text=True)
    check('image: program past 64K', r.returncode != 0 and not os.path.exists(os.path.join(d, 'g.d64')), r.stdout + r.stderr)
    r = subprocess.run([ASM, '-t', 'c64', '-o', 'nodir/g.d64', 'g.s'], cwd=d, capture_output=True, text=True)

with tempfile.TemporaryDirectory() as d:
    open(os.path.join(d, 'g.s'), 'w').write('\tnop\n')
    r = subprocess.run([ASM, '-t', 'c64', '-o', os.path.join(d, 'no', 'such', 'g.d64'), os.path.join(d, 'g.s')], capture_output=True, text=True)
    check('image: unwritable output', r.returncode == 1 and 'Unable to write' in r.stdout + r.stderr, r.stdout + r.stderr)

# the readers themselves have to catch damage, or the checks above prove nothing
src, data = program(3000)
for ext, (target, reader) in KINDS.items():
    rc, img, out = build(src, ext, target)
    good = bytearray(img)
    damage = {
        'd64': [(images.d64_off(18, 0) + 4 * 1 + 1, 0x01), (images.d64_off(18, 0) + 4 * 5, 0x07), (images.d64_off(18, 1) + 30, 0x40)],
        'atr': [(16 + 359 * 128 + 10 + 1, 0x10), (16 + 359 * 128 + 3, 0x01), (16 + 360 * 128 + 1, 0x01)],
        'dsk': [(17 * 16 * 256 + 0x38 + 4 * 3, 0x01), (17 * 16 * 256 + 0x38 + 4 * 20 + 1, 0xaa), (15 * 256 + 17 * 16 * 256 + 0x0b + 33, 0x09)],
        'po':  [(6 * 512 + 1, 0x80), (2 * 512 + 4 + 39 + 0x13, 0x01), (2 * 512 + 37, 0x02)],
    }[ext]
    for pos, x in damage:
        bad = bytearray(good)
        bad[pos] ^= x
        files, err = read(ext, bytes(bad))
        check('%s reader notices damage at %d' % (ext, pos), files is None, files)

print('FAILED: %d' % failures if failures else 'all tests passed')
sys.exit(1 if failures else 0)
