# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('aud3','''INCLUDE "IO.ACT"
PROC two(BYTE a,b)
 PrintB(a) Put(32) PrintBE(b)
RETURN
CARD FUNC add(CARD a,b,c)
RETURN (a+b+c)
PROC main()
 two(7)
 two()
 PrintCE(add(1,2))
 Print("before") PutE()
 Break()
 PrintE("not reached")
RETURN
''')
m,sym,at=run('aud3')
ck('out',at.text(),"7 0\n0 0\n3\nbefore\n")
done()
