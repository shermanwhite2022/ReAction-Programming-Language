# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
E=b'\x9b'
# input: strings and numbers from the console
prog('i1','''INCLUDE "IO.ACT"
BYTE ARRAY s(40)
BYTE b
CARD c
INT i
BYTE ch
PROC main()
 InputS(s)
 Print("[") Print(s) Print("]") PutE()
 b=InputB()
 c=InputC()
 i=InputI()
 PrintBE(b) PrintCE(c) PrintIE(i)
 ch=GetD(0)
 PrintBE(ch)
RETURN
''')
m,sym,at=run('i1',stdin=b'hello world'+E+b'42'+E+b'60000'+E+b'-123'+E+b'Z')
ck('input',at.text(),"[hello world]\n42\n60000\n-123\n90\n")
# files: write, read back, EOF flag, append
prog('f1','''INCLUDE "IO.ACT"
BYTE ARRAY line(40)
CARD total
PROC main()
 Open(1,"D:TEST.TXT",8,0)
 PrintDE(1,"first line")
 PrintCDE(1,500)
 Close(1)
 Open(1,"TEST.TXT",9,0)
 PrintDE(1,"appended")
 Close(1)
 Open(2,"D:TEST.TXT",4,0)
 total=0
 DO
  InputSD(2,line)
  IF EOF(2) THEN EXIT FI
  total==+1
  PrintE(line)
 OD
 Close(2)
 PrintCE(total)
RETURN
''')
m,sym,at=run('f1')
ck('file contents',bytes(at.files['TEST.TXT']),b'first line'+E+b'500'+E+b'appended'+E)
ck('file read',at.text(),"first line\n500\nappended\n3\n")
# file not found -> Error shows the CIO status and ends the program
prog('e1','''INCLUDE "IO.ACT"
PROC main()
 Print("before") PutE()
 Open(3,"D:NOFILE.DAT",4,0)
 Print("after") PutE()
RETURN
''')
m,sym,at=run('e1')
ck('error',at.text(),"before\nERROR 170\n"); ck('exited',at.exited,True)
# character I/O to a file with GetD and EOF
prog('g1','''INCLUDE "IO.ACT"
BYTE n
PROC main()
 Open(1,"D:C.BIN",8,0)
 PutD(1,'x) PutD(1,'y) PutD(1,'z)
 Close(1)
 Open(1,"D:C.BIN",4,0)
 n=0
 DO
  PutD(0,GetD(1))
  IF EOF(1) THEN EXIT FI
  n==+1
 OD
 PutE()
 PrintBE(n)
RETURN
''')
m,sym,at=run('g1')
ck('getd',at.text(),"xyz\x00\n3\n")
# Str/Val
prog('v1','''INCLUDE "IO.ACT"
BYTE ARRAY s(10)
PROC main()
 StrC(4096,s) Print(s) PutE()
 StrI(-300,s) Print(s) PutE()
 StrB(7,s) Print(s) PutE()
 PrintCE(ValC("  321xyz"))
 PrintIE(ValI("-45"))
 PrintBE(ValB("200"))
RETURN
''')
m,sym,at=run('v1')
ck('strval',at.text(),"4096\n-300\n7\n321\n-45\n200\n")
done()
