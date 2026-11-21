# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('s1','''INCLUDE "IOOUT.ACT"
PROC main()
 PrintCE(77)
 PrintE("only output")
RETURN
''')
m,sym,at=run('s1'); ck('IOOUT alone',at.text(),"77\nonly output\n")
prog('s2','''INCLUDE "IOIN.ACT"
INCLUDE "IOOUT.ACT"
INCLUDE "IOCORE.ACT"
INCLUDE "IO.ACT"
CARD n
PROC main()
 n=InputC()
 PrintCE(n+1)
RETURN
''')
m,sym,at=run('s2',stdin=b'41\x9b'); ck('parts, repeats and IO.ACT',at.text(),"42\n")
prog('s3','''INCLUDE "IONUM.ACT"
BYTE ARRAY s(8)
CARD v
PROC main()
 StrC(321,s)
 v=ValC(s)+1
RETURN
''')
m,sym,at=run('s3'); ck('IONUM alone, no machine code',(sym.get('varx00000000') is not None) and ('io_put' not in sym),True)
done()
