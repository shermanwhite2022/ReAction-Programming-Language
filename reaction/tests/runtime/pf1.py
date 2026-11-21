# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
prog('pf1','''INCLUDE "IOOUT.ACT"
BYTE ARRAY name="Bob"
BYTE b=[200]
INT i=[-1234]
CARD c=[65000]
BYTE POINTER p
PROC main()
 PrintF("plain text%E")
 PrintF("%S is %U years, %I below%E", name, 41, i)
 PrintF("%C%C%C%E", 'A, 'B, 'C)
 PrintF("hex %H %H %H %H%E", 255, 4096, 0, c)
 PrintF("100%% sure%E")
 PrintF("%I %U %I%E", -1, -1, 32767)
 PrintF("%S|%S%E", "lit", "")
 p=name
 PrintF("ptr %S byte %U%E", p, b)
 PrintF("five: %U %U %U %U %U%E", 1, 2, 3, 4, 5)
 PrintF("six: %U %U %U %U %U %U|%E", 1, 2, 3, 4, 5)
 PrintF("few: %U %U %U%E", 7)
 PrintF("odd %X and trailing %")
 PutE()
 PrintFD(0, "to channel 0: %U%E", 9)
RETURN
''')
m,sym,at=run('pf1')
exp="""plain text
Bob is 41 years, -1234 below
ABC
hex FF 1000 0 FDE8
100% sure
-1 65535 32767
lit|
ptr Bob byte 200
five: 1 2 3 4 5
six: 1 2 3 4 5 |
few: 7 0 0
odd %X and trailing %
to channel 0: 9
""".split('\n')
got=at.text().split('\n')
for g,e in zip(got,exp): ck('PrintF '+e,g,e)
ck('lines',len(got),len(exp))
import subprocess
def bad(src,frag):
    open('/tmp/t7/bad.act','w').write(src)
    r=subprocess.run([W+'/rc','/tmp/t7/bad.act','-o','/tmp/t7/bad.a'],capture_output=True,text=True)
    ck('error: '+frag,('ERR' in r.stdout) and frag in r.stdout,True)
bad('INCLUDE "IOOUT.ACT"\nPROC main()\n PrintF("x",1,2,3,4,5,6)\nRETURN\n','too many')
bad('INCLUDE "IOOUT.ACT"\nPROC main()\n PrintF()\nRETURN\n','takes 1 to 6')
bad('PROC f(CARD a=1, CARD b)\nRETURN\nPROC main()\nRETURN\n','without a default')
bad('PROC f(REAL a=1)\nRETURN\nPROC main()\nRETURN\n','REAL parameter')
bad('PROC f(CARD a, CARD b=2)\nRETURN\nPROC main()\n f()\nRETURN\n','takes 1 to 2')
done()
