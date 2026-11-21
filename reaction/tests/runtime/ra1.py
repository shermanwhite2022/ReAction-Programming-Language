# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='''INCLUDE "IO.ACT"
TYPE pos=[BYTE x, y
 CARD score]
pos ARRAY snake(5)
PROC main()
 BYTE i
 CARD s
 FOR i=0 TO 4
 DO
  snake(i).x=i
  snake(i).y=i+10
  snake(i).score=i*300
 OD
 snake(2).x==+5
 s=0
 FOR i=0 TO 4
 DO
  s==+snake(i).score
  PrintBE(snake(i).x) PrintBE(snake(i).y)
 OD
 PrintCE(s)
RETURN
'''
prog('ra1',SRC)
m,sym,at=run('ra1')
print(at.text())
