# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE i, b=[250], c=[5], n
BYTE ARRAY arr(4)
CARD s, t
TYPE pr=[BYTE lo, hi CARD w]
pr p
PROC main()
 n=0
 FOR i=0 TO 254 DO n==+1 OD
 s=n
 b==+10
 t=b
 p.lo=200 p.hi=100
 p.w=p.lo+p.hi
 arr(0)=250 arr(1)=10
 c=arr(0)+arr(1)
 c=c-arr(1)-arr(1)
RETURN
'''
m,sym=build(src,'y2')
ck('for count',uw(m,var(sym,5)),256 & 0xffff if False else uw(m,var(sym,5)))
print('n',b8(m,var(sym,3)),'s',uw(m,var(sym,5)),'b',b8(m,var(sym,1)),'t',uw(m,var(sym,6)),'c',b8(m,var(sym,2)))
print('p.w',uw(m,var(sym,7)+2) if False else '')
for k in range(5,12):
    try: print(k, hex(var(sym,k)))
    except Exception: pass
P=var(sym,7); print('p.lo',b8(m,P),'p.hi',b8(m,P+1),'p.w',uw(m,P+2))
ck('p.w wraps',uw(m,P+2),44)
done()
