# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='''INCLUDE "IO.ACT"
BYTE ARRAY line(40)
CARD total
PROC main()
 Open(1,"D:TEST",8,0)
 PrintDE(1,"first")
 PrintCDE(1,500)
 Close(1)
 Open(1,"TEST",9,0)
 PrintDE(1,"more")
 Close(1)
 Open(2,"TEST",4,0)
 total=0
 DO
  InputSD(2,line)
  IF EOF(2) THEN EXIT FI
  total==+1
  PrintE(line)
 OD
 Close(2)
 PrintCE(total)
 Open(1,"MISSING",4,0)
 Print("not reached")
RETURN
'''
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def setc(m,c): m.p=(m.p&~1)|(1 if c else 0)
def prodos():
    sym=build(SRC,'p4','prodos'); d=open('/tmp/t7/p4.xex','rb').read()
    m=MPU(); mem=m.memory; mem[0x6000:0x6000+len(d)]=list(d); mem[0x87]=0; mem[0x88]=0x40
    files={}; refs={}; out=bytearray(); nref=[0]
    ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    m.pc=0x6000; n=0; exited=False
    def w16(a): return mem[a]|mem[a+1]<<8
    while m.pc!=ret+1 and n<20000000:
        pc=m.pc
        if pc==0xfded: out.append(m.a&0x7f); rts(m); n+=1; continue
        if pc==0xbf00:
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; ra=lo|hi<<8
            cmd=mem[ra+1]; pl=w16(ra+2); m.sp+=2; m.pc=ra+4
            err=0
            def path(): 
                l=mem[w16(pl+1)]; return bytes(mem[w16(pl+1)+1:w16(pl+1)+1+l]).decode()
            if cmd==0xc0:
                p=path()
                if p in files: err=0x47
                else: files[p]=bytearray()
            elif cmd==0xc8:
                p=path()
                if p not in files: err=0x46
                else:
                    nref[0]+=1; refs[nref[0]]={'p':p,'pos':0}; mem[pl+5]=nref[0]
                    assert mem[w16(pl+3)] is not None and w16(pl+3)&0xff==0, 'buffer not page aligned'
            elif cmd==0xcb:
                r=refs[mem[pl+1]]; ln=w16(pl+4); b=bytes(mem[w16(pl+2):w16(pl+2)+ln])
                f=files[r['p']]; f[r['pos']:r['pos']+ln]=b; r['pos']+=ln; mem[pl+6]=ln&255; mem[pl+7]=ln>>8
            elif cmd==0xca:
                r=refs[mem[pl+1]]; ln=w16(pl+4); f=files[r['p']]; got=f[r['pos']:r['pos']+ln]
                mem[w16(pl+2):w16(pl+2)+len(got)]=list(got); r['pos']+=len(got); mem[pl+6]=len(got)&255; mem[pl+7]=len(got)>>8
                if len(got)<ln: err=0x4c
            elif cmd==0xcc: refs.pop(mem[pl+1],None)
            elif cmd==0xd0: files[refs[mem[pl+1]]['p']]=bytearray()
            elif cmd==0xd1:
                l=len(files[refs[mem[pl+1]]['p']]); mem[pl+2]=l&255; mem[pl+3]=(l>>8)&255; mem[pl+4]=l>>16
            elif cmd==0xce:
                refs[mem[pl+1]]['pos']=mem[pl+2]|mem[pl+3]<<8|mem[pl+4]<<16
            elif cmd==0x65: exited=True; break
            else: raise Exception('mli %x'%cmd)
            m.a=err; setc(m,err); 
            if err==0: m.p|=2
            else: m.p&=~2
            n+=1; continue
        m.step(); n+=1
    return bytes(out).decode(),files,n<20000000
def c64():
    sym=build(SRC,'c4','c64'); d=open('/tmp/t7/c4.xex','rb').read()[2:]
    m=MPU(); mem=m.memory; mem[0x2000:0x2000+len(d)]=list(d); mem[0x62]=0; mem[0x63]=0x40
    files={}; lfs={}; st={'nm':b'','lf':0,'dev':0,'sa':0,'in':None,'out':None,'status':0}; out=bytearray(); chans={}
    ret=0x4fff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    m.pc=0x2000; n=0
    while m.pc!=ret+1 and n<20000000:
        pc=m.pc
        if 0xffb7<=pc<=0xffd2 and pc in (0xffba,0xffbd,0xffc0,0xffc3,0xffc6,0xffc9,0xffcc,0xffcf,0xffd2,0xffb7):
            c=0
            if pc==0xffba: st['lf'],st['dev'],st['sa']=m.a,m.x,m.y
            elif pc==0xffbd: ln=m.a; ad=m.x|m.y<<8; st['nm']=bytes(mem[ad:ad+ln]).decode()
            elif pc==0xffc0:
                nm=st['nm']; lf=st['lf']
                if st['dev']<8: chans[lf]={'dev':st['dev']}
                else:
                    base,_,opt=nm.partition(',S,'); mode=opt
                    if mode=='W': files[base]=bytearray(); chans[lf]={'f':base,'pos':0,'w':1}
                    elif mode=='A': files.setdefault(base,bytearray()); chans[lf]={'f':base,'pos':len(files[base]),'w':1}
                    else:
                        if base not in files: chans[lf]={'f':None,'pos':0}; 
                        else: chans[lf]={'f':base,'pos':0}
            elif pc==0xffc3: chans.pop(m.a,None)
            elif pc==0xffc6: st['in']=m.x; st['status']=0
            elif pc==0xffc9: st['out']=m.x
            elif pc==0xffcc: st['in']=None; st['out']=None
            elif pc==0xffcf:
                ch=chans.get(st['in']) if st['in'] is not None else None
                if ch is None: m.a=0x0d; st['status']=0
                elif ch['f'] is None: m.a=0; st['status']=0x42
                else:
                    f=files[ch['f']]; p=ch['pos']
                    if p<len(f): m.a=f[p]; ch['pos']=p+1; st['status']=0x40 if p+1>=len(f) else 0
                    else: m.a=0; st['status']=0x40
                m.p=(m.p&~2)|(2 if m.a==0 else 0)
            elif pc==0xffd2:
                a=m.a
                ch=chans.get(st['out']) if st['out'] is not None else None
                if ch is None: out.append(a)
                else: files[ch['f']][ch['pos']:ch['pos']+1]=bytes([a]); ch['pos']+=1
            elif pc==0xffb7: m.a=st['status']; m.p=(m.p&~2)|(2 if m.a==0 else 0)
            if pc in (0xffba,0xffbd,0xffc0,0xffc3,0xffc6,0xffc9,0xffcc): m.p&=~1
            rts(m); n+=1; continue
        m.step(); n+=1
    return bytes(out).decode('latin1'),files,n<20000000
EXP="first\r500\rmore\r3\r"
o,f,fin=prodos(); print(repr(o),{k:bytes(v) for k,v in f.items()})
ck('prodos text',o,"first\r500\rmore\r3\rERROR 170\r"); ck('prodos file',bytes(f['TEST']),b'first\r500\rmore\r')
o,f,fin=c64(); print(repr(o),{k:bytes(v) for k,v in f.items()})
ck('c64 text (missing file is only seen on first read)',o,"first\r500\rmore\r3\rnot reached"); ck('c64 file',bytes(f['TEST']),b'first\r500\rmore\r')
done()
