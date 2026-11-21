# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
import random
random.seed(3)
cases=[(2,0),(0,0),(0,5),(1,100),(2,10),(2,15),(2,16),(3,7),(7,5),(10,4),(10,5),(255,2),(65535,3),(12345,678),(5,65535),(3,40000)]
cases+= [(random.randrange(65536),random.randrange(40)) for _ in range(20)]
ok=0
for a,e in cases:
    src='CARD a=[%d]\nCARD e=[%d]\nCARD r\nCARD r2\nPROC main()\n r=a**e\n r2=a**3\nRETURN\n'%(a,e)
    m,sym=build(src,'x')
    ck('%d**%d'%(a,e),uw(m,var(sym,2)),pow(a,e,65536))
    ck('%d**3'%a,uw(m,var(sym,3)),pow(a,3,65536))
# negative exponent, INT, precedence/mixed
src='INT n=[-2]\nINT r1,r2,r3,r4\nBYTE b=[3]\nCARD r5\nPROC main()\n r1=2**n\n r2=n**3\n r3=n**2\n r4=2**b+1\n r5=b**b\nRETURN\n'
m,sym=build(src,'x')
for i,(nm,e) in enumerate([('2**-2',0),('(-2)**3',-8),('(-2)**2',4),('2**b+1',9),('b**b',27)]):
    g=w(m,var(sym,1+i)) if i<4 else uw(m,var(sym,6)); ck(nm,g,e)
src='BYTE b=[9]\nBYTE c=[5]\nCARD r1,r2,r3\nPROC main()\n r1=2**b\n r2=c**4\n r3=c**c\nRETURN\n'
m,sym=build(src,'x')
ck('2**9 (byte exponent, 16-bit)',uw(m,var(sym,2)),512)
ck('byte base wraps 5**4',uw(m,var(sym,3)),625&255)
ck('byte base wraps 5**5',uw(m,var(sym,4)),3125&255)
done()
