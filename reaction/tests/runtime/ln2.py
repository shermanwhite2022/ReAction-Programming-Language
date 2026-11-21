# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,time; sys.path.insert(0,'/tmp/t7')
import iorun
from iorun import *
SCR=0xad20
class GA(iorun.Atari):
    opened=None
    def cio(s):
        m=s.m; mem=m.memory; x=m.x; n=x>>4; base=0x340+x
        if n==6:
            cmd=mem[base+2]; st=1
            if cmd==3:
                mem[0x58]=SCR&255; mem[0x59]=SCR>>8; GA.opened=m.processorCycles
                a=0x9F00; i=a
                for b in (0x70,0x70,0x70): mem[i]=b; i+=1
                mem[i]=0x4D; mem[i+1]=SCR&255; mem[i+2]=SCR>>8; i+=3
                for k in range(79): mem[i]=0x0D; i+=1
                w=SCR+3840; mem[i]=0x42; mem[i+1]=w&255; mem[i+2]=w>>8; mem[i+3]=2; mem[i+4]=2; mem[i+5]=2; i+=6
                mem[i]=0x41; mem[i+1]=a&255; mem[i+2]=a>>8
                mem[0x230]=a&255; mem[0x231]=a>>8
            m.a=m.a; m.y=st
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
            return
        return super().cio()
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
src=open('/tmp/ln/lander.act').read()
sym=build(src,'lander')
m=iorun.load('/tmp/t7/lander.xex'); mem=m.memory
[mem.__setitem__(i_,0xAA) for i_ in range(sym['zbstart'],sym['zbend'])]; print('BSS',hex(sym['zbstart']),hex(sym['zbend']))
mem[0x8e]=0; mem[0x8f]=0x80; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
mem[0x2fc]=0xff; mem[0xd20f]=0xff; mem[0xd014]=15; mem[0x270]=100; mem[0x278]=1; mem[0x284]=1
at=GA(m,None,b'')
ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0
WA=sym.get('ms_wait'); wc=0
mem[WA]=0x60   # Wait returns at once: the time between the calls is the work of a frame
from PIL import Image
pal={0:(0,0,0),1:(200,30,30),2:(240,240,240),3:(130,130,130)}
def shot(name):
    im=Image.new('RGB',(160,96))
    for y in range(96):
        for x in range(160):
            v=(mem[SCR+y*40+(x>>2)]>>(6-2*(x&3)))&3
            im.putpixel((x,y),pal[v])
    im.resize((640,384),Image.NEAREST).save(name)
events=[(2_000_000,'key',0x05)]
wk={60:0x21,90:0x21,120:0x21,140:0x16,150:0x16,200:0x21,230:0x21,260:0x21}
hold=[]
shots=[int(a) for a in sys.argv[1:]] or [20]
t0=time.time(); started=None
while n<500_000_000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    if m.pc==WA:
        wc+=1
        if wc in wk: mem[0x2fc]=wk[wc]
    c=m.processorCycles
    while events and c>=events[0][0]:
        mem[0x2fc]=events.pop(0)[2]
    mem[0xd20f]=0xff
    for (a,code,b) in hold:
        if a<=c<b: mem[0xd20f]=0xfb; mem[0xd209]=code
    m.step(); n+=1
    if GA.opened and started is None: started=c
    if started and shots and wc>=shots[0]:
        shot('/tmp/ln/s%d.png'%shots[0]); print("shot",shots[0],"wc",wc,"cycles",c); lastc=globals().get("lastc"); lastw=globals().get("lastw"); print("  per wait since previous shot:",(c-lastc)/(wc-lastw) if lastc else "-"); lastc=c; lastw=wc; shots.pop(0)
    if not shots: break
print([mem[0x6c00+i] for i in (0,1,2,3,4,5,6,100,101,102,103)], mem[0x230], mem[0x231]); p=mem[0x230]|mem[0x231]<<8; n13=0; i=0
while mem[p+i]!=0x41:
    if mem[p+i]&15==13: n13+=1
    i+=3 if mem[p+i]&0x40 else 1
print('DL at',hex(p),'mode-7 lines',n13,'text lines',sum(1 for k in range(i) if mem[p+k]==2))
print(repr(at.text()[-80:]))
import os
sa=sym.get('varx00000316')
print('spr sym',sa, [k for k in sym if k.lower().startswith('spr')][:5])
if sa is not None:
    for a_ in (0,4):
      for ph in (0,):
        for f in (0,1):
            base=sa+((f*16+a_)*4+ph)*48
            print('f',f,'a',a_,'ph',ph)
            for y in range(12):
                print(''.join('.#o@'[(mem[base+y*4+(x>>2)]>>(6-2*(x&3)))&3] for x in range(16)))
