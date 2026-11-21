# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
exec(open('/tmp/t7/gap.py').read().split("bad=0")[0])
S='''INCLUDE "IO.ACT"
INCLUDE "GRAPHICS.ACT"
PROC main()
 Graphics(8+16)
 color=1
 Position(10,10) DrawTo(30,10) DrawTo(30,20) DrawTo(10,20) DrawTo(10,10)
 color=2
 Position(11,11) Fill(11,19)
RETURN
'''
out,mem,sw,clicks,m=run_apple(S,'gf1')
def pix(x,y):
    b=mem[hgr_addr(y)+x//7]; return (b>>(x%7))&1
got={(x,y) for y in range(192) for x in range(280) if pix(x,y)}
exp={(x,y) for x in range(10,31) for y in range(10,21)}
print("fill",got==exp,len(got),len(exp)); sys.exit(got!=exp)
