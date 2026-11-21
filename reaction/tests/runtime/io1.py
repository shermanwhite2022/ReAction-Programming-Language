# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('o1','''INCLUDE "IO.ACT"
PROC main()
 Print("Hello") PutE()
 PrintCE(1234)
 PrintBE(255)
 PrintIE(-56)
 PrintIE(0)
 PrintCE(65535)
 PrintIE(-32768)
 Put('A) Put('B) PutE()
 PrintC(7) Print(" and ") PrintI(-7) PutE()
RETURN
''')
m,sym,at=run('o1')
ck('output',at.text(),"Hello\n1234\n255\n-56\n0\n65535\n-32768\nAB\n7 and -7\n")
done()
