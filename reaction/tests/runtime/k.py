# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
import subprocess
src='''CLASS shape
 BYTE kind
 CARD hits
 PROC init(BYTE k)
  kind=k
 RETURN
 CARD FUNC area()
 RETURN(0)
SSALC

CLASS rect EXTENDS shape
 CARD w, h
 PROC init(CARD ww, CARD hh)
  super.init(2)
  w=ww
  h=hh
 RETURN
 CARD FUNC area()
 RETURN(w*h)
SSALC

CLASS circle EXTENDS shape
 REAL r
 PROC init(REAL rad)
  super.init(3)
  r=rad
 RETURN
 CARD FUNC area()
 RETURN(r*r*3.0)
SSALC

CLASS plain
 INT v
SSALC

CLASS sq EXTENDS rect
 PROC init(CARD s)
  super.init(s, s)
  kind=9
 RETURN
SSALC

CLASS kid EXTENDS shape
SSALC

shape s1
rect r1, r2
circle c1
sq q1
kid k1
plain p1
rect POINTER rp
CARD a1, a2, a3, a4, a5, a6, a7
BYTE k2, k3, k4, k5, k6
REAL rr
INT iv

PROC main()
 s1=NEW shape(7)
 k2=s1.kind
 r1=NEW rect(30, 40)
 a1=r1.area()
 k3=r1.kind
 r2=NEW rect(iv+5, 6+iv)
 a2=r2.area()
 c1=NEW circle(2)
 a3=c1.area()
 k4=c1.kind
 q1=NEW sq(9)
 a4=q1.area()
 k5=q1.kind
 k1=NEW kid(5)
 k6=k1.kind
 p1=NEW plain
 rp=NEW rect(2, 3)
 a5=rp.area()
 rr=1.5
 c1=NEW circle(rr+1)
 a6=c1.area()
 a7=r1.area()+q1.area()
RETURN
'''
m,sym=build(src,'k')
names="s1 r1 r2 c1 q1 k1 p1 rp a1 a2 a3 a4 a5 a6 a7 k2 k3 k4 k5 k6 rr iv".split()
V={nm:var(sym,12+k) for k,nm in enumerate(names)}
ck('shape(7).kind',b8(m,V['k2']),7)
ck('rect(30,40).area',uw(m,V['a1']),1200); ck('rect.kind via super.init',b8(m,V['k3']),2)
ck('rect(iv+5,6+iv) INT args',uw(m,V['a2']),30)
ck('circle(2) int->REAL arg',uw(m,V['a3']),12); ck('circle kind',b8(m,V['k4']),3)
ck('sq(9).area (init chain)',uw(m,V['a4']),81); ck('sq kind set after super.init',b8(m,V['k5']),9)
ck('kid inherits parent init(5)',b8(m,V['k6']),5)
ck('NEW through pointer',uw(m,V['a5']),6)
ck('circle(rr+1) REAL arg',uw(m,V['a6']),18)
ck('area sum',uw(m,V['a7']),1281)
def chk(src,frag):
    open('/tmp/t6/bad.act','w').write(src)
    r=subprocess.run(['/home/claude/rx_work/emit2/rc','/tmp/t6/bad.act','-o','/tmp/t6/bad.a'],capture_output=True,text=True)
    ck('error: '+frag,('ERR' in r.stdout) and frag in r.stdout,True)
H="CLASS a\n INT v\n PROC init(INT x, INT y)\n  v=x\n RETURN\nSSALC\nCLASS b\n INT w\nSSALC\na pa\nb pb\nPROC main()\n"
chk(H+" pa=NEW a\nRETURN\n","takes 2 argument")
chk(H+" pa=NEW a(1)\nRETURN\n","takes 2 argument(s), 1 given")
chk(H+" pa=NEW a(1,2,3)\nRETURN\n","too many")
chk(H+" pb=NEW b(1)\nRETURN\n","no constructor")
chk("TYPE t=[BYTE x]\nt pt\nPROC main()\n pt=NEW t(1)\nRETURN\n","only be given to a CLASS")
done()
