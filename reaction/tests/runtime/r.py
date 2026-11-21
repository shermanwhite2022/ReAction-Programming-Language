# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''TYPE pt=[BYTE x, y CARD z REAL r INT i]
TYPE big=[BYTE tag CARD ARRAY v(200) INT last]
pt a, b, c
big g1, g2
pt POINTER p, q
BYTE ARRAY buf(40)
BYTE sink

PROC main()
 a.x=1 a.y=2 a.z=3000 a.r=2.5 a.i=-7
 b=a
 a.x=99
 c=b
 b.z=5
 g1.tag=9 g1.v(0)=11 g1.v(199)=33 g1.last=-4
 g2=g1
 g1.v(199)=0
 p=@a
 q=@c
 q^=p^
 a.y=55
 p^=b
 b=q^
 p=buf
 q=buf+3
 buf(0)=1 buf(1)=2 buf(2)=3 buf(3)=4 buf(4)=5 buf(5)=6 buf(6)=7 buf(7)=8 buf(8)=9
 p=buf
 q=buf+3
 q^=p^
RETURN
'''
m,sym=build(src,'r')
A=var(sym,0);B=var(sym,1);C=var(sym,2);G1=var(sym,3);G2=var(sym,4)
print("a",[b8(m,A+k) for k in range(12)])
ck('a.x restored by p^=b',b8(m,A),1); ck('b.x = c.x',b8(m,B),99)
ck('b.z from c',uw(m,B+2),3000); ck('b.i',w(m,B+8),-7)
ck('c.x',b8(m,C),99); ck('c.z',uw(m,C+2),3000)
ck('g2.tag',b8(m,G2),9); ck('g2.v0',uw(m,G2+1),11); ck('g2.v199',uw(m,G2+1+398),33); ck('g2.last',w(m,G2+401),-4)
ck('g1.v199',uw(m,G1+1+398),0)
# after p^=b : a == b contents (x=1,y=2...). q^=p^ made c=a earlier (x=99,y=2); then a.y=55 ; p^=b -> a=b
ck('a=b y',b8(m,A+1),2); ck('a=b z',uw(m,A+2),5)
ck('c.y',b8(m,C+1),2)
# b=q^ : b = c (x=99)
ck('b=c.x',b8(m,B),99)
BF=var(sym,7); exp=[1,2,3,1,2,3,4,5,6,7,8,9,0,0,0]
ck('overlapping copy (backwards)',[b8(m,BF+k) for k in range(15)],exp)
done()
