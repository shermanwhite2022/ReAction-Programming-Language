# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE b=[200]
CARD c=[50000]
INT n=[-300]
REAL f=[2.5]
REAL g
REAL ARRAY ra(4)
INT ARRAY ia(4)
BYTE ARRAY ba(4)
REAL POINTER rp
INT POINTER ip
TYPE rec=[REAL fr INT ii BYTE bb CARD cc]
rec r
BYTE sb
CARD sc
INT si
REAL s1,s2,s3,s4,s5,s6,s7,s8,s9
INT i1,i2,i3,i4,i5,i6,i7,i8
CARD u1,u2,u3

REAL FUNC half(REAL x)
RETURN(x/2)
INT FUNC rnd(REAL x)
RETURN(x)
REAL FUNC toreal(INT x)
RETURN(x)
INT FUNC ir()
RETURN(7.9)
REAL FUNC fr()
RETURN(n)
CARD FUNC cf(REAL x)
RETURN(x)

PROC main()
 g=b s1=g
 g=c s2=g
 g=n s3=g
 sb=f si=-f sc=60000.5
 ra(1)=n ia(2)=f ba(3)=f
 s4=ra(1) i1=ia(2) i2=ba(3)
 rp=@s5 rp^=c
 ip=@i3 ip^=f
 r.fr=b r.ii=-f r.bb=f r.cc=48000.9
 s6=half(7) s7=half(n)
 i4=rnd(9.99) i5=rnd(-9.99)
 s8=toreal(n) s9=fr()
 i6=ir() u1=cf(40000.5)
 i7=f<3 i8=n<f
 u2=c>f
 u3=ba(f)
 g=0 g==+n g==+1 i1==+f
RETURN
'''
m,sym=build(src,'f')
names="b c n f g ra ia ba rp ip r sb sc si s1 s2 s3 s4 s5 s6 s7 s8 s9 i1 i2 i3 i4 i5 i6 i7 i8 u1 u2 u3".split()
V={nm:var(sym,k) for k,nm in enumerate(names)}
def rd(a): return unpack(bytes(m.memory[a:a+4]))
ck('g=b',rd(V['s1']),200.0); ck('g=c (unsigned)',rd(V['s2']),50000.0); ck('g=n',rd(V['s3']),-300.0)
ck('sb=f',b8(m,V['sb']),2); ck('si=-f',w(m,V['si']),-2); ck('sc=60000.5',uw(m,V['sc']),60000)
ck('ra(1)=n',rd(V['ra']+4),-300.0); ck('ia(2)=f',w(m,V['ia']+4),2); ck('ba(3)=f',b8(m,V['ba']+3),2)
ck('s4',rd(V['s4']),-300.0); ck('i1 (2 + 2.5 = 4)',w(m,V['i1']),4); ck('i2',w(m,V['i2']),2)
ck('rp^=c',rd(V['s5']),50000.0); ck('ip^=f',w(m,V['i3']),2)
R=V['r']
ck('r.fr=b',rd(R),200.0); ck('r.ii=-f',w(m,R+4),-2); ck('r.bb=f',b8(m,R+6),2); ck('r.cc=48000.9',uw(m,R+7),48000)
ck('half(7)',rd(V['s6']),3.5); ck('half(n)',rd(V['s7']),-150.0)
ck('rnd(9.99)',w(m,V['i4']),9); ck('rnd(-9.99)',w(m,V['i5']),-9)
ck('toreal(n)',rd(V['s8']),-300.0); ck('fr()=n',rd(V['s9']),-300.0)
ck('ir()=7.9',w(m,V['i6']),7); ck('cf(40000.5)',uw(m,V['u1']),40000)
ck('f<3',w(m,V['i7']),1); ck('n<f',w(m,V['i8']),1)
ck('c>f',uw(m,V['u2']),1)
ck('g==+n..',rd(V['g']),-300.0+1)
done()
