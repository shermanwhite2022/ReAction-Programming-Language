# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
exec(open('/tmp/t7/jy1.py').read().split("SRC='''")[0].replace("from iorun import *",""))
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
PL=0xbc00
class DOS:
    def __init__(s,files=None):
        s.files={k:bytearray(v) for k,v in (files or {}).items()}; s.open={}; s.log=[]
    def nm(s,m,a):
        raw=bytes(m.memory[a:a+30])
        assert all(b>=0x80 for b in raw),("name needs bit 7",raw)
        return bytes(b&0x7f for b in raw).decode().rstrip(' ')
    def fm(s,m):
        mem=m.memory; P=lambda o:mem[PL+o]; W=lambda o:mem[PL+o]|mem[PL+o+1]<<8
        op=P(0); rc=0; X=m.x
        for o in (0x0c,0x0e,0x10): assert mem[PL+o]==0,"buffers not page aligned"
        buf=(mem[PL+0x0d],mem[PL+0x0f],mem[PL+0x11]); assert len(set(buf))==3 and 0 not in buf
        key=W(0x0c)
        def setpos(f): mem[PL+2]=0;mem[PL+3]=0;mem[PL+4]=f['pos']&255;mem[PL+5]=f['pos']>>8
        s.log.append((op,P(1)))
        if op in (1,5,9):
            assert P(5) in (1,2) and P(6)==6,"drive/slot"
            name=s.nm(m,W(8))
        if op==1:
            assert P(4)==0 and W(2)==0
            if name not in s.files:
                if X!=0: rc=6
                else: s.files[name]=bytearray()
            if rc==0: s.open[key]={'n':name,'pos':0,'placed':False}
        elif op==2: s.open.pop(key,None)
        elif op in (3,4):
            f=s.open[key]; assert f['placed'],"READ/WRITE before POSITION"
            d=s.files[f['n']]; sub=P(1)
            if op==3 and sub==1:
                if f['pos']<len(d): mem[PL+8]=d[f['pos']]; f['pos']+=1
                else: rc=5
            elif op==3 and sub==2:
                cnt=W(6); a=W(8); got=d[f['pos']:f['pos']+cnt]
                mem[a:a+len(got)]=list(got); f['pos']+=len(got)
                if len(got)<cnt: rc=5
            elif op==4 and sub==1:
                d[f['pos']:f['pos']+1]=bytes([P(8)]); f['pos']+=1
            elif op==4 and sub==2:
                cnt=W(6)+1; a=W(8); d[f['pos']:f['pos']+cnt]=bytes(mem[a:a+cnt]); f['pos']+=cnt
            else: raise Exception("subcode %d"%sub)
            setpos(f)
        elif op==10:
            f=s.open[key]; assert W(2)==0; f['pos']=W(4); f['placed']=True
        elif op==5:
            if name in s.files: del s.files[name]
            else: rc=6
        elif op==9:
            new=s.nm(m,W(2))
            if name in s.files: s.files[new]=s.files.pop(name)
            else: rc=6
        else: raise Exception("op %d"%op)
        mem[PL+0x0a]=rc; m.p=(m.p&~1)|(1 if rc else 0)
def run_dos(src,files=None,tag='dos1'):
    build(src,tag,'appleii'); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[4:]
    m=MPU(); mem=m.memory; mem[0x6000:0x6000+len(d)]=list(d)
    mem[0x8e]=0; mem[0x8f]=0x40
    dos=DOS(files); out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=0x6000; n=0
    while m.pc!=ret+1 and n<20000000:
        pc=m.pc
        if pc==0x3dc: m.y=PL&255; m.a=PL>>8; rts(m); n+=1; continue
        if pc==0x3d6: dos.fm(m); rts(m); n+=1; continue
        if pc==0xfded: out.append(m.a&0x7f); rts(m); n+=1; continue
        if pc==0x3d0: break
        m.step(); n+=1
    assert n<20000000,"timeout"
    return bytes(out).decode('latin1'),dos
SRC='''INCLUDE "IO.ACT"
BYTE ARRAY line(40), buf(20)
CARD s1,s2,sec,n
BYTE o1,o2,off,e
PROC main()
 Open(1,"D:T",8,0)
 PrintDE(1,"alpha")
 Note(1,@s1,@o1)
 PrintDE(1,"beta")
 Close(1)
 Open(1,"T",4,0)
 Point(1,s1,o1)
 InputSD(1,line)
 PrintE(line)
 InputSD(1,line)
 PrintE(line)
 PrintBE(EOF(1))
 Close(1)
 Open(1,"D2:T",4,0)
 n=BGet(1,buf,4)
 PrintCE(n)
 PrintCE(buf(0)) PrintCE(buf(3))
 n=BGet(1,buf,20)
 PrintCE(n)
 Close(1)
 Open(2,"U",8,0)
 buf(0)=65 buf(1)=66 buf(2)=67
 BPut(2,buf,3)
 Close(2)
 Open(2,"T",9,0)
 PrintDE(2,"gamma")
 Close(2)
 Open(1,"T",4,0)
 Note(1,@s2,@o2)
 PrintCE(s2) PrintCE(o2)
 Close(1)
 Open(1,"U",12,0)
 buf(0)=90
 BPut(1,buf,1)
 Close(1)
 Open(1,"U",4,0)
 n=BGet(1,buf,3)
 Note(1,@s2,@o2)
 PrintCE(buf(0)) PrintCE(buf(1)) PrintCE(s2) PrintCE(o2)
 Point(1,0,1)
 n=BGet(1,buf,2)
 PrintCE(buf(0)) PrintCE(n)
 Close(1)
 Rename("T","v")
 Delete("D:U")
 Delete("NOPE")
 Print("not reached")
RETURN
'''
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
out,dos=run_dos(SRC)
ck('output',out.replace('\r','\n'),"beta\n\n1\n4\n225\n232\n7\n0\n0\n90\n66\n0\n3\n66\n2\nERROR 170\n")
ck('files',sorted(dos.files),['V'])
ck('V',bytes(dos.files['V']),bytes(b|0x80 for b in b'alpha\rbeta\rgamma\r'))
print("bad",bad); sys.exit(bad)
