# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('bl1','''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
BYTE ARRAY src(12)=[1 2 3 4 5 6 7 8 9 10 11 12]
BYTE ARRAY dst(100)
PROC main()
 BYTE i
 SetBlock(dst, 100, 99)
 Blit(dst+5, src, 4, 3, 20)
 FOR i=0 TO 99
 DO
  IF dst(i)#99 THEN PrintB(i) Put(58) PrintB(dst(i)) Put(32) FI
 OD
 Blit(dst, src, 0, 3, 20)
 Blit(dst, src, 3, 0, 20)
 PrintBE(dst(0))
 Blit(dst, src, 12, 1, 300)
 PrintBE(dst(12))
RETURN
''')
m,sym,at=run('bl1')
ck('blit',at.text().split('\n')[0].strip(),'5:1 6:2 7:3 8:4 25:5 26:6 27:7 28:8 45:9 46:10 47:11 48:12 99')
ck('zero sizes',at.text().split('\n')[1].strip(),'99')
done()
