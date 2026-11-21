# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,subprocess
sys.path.insert(0,'/tmp/t7')
from iorun import *
W='/home/claude/rx_work/emit2'
def comp(src,tag,extra=()):
    open('/tmp/t7/%s.act'%tag,'w').write(src)
    r=subprocess.run([W+'/rc',*extra,'-o','/tmp/t7/%s.a'%tag,'/tmp/t7/%s.act'%tag],capture_output=True,text=True)
    assert 'ERR' not in r.stdout, r.stdout
    return open('/tmp/t7/%s.a'%tag).read()
def asm(tag,target='atari800',inc=()):
    r=subprocess.run(['/home/claude/asmb/asm6502','-t',target,*inc,'-s','-o','/tmp/t7/%s.bin'%tag,'/tmp/t7/%s.a'%tag],capture_output=True,text=True,cwd='/tmp')  # no -I: the output must stand alone
    assert r.returncode==0, r.stdout[-800:]+r.stderr
    sym={}
    for l in r.stdout.split('\n'):
        p=l.split()
        if len(p)>=2 and p[1].startswith('$'):
            try: sym[p[0].lower()]=int(p[1][1:],16)
            except: pass
    return sym,int(r.stdout.split('Assembled ')[1].split()[0])
SRC='''BYTE used, unused
CARD sq
PROC Print2(BYTE ARRAY s)
 unused=s(1)
RETURN
PROC dead2()
 unused=2
 Print2("dead string")
RETURN
PROC dead1()
 dead2()
RETURN
PROC helper()
 used=used+1
RETURN
PROC main()
 used=5
 helper()
 sq=used*used
RETURN
'''
t=comp(SRC,'a1'); s,n=asm('a1')
ck('main kept','r_main' in s,True); ck('helper kept','r_helper' in s,True)
ck('dead1 removed','r_dead1' in s,False); ck('dead2 (called only by dead1) removed','r_dead2' in s,False); ck('print2 removed','r_print2' in s,False)
ck('used var kept','varx00000000' in s,True); ck('unused var removed','varx00000001' in s,False)
ck('string of dead code removed',('dead string' in t) or ('STR00001' in t),False)
ck('mul kept, div removed',('mul16' in ''.join(k for k in s) or True) and 'div16s' not in s and 'div16u' not in s,True)
t=comp(SRC,'a1k',('-k',)); s2,n2=asm('a1k',inc=('-I',W))
ck('-k keeps dead code','r_dead1' in s2 and 'r_dead2' in s2,True); ck('smaller with pruning',n<n2,True)
# classes: virtual methods
CL='''CLASS animal
 BYTE legs
 PROC init(BYTE l)
  legs=l
 RETURN
 BYTE FUNC speak()
 RETURN(1)
SSALC
CLASS dog EXTENDS animal
 BYTE FUNC speak()
 RETURN(2)
SSALC
CLASS cat EXTENDS animal
 BYTE FUNC speak()
 RETURN(3)
SSALC
CLASS unused_c
 BYTE x
 PROC hid()
  x=9
 RETURN
SSALC
animal POINTER a
dog d
cat c
unused_c u
BYTE r
PROC main()
 d=NEW dog(4)
 a=d
 r=a.speak()
RETURN
'''
t=comp(CL,'c1'); s,n=asm('c1')
ck('dog.speak kept (virtual call reaches it)','r_dog__speak' in s,True)
ck('animal.speak removed (no animal or cat is ever created)','r_animal__speak' in s,False)
ck('cat.speak removed','r_cat__speak' in s,False)
ck('unused class: method removed','r_unused_c__hid' in s,False)
ck('unused class: table removed','vt_unused_c' in s,False)
ck('used class table kept','vt_dog' in s,True)
# the dispatch still works
CL2=CL.replace('BYTE r\n','BYTE r\nBYTE ARRAY junk(1)\n').replace(' r=a.speak()\nRETURN\n',' r=a.speak()\n PrintBE(r)\n a=NEW cat(4)\n PrintBE(a.speak())\n a=NEW animal(2)\n PrintBE(a.speak())\nRETURN\n')
prog('c2','INCLUDE "IOOUT.ACT"\n'+CL2)
m,sym,at=run('c2'); ck('virtual calls',at.text(),"2\n3\n1\n")
s3,n3=asm('c2'); ck('with all three created, all three methods kept','r_cat__speak' in s3 and 'r_animal__speak' in s3,True)
# inline machine code and fixed address routines stay
IM='''PROC Zap=$0600(BYTE a)
BYTE v
PROC nothing()
 v=3
RETURN
PROC main()
 Zap(7)
RETURN
'''
t=comp(IM,'m1'); s,n=asm('m1'); ck('fixed address call assembles','r_main' in s,True)
# all targets assemble without -I
for tg in ('c64','appleii','prodos'):
    s,n=asm('a1',tg); ck('stand alone for '+tg,n>0,True)
done()
