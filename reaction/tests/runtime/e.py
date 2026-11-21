# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''INCLUDE "D:lib.act"
DEFINE LIM="15", K=[7], NAME="msgtxt"
TYPE pt=[BYTE x,y CARD z BYTE ARRAY tag(4)]
BYTE ARRAY arr(LIM)
BYTE ARRAY buf(SIZEOF(pt))
BYTE POINTER bp
CARD a1, a2, a3, a4, a5, a6, a7, a8
BYTE b1, b2, b3

CLASS counter
 CARD count
 BYTE incr
 BYTE ARRAY hist(4)
 PROC reset()
  count=0
  incr=1
 RETURN
 CARD FUNC bump()
  count==+incr
  hist(0)=count
 RETURN(count)
 PROC setstep(BYTE s)
  incr=s
  bump()
 RETURN
SSALC

counter c1, c2
counter POINTER cp

PROC counter.twicebump()
 bump()
 bump()
RETURN

PROC useit(counter c)
 c.bump()
RETURN

PROC main()
 a1=twice(K)
 a2=LIM
 arr(14)=9
 b1=arr(14)
 a3=SIZEOF(pt)
 a4=SIZEOF(CARD)+SIZEOF(counter)+SIZEOF(pt.tag)+SIZEOF(arr)
 SYSMEM($7000)=42
 SYSMEM($7001)==+1
 b2=SYSMEM($7000)+SYSMEM($7001)
 c1.reset()
 c1.setstep(5)
 c1.bump()
 c2.reset()
 c2.bump()
 c1.twicebump()
 a5=c1.count
 cp=NEW counter
 cp.reset()
 cp.setstep(3)
 cp.bump()
 a6=cp.count
 c2=NEW counter
 c2.reset()
 useit(c2)
 useit(c2)
 a7=c2.count
 a8=c1.bump()
 bp=@buf
 bp(2)=77
 b3=buf(2)+bp(2)
RETURN
'''
open('/tmp/t6/inc/e.act','w').write(src)
import os
r=subprocess.run([W+'/rc','/tmp/t6/inc/e.act','-o','aout.a'],capture_output=True,text=True,cwd=W)
if 'ERR' in r.stdout or r.returncode or 'rror' in r.stdout: print(r.stdout,r.stderr); sys.exit(1)
r=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','.','-s','-o','/tmp/t6/e.xex','aout.a'],capture_output=True,text=True,cwd=W)
if r.returncode: print(r.stdout[-1500:],r.stderr); sys.exit(1)
sym={}
for l in r.stdout.split('\n'):
    p=l.split()
    if len(p)>=2 and p[1].startswith('$'):
        try: sym[p[0].lower()]=int(p[1][1:],16)
        except: pass
m=load('/tmp/t6/e.xex'); mem=m.memory; mem[0x8e]=0; mem[0x8f]=0x40
call(m,0x2000,60000000)
# variable numbering: arr buf bp a1..a8 b1..b3 c1 c2 cp (decl order, classes' fields excluded)
nm=['x_','arr','buf','bp','a1','a2','a3','a4','a5','a6','a7','a8','b1','b2','b3','c1','c2','cp']
V={n:var(sym,k) for k,n in enumerate(nm)}
ck('twice(K)',uw(m,V['a1']),14)
ck('LIM',uw(m,V['a2']),15)
ck('arr(14)',b8(m,V['b1']),9)
ck('SIZEOF(pt)',uw(m,V['a3']),1+1+2+4)
ck('SIZEOF sum',uw(m,V['a4']),2+(2+2+1+4)+4+15)
ck('SYSMEM',b8(m,V['b2']),43)
ck('c1.count',uw(m,V['a5']),20)       # setstep(5)->5, bump->10, twicebump->20? see below
ck('cp.count',uw(m,V['a6']),3+3)
ck('c2 new',uw(m,V['a7']),2)
ck('a8',uw(m,V['a8']),25)
ck('bp(2)',b8(m,V['b3']),(77+77)&255)
done()
