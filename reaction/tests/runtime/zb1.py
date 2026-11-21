# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# zero block: variables and arrays that start as zero are reserved (.alloc), not stored, and cleared at start.
# iorun.run() fills the zero block with $AA before the program starts, so a value of 0 proves the clear loop ran.
import sys,subprocess; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('zb1','''INCLUDE "IO.ACT"
BYTE ARRAY big(700)
CARD c
BYTE b
PROC main()
 BYTE i, bad
 CARD t
 bad=0
 FOR t=0 TO 699
 DO
  IF big(t)#0 THEN bad=bad+1 FI
 OD
 PrintBE(bad)
 PrintCE(c)
 PrintBE(b)
RETURN
''')
m,sym,at=run('zb1')
ck('zero block cleared',at.text().split('\n')[:3],['0','0','0'])
# the program file does not contain the 700 bytes
import os
a_=open('/tmp/t7/zb1.a').read()
ck('reserved, not stored',('.alloc 700' in a_) and ('.bzero 700' not in a_),True)
# a program that reaches the stack is refused by the assembler (default Atari stack top is $8000)
src='DEFINE N="1"\nBYTE ARRAY a(30000)\nPROC main()\n a(0)=1\nRETURN\n'
open('/tmp/t7/zb2.act','w').write(src)
r=subprocess.run(['/home/claude/rx_work/emit2/rc','/tmp/t7/zb2.act','-o','/tmp/t7/zb2.a'],capture_output=True,text=True)
r=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','/home/claude/rx_work/emit2','-o','/tmp/t7/zb2.xex','/tmp/t7/zb2.a'],capture_output=True,text=True)
ck('stack check',('reaches the run time stack' in r.stdout+r.stderr),True)
r=subprocess.run(['/home/claude/rx_work/emit2/rc','-S','0xb000','/tmp/t7/zb2.act','-o','/tmp/t7/zb2.a'],capture_output=True,text=True)
r=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','/home/claude/rx_work/emit2','-o','/tmp/t7/zb2.xex','/tmp/t7/zb2.a'],capture_output=True,text=True)
ck('stack moved',r.returncode,0)
done()
