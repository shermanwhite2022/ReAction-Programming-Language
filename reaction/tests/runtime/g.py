# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''INT n=[-5]
CARD c=[100]
BYTE b=[7]
INT q1,q2,q3,q4,q5,q6
CARD u1
PROC main()
 q1=n<c
 q2=n<200
 q3=n<300
 q4=n<40000
 q5=n<b
 q6=n/2
 u1=n+c
RETURN
'''
m,sym=build(src,'g')
V={nm:var(sym,k) for k,nm in enumerate("n c b q1 q2 q3 q4 q5 q6 u1".split())}
ck('INT < CARD is unsigned (Action!)',w(m,V['q1']),0)
ck('INT < const 200 is signed',w(m,V['q2']),1)
ck('INT < const 300 is signed',w(m,V['q3']),1)
ck('INT < const 40000 (a CARD) unsigned',w(m,V['q4']),0)
ck('INT < BYTE signed',w(m,V['q5']),1)
ck('INT/2 signed',w(m,V['q6']),-2)
ck('INT+CARD wraps as CARD',uw(m,V['u1']),95)
done()
