# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# fs1.py - Graphics(n+16) full screen fix: the display list after the OS opened the screen
import sys
sys.path.insert(0,'/tmp/t7')
import iorun
from iorun import *

ANT={1:6,2:7,3:8,4:9,5:10,6:11,7:13,8:15,9:15,10:15,11:15,15:14}
FULL={1:24,2:12,3:24,4:48,5:48,6:96,7:96,8:192,9:192,10:192,11:192,15:192}
WIN ={1:20,2:10,3:20,4:40,5:40,6:80,7:80,8:160,9:160,10:160,11:160,15:160}
BPL ={1:20,2:20,3:10,4:10,5:20,6:20,7:40,8:40,9:40,10:40,11:40,15:40}
DL=0xAFA2
START={'default':0xB060,8:0x9072,9:0x9072,10:0x9072,11:0x9072,15:0x9072}
MODE={'buggy':True}

def osdl(mem,mode,start,buggy):
    n=WIN[mode] if buggy else FULL[mode]
    a=start; i=DL
    for b in (0x70,0x70,0x70): mem[i]=b; i+=1
    mem[i]=0x40|ANT[mode]; mem[i+1]=a&255; mem[i+2]=a>>8; i+=3
    for k in range(1,n):
        na=a+BPL[mode]
        if (na&0xF000)!=((na+BPL[mode]-1)&0xF000) and False: pass
        a=na
        mem[i]=ANT[mode]; i+=1
    if buggy:
        mem[i]=0x42; w=start+FULL[mode]*BPL[mode]; mem[i+1]=w&255; mem[i+2]=w>>8
        mem[i+3]=2; mem[i+4]=2; mem[i+5]=2; i+=6
    mem[i]=0x41; mem[i+1]=DL&255; mem[i+2]=DL>>8
    mem[0x230]=DL&255; mem[0x231]=DL>>8

class GA(iorun.Atari):
    cfg=None
    def cio(s):
        m=s.m; mem=m.memory; x=m.x; n=x>>4; base=0x340+x
        if n==6:
            cmd=mem[base+2]; st=1
            if cmd==3:
                mode=mem[base+0x0b]&15
                start=START.get(mode,START['default'])
                mem[0x58]=start&255; mem[0x59]=start>>8
                osdl(mem,mode,start,GA.cfg)
                for k in range(0,0x1000): mem[(start+k)&0xffff]=0xFF if (start+k)<0xC000 else mem[(start+k)&0xffff]
            m.y=st
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
            return
        return super().cio()
iorun.Atari=GA

def run(mode,buggy,plus=16):
    GA.cfg=buggy
    src='INCLUDE "IO.ACT"\nINCLUDE "GRAPHICS.ACT"\nPROC Main()\n Graphics(%d)\nRETURN\n'%(mode+plus)
    sym=build(src,'fs')
    m=iorun.load('/tmp/t7/fs.xex'); mem=m.memory
    mem[0x8e]=0; mem[0x8f]=0x80; mem[10]=EXIT&255; mem[11]=EXIT>>8; mem[0xe456]=0x60
    at=GA(m,None,b'')
    ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=0x2000; n=0
    while n<2_000_000:
        if m.pc==0xe456: at.cio(); n+=1; continue
        if m.pc==0xbfff+1 or m.pc==EXIT: break
        m.step(); n+=1
    return mem

def walk(mem):
    p=mem[0x230]|mem[0x231]<<8; i=0; out=[]; lines=0; lms=[]; blanks=0; first=p
    while True:
        b=mem[p+i]
        if b==0x41:
            tgt=mem[p+i+1]|mem[p+i+2]<<8; return dict(dl=p,lines=lines,lms=lms,blank=blanks,jvb=tgt,len=i+3,text=out)
        if b==0x70: blanks+=1; i+=1; continue
        op=b&15
        if op==2 and not lines==0 or op==2 and out is not None and b in (2,0x42):
            out.append(b)
        else: lines+=1
        if b&0x40: lms.append(mem[p+i+1]|mem[p+i+2]<<8); i+=3
        else: i+=1
        if i>400: return None

bad=0
def check(name,cond):
    global bad
    print(("ok   " if cond else "FAIL ")+name)
    if not cond: bad+=1

for mode in (7,5,6,3,8,11,15):
    mem=run(mode,True)
    w=walk(mem)
    check("mode %d+16 buggy OS: %d lines (%s)"%(mode,w['lines'],'rebuilt' if w['dl']!=DL else 'NOT rebuilt'), w['lines']==FULL[mode] and w['dl']!=DL and not w['text'] and w['jvb']==w['dl'] and w['dl']&255==0)
    start=START.get(mode,START['default'])
    check("   first LMS is the OS's screen start $%04x"%start, w['lms'][0]==start)
    if mode in (8,11,15):
        check("   4K boundary has its own LMS: %s"%[hex(a) for a in w['lms']], len(w['lms'])==2 and w['lms'][1]==0xA000)
    # cleared: every line's bytes zero
    bad_clear=0
    a=start
    for k in range(FULL[mode]):
        ad=a
        if ((ad&0xF000)!=((ad+BPL[mode]-1)&0xF000)) : ad=(ad&0xF000)+0x1000
        if any(mem[ad+j]!=0 for j in range(BPL[mode])): bad_clear+=1
        a=ad+BPL[mode]
    check("   every line cleared", bad_clear==0)

mem=run(7,False); w=walk(mem)
check("mode 7+16 correct OS: list untouched (at $%04x, %d lines)"%(w['dl'],w['lines']), w['dl']==DL and w['lines']==96)
mem=run(7,True,0); w=walk(mem)
check("mode 7 (with window) buggy or not: list untouched, %d lines + text"%w['lines'], w['dl']==DL and w['lines']==80)
mem=run(7,True,16+32); w=walk(mem)
nz=sum(1 for k in range(96*40) if mem[0xB060+k]==0xFF)
check("mode 7+16+32 (no clear): list rebuilt, screen memory kept (%d of 3840 bytes still $FF)"%nz, w['lines']==96 and w['dl']!=DL and nz>3000)
print("failures",bad)
