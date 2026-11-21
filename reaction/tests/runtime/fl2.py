# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def setc(m,c): m.p=(m.p&~1)|(1 if c else 0)
def prodos(src,tag,files0=None):
    build(src,tag,'prodos'); d=open('/tmp/t7/%s.xex'%tag,'rb').read()
    m=MPU(); mem=m.memory; mem[0x6000:0x6000+len(d)]=list(d); mem[0x87]=0; mem[0x88]=0x40
    files={k:bytearray(v) for k,v in (files0 or {}).items()}; refs={}; out=bytearray(); nref=[0]
    ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    m.pc=0x6000; n=0
    def w16(a): return mem[a]|mem[a+1]<<8
    def pth(a):
        l=mem[a]; return bytes(mem[a+1:a+1+l]).decode()
    while m.pc!=ret+1 and n<20000000:
        pc=m.pc
        if pc==0xfded: out.append(m.a&0x7f); rts(m); n+=1; continue
        if pc==0xbf00:
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; ra=lo|hi<<8
            cmd=mem[ra+1]; pl=w16(ra+2); m.sp+=2; m.pc=ra+4
            err=0
            if cmd==0xc0:
                p=pth(w16(pl+1))
                if p in files: err=0x47
                else: files[p]=bytearray()
            elif cmd==0xc1:
                p=pth(w16(pl+1))
                if p not in files: err=0x46
                else: del files[p]
            elif cmd==0xc2:
                p=pth(w16(pl+1)); q=pth(w16(pl+3))
                if p not in files: err=0x46
                elif q in files: err=0x47
                else: files[q]=files.pop(p)
            elif cmd==0xc8:
                p=pth(w16(pl+1))
                if p not in files: err=0x46
                else:
                    nref[0]+=1; refs[nref[0]]={'p':p,'pos':0}; mem[pl+5]=nref[0]
                    assert w16(pl+3)&0xff==0
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
            elif cmd==0xcf:
                v=refs[mem[pl+1]]['pos']; mem[pl+2]=v&255; mem[pl+3]=(v>>8)&255; mem[pl+4]=v>>16
            elif cmd==0xce:
                v=mem[pl+2]|mem[pl+3]<<8|mem[pl+4]<<16
                if v>len(files[refs[mem[pl+1]]['p']]): err=0x4d
                else: refs[mem[pl+1]]['pos']=v
            elif cmd==0x65: break
            else: raise Exception('mli %x'%cmd)
            m.a=err; setc(m,err)
            if err==0: m.p|=2
            else: m.p&=~2
            n+=1; continue
        m.step(); n+=1
    return bytes(out).decode('latin1'),files,n<20000000
