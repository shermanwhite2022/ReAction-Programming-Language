# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
from iorun import *
SRC='''INCLUDE "IO.ACT"
STRING g(20)="hello"
ZTRING z(10)="abc"
STRING e
STRING small(4)
PROC Show(STRING s)
 PrintE(s)
RETURN
PROC main()
 STRING t(30), u(40)
 ZTRING w(12)
 BYTE n
 PrintE(g)
 t = g
 PrintE(t)
 t = "world"
 PrintE(t)
 u = g + " " + t
 PrintE(u)
 small = "toolong"
 PrintE(small)
 w = u
 PrintBE(w(0))
 PrintBE(w(11))
 t = w
 PrintE(t)
 z = "xy"
 u = z + g
 PrintE(u)
 u = "a" + z
 PrintE(u)
 Show(g + "!")
 IF g = "hello" THEN PrintE("eq") FI
 IF g # t THEN PrintE("ne") FI
 IF "abc" < "abd" THEN PrintE("lt") FI
 IF "abc" < "ab" THEN PrintE("bad") ELSE PrintE("ge") FI
 IF "ab" < "abc" THEN PrintE("shorter") FI
 IF z = "xy" THEN PrintE("zeq") FI
 IF z > "xa" THEN PrintE("zgt") FI
 e = "q"
 PrintBE(e(0))
 u = ""
 PrintBE(u(0))
 u = u + u
 PrintBE(u(0))
 PrintZE(z)
 PrintBE(ZLen(z))
 PrintBE(SLen(g))
RETURN
'''
def rts(m):
    mem=m.memory; lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1
def other(target,tag,pre,hooks,stops,hi=False,skip=2):
    build(SRC,tag,target); d=open('/tmp/t7/%s.xex'%tag,'rb').read()[skip:]
    m=MPU(); mem=m.memory; ORG=0x2000 if target=='c64' else 0x6000; mem[ORG:ORG+len(d)]=list(d)
    rs={'c64':0x62,'prodos':0x87,'appleii':0x87}[target]; mem[rs]=0; mem[rs+1]=0x40
    for a,v in pre.items(): mem[a]=v
    out=bytearray(); ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8
    m.pc=ORG; n=0
    while m.pc!=ret+1 and n<5000000:
        pc=m.pc
        if pc in stops: break
        if pc in hooks: hooks[pc](m,out); rts(m); n+=1; continue
        m.step(); n+=1
    return bytes(out).decode('latin1'),n<5000000
bad=0
def ck(n,g,e):
    global bad
    ok=(g==e); print(("ok  " if ok else "BAD ")+n,repr(g),'' if ok else repr(e)); bad+=(not ok)
def main():
    exp="hello\nhello\nworld\nhello world\ntool\n104\n0\nhello world\nxyhello\naxy\nhello!\neq\nne\nlt\nge\nshorter\nzeq\nzgt\n1\n0\n0\nxy\n2\n5\n"
    h={0xffd2:lambda m,o:o.append(m.a), 0xffcc:lambda m,o:None}
    o,fin=other('c64','sc1',{},h,{0xa474})
    ck('c64',o.replace('\r','\n'),exp)
    h={0xfded:lambda m,o:o.append(m.a&0x7f)}
    for tg,stop in (('appleii',0x3d0),('prodos',0xbf00)):
        o,fin=other(tg,'sa1'+tg,{},h,{stop},skip=4 if tg=='appleii' else 0)
        ck(tg,o.replace('\r','\n'),exp)
    print("bad",bad); sys.exit(bad)
main()
