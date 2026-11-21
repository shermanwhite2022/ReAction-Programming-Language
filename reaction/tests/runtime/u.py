# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
import random
random.seed(7)
vals=[0,1,2,100,255,256,32767,32768,40000,65535,12345,50000,60000,7]
pairs=random.sample([(a,b) for a in vals for b in vals],30)
ops=[('>',lambda a,b:int(a>b)),('>=',lambda a,b:int(a>=b)),('<',lambda a,b:int(a<b)),('<=',lambda a,b:int(a<=b)),
     ('/',lambda a,b:(a//b if b else 0xffff)),('MOD',lambda a,b:(a%b if b else a)),('+',lambda a,b:(a+b)&0xffff),('*',lambda a,b:(a*b)&0xffff),('=',lambda a,b:int(a==b))]
bad_total=0
for a,b in pairs:
    body="".join(" r%d=a%sb\n"%(i,o) for i,(o,_) in enumerate(ops))
    decl="CARD a=[%d]\nCARD b=[%d]\n"%(a,b)+"".join("CARD q%d\n"%i for i in range(len(ops)))
    body=body.replace("r%d"%0,"q0")
    body="".join(" q%d=a %s b\n"%(i,o) for i,(o,_) in enumerate(ops))
    src=decl+"PROC main()\n"+body+"RETURN\n"
    m,sym=build(src,'u')
    for i,(o,f) in enumerate(ops):
        g=uw(m,var(sym,2+i)); ex=f(a,b)
        if g!=ex: print("BAD",a,o,b,"got",g,"exp",ex); bad_total+=1
print("unsigned bad",bad_total)
bad+=bad_total
# mixed rules, real conversion, FOR loops
src='''CARD c=[40000]
INT n=[-5]
BYTE b=[200]
CARD q1, q2, q3, q4, q5, i, s
REAL f1, f2
INT q6
PROC main()
 q1=c>100
 q2=n<c
 q3=b+100
 q4=c/3
 f1=c
 f2=n
 s=0
 FOR i=40000 TO 40010 DO s==+1 OD
 q5=s
 q6=c/3
RETURN
'''
m,sym=build(src,'u2')
V={n:var(sym,k) for k,n in enumerate(['c','n','b','q1','q2','q3','q4','q5','i','s','f1','f2','q6'])}
ck('CARD>const',uw(m,V['q1']),1)
ck('INT<CARD signed mix (-5<40000 as INT = 40000 is -25536)',uw(m,V['q2']),0)
ck('BYTE+100 (8-bit wrap)',uw(m,V['q3']),44)
ck('CARD/3',uw(m,V['q4']),13333)
ck('CARD->REAL',unpack([m.memory[V['f1']+k] for k in range(4)]),40000.0)
ck('INT->REAL',unpack([m.memory[V['f2']+k] for k in range(4)]),-5.0)
ck('FOR CARD above 32767',uw(m,V['q5']),11)
done()
