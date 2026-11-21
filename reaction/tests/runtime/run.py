# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,subprocess
sys.path.insert(0,'/home/claude/fpwork')
from fptest import *
W='/home/claude/rx_work/emit2'
def build(src,tag):
    open('/tmp/t6/%s.act'%tag,'w').write(src)
    r=subprocess.run([W+'/rc','/tmp/t6/%s.act'%tag,'-o','aout.a'],capture_output=True,text=True,cwd=W)
    if 'ERR' in r.stdout or r.returncode: print(r.stdout,r.stderr); sys.exit(1)
    r=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','.','-s','-o','/tmp/t6/%s.xex'%tag,'aout.a'],capture_output=True,text=True,cwd=W)
    if r.returncode: print(r.stdout[-1500:],r.stderr); sys.exit(1)
    sym={}
    for l in r.stdout.split('\n'):
        p=l.split()
        if len(p)>=2 and p[1].startswith('$'):
            try: sym[p[0].lower()]=int(p[1][1:],16)
            except: pass
    m=load('/tmp/t6/%s.xex'%tag); m.memory[0x8e]=0; m.memory[0x8f]=0x40
    n=call(m,0x2000,60000000)
    return m,sym
def var(sym,k): return sym.get('varx%08d'%k, 0x0300)	# a variable nothing uses is removed
def b8(m,a): return m.memory[a]
def w(m,a):
    v=m.memory[a]|m.memory[a+1]<<8; return v-65536 if v&0x8000 else v
def uw(m,a): return m.memory[a]|m.memory[a+1]<<8
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,g,e); bad+=(not ok)
def done():
    print("bad",bad); sys.exit(1 if bad else 0)
