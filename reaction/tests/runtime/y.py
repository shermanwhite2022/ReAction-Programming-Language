# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE a=[200]
BYTE b=[100]
BYTE c=[3]
CARD w=[1000]
CARD q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11
BYTE r12
PROC main()
 q1=a+b
 q2=c-5
 q3=a*c
 q4=a+w
 q5=c-5+w
 q6=a+100
 q7=300+c
 q8=a LSH 1
 q9=a-b
 q10=a+b>50
 q11=100+200
 r12=a+b
RETURN
'''
m,sym=build(src,'y')
exp=[44,254,88,1200,0,44,303,144,100,0,300,44]
# q5: (c-5) is byte 254, + w(1000) -> 1254
exp[4]=1254
# q10: (a+b) wraps to 44 > 50 -> 0
for i,e in enumerate(exp):
    k=4+i
    g=uw(m,var(sym,k)) if i<11 else b8(m,var(sym,k))
    ck('q%d'%(i+1),g,e)
done()
