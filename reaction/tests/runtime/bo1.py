# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,time; sys.path.insert(0,'/tmp/t7')
import iorun
from iorun import *
exec(open('/tmp/t7/gr1.py').read().split("iorun.Atari=GAtari")[0].split("class GAtari")[0])
class GA(iorun.Atari):
    opened=None
    def cio(s):
        m=s.m; mem=m.memory; x=m.x; n=x>>4; base=0x340+x
        if n==6:
            cmd=mem[base+2]; st=1; ret=m.a
            if cmd==3:
                mem[0x58]=0x20; mem[0x59]=0xad; GA.opened=m.processorCycles
                s.aux2=mem[base+11]
            elif cmd==11:
                row=mem[0x54]; col=mem[0x55]|mem[0x56]<<8
                # mode 7: 160x96, 2 bits per pixel, 40 bytes per row
                a=0xad20+row*40+(col>>2); sh=6-2*(col&3)
                mem[a]=(mem[a]&~(3<<sh))|((m.a&3)<<sh)
            elif cmd==17:
                oc=mem[0x5b]|mem[0x5c]<<8; orow=mem[0x5a]; col=mem[0x55]|mem[0x56]<<8; row=mem[0x54]
                c=mem[0x2fb]&3
                x0,y0=oc,orow; dx=abs(col-oc); dy=-abs(row-orow); sx=1 if oc<col else -1; sy=1 if orow<row else -1; e=dx+dy
                while True:
                    a=0xad20+y0*40+(x0>>2); sh=6-2*(x0&3); mem[a]=(mem[a]&~(3<<sh))|(c<<sh)
                    if (x0,y0)==(col,row): break
                    e2=2*e
                    if e2>=dy: e+=dy; x0+=sx
                    if e2<=dx: e+=dx; y0+=sy
            m.a=ret; m.y=st
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
            return
        r=super().cio()
        if n==0: print('E:',repr(bytes(s.out)[-12:]),m.processorCycles)
        return r
iorun.Atari=GA
cnt=[0]
_load=iorun.load
def load2(p):
    m=_load(p); st=m.step
    def step():
        cnt[0]+=1
        if cnt[0]%7000==0: m.memory[0x14]=(m.memory[0x14]+1)&255
        return st()
    m.step=step; return m
iorun.load=load2
src=open('/tmp/bo/boing.act').read()
sym=build(src,'boing')
m=iorun.load('/tmp/t7/boing.xex'); mem=m.memory
mem[0x8e]=0; mem[0x8f]=0x80; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
at=GA(m,None,b'')
ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0; t0=time.time()
from PIL import Image
import colorsys
def atari_rgb(v):
    hue=v>>4; lum=(v&15)/15.0
    if hue==0: return tuple(int(255*lum) for _ in range(3))
    h=((hue-1)/15.0*360+30)%360   # rough NTSC hue wheel: 1 gold, 3 red-orange, 4 pink, 7 blue, 9 cyan, 12 green
    r,g,b=colorsys.hls_to_rgb(h/360,0.15+0.7*lum,0.7)
    return (int(r*255),int(g*255),int(b*255))
def getpal():
    return {0:atari_rgb(mem[0x2c8]),1:atari_rgb(mem[0x2c4]),2:atari_rgb(mem[0x2c5]),3:atari_rgb(mem[0x2c6])}
def shot(name):
    pal=getpal()
    im=Image.new('RGB',(160,96))
    for y in range(96):
        for x in range(160):
            v=(mem[0xad20+y*40+(x>>2)]>>(6-2*(x&3)))&3
            im.putpixel((x,y),pal[v])
    im.resize((640,384),Image.NEAREST).save(name)
frames=[int(a) for a in sys.argv[1:]] or [60]
wc=[0]; WA=sym.get('ms_wait')
shots=[]
while n<400_000_000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    if m.pc==EXIT: break
    if m.pc==WA: wc[0]+=1
    m.step(); n+=1
    if GA.opened and 'start' not in globals():
        start=m.processorCycles; print("precompute cycles",GA.opened,"= %.1f s at 1.79MHz"%(GA.opened/1.79e6), "steps",n, "wall %.0f"%(time.time()-t0)); sys.stdout.flush()
    if GA.opened and frames and m.processorCycles-GA.opened>frames[0]*29780:
        shot('/tmp/bo/f%d.png'%frames[0]); print('shot',frames[0],'wait calls',wc[0]); frames.pop(0)
    if not frames: break
print("pc",hex(m.pc),"y?",mem[0x14],"n",n)
inv={v:k for k,v in sym.items()}
import bisect
ks=sorted(inv); print(inv[ks[bisect.bisect_right(ks,m.pc)-1]])
