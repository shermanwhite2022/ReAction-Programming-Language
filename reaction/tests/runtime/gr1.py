# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
import iorun
from iorun import *
W_,H_,C_=[40,20,20,40,80,80,160,160,320,80,80,80,40,40,160,160],[24,24,12,24,48,48,96,96,192,192,192,192,24,12,192,192],None
class GAtari(iorun.Atari):
    def __init__(s,*a,**k):
        super().__init__(*a,**k); s.scr=None; s.px={}; s.glog=[]; s.mode=None
    def cio(s):
        m=s.m; mem=m.memory; x=m.x
        n=x>>4; base=0x340+x
        if n==6:
            cmd=mem[base+2]; st=1; ret=m.a
            row=mem[0x54]; col=mem[0x55]|mem[0x56]<<8
            if cmd==3:
                nm=bytes(mem[mem[base+4]|mem[base+5]<<8:][:2]); assert nm==b'S:',nm
                s.mode=mem[base+10]&15; s.aux1=mem[base+10]; s.aux2=mem[base+11]; s.px={}; s.glog.append(('open',s.aux1,s.aux2))
                s.mode=s.aux2
            elif cmd==12: s.glog.append(('close',))
            else:
                w,h=W_[s.mode],H_[s.mode]
                if col>=w or row>=h: st=141
                elif cmd==11: assert mem[base+8]==0 and mem[base+9]==0; s.px[(col,row)]=m.a; s.glog.append(('put',col,row,m.a))
                elif cmd==7: ret=s.px.get((col,row),0)
                elif cmd in (17,18):
                    assert mem[base+10]==12
                    oc=mem[0x5b]|mem[0x5c]<<8; orow=mem[0x5a]
                    s.glog.append(('draw' if cmd==17 else 'fill',oc,orow,col,row,mem[0x2fb],mem[0x2fd]))
                    if cmd==17:
                        dx=abs(col-oc); dy=-abs(row-orow); sx=1 if oc<col else -1; sy=1 if orow<row else -1; e=dx+dy; x0,y0=oc,orow
                        s.px[(x0,y0)]=mem[0x2fb]
                        while (x0,y0)!=(col,row):
                            e2=2*e
                            if e2>=dy: e+=dy; x0+=sx
                            if e2<=dx: e+=dx; y0+=sy
                            s.px[(x0,y0)]=mem[0x2fb]
                else: st=132
            m.a=ret; m.y=st
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
            return
        return super().cio()
iorun.Atari=GAtari
_load=iorun.load
def load2(p):
    m=_load(p); st=m.step; c=[0]
    def step():
        c[0]+=1
        if c[0]%500==0: m.memory[0x14]=(m.memory[0x14]+1)&255
        return st()
    m.step=step; return m
iorun.load=load2
SRC='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
INCLUDE "SOUND.ACT"
PROC main()
 Graphics(8+16)
 color=1
 SetColor(2,9,4)
 SetColor(1,9,14)
 PrintCE(GrWidth()) PrintCE(GrHeight()) PrintCE(GrColors())
 Plot(10,20)
 Position(5,5)
 DrawTo(50,60)
 PrintCE(Locate(10,20))
 PrintCE(Locate(50,60))
 color=2
 Fill(100,100)
 Sound(1,100,10,8)
 Tone(50,3)
 Graphics(7)
 PrintCE(GrWidth()) PrintCE(GrHeight()) PrintCE(GrColors())
 Plot(400,10)
 Print("not reached")
RETURN
'''
prog('gr1',SRC)
m,sym,at=run('gr1',pre={0x14:0})
mem=m.memory
ck('exit via error',at.exited,True)
ck('text',at.text(),"3201922\n1\n1\n160804\nERROR 141\n".replace("3201922","320\n192\n2").replace("160804","160\n80\n4").replace("\n1\n1\n","\n1\n1\n"))
ck('palette',(mem[0x2c6],mem[0x2c5]),(0x94,0x9e))
ck('log0',at.glog[:3],[('close',),('open',28,8),('put',10,20,1)])
ck('draw',[g for g in at.glog if g[0]=='draw'],[('draw',5,5,50,60,1,1)])
ck('fill',[g for g in at.glog if g[0]=='fill'],[('fill',50,60,100,100,2,2)])
ck('graphics 7',[g for g in at.glog if g[0]=='open'][1],('open',12,7))
ck('pokey',(mem[0xd202],mem[0xd203],mem[0xd200],mem[0xd201]),(100,0xa8,50,0))
done()
