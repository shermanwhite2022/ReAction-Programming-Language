# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,random; sys.path.insert(0,'/tmp/t7')
from iorun import *
random.seed(5)
vals=[]
for _ in range(60):
    e=random.randint(-30,30); v=random.uniform(1,10)*10**e
    vals.append(v if random.random()<.5 else -v)
vals+= [123456.0,0.00012345,7.0,65535.0,70000.0,2.5E-5,33.333333,1.0E37,-9.99E-37]
lits=['%.8E'%v for v in vals]
src='INCLUDE "IOREAL.ACT"\nPROC main()\n'+''.join(' PrintRE(%s)\n'%l for l in lits)
# reading: strings in and out
strs=['12.5','-0.75','1E3','.5','  42','1.5e-3','+7','123456789','0.000001','3.14159265','-2E+10','5.']
src+=''.join(' PrintRE(ValR("%s"))\n'%t for t in strs)
src+=' PrintRE(InputR())\n PrintRE(InputR()+InputR())\n'
src+='RETURN\n'
prog('rl2',src)
m,sym,at=run('rl2',stdin=b'2.25\x9b-1.5E2\x9b100\x9b')
out=at.text().split('\n')
bad=0
for v,l,o in zip(vals,lits,out):
    g=float(o); err=abs(g-v)/abs(v)
    if err>6e-6: print('BAD',l,o,err); bad+=1
exp=[12.5,-0.75,1000,.5,42,.0015,7,123456789,1e-6,3.14159265,-2e10,5.0,2.25,-50.0]
for t,o,e in zip(strs+['in1','in2'],out[len(vals):],exp):
    g=float(o); err=abs(g-e)/abs(e)
    if err>6e-6: print('BAD',t,o,e); bad+=1
print(len(out),'lines; bad',bad)
print(out[:4],out[len(vals):])
sys.exit(bad)
