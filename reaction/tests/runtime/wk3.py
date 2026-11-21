# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('wk3','''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
PROC main()
 BYTE k, i
 FOR i=1 TO 8
 DO
  k=KeyHit()
  WHILE k=0
  DO
   k=KeyHit()
  OD
  PrintBE(k)
 OD
RETURN
''')
import iorun
sym=build(iorun._src['wk3'],'wk3')
m=load('/tmp/t7/wk3.xex'); mem=m.memory
mem[0x8e]=0; mem[0x8f]=0x40; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
mem[0x2fc]=0xff
codes=[0x3f,0x7f,0xbf,0x21,0x00,0x0c,0x1f,0x32]
at=Atari(m,None,b'')
ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0; ci=0
while m.pc!=ret+1 and n<3000000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    if mem[0x2fc]==0xff and n%500==0 and ci<len(codes): mem[0x2fc]=codes[ci]; ci+=1
    m.step(); n+=1
got=[int(x) for x in at.text().split()]
ck('keys',got,[97,65,1,32,108,155,49,48])
done()
