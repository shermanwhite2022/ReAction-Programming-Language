# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE ARRAY ba(5)=[10 20 30 40 50]
CARD ARRAY ca(4)
INT ARRAY ia(3)=[-1 -2 -3]
REAL ARRAY rb(3)
BYTE ARRAY msg="Hello"
BYTE POINTER bp
CARD i, q1, q2, q3, q4, q5, cs, sumb, mlen, w1, w2
REAL f1, f2
INT n1

CARD FUNC total(BYTE ARRAY a, CARD cnt)
 CARD k, s
 s=0
 FOR k=0 TO cnt-1 DO s==+a(k) OD
RETURN(s)

REAL FUNC half(REAL x)
RETURN(x/2.0)

BYTE FUNC lowb(CARD v)
RETURN(v+256)

INT FUNC sgn(INT v)
 IF v<0 THEN RETURN(-1) FI
 IF v>0 THEN RETURN(1) FI
RETURN(0)

PROC fill(CARD ARRAY a, CARD n)
 CARD j
 FOR j=0 TO n-1 DO a(j)=j*j OD
RETURN

PROC main()
 q1=ba(2)
 ba(1)=99
 ba(3)==+5
 q2=ba(1)+ba(3)
 fill(ca,4)
 q3=ca(3)
 n1=ia(1)
 ia(2)=-100
 rb(0)=1.5
 rb(1)=rb(0)*4.0
 f1=rb(1)
 f2=half(7.0)
 sumb=total(ba,5)
 mlen=msg(0)
 w1=lowb(300)
 w2=sgn(-9)+sgn(9)*10
 CASE w2
  1,2 THEN cs=100
  9 THEN cs=9
  ELSE cs=55
 ESAC
 bp=@ba
 q4=bp^
 bp^=7
 q5=ba(0)
RETURN
'''
m,sym=build(src,'b')
nm=['ba','ca','ia','rb','msg','bp','i','q1','q2','q3','q4','q5','cs','sumb','mlen','w1','w2','f1','f2','n1']
V={n:var(sym,k) for k,n in enumerate(nm)}
ck('q1=ba(2)',uw(m,V['q1']),30)
ck('ba(1)',b8(m,V['ba']+1),99); ck('ba(3)',b8(m,V['ba']+3),45)
ck('q2',uw(m,V['q2']),144)
ck('ca',[uw(m,V['ca']+2*k) for k in range(4)],[0,1,4,9]); ck('q3',uw(m,V['q3']),9)
ck('n1',w(m,V['n1']),-2); ck('ia(2)',w(m,V['ia']+4),-100)
ck('rb(1)',unpack([m.memory[V['rb']+4+k] for k in range(4)]),6.0)
ck('f1',unpack([m.memory[V['f1']+k] for k in range(4)]),6.0)
ck('f2',unpack([m.memory[V['f2']+k] for k in range(4)]),3.5)
ck('total',uw(m,V['sumb']),10+99+30+45+50)
ck('msg len',uw(m,V['mlen']),5)
ck('lowb',uw(m,V['w1']),44)
ck('sgn',w(m,V['w2']),9)
ck('case',uw(m,V['cs']),9)
ck('q4',uw(m,V['q4']),10); ck('q5',uw(m,V['q5']),7)
done()
