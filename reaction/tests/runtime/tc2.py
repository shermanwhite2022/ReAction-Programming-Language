# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
class _F: pass
fl2=_F()
_g={}
exec(open('/tmp/t7/fl2.py').read().split("SRC=open")[0],_g)
fl2.prodos=_g['prodos']; fl2.c64=_g['c64']
SRC=open('/tmp/t7/tc1.py').read().split("prog('tc1','''")[1].split("''')")[0]
NOOPEN=SRC.replace(''' TRY
  Open(1,"NOFILE",4,0)
  PrintE("not reached")
 CATCH(e)
  PrintBE(e)
 YRT
''','')
assert NOOPEN!=SRC
SRC3='''INCLUDE "IO.ACT"
CARD n
BYTE e
BYTE FUNC F(BYTE x)
 TRY
  IF x>5 THEN
   RETURN(x+1)
  FI
  THROW x
 CATCH(e)
  RETURN(e+100)
 YRT
RETURN(0)
PROC main()
 BYTE i
 n=0
 FOR i=1 TO 20
 DO
  n==+F(i)
 OD
 PrintCE(n)
 THROW 5
RETURN
'''
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
prog('tc3',SRC3); m,sym,at=run('tc3'); ck('atari leak/return',at.text(),"725\nERROR 5\n")
o=fl2.prodos(SRC,'tp1')[0]; ck('prodos',o,"1\rcaught\r42\rno error\r170\r7\r8\r1\rERROR 99\r")
o=fl2.prodos(SRC3,'tp3')[0]; ck('prodos leak',o,"725\rERROR 5\r")
o=fl2.c64(NOOPEN,'tc1')[0]; ck('c64',o,"1\rcaught\r42\rno error\r7\r8\r1\rERROR 99\r")
o=fl2.c64(SRC3,'tc3')[0]; ck('c64 leak',o,"725\rERROR 5\r")
print('bad',bad); sys.exit(bad)
