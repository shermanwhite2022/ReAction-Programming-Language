# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
vals=[0.5,1.9,32767.9,32768.0,32768.5,40000.7,65535.0,65535.9,65536.0,1e6,-300.0,-0.5]
def exp_card(v):
    import math
    if v<0: return (int(v))&0xffff if v>-32769 else 0
    t=int(v)
    return 65535 if t>65535 else t
src='REAL f\nCARD c\nPROC main()\n c=f\nRETURN\n'
bad_=0
for v in vals:
    src='REAL f=[%r]\nCARD c\nPROC main()\n c=f\nRETURN\n'%v
    m,sym=build(src,'f2')
    ck('CARD=%r'%v,uw(m,var(sym,1)),exp_card(v))
# other contexts
src='''REAL f=[3.7]
REAL one=[1.0]
INT n=[0]
CARD cnt=[0]
CARD ARRAY ca(10)
INT i1,i2,i3
BYTE j
PROC main()
 IF f THEN i1=1 ELSE i1=2 FI
 f=0.0
 IF f THEN i2=1 ELSE i2=2 FI
 f=2.9
 ca(f)=77
 i3=ca(2)
 FOR j=1 TO f DO cnt==+1 OD
 WHILE f>0.5 DO f==-1.0 n==+1 OD
RETURN
'''
m,sym=build(src,'f2')
names="f one n cnt ca i1 i2 i3 j".split()
V={nm:var(sym,k) for k,nm in enumerate(names)}
ck('IF f (nonzero)',w(m,V['i1']),1); ck('IF f (zero)',w(m,V['i2']),2)
ck('ca(f) index',w(m,V['i3']),77); ck('FOR to REAL limit',uw(m,V['cnt']),2); ck('WHILE f>0.5',w(m,V['n']),3)
done()
