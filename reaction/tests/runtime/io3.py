# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys; sys.path.insert(0,'/tmp/t7')
import iorun
from iorun import *
import subprocess
def runt(tag,src,target,hooks,stdin=b'',maxsteps=30000000):
    sym=build(src,tag,target)
    m=load('/tmp/t7/%s.xex'%tag) if target=='atari800' else loadraw('/tmp/t7/%s.xex'%tag,sym)
    return m,sym
def loadraw(path,sym):
    d=open(path,'rb').read(); m=MPU()
    # c64 .prg / apple binary: try 2-byte load address header
    a=d[0]|d[1]<<8; m.memory[a:a+len(d)-2]=list(d[2:]); return m
src='''INCLUDE "IO.ACT"
BYTE ARRAY s(30)
PROC main()
 Print("Hi") PutE()
 PrintCE(1234) PrintIE(-5)
 InputS(s)
 Print(s) PutE()
RETURN
'''
open('/tmp/t7/t3.act','w').write(src)
for target,zp,putch,rdkey,getln,eol in (('c64',(0x62,),0xffd2,0xffcf,None,0x0d),('appleii',(0x87,),0xfded,0xfd0c,0xfd6f,0x0d),('prodos',(0x87,),0xfded,0xfd0c,0xfd6f,0x0d)):
    sym=build(src,'t3'+target,target)
    path='/tmp/t7/t3%s.xex'%target
    d=open(path,'rb').read()
    m=MPU(); mem=m.memory
    # find a load address: asm output format per target; probe header
    if target=='c64': body=d[2:]
    elif target=='appleii': body=d[4:]
    else: body=d
    ORG=0x2000 if target=='c64' else 0x6000
    mem[ORG:ORG+len(body)]=list(body)
    zp0=zp[0]; mem[zp0]=0; mem[zp0+1]=0x40
    out=bytearray(); inq=bytearray(b'typed text\r')
    ret=0xbfff; m.sp=0xfd; mem[0x1fe]=ret&255; mem[0x1ff]=ret>>8; m.sp=0xfd
    for a in (putch,rdkey,0xffe4,0xffb7,0xffcc,getln): 
        if a: mem[a]=0x60
    m.pc=ORG; n=0
    while m.pc!=ret+1 and n<5000000:
        pc=m.pc
        if target=='c64' and pc==0xffd2:
            out.append(m.a)
        elif target!='c64' and pc==0xfded: out.append(m.a&0x7f)
        elif target=='c64' and pc==0xffcf:
            m.a=inq.pop(0) if inq else 0x0d; m.p&=~2
        elif target!='c64' and pc==0xfd0c: m.a=(inq.pop(0) if inq else 13)|0x80
        elif target!='c64' and pc==0xfd6f:
            k=0
            while inq:
                b=inq.pop(0)
                if b==13: break
                mem[0x200+k]=b|0x80; k+=1
            m.x=k
        if pc in (0xffd2,0xffcf,0xfded,0xfd0c,0xfd6f,0xffb7,0xffe4,0xffcc):
            lo=mem[0x100+m.sp+1]; hi=mem[0x100+m.sp+2]; m.sp+=2; m.pc=(lo|hi<<8)+1; n+=1
            if pc==0xffb7: m.a=0; m.p|=2
            continue
        m.step(); n+=1
    print(target,n<5000000,bytes(out))
