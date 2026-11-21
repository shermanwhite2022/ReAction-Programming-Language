# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='''INCLUDE "MISC.ACT"
INCLUDE "IO.ACT"
PROC main()
 PrintBE(Stick(0))
 PrintBE(STrig(0))
 PrintBE(Paddle(1))
 PrintBE(PTrig(1))
 PrintBE(Stick(9))
 PrintE("not reached")
RETURN
'''
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def other(target,tag,pre,hooks,stops,hi=False,skip=2):
    build(SRC,tag,target); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[skip:]
    m=MPU(); mem=m.memory; ORG=0x2000 if target=='c64' else 0x6000; mem[ORG:ORG+len(d)]=list(d)
    rs={'c64':0x62,'prodos':0x87,'appleii':0x87}[target]; mem[rs]=0; mem[rs+1]=0x40
    for a,v in pre.items(): mem[a]=v
    out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=ORG; n=0
    while m.pc!=ret+1 and n<5000000:
        pc=m.pc
        if pc in stops: break
        if pc in hooks: hooks[pc](m,out); rts(m); n+=1; continue
        m.step(); n+=1
    return bytes(out).decode('latin1'),n<5000000
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
def main():
    global bad
    # Atari
    prog('jy1',SRC)
    m,sym,at=run('jy1',pre={0x278:11,0x284:0,0x271:100,0x27d:1})
    ck('atari',at.text(),"11\n0\n100\n1\nERROR 146 not supported on this hardware\n")
    # C64
    h={0xffd2:lambda m,o:o.append(m.a), 0xffcc:lambda m,o:None}
    o,fin=other('c64','jc1',{0xdc00:0xeb,0xdc01:0xf7,0xd41a:100},h,{0xa474})
    ck('c64',o,"11\r0\r100\r0\rERROR 146 not supported on this hardware\r")
    def pread(m,o): m.y={0:30,1:200,2:128,3:128}[m.x]
    h={0xfded:lambda m,o:o.append(m.a&0x7f), 0xfb1e:pread}
    for t,stop in (('appleii',0x3d0),('prodos',0xbf00)):
        o,fin=other(t,'ja1'+t,{0xc061:0x80,0xc062:0},h,{stop},skip=4 if t=='appleii' else 0)
        ck(t,o,"9\r0\r200\r1\rERROR 146 not supported on this hardware\r")
    print("bad",bad); sys.exit(bad)


if __name__=='__main__': main()
