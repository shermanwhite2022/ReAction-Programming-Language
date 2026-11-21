# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''CLASS shape
 CARD hits
 BYTE kind
 PROC init()
  kind=1
 RETURN
 CARD FUNC area()
 RETURN(0)
 CARD FUNC twice()
 RETURN(area()*2)
 PROC touch()
  hits==+1
 RETURN
SSALC

CLASS rect EXTENDS shape
 CARD w, h
 PROC init()
  super.init()
  kind=2
  w=3
  h=4
 RETURN
 CARD FUNC area()
 RETURN(w*h)
SSALC

CLASS square EXTENDS rect
 CARD FUNC area()
  touch()
 RETURN(super.area()+1)
SSALC

shape sp, sq
rect POINTER rp
CARD a1, a2, a3, a4, a5, a6, a7, a8, a9
BYTE k1, k2, k3

CARD FUNC measure(shape s)
 s.touch()
RETURN(s.twice())

PROC main()
 sp=NEW shape
 a1=sp.area()
 k1=sp.kind
 sq=NEW rect
 a2=sq.area()
 a3=sq.twice()
 k2=sq.kind
 sq=NEW square
 a4=sq.area()
 a5=sq.twice()
 a6=measure(sp)
 a7=measure(sq)
 rp=NEW rect
 rp.w=10
 a8=rp.area()
 a9=sq.hits
 k3=rp.kind
RETURN
'''
m,sym=build(src,'o')
nm=['sp','sq','rp','a1','a2','a3','a4','a5','a6','a7','a8','a9','k1','k2','k3']
# routine-local numbers come first: init/area/twice/touch/... each method has this (+ params); globals declared after classes
import re
# find by compiler order: count method this params declared before sp
pre=0
base=None
V={}
# method 'this' variables: shape(init,area,twice,touch)=4, rect(init,area)=2, square(area)=1 -> 7; measure's s -> after globals
off=7
V={n:var(sym,off+i) for i,n in enumerate(nm)}
ck('shape.area',uw(m,V['a1']),0)
ck('shape.kind (init)',b8(m,V['k1']),1)
ck('rect.area',uw(m,V['a2']),12)
ck('rect.twice (virtual from base)',uw(m,V['a3']),24)
ck('rect init sets kind via super',b8(m,V['k2']),2)
ck('square.area',uw(m,V['a4']),13)
ck('square.twice',uw(m,V['a5']),26)
ck('measure(shape)',uw(m,V['a6']),0)
ck('measure(square)',uw(m,V['a7']),26)
ck('rp.area',uw(m,V['a8']),40)
ck('hits counts',uw(m,V['a9']),4)
ck('rp.kind',b8(m,V['k3']),2)
done()
