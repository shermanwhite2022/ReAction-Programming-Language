# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE b
CARD c, d, e, s, t, u, v, x, y
INT n

CARD FUNC add(CARD p, CARD q)
 CARD r
 r=p+q
RETURN(r)

PROC setb(BYTE v)
 b=v
RETURN

CARD FUNC fact(CARD k)
 CARD acc, j
 acc=1
 FOR j=2 TO k
 DO
  acc=acc*j
 OD
RETURN(acc)

PROC main()
 c=add(3,4)
 d=add(add(1,2),add(10,20))
 setb(77)
 e=fact(5)
 s=0
 FOR t=1 TO 10 DO s==+t OD
 u=0
 FOR t=10 TO 1 STEP -2 DO u==+1 OD
 v=0
 WHILE v<7 DO v==+1 OD
 x=0
 DO
  x==+3
  IF x>10 THEN EXIT FI
 OD
 y=0
 DO y==+1 UNTIL y=4 OD
 n=-5
 IF n<0 THEN n=1 ELSEIF n=0 THEN n=2 ELSE n=3 FI
RETURN
'''
m,sym=build(src,'a')
names=['b','c','d','e','s','t','u','v','x','y','n']
# declaration order: b, c d e s t u v x y, n, then add(p,q,r) setb(v) fact(k,acc,j) -> varx numbers in order
V={}
for i,nm in enumerate(['b','c','d','e','s','t','u','v','x','y','n']): V[nm]=var(sym,i)
ck('c=add(3,4)',uw(m,V['c']),7)
ck('d=add(add)',uw(m,V['d']),33)
ck('b',b8(m,V['b']),77)
ck('e=5!',uw(m,V['e']),120)
ck('sum1..10',uw(m,V['s']),55)
ck('step -2',uw(m,V['u']),5)
ck('while',uw(m,V['v']),7)
ck('exit',uw(m,V['x']),12)
ck('until',uw(m,V['y']),4)
ck('if',w(m,V['n']),1)
done()
