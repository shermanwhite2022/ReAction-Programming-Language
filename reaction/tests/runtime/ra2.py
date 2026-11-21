# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('ra2','''INCLUDE "IO.ACT"
TYPE ent=[BYTE x, y, hp
 CARD pts]
ent ARRAY e(300)
PROC init()
 CARD i
 FOR i=0 TO 299
 DO
  e(i).x=i MOD 200
  e(i).hp=3
  e(i).pts=i*2
 OD
RETURN
BYTE FUNC hit(CARD n, BYTE d)
 e(n).hp==-d
 IF e(n).hp>200 THEN RETURN(0) FI
RETURN(e(n).hp)
PROC main()
 BYTE r
 init()
 PrintCE(e(299).pts)
 PrintCE(e(250).pts+e(7).pts)
 PrintBE(e(250).x+e(7).x)
 r=hit(299,1) PrintBE(r)
 r=hit(299,5) PrintBE(r)
 PrintCE(SIZEOF(ent))
RETURN
''')
m,sym,at=run('ra2'); print(at.text().split())
