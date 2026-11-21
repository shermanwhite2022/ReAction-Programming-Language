#!/usr/bin/env python3
# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
"""Compile every ReAction! program in tests/audit/ and examples/ with rc, then assemble the
result with asm6502 for the Atari 800.  Nothing is run: this checks that the compiler accepts
the programs and that the assembler accepts what the compiler emits.

    python3 tests/audit/run.py [--rc ./rc] [--asm ../asm6502/asm6502] [files.act ...]

Programs in tests/audit/errors/ are the opposite: rc must reject them with an ERR message.

Run it from the reaction/ directory (make check does).  Exit status is 1 if anything failed.
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))      # the reaction/ directory

ap = argparse.ArgumentParser()
ap.add_argument('--rc', default=os.path.join(ROOT, 'rc'))
ap.add_argument('--asm', default=os.path.join(ROOT, '..', 'asm6502', 'asm6502'))
ap.add_argument('files', nargs='*')
args = ap.parse_args()

rc = os.path.abspath(args.rc)
asm = os.path.abspath(args.asm)
for tool in (rc, asm):
    if not os.path.exists(tool):
        sys.exit('missing %s (build it first)' % tool)

files = args.files or (sorted(glob.glob(os.path.join(HERE, '*.act'))) +
                       sorted(glob.glob(os.path.join(ROOT, 'examples', '*.act'))))
bad = 0
with tempfile.TemporaryDirectory() as tmp:
    for f in files:
        f = os.path.abspath(f)
        name = os.path.splitext(os.path.basename(f))[0]
        out_a = os.path.join(tmp, name + '.a')
        # rc finds includes/ next to itself; the assembler needs -I for the .include lines
        r = subprocess.run([rc, f, '-o', out_a], capture_output=True, text=True, cwd=ROOT)
        msg = (r.stdout + r.stderr).strip()
        ok = r.returncode == 0 and 'ERR' not in msg
        if ok:
            a = subprocess.run([asm, '-t', 'atari800', '-I', ROOT, '-o', os.path.join(tmp, name + '.xex'), out_a],
                               capture_output=True, text=True, cwd=ROOT)
            if a.returncode:
                ok = False
                msg = 'asm6502: ' + (a.stdout + a.stderr)[-300:]
        bad += not ok
        print(('ok   ' if ok else 'FAIL ') + os.path.relpath(f, ROOT), '' if ok else msg[:260].replace('\n', ' | '))
    for f in sorted(glob.glob(os.path.join(HERE, 'errors', '*.act'))):
        r = subprocess.run([rc, f, '-o', os.path.join(tmp, 'bad.a')], capture_output=True, text=True, cwd=ROOT)
        msg = (r.stdout + r.stderr).strip()
        ok = r.returncode != 0 and 'ERR' in msg
        bad += not ok
        print(('ok   ' if ok else 'FAIL ') + os.path.relpath(f, ROOT), '(rejected as expected)' if ok else 'was accepted')
print('failures:', bad)
sys.exit(1 if bad else 0)
