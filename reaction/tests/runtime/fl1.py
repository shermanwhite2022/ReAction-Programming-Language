# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='''INCLUDE "IO.ACT"
BYTE ARRAY line(40), buf(20)
CARD s1,s2,sec,n
BYTE o1,o2,off
PROC main()
 Open(1,"D:T",8,0)
 PrintDE(1,"alpha")
 Note(1,@s1,@o1)
 PrintDE(1,"beta")
 Close(1)
 Open(1,"T",4,0)
 Point(1,s1,o1)
 InputSD(1,line)
 PrintE(line)
 Close(1)
 Open(1,"T",4,0)
 n=BGet(1,buf,4)
 PrintCE(n)
 PrintCE(buf(0)) PrintCE(buf(3))
 Close(1)
 Open(2,"U",8,0)
 buf(0)=65 buf(1)=66 buf(2)=67
 BPut(2,buf,3)
 Close(2)
 Rename("T","V")
 Delete("D:U")
 Delete("NOPE")
 Print("not reached")
RETURN
'''


prog('fl1',SRC)
m,sym,at=run('fl1',files={})
ck('exit via error', at.exited, True)
ck('text',at.text(),"beta\n4\n97\n104\nERROR 170\n")
ck('files',sorted(at.files),['U','V'][::1] if False else ['V'])
ck('V content',bytes(at.files['V']),b'alpha\x9bbeta\x9b')
done()
