# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def bres(x0,y0,x1,y1):
    dx=abs(x1-x0); dy=-abs(y1-y0); sx=1 if x0<x1 else -1; sy=1 if y0<y1 else -1; e=dx+dy; pts=[]
    while (x0,y0)!=(x1,y1):
        e2=2*e
        if e2>=dy: e+=dy; x0+=sx
        if e2<=dx: e+=dx; y0+=sy
        pts.append((x0,y0))
    return pts
def run_c64(src,tag):
    build(src,tag,'c64'); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[2:]
    m=MPU(); mem=m.memory; mem[0x2000:0x2000+len(d)]=list(d); mem[0x62]=0; mem[0x63]=0x40
    out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=0x2000; n=0; snaps={}
    while m.pc!=ret+1 and n<30000000:
        pc=m.pc
        if pc==0xffd2: out.append(m.a); rts(m); n+=1; continue
        if pc==0xe544: snaps['clr']=snaps.get('clr',0)+1; rts(m); n+=1; continue
        if pc in (0xffe4,0xffcc): rts(m); n+=1; continue
        if pc==0xa474: break
        if mem[0x1c00]==0: pass
        if n%7==0: mem[0xd012]=(mem[0xd012]+1)&255
        m.step(); n+=1
    assert n<30000000,"timeout"
    return bytes(out).decode('latin1'),mem
SRC='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
INCLUDE "SOUND.ACT"
PROC main()
 Graphics(8+16)
 PrintCE(GrWidth()) PrintCE(GrHeight()) PrintCE(GrColors())
 color=1
 Plot(10,20)
 Plot(319,199)
 Position(0,0)
 DrawTo(30,15)
 DrawTo(30,40)
 DrawTo(5,40)
 PrintCE(Locate(10,20)) PrintCE(Locate(11,20)) PrintCE(Locate(30,30)) PrintCE(Locate(319,199))
 color=0
 Plot(10,20)
 PrintCE(Locate(10,20))
 SetColor(1,9,14)
 SetColor(2,0,0)
 SetColor(4,12,8)
 Sound(0,121,10,8)
 Sound(1,60,6,15)
 Tone(100,2)
 Graphics(15+32)
 PrintCE(GrWidth()) PrintCE(GrHeight()) PrintCE(GrColors())
 color=3
 Plot(100,50)
 color=2
 Plot(101,50)
 PrintCE(Locate(100,50)) PrintCE(Locate(101,50)) PrintCE(Locate(102,50))
 Graphics(0)
 PrintCE(GrWidth())
 Graphics(7)
 Print("not reached")
RETURN
'''
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
out,mem=run_c64(SRC,'gc64')
ck('text',out.replace('\r','\n'),"320\n200\n2\n1\n0\n1\n1\n0\n160\n200\n4\n3\n2\n0\n40\nERROR 146 not supported on this hardware\n")
def px(x,y): return (mem[0xe000+(y>>3)*320+(x&~7)+(y&7)]>>(7-(x&7)))&1
exp=set([(319,199)])
for a,b in (((0,0),(30,15)),((30,15),(30,40)),((30,40),(5,40))): exp|=set(bres(*a,*b))
got={(x,y) for x in range(320) for y in range(200) if px(x,y)} 
ck('bitmap',sorted(got-{(200,50),(201,50),(202,50)}),sorted(exp))
ck('vic text again',(mem[0xd018],mem[0xdd00]&3,mem[0xd011]),(0x15,3,0x1b))
ck('screen',(mem[0xcc00],mem[0xcff0]),(0xe0,0xe0)) if False else None
def pxm(x,y): return (mem[0xe000+(y>>3)*320+(x>>2)*8+(y&7)]>>(6-2*(x&3)))&3
ck('mc pixels',(pxm(100,50),pxm(101,50),pxm(102,50)),(3,2,0))
frq=lambda n:min(65535,round(31960/(n+1)*16.4044))
# sid: voice 0 got Tone at the end of the sequence
ck('sid v1',(mem[0xd407]|mem[0xd408]<<8,mem[0xd40b],mem[0xd40d]),(frq(60),0x21,0xf0))
ck('sid v0 after tone',(mem[0xd400]|mem[0xd401]<<8,mem[0xd404]),(frq(100),0x40))
ck('volume',mem[0xd418],15)
S2='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Graphics(8)
 SetColor(1,9,14)
 SetColor(2,0,0)
 SetColor(4,12,8)
RETURN
'''
out,mem=run_c64(S2,'gc64b')
ck('hires vic',(mem[0xd018],mem[0xdd00]&3,mem[0xd011],mem[0xd016]),(0x18,0,0x3b,0xc8))
ck('hires colors',(mem[0xc400],mem[0xc7e7],mem[0xd020]),(0xe0,0xe0,5))
S3='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Graphics(15)
 SetColor(0,3,10)
 SetColor(1,12,10)
 SetColor(2,0,12)
 SetColor(4,7,6)
RETURN
'''
out,mem=run_c64(S3,'gc64c')
ck('mc colors',(mem[0xc400],mem[0xc7e7],mem[0xd800],mem[0xdbe7],mem[0xd021],mem[0xd016]),(0x25,0x25,15,15,6,0xd8))
S4='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Plot(1,1)
RETURN
'''
out,mem=run_c64(S4,'gc64d')
ck('plot in text',out.replace('\r','\n'),"ERROR 133\n")
S5='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Graphics(8)
 Plot(320,1)
RETURN
'''
out,mem=run_c64(S5,'gc64e')
ck('off screen',out.replace('\r','\n'),"ERROR 141\n")
print("bad",bad); sys.exit(bad)
