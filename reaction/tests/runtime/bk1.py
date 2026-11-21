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
src=open('/tmp/bk/breakout.act').read()
sym=build(src,'breakout')
m=iorun.load('/tmp/t7/breakout.xex'); mem=m.memory
mem[0x8e]=0; mem[0x8f]=0x80; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
mem[0x2fc]=0xff; mem[0xd20f]=0xff; mem[0xd014]=15; mem[0x270]=100; mem[0x278]=1; mem[0x284]=1
at=GA(m,None,b'')
ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
m.pc=0x2000; n=0
WA=sym.get('ms_wait'); wc=0
from PIL import Image
pal={0:(0,0,0),1:(200,30,30),2:(240,240,240),3:(130,130,130)}
def shot(name):
    im=Image.new('RGB',(160,96))
    for y in range(96):
        for x in range(160):
            v=(mem[SCR+y*40+(x>>2)]>>(6-2*(x&3)))&3
            im.putpixel((x,y),pal[v])
    im.resize((640,384),Image.NEAREST).save(name)
events=[(2_000_000,'key',0x05),(6_000_000,'key',0x21)]   # K, then space
hold=[(9_000_000,0x22,40_000_000)]
shots=[int(a) for a in sys.argv[1:]] or [20]
t0=time.time(); started=None
while n<500_000_000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    if m.pc==WA: wc+=1
    c=m.processorCycles
    while events and c>=events[0][0]:
        mem[0x2fc]=events.pop(0)[2]
    mem[0xd20f]=0xff
    for (a,code,b) in hold:
        if a<=c<b: mem[0xd20f]=0xfb; mem[0xd209]=code
    m.step(); n+=1
    if GA.opened and started is None: started=c
    if started and shots and wc>=shots[0]:
        shot('/tmp/bk/s%d.png'%shots[0]); print("shot",shots[0],"wc",wc,"cycles",c); lastc=globals().get("lastc"); lastw=globals().get("lastw"); print("  per wait since previous shot:",(c-lastc)/(wc-lastw) if lastc else "-"); lastc=c; lastw=wc; shots.pop(0)
    if not shots: break
print([mem[0x6c00+i] for i in (0,1,2,3,4,5,6,100,101,102,103)], mem[0x230], mem[0x231]); print(repr(at.text()[-80:]))
