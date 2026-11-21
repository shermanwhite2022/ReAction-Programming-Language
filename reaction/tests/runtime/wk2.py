# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
exec(open('/tmp/t7/gap.py').read().split("bad=0")[0])
from iorun import build, MPU
import iorun
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
SRC='''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
PROC main()
 CARD t0, t1
 BYTE k
 k=KeyHit()
 PrintBE(k)
 Wait(5)
 k=KeyHit()
 PrintBE(k)
 k=KeyHit()
 PrintBE(k)
 t0=Ticks()
 PrintCE(1)
RETURN
'''
# C64
SRC2=SRC.replace(" t0=Ticks()\n PrintCE(1)\n"," t0=Ticks()\n Wait(7)\n t1=Ticks()\n PrintCE(t1-t0)\n")
build(SRC2,'wk2c','c64'); d=open('/tmp/t7/wk2c.xex','rb').read()[2:]
m=MPU(); mem=m.memory; mem[0x2000:0x2000+len(d)]=list(d); mem[0x62]=0; mem[0x63]=0x40
out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0; keys=[0,0x41,0]; kcalls=0
while m.pc!=ret+1 and n<5000000:
    pc=m.pc
    if pc==0xffd2: out.append(m.a); rts(m); n+=1; continue
    if pc==0xffcc: rts(m); n+=1; continue
    if pc==0xffe4:
        m.a=keys[min(kcalls,2)]; kcalls+=1; rts(m); n+=1; continue
    if pc==0xa474: break
    if n%300==0:
        mem[0xa2]=(mem[0xa2]+1)&255
        if mem[0xa2]==0: mem[0xa1]=(mem[0xa1]+1)&255
    m.step(); n+=1
got=bytes(out).decode('latin1').replace('\r','\n')
# the fake clock ticks every 300 steps, so a Wait(7) can span 7 or 8 ticks depending on code speed/phase
ck('c64',got if got in ("0\n65\n0\n7\n","0\n65\n0\n8\n") else got,"0\n65\n0\n7\n" if got.endswith("7\n") else "0\n65\n0\n8\n")
# Apple
build(SRC,'wk2a','appleii')
S3=SRC.replace(" t0=Ticks()\n PrintCE(1)\n","")
out,mem,sw,clicks,m=run_apple(S3,'wk2b')
ck('apple keys (no key)',out.replace('\r','\n'),"0\n0\n0\n")
# apple with a key at $C000 and cycle count of Wait
S4='''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
PROC main()
 BYTE k
 Wait(60)
 k=KeyHit()
 PrintBE(k)
 k=KeyHit()
 PrintBE(k)
 t=Ticks()
RETURN
'''.replace(" t=Ticks()\n","")
import subprocess
build(S4,'wk2d','appleii'); d=open('/tmp/t7/wk2d.xex','rb').read()[4:]
m=MPU(); mem=m.memory; mem[0x6000:0x6000+len(d)]=list(d); mem[0x87]=0; mem[0x88]=0x40
out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x6000; n=0; c0=None; c1=None; strobe=1; mem[0xc000]=0xc1
while m.pc!=ret+1 and n<8000000:
    pc=m.pc
    if pc==0xfded: out.append(m.a&0x7f); rts(m); n+=1; continue
    op=mem[pc]
    if op==0x2c and mem[pc+1]==0x10 and mem[pc+2]==0xc0: mem[0xc000]=0x41   # clear strobe: bit 7 off
    m.step(); n+=1
    if c0 is None and n>5: c0=m.processorCycles
cyc=m.processorCycles
ck('apple key',bytes(out).decode('latin1').replace('\r','\n'),"65\n0\n")
print("cycles for Wait(60) + rest:",cyc, "per frame ~", (cyc-0)/60)
ck('apple Wait length', 60*17045*0.98 < cyc < 60*17045*1.03+20000, True)
# Ticks on Apple: error 146
S5='INCLUDE "IO.ACT"\nINCLUDE "MISC.ACT"\nPROC main()\n CARD t\n t=Ticks()\nRETURN\n'
out,mem,sw,clicks,m=run_apple(S5,'wk2e')
ck('apple ticks',out,"ERROR 146 not supported on this hardware\r")
print("bad",bad); sys.exit(bad)
