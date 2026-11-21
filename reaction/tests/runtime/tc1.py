# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('tc1','''INCLUDE "IO.ACT"
BYTE e
PROC Inner(BYTE n)
 IF n=2 THEN
  THROW 42
 FI
 PrintBE(n)
RETURN
PROC main()
 BYTE i
 TRY
  Inner(1)
  Inner(2)
  PrintE("not reached")
 CATCH(e)
  PrintE("caught")
  PrintBE(e)
 YRT
 TRY
  PrintE("no error")
 CATCH
  PrintE("not reached")
 YRT
 TRY
  Open(1,"NOFILE",4,0)
  PrintE("not reached")
 CATCH(e)
  PrintBE(e)
 YRT
 TRY
  TRY
   THROW 7
  CATCH(e)
   PrintBE(e)
   THROW 8
  YRT
 CATCH(e)
  PrintBE(e)
 YRT
 FOR i=1 TO 3
 DO
  TRY
   IF i=2 THEN EXIT FI
   PrintBE(i)
  CATCH
  YRT
 OD
 THROW 99
 PrintE("not reached")
RETURN
''')
m,sym,at=run('tc1')
ck('output',at.text(),"1\ncaught\n42\nno error\n170\n7\n8\n1\nERROR 99\n")
ck('exited',at.exited,True)
done()
