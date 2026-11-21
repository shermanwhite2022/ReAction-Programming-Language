# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src='''BYTE tgt, r1
CARD r2, v2

BYTE FUNC AddOne=*(BYTE v)[ $18 $69 $01 $60 ]
CARD FUNC Double=*(CARD v)[ $86 $91 $0A $26 $91 $A6 $91 $60 ]
PROC SetTgt=*(BYTE v)[ $8D tgt $60 ]

PROC main()
 [ $A9 $07 $8D tgt ]
 r1=AddOne(tgt)
 r2=Double(300)
 SetTgt(r1+10)
 v2=r2+tgt
RETURN
'''
m,sym=build(src,'m')
V={n:var(sym,k) for k,n in enumerate(['tgt','r1','r2','v2'])}
ck('inline lda/sta',b8(m,V['r1']),8)
ck('AddOne machine code func',b8(m,V['r1']),8)
ck('Double (A,X)',uw(m,V['r2']),600)
ck('SetTgt with var address',b8(m,V['tgt']),18)
ck('v2',uw(m,V['v2']),618)
done()
