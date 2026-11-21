# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def run_apple(src,tag,target='appleii',skip=4,limit=40000000):
    build(src,tag,target); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[skip:]
    m=MPU(); mem=m.memory; mem[0x6000:0x6000+len(d)]=list(d); mem[0x87]=0; mem[0x88]=0x40
    out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=0x6000; n=0; sw=[]; clicks=[]
    while m.pc!=ret+1 and n<limit:
        pc=m.pc
        if pc==0xfded: out.append(m.a&0x7f); rts(m); n+=1; continue
        if pc==0xfc58: sw.append('home'); rts(m); n+=1; continue
        if pc in (0x3d0,0xbf00): break
        op=mem[pc]
        if op==0xad:   # lda abs: soft switches / speaker
            a=mem[pc+1]|mem[pc+2]<<8
            if a==0xc030: clicks.append(m.processorCycles)
            elif 0xc050<=a<=0xc057: sw.append(a)
        m.step(); n+=1
    assert n<limit,"timeout"
    return bytes(out).decode('latin1'),mem,sw,clicks,m
def hgr_addr(y): return 0x2000+(y&7)*0x400+((y>>3)&7)*0x80+(y>>6)*40
SRC='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
INCLUDE "SOUND.ACT"
PROC main()
 Graphics(8+16)
 PrintCE(GrWidth()) PrintCE(GrHeight()) PrintCE(GrColors())
 color=1
 Plot(0,0)
 Plot(1,0)
 Plot(279,191)
 Plot(7,10)
 color=2
 Plot(100,100)
 Position(0,0)
 color=1
 DrawTo(40,30)
 PrintCE(Locate(7,10)) PrintCE(Locate(100,100)) PrintCE(Locate(101,100)) PrintCE(Locate(0,0))
 color=0
 Plot(7,10)
 PrintCE(Locate(7,10))
 Sound(0,1,10,8)
 Print("not reached")
RETURN
'''
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
def bres(x0,y0,x1,y1):
    dx=abs(x1-x0); dy=-abs(y1-y0); sx=1 if x0<x1 else -1; sy=1 if y0<y1 else -1; e=dx+dy; pts=[]
    while (x0,y0)!=(x1,y1):
        e2=2*e
        if e2>=dy: e+=dy; x0+=sx
        if e2<=dx: e+=dx; y0+=sy
        pts.append((x0,y0))
    return pts
for tg,skip in (('appleii',4),('prodos',0)):
    out,mem,sw,clicks,m=run_apple(SRC,'ga1'+tg,tg,skip)
    ck(tg+' text',out.replace('\r','\n'),"280\n192\n3\n1\n2\n0\n1\n0\nERROR 146 not supported on this hardware\n")
    def pix(x,y): 
        b=mem[hgr_addr(y)+x//7]; return (b>>(x%7))&1
    got={(x,y) for y in range(192) for x in range(280) if pix(x,y)}
    exp={(0,0),(1,0),(279,191),(100,100)}|set(bres(0,0,40,30))
    ck(tg+' pixels',sorted(got),sorted(exp))
    ck(tg+' palette bit',(mem[hgr_addr(100)+100//7]>>7,mem[hgr_addr(0)]>>7),(1,0))
    ck(tg+' switches',[a for a in sw if isinstance(a,int)][:4],[0xc050,0xc057,0xc054,0xc052])
# mixed mode, height 160, off-screen error, text mode error, tone timing
S2='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Graphics(8)
 PrintCE(GrHeight())
 Plot(5,159)
 Plot(5,160)
RETURN
'''
out,mem,sw,clicks,m=run_apple(S2,'ga2')
ck('mixed',out.replace('\r','\n'),"160\nERROR 141\n")
ck('mixed switch',[a for a in sw if isinstance(a,int)][:4],[0xc050,0xc057,0xc054,0xc053])
S3='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Plot(1,1)
RETURN
'''
out,mem,sw,clicks,m=run_apple(S3,'ga3')
ck('plot in text',out.replace('\r','\n'),"ERROR 133\n")
S4='''INCLUDE "IO.ACT"
INCLUDE "SOUND.ACT"
PROC main()
 Tone(100,30)
 SndRst()
 Tone(255,0)
RETURN
'''
out,mem,sw,clicks,m=run_apple(S4,'ga4')
dur=(clicks[-1]-clicks[0])
rate=len(clicks)/(dur/1.0227e6)/2
ck('tone clicks',len(clicks),len(clicks))
print("tone: %d clicks, %.3f s, %.1f Hz (wanted 0.5 s, 316.4 Hz)"%(len(clicks),dur/1.0227e6,rate))
ck('tone length',0.47<dur/1.0227e6<0.53,True)
ck('tone pitch',311<rate<322,True)
print("bad",bad); sys.exit(bad)
