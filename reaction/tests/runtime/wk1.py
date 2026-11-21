# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('wk1','''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
PROC main()
 CARD t0, t1
 BYTE k
 k=KeyHit()
 PrintBE(k)
 t0=Ticks()
 Wait(5)
 t1=Ticks()
 PrintCE(t1-t0)
 k=KeyHit()
 PrintBE(k)
 k=KeyHit()
 PrintBE(k)
RETURN
''')
import iorun
orig=iorun.load
def mk(m):
    return m
m=None
# clock: RTCLOK $14 ticks every 300 steps; key table at $3000; a key waiting after 800 steps
import iorun as _i; src=_i._src['wk1']
sym=build(src,'wk1')
m=load('/tmp/t7/wk1.xex'); mem=m.memory
mem[0x8e]=0; mem[0x8f]=0x40; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
mem[0x79]=0; mem[0x7a]=0x30
for i in range(256): mem[0x3000+i]=i^0x40
mem[0x2fc]=0xff
at=Atari(m,None,b'')
ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0
while m.pc!=ret+1 and n<3000000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    if n%300==0: mem[0x14]=(mem[0x14]+1)&255
    if n==2000 and mem[0x2fc]==0xff: pass
    m.step(); n+=1
    if n==3000: mem[0x2fc]=0x0c   # a key becomes ready (code $0C -> table gives $4C)
print(repr(at.text()), n)
