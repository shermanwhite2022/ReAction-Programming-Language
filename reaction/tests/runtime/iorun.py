# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,subprocess
sys.path.insert(0,'/home/claude/fpwork')
from fptest import *
W='/home/claude/rx_work/emit2'
EXIT=0xbffe
class Atari:
    def __init__(s,m,files=None,stdin=b''):
        s.m=m; s.files=dict(files or {}); s.out=bytearray(); s.inp=bytearray(stdin)
        s.ch={0:{'dev':'E','mode':12}}; s.exited=False; s.log=[]
    def cio(s):
        m=s.m; mem=m.memory; x=m.x; base=0x340+x; n=x>>4
        cmd=mem[base+2]; buf=mem[base+4]|mem[base+5]<<8; ln=mem[base+8]|mem[base+9]<<8
        a=m.a; st=1; ret=a
        def setlen(v): mem[base+8]=v&255; mem[base+9]=v>>8
        if cmd==3:
            nm=bytearray(); p=buf
            while mem[p]!=0x9b: nm.append(mem[p]); p+=1
            nm=nm.decode(); mode=mem[base+10]; s.log.append(('open',n,nm,mode))
            dev,_,fn=nm.partition(':')
            if dev[:1] in 'DD' and dev[:1]=='D':
                if mode==4:
                    if fn not in s.files: st=170
                    else: s.ch[n]={'dev':'D','fn':fn,'pos':0,'mode':4}
                elif mode in (8,9):
                    if mode==8 or fn not in s.files: s.files[fn]=bytearray()
                    s.ch[n]={'dev':'D','fn':fn,'mode':mode}
                else: st=131
            elif dev in ('E','S','P','K'): s.ch[n]={'dev':dev,'mode':mode}
            else: st=130
        elif cmd==12:
            if n in s.ch and n!=0: del s.ch[n]
        elif cmd in (32,33):
            nm=bytearray(); p=buf
            while mem[p]!=0x9b: nm.append(mem[p]); p+=1
            nm=nm.decode(); s.log.append(('xio',cmd,nm))
            if cmd==33:
                fn=nm.partition(':')[2]
                if fn in s.files: del s.files[fn]
                else: st=170
            else:
                o,_,n2=nm.partition(',')
                fn=o.partition(':')[2]
                if fn in s.files: s.files[n2]=s.files.pop(fn)
                else: st=170
        elif cmd in (37,38):
            c=s.ch.get(n)
            if c is None or c.get('dev')!='D': st=133
            elif cmd==38:
                p=c.get('pos',0) if c['mode']==4 else len(s.files[c['fn']])
                sec=p//125+1; off=p%125
                mem[base+12]=sec&255; mem[base+13]=sec>>8; mem[base+14]=off
            else:
                sec=mem[base+12]|mem[base+13]<<8; off=mem[base+14]
                c['pos']=(sec-1)*125+off
        elif cmd==7 and ln>0:
            c=s.ch.get(n)
            if c is None: st=133
            elif c['dev']=='D':
                d=s.files[c['fn']]; p=c['pos']; got=d[p:p+ln]
                mem[buf:buf+len(got)]=list(got); c['pos']=p+len(got); setlen(len(got))
                if len(got)<ln: st=136
            else: st=132
        elif cmd in (5,7):
            c=s.ch.get(n)
            if c is None: st=133
            else:
                if c['dev'] in 'EK': src=s.inp
                else: src=None
                if src is not None:
                    if cmd==7:
                        if not src: st=136; ret=0
                        else: ret=src.pop(0)
                    else:
                        got=0
                        while got<ln and src:
                            b=src.pop(0); mem[buf+got]=b; got+=1
                            if b==0x9b: break
                        setlen(got)
                        if got==0: st=136
                else:
                    d=s.files[c['fn']]; p=c['pos']
                    if cmd==7:
                        if p>=len(d): st=136; ret=0
                        else: ret=d[p]; c['pos']=p+1
                    else:
                        got=0
                        while got<ln and p<len(d):
                            b=d[p]; mem[buf+got]=b; got+=1; p+=1
                            if b==0x9b: break
                        c['pos']=p; setlen(got)
                        if got==0: st=136
        elif cmd in (9,11):
            c=s.ch.get(n)
            if c is None: st=133
            else:
                data=bytes([a]) if (cmd==11 and ln==0) else bytes(mem[buf:buf+ln])
                if c['dev'] in 'ESP': s.out+=data
                elif c['dev']=='D': s.files[c['fn']]+=data
        else: st=132
        m.a=ret; m.y=st
        # RTS
        lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
    def text(s): return bytes(s.out).replace(b'\x9b',b'\n').decode('latin1')
def build(src,tag,target='atari800'):
    open('/tmp/t7/%s.act'%tag,'w').write(src)
    import os
    r=subprocess.run([W+'/rc',*os.environ.get('RCFLAGS','').split(),'/tmp/t7/%s.act'%tag,'-o','/tmp/t7/%s.a'%tag],capture_output=True,text=True,cwd=W)
    if 'ERR' in r.stdout or r.returncode: print(r.stdout,r.stderr); sys.exit(1)
    r=subprocess.run(['/home/claude/asmb/asm6502','-t',target,'-I',W,'-s','-o','/tmp/t7/%s.xex'%tag,'/tmp/t7/%s.a'%tag],capture_output=True,text=True)
    if r.returncode: print(r.stdout[-1500:],r.stderr); sys.exit(1)
    sym={}
    for l in r.stdout.split('\n'):
        p=l.split()
        if len(p)>=2 and p[1].startswith('$'):
            try: sym[p[0].lower()]=int(p[1][1:],16)
            except: pass
    return sym
def run(tag,files=None,stdin=b'',maxsteps=30000000,pre=None):
    sym=build(open('/tmp/t7/%s.act'%tag).read() if False else _src[tag],tag)
    m=load('/tmp/t7/%s.xex'%tag); mem=m.memory
    for i_ in range(sym.get('zbstart',0),sym.get('zbend',0)): mem[i_]=0xAA   # the zero block must be cleared by the program itself
    mem[0x8e]=0; mem[0x8f]=0x40; mem[10]=EXIT&255; mem[11]=EXIT>>8
    mem[0xe456]=0x60
    for a_,v_ in (pre or {}).items(): mem[a_]=v_
    at=Atari(m,files,stdin)
    ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    m.pc=0x2000; n=0
    while m.pc!=ret+1 and n<maxsteps:
        if m.pc==0xe456: at.cio(); n+=1; continue
        if m.pc==EXIT: at.exited=True; break
        if mem[m.pc]==0 and m.pc!=0: raise Exception("BRK at %04x"%m.pc)
        m.step(); n+=1
    if n>=maxsteps: raise Exception("timeout")
    return m,sym,at
_src={}
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
def prog(tag,src): _src[tag]=src
def done():
    print("bad",bad); sys.exit(1 if bad else 0)
