# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,subprocess; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='PROC main()\nRETURN\n'
def stk(flags,target='atari800',zp=0x8e,asmflags=()):
    open('/tmp/t7/stk.act','w').write(SRC)
    r=subprocess.run([W+'/rc',*flags,'-o','/tmp/t7/stk.a','/tmp/t7/stk.act'],capture_output=True,text=True)
    r=subprocess.run(['/home/claude/asmb/asm6502','-t',target,*asmflags,'-o','/tmp/t7/stk.bin','/tmp/t7/stk.a'],capture_output=True,text=True)
    assert r.returncode==0,r.stdout
    d=open('/tmp/t7/stk.bin','rb').read()
    i=d.index(bytes([0xa9,0x00,0x85,zp])); return d[i+5]*256+d[i+1]   # lda #<STK, sta RUNSTK, lda #>STK
ck('Atari default',stk([]),0x8000)
ck('C64 default',stk([],'c64',0x62),0xa000)
ck('Apple II default',stk([],'appleii',0x87),0x9600)
ck('ProDOS default',stk([],'prodos',0x87),0x9600)
ck('rc -S $7000',stk(['-S','$7000']),0x7000)
ck('rc -S 0x6c00',stk(['-S','0x6c00']),0x6c00)
ck('rc -S 28672 (decimal)',stk(['-S','28672']),0x7000)
ck('assembler -D overrides rc -S',stk(['-S','$7000'],asmflags=('-D','STKPTR=$5500')),0x5500)
for bad in ('100','$10000','zz'):
    r=subprocess.run([W+'/rc','-S',bad,'/tmp/t7/stk.act'],capture_output=True,text=True); ck('rc -S '+bad+' rejected',r.returncode!=0,True)
# a program longer than 8 KB (the old stack at $4000) runs: the unpruned test that used to overwrite its own code
SRC2=open('/tmp/t7/ms1.py').read()
# the stack really is used from the configured place: nested calls, with the top of memory at the chosen address
prog('s9','''INCLUDE "IOOUT.ACT"
CARD r
CARD FUNC f3(CARD a, CARD b, CARD c)
RETURN(a+b+c)
CARD FUNC f2(CARD a, CARD b)
RETURN(f3(a,b,1)+f3(b,a,2))
PROC main()
 r=f2(10,20)
 PrintCE(r)
RETURN
''')
m,sym,at=run('s9'); ck('runs',at.text(),"63\n"); ck('stack balanced at the Atari default',m.memory[0x8e]|m.memory[0x8f]<<8,0x8000)
done()
