# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import sys,re; sys.path.insert(0,'/tmp/t7')
exec(open('/tmp/t7/bo1.py').read().split("m.pc=0x2000")[0])
a=open('/tmp/bo/boing.a').read()
def lab(nm,n): return re.search(r'^(\S+)\s+\.alloc %d\s+; %s'%(n,nm),a,re.M).group(1) if False else re.search(r'^(\S+)\s+\.alloc %d\s+; %s '%(n,nm),a,re.M).group(1)
r=subprocess.run if False else None
import subprocess
res=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','/home/claude/rx_work/emit2','-s','-o','/tmp/t7/p.xex','/tmp/bo/boing.a'],capture_output=True,text=True)
S={}
for l in res.stdout.split('\n'):
    p=l.split()
    if len(p)>=2 and p[1].startswith('$'): S[p[0].lower()]=int(p[1][1:],16)
bl=S[lab('ball',1600).lower()]; fr=S[lab('frames',4224).lower()]
m.pc=0x2000; n=0
while n<400_000_000:
    if m.pc==0xe456: at.cio(); n+=1; continue
    m.step(); n+=1
    if bytes(at.out).count(b'frames'): break
from PIL import Image
im=Image.new('RGB',(40*4+8*60,40*4))
for y in range(40):
    for x in range(40):
        v=mem[bl+y*40+x]
        c=(0,0,60) if v==255 else (((v>>3)*8)%256,(v&7)*32,128)
        for dy in range(4):
            for dx in range(4): im.putpixel((x*4+dx,y*4+dy),c)
pal={0:(20,20,90),1:(220,30,30),2:(240,240,240),3:(130,130,130)}
for f in range(8):
    for r in range(44):
        for c in range(12):
            b=mem[fr+f*624+r*12+c]
            for j in range(4):
                v=(b>>(6-2*j))&3
                X=160+f*60+(c*4+j)//1*1; 
                if X<im.size[0] and r<40: im.putpixel((160+f*20+ (c*4+j)//3 ,r*4),pal[v])
im.save('/tmp/bo/dump.png')
f0=Image.new('RGB',(48*3,44*3))
for r in range(44):
    for c in range(12):
        b=mem[fr+r*12+c]
        for j in range(4):
            v=(b>>(6-2*j))&3
            for dy in range(3):
                for dx in range(3): f0.putpixel(((c*4+j)*3+dx,r*3+dy),pal[v])
f0.save('/tmp/bo/frame0.png')
