# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('aud2','''INCLUDE "IO.ACT"
PROC main()
 BYTE i,cnt
 INT j
 CARD c
 cnt=0
 FOR j=10 TO 1 STEP -3 DO cnt==+1 OD
 PrintBE(cnt)
 cnt=0
 FOR c=10 TO 1 STEP -3 DO cnt==+1 OD
 PrintBE(cnt)
 cnt=0
 FOR i=10 TO 1 STEP -3 DO cnt==+1 OD
 PrintBE(cnt)
 cnt=0
 FOR i=5 TO 1 DO cnt==+1 OD
 PrintBE(cnt)
 cnt=0
 FOR j=-3 TO 3 DO cnt==+1 OD
 PrintBE(cnt)
RETURN
''')
m,sym,at=run('aud2')
print(repr(at.text()))
