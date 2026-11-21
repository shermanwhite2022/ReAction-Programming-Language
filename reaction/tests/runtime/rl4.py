# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from jy1 import other,ck,build,rts
import jy1
src='''INCLUDE "IO.ACT"
FLOAT a
PROC main()
 a=ValR("2.5")
 PrintRE(a*a+0.125)
 PrintRE(-1.0E-5)
 PrintRE(ValR("-7.25E3"))
RETURN
'''
jy1.SRC=src
h={0xffd2:lambda m,o:o.append(m.a), 0xffcc:lambda m,o:None}
o,f=other('c64','rc64',{},h,{0xa474}); ck('c64 real',o,"6.375\r-1E-05\r-7250\r")
h={0xfded:lambda m,o:o.append(m.a&0x7f)}
o,f=other('prodos','rpd',{},h,{0xbf00},skip=0); ck('prodos real',o,"6.375\r-1E-05\r-7250\r")
