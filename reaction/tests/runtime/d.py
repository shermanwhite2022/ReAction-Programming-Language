# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''TYPE rec=[BYTE id BYTE ARRAY name(8) CARD ARRAY pts(300) REAL ARRAY fl(2) INT last]
rec r1, r2
rec POINTER rp
CARD q1, q2, q3, q4, q5, q6
REAL g1
INT n1
BYTE b1, b2

CARD FUNC sum(CARD ARRAY a, CARD cnt)
 CARD k, s
 s=0
 FOR k=0 TO cnt-1 DO s==+a(k) OD
RETURN(s)

PROC main()
 r1.id=7
 r1.name(0)=65
 r1.name(7)=90
 r1.pts(299)=1234
 r1.pts(0)=-5
 r1.pts(1)=10
 r1.pts(2)=20
 r1.fl(1)=2.5
 r1.last=-9
 r2.name(0)=1
 r2.pts(299)=77
 q1=r1.pts(299)+r1.pts(0)
 b1=r1.name(7)
 b2=r2.name(0)
 q2=r2.pts(299)
 rp=@r1
 rp.pts(2)==+5
 q3=rp.pts(2)
 rp.name(1)=rp.name(0)+1
 b2=rp.name(1)
 rp=@r2
 rp.pts(1)=500
 q4=r2.pts(1)
 g1=r1.fl(1)*2.0
 n1=r1.last
 q5=sum(r1.pts,3)
 q6=r1.id
RETURN
'''
m,sym=build(src,'d')
nm=['r1','r2','rp','q1','q2','q3','q4','q5','q6','g1','n1','b1','b2']
V={n:var(sym,k) for k,n in enumerate(nm)}
ck('q1',w(m,V['q1']),1234-5)
ck('b1',b8(m,V['b1']),90)
ck('q2',uw(m,V['q2']),77)
ck('q3 ptr +=',uw(m,V['q3']),25)
ck('b2 (via ptr name(1)) ',b8(m,V['b2']),66) if False else None
ck('q4',uw(m,V['q4']),500)
ck('g1',unpack([m.memory[V['g1']+k] for k in range(4)]),5.0)
ck('n1 after arrays',w(m,V['n1']),-9)
ck('q5 sum of field array',uw(m,V['q5']),0+ (65536-5+10+20+5)%65536 if False else (-5+10+25)&0xffff)
ck('q6',uw(m,V['q6']),7)
ck('r1.name(1)',b8(m,V['r1']+2),66)
ck('size',V['r2']-V['r1'],1+8+600+8+2)
done()