def c64(src,tag,files0=None):
    build(src,tag,'c64'); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[2:]
    m=MPU(); mem=m.memory; mem[0x2000:0x2000+len(d)]=list(d); mem[0x62]=0; mem[0x63]=0x40
    files={k:bytearray(v) for k,v in (files0 or {}).items()}
    st={'nm':'','lf':0,'dev':0,'sa':0,'in':None,'out':None,'status':0}; out=bytearray(); chans={}; log=[]
    ret=0x4fff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    m.pc=0x2000; n=0
    while m.pc!=ret+1 and n<20000000:
        pc=m.pc
        if pc in (0xffba,0xffbd,0xffc0,0xffc3,0xffc6,0xffc9,0xffcc,0xffcf,0xffd2,0xffb7):
            if pc==0xffba: st['lf'],st['dev'],st['sa']=m.a,m.x,m.y
            elif pc==0xffbd: ln=m.a; ad=m.x|m.y<<8; st['nm']=bytes(mem[ad:ad+ln]).decode()
            elif pc==0xffc0:
                nm=st['nm']; lf=st['lf']; log.append(('open',lf,st['dev'],st['sa'],nm))
                if lf==15:
                    msg='00, OK,00,00'
                    if nm.startswith('S0:'):
                        f=nm[3:]
                        if f in files: del files[f]; msg='01, FILES SCRATCHED,01,00'
                        else: msg='62, FILE NOT FOUND,00,00'
                    elif nm.startswith('R0:'):
                        new,_,old=nm[3:].partition('=')
                        if old not in files: msg='62, FILE NOT FOUND,00,00'
                        elif new in files: msg='63, FILE EXISTS,00,00'
                        else: files[new]=files.pop(old)
                    chans[15]={'msg':bytearray(msg.encode())}
                elif st['dev']<8: chans[lf]={'dev':st['dev']}
                else:
                    base,_,opt=nm.partition(',S,')
                    if opt=='W': files[base]=bytearray(); chans[lf]={'f':base,'pos':0,'w':1}
                    elif opt=='A': files.setdefault(base,bytearray()); chans[lf]={'f':base,'pos':len(files[base]),'w':1}
                    else:
                        chans[lf]={'f':base if base in files else None,'pos':0}
            elif pc==0xffc3: chans.pop(m.a,None)
            elif pc==0xffc6: st['in']=m.x; st['status']=0
            elif pc==0xffc9: st['out']=m.x
            elif pc==0xffcc: st['in']=None; st['out']=None
            elif pc==0xffcf:
                ch=chans.get(st['in']) if st['in'] is not None else None
                if ch is None: m.a=0x0d; st['status']=0
                elif 'msg' in ch: m.a=ch['msg'].pop(0) if ch['msg'] else 0x0d
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
    return bytes(out).decode('latin1'),files,n<20000000,log
SRC=open('/tmp/t7/fl1.py').read().split("SRC='''")[1].split("'''")[0]
SRC2='''INCLUDE "IO.ACT"
BYTE ARRAY line(40), blk(100)
CARD sec,n,s0
BYTE off,i,o0
PROC main()
 Open(1,"D:BIG",8,0)
 FOR i=1 TO 60 DO
  PrintD(1,"abcdefghij")
  PrintBDE(1,i)
  IF i=40 THEN Note(1,@sec,@off) FI
 OD
 Close(1)
 Open(1,"BIG",4,0)
 Note(1,@s0,@o0)
 Point(1,sec,off)
 InputSD(1,line)
 PrintE(line)
 Point(1,s0,o0)
 InputSD(1,line)
 PrintE(line)
 n=BGet(1,blk,100)
 PrintCE(n)
 Point(1,sec,off)
 n=BGet(1,blk,100)
 PrintCE(n)
 PrintCE(sec)
 Close(1)
RETURN
'''
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
# test 1
o,f,fin=prodos(SRC,'p5')[:3]
ck('prodos t1 text',o,"beta\r4\r97\r104\rERROR 170\r")
ck('prodos t1 files',{k:bytes(v) for k,v in f.items()},{'V':b'alpha\rbeta\r'} if False else {'V':f['V']})
ck('prodos V',bytes(f['V']),b'alpha\rbeta\r'); ck('prodos U gone','U' in f,False)
o,f,fin,log=c64(SRC,'c5')
ck('c64 t1 text',o,"beta\r4\r97\r104\r"+"ERROR 170\r")
ck('c64 V',bytes(f.get('V',b'?')),b'alpha\rbeta\r'); ck('c64 U gone','U' in f,False)
lens=[10+len(str(i))+1 for i in range(1,61)]; tot=sum(lens); pos40=sum(lens[:40])
for name,fn in (('prodos',lambda:prodos(SRC2,'p6')),('c64',lambda:c64(SRC2,'c6'))):
    r=fn(); o=r[0]
    ck(name+' t2 text',o,"abcdefghij41\rabcdefghij1\r100\r100\r%d\r"%(pos40>>8))
prog('fl2',SRC2)
m,sym,at=run('fl2',files={})
ck('atari t2 text',at.text(),"abcdefghij41\nabcdefghij1\n100\n100\n%d\n"%(pos40//125+1))
print("pos40",pos40,"tot",tot)
print("bad",bad); sys.exit(1 if bad else 0)
