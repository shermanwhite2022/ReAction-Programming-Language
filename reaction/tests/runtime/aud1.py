# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('aud1','''INCLUDE "IO.ACT"
PROC main()
 BYTE a=[2],b=[3],c=[4],x,n=[5],i,cnt
 CARD cc,dd
 INT ii
 PrintBE(a+b & c)
 PrintBE(a LSH 1 + 1)
 PrintBE(a+b LSH 1)
 a=1 b=2 c=3
 PrintBE(a % b ! c)
 PrintBE(a ! b % c)
 a=2 b=3 c=4
 PrintBE(a*b MOD c)
 PrintBE(a LSH b RSH 1)
 ii=-a*b
 PrintIE(ii)
 x=200
 x=x+100
 PrintBE(x)
 cc=200
 cc=cc+100
 PrintCE(cc)
 cc=$1234
 x=cc
 PrintBE(x)
 ii=-7
 dd=ii/2
 PrintIE(ii/2)
 PrintIE(ii MOD 3)
 cnt=0
 FOR i=1 TO n DO n=2 cnt==+1 OD
 PrintBE(cnt)
 cnt=0
 FOR i=10 TO 1 STEP -3 DO cnt==+1 OD
 PrintBE(cnt)
 x=$F0
 x==& $3C
 PrintBE(x)
 x==LSH (1+1)
 PrintBE(x)
 IF a+b>c AND c=4 THEN PrintE("t1") FI
 IF a=9 OR (b=3 AND c=4) THEN PrintE("t2") FI
 IF a THEN PrintE("t3") FI
 x=0
 IF x THEN PrintE("bad") ELSEIF a=2 THEN PrintE("t4") FI
 PrintF("%S|%I|%U|%C|%H|%%|%E","str",-5,500,'A,255)
 PrintF("%U %U %U %U %U",1,2,3,4,5)
RETURN
''')
m,sym,at=run('aud1')
print(repr(at.text()))
