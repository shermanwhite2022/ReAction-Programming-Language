# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('st1','''INCLUDE "IO.ACT"
STRING g(20)="hello"
ZTRING z(10)="abc"
STRING e
STRING small(4)
PROC Show(STRING s)
 PrintE(s)
RETURN
PROC main()
 STRING t(30), u(40)
 ZTRING w(12)
 BYTE n
 PrintE(g)
 t = g
 PrintE(t)
 t = "world"
 PrintE(t)
 u = g + " " + t
 PrintE(u)
 small = "toolong"
 PrintE(small)
 w = u
 PrintBE(w(0))
 PrintBE(w(11))
 t = w
 PrintE(t)
 z = "xy"
 u = z + g
 PrintE(u)
 u = "a" + z
 PrintE(u)
 Show(g + "!")
 IF g = "hello" THEN PrintE("eq") FI
 IF g # t THEN PrintE("ne") FI
 IF "abc" < "abd" THEN PrintE("lt") FI
 IF "abc" < "ab" THEN PrintE("bad") ELSE PrintE("ge") FI
 IF "ab" < "abc" THEN PrintE("shorter") FI
 IF z = "xy" THEN PrintE("zeq") FI
 IF z > "xa" THEN PrintE("zgt") FI
 e = "q"
 PrintBE(e(0))
 u = ""
 PrintBE(u(0))
 u = u + u
 PrintBE(u(0))
 PrintZE(z)
 PrintBE(ZLen(z))
 PrintBE(SLen(g))
RETURN
''')
m,sym,at=run('st1')
ck('output',at.text(),"hello\nhello\nworld\nhello world\ntool\n104\n0\nhello world\nxyhello\naxy\nhello!\neq\nne\nlt\nge\nshorter\nzeq\nzgt\n1\n0\n0\nxy\n2\n5\n")
done()
