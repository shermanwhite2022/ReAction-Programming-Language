# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('bx1','''INCLUDE "IO.ACT"
INCLUDE "MISC.ACT"
BYTE ARRAY src(12)=[1 2 3 4 5 6 7 8 9 10 11 12]
BYTE ARRAY dst(100)
PROC main()
 BYTE i
 SetBlock(dst, 100, 99)
 BlitXor(dst+5, src, 4, 3, 20)
 BlitXor(dst+5, src, 4, 3, 20)
 BlitXor(dst+5, src, 2, 1, 20)
 FOR i=0 TO 99
 DO
  IF dst(i)#99 THEN PrintB(i) Put(58) PrintB(dst(i)) Put(32) FI
 OD
 BlitXor(dst, src, 0, 3, 20)
 BlitXor(dst, src, 3, 0, 20)
 PrintBE(dst(0))
 BlitXor(dst, src, 12, 1, 300)
 PrintBE(dst(12))
RETURN
''')
m,sym,at=run('bx1')
ck('blitxor',at.text().split('\n')[0].strip(),'5:98 6:97 99')
ck('zero sizes',at.text().split('\n')[1].strip(),'99')
done()
