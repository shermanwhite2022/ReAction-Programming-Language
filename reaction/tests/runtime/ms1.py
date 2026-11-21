# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
# ---- Peek Poke PeekC PokeC Zero SetBlock MoveBlock
prog('m1','''INCLUDE "IOOUT.ACT"
INCLUDE "MISC.ACT"
BYTE ARRAY buf(20), other(20)
BYTE POINTER p
CARD i, addr
PROC main()
 Poke($0600, 77)
 PrintBE(Peek($0600))
 PokeC($0602, 51966)
 PrintBE(Peek($0602))
 PrintBE(Peek($0603))
 PrintCE(PeekC($0602))
 PokeC($0610, 1000)
 PrintCE(PeekC($0610) + 1)
 FOR i=0 TO 19 DO buf(i)=i+1 OD
 Zero(buf, 5)
 PrintBE(buf(4)) PrintBE(buf(5))
 SetBlock(buf, 10, 200)
 PrintBE(buf(0)) PrintBE(buf(9)) PrintBE(buf(10))
 p=buf
 SetBlock(p, 2, 9)
 PrintBE(buf(1)) PrintBE(buf(2))
 FOR i=0 TO 19 DO buf(i)=i OD
 MoveBlock(other, buf, 20)
 PrintBE(other(0)) PrintBE(other(19))
 addr=$0620
 SetBlock(addr, 600, 5)
 PrintBE(Peek($0620)) PrintBE(Peek($0620+599)) PrintBE(Peek($0620+600))
 Zero(addr, 300)
 PrintBE(Peek($0620+299)) PrintBE(Peek($0620+300))
RETURN
''')
m,sym,at=run('m1')
ck('peek/poke/block',at.text().split(),"77 254 202 51966 1001 0 6 200 200 11 9 200 0 19 5 5 0 0 5".split())
# overlapping moves with absolute addresses
prog('m2','''INCLUDE "IOOUT.ACT"
INCLUDE "MISC.ACT"
CARD i
PROC main()
 FOR i=0 TO 9 DO Poke($0700+i, i) OD
 MoveBlock($0702, $0700, 8)
 FOR i=0 TO 9 DO PrintB(Peek($0700+i)) Put(32) OD
 PutE()
 FOR i=0 TO 9 DO Poke($0700+i, i) OD
 MoveBlock($0700, $0702, 8)
 FOR i=0 TO 9 DO PrintB(Peek($0700+i)) Put(32) OD
 PutE()
 MoveBlock($0800, $0700, 0)
RETURN
''')
m,sym,at=run('m2')
ck('overlap up',at.text().split('\n')[0].split(),"0 1 0 1 2 3 4 5 6 7".split())
ck('overlap down',at.text().split('\n')[1].split(),"2 3 4 5 6 7 8 9 8 9".split())
# ---- Rand against the reference generator
def ref(state,n,rng):
    out=[]
    for _ in range(n):
        x=state
        x^=(x<<7)&0xffff; x^=x>>9; x^=(x<<8)&0xffff
        state=x; r=x>>8
        out.append(r if rng==0 else (r*rng)>>8)
    return out,state
prog('r1','''INCLUDE "IOOUT.ACT"
INCLUDE "MISC.ACT"
CARD i
PROC main()
 FOR i=1 TO 40 DO PrintBE(Rand(0)) OD
 FOR i=1 TO 40 DO PrintBE(Rand(10)) OD
 RandSeed(1234)
 FOR i=1 TO 20 DO PrintBE(Rand(100)) OD
 RandSeed(0)
 FOR i=1 TO 5 DO PrintBE(Rand(0)) OD
 FOR i=1 TO 3 DO PrintBE(Rand(1)) OD
RETURN
''')
m,sym,at=run('r1'); got=[int(x) for x in at.text().split()]
a,st=ref(0xACE1,40,0); b,st=ref(st,40,10); c,st=ref(1234,20,100); d,st=ref(1,5,0); e,st=ref(st,3,1)
ck('Rand(0) matches the generator',got[:40],a); ck('Rand(10)',got[40:80],b); ck('RandSeed(1234)',got[80:100],c)
ck('RandSeed(0) is seed 1',got[100:105],d); ck('Rand(1) is always 0',got[105:108],[0,0,0])
# the generator visits every non-zero state once
x=0xACE1; seen=0
while True:
    x^=(x<<7)&0xffff; x^=x>>9; x^=(x<<8)&0xffff; seen+=1
    if x==0xACE1: break
ck('period 65535',seen,65535)
# a spread: Rand(6) over 6000 calls
prog('r2','''INCLUDE "IOOUT.ACT"
INCLUDE "MISC.ACT"
CARD i
BYTE ARRAY cnt(6)
BYTE v
PROC main()
 Zero(cnt, 6)
 FOR i=1 TO 600 DO
  v=Rand(6)
  cnt(v)==+1
 OD
 FOR i=0 TO 5 DO PrintBE(cnt(i)) OD
RETURN
''')
m,sym,at=run('r2',maxsteps=60000000); cn=[int(x) for x in at.text().split()]
ck('Rand(6): all values in range, all hit',(sum(cn)==600 and min(cn)>60),True); print(cn)

# ---- strings
prog('s1','''INCLUDE "IOOUT.ACT"
INCLUDE "STRING.ACT"
BYTE ARRAY a(30), b(30), long(256)
CARD i
PROC cmp(BYTE ARRAY x, BYTE ARRAY y)
 PrintIE(SCompare(x, y))
RETURN
PROC show(BYTE ARRAY s)
 Put(60) Print(s) Put(62) PrintBE(s(0))
RETURN
PROC main()
 cmp("ABC", "ABC")
 cmp("ABC", "ABD")
 cmp("ABD", "ABC")
 cmp("AB", "ABC")
 cmp("ABC", "AB")
 cmp("", "")
 cmp("", "A")
 cmp("a", "B")
 cmp("\x80", "A")
 SCopy(a, "Hello, world")
 show(a)
 SCopy(b, a)
 show(b)
 SCopy(b, "")
 show(b)
 SCopyS(b, a, 1, 5)
 show(b)
 SCopyS(b, a, 8, 12)
 show(b)
 SCopyS(b, a, 8, 200)
 show(b)
 SCopyS(b, a, 5, 3)
 show(b)
 SCopyS(b, a, 0, 2)
 show(b)
 SCopy(b, "..........")
 SAssign(b, "abc", 3, 5)
 show(b)
 SAssign(b, "abcdef", 2, 4)
 show(b)
 SAssign(b, "xy", 9, 20)
 show(b)
 SAssign(b, "Q", 12, 12)
 show(b)
 SAssign(b, "zz", 5, 4)
 show(b)
 SCopy(long, "start")
 FOR i=6 TO 255 DO long(i)=65 OD
 long(0)=255
 SCopy(a, long)
 PrintBE(a(0)) PrintBE(a(255))
RETURN
''')
m,sym,at=run('s1')
exp="""0
-1
1
-1
1
0
-1
1
1
<Hello, world>12
<Hello, world>12
<>0
<Hello>5
<world>5
<world>5
<>0
<He>2
<..abc.....>10
<.abcc.....>10
<.abcc...xy>10
<.abcc...xylQ>12
<.abcc...xylQ>12
255
65""".split('\n')
got=at.text().split('\n')
ck('SCompare',got[:9],exp[:9])
for g,e in zip(got[9:],exp[9:]): ck('str '+e,g,e)
ck('number of lines',len(got)-1,len(exp))
done()
