# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
import subprocess
src='''TYPE pt=[BYTE x, y CARD z REAL r INT i]
pt POINTER p, q
BYTE ARRAY buf(40)
PROC main()
 buf(3)=1 buf(4)=2 buf(5)=3 buf(12)=9
 p=buf+3
 q=buf
 q^=p^
RETURN
'''
m,sym=build(src,'r2')
BF=var(sym,2)
ck('overlapping copy (forwards)',[b8(m,BF+k) for k in range(15)],[1,2,3,1,2,3]+[0]*8+[9]) if False else None
exp=[0]*15; exp[0:3]=[1,2,3]; exp[3:6]=[1,2,3]; exp[14]=9
# source bytes 3..14 -> 0..11 : buf[0..11] = old[3..14]
old=[0]*40; old[3:6]=[1,2,3]; old[12]=9
SZ=10; exp=old[3:3+SZ]+old[SZ:15]
ck('overlapping copy (forwards)',[b8(m,BF+k) for k in range(15)],exp)
def chkerr(src,frag):
    open('/tmp/t6/bad.act','w').write(src)
    r=subprocess.run(['/home/claude/rx_work/emit2/rc','/tmp/t6/bad.act','-o','/tmp/t6/bad.a'],capture_output=True,text=True)
    ck('error: '+frag,('ERR' in r.stdout) and frag in r.stdout,True)
H='TYPE pt=[BYTE x]\nTYPE q2=[BYTE x]\npt a\nq2 b\nCARD n\n'
chkerr(H+'PROC main()\n a=b\nRETURN\n','same type')
chkerr(H+'PROC main()\n n=a\nRETURN\n','not a record of that type')
chkerr(H+'PROC main()\n a=5\nRETURN\n','same type')
chkerr(H+'PROC main()\n n=a+1\nRETURN\n','operand')
done()
