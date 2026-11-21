# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
exec(open('/tmp/t6/run.py').read())
src=open('/tmp/t6/c.act').read()
tag='c'
r=subprocess.run([W+'/rc','/tmp/t6/c.act','-o','aout.a'],capture_output=True,text=True,cwd=W)
assert 'ERR' not in r.stdout, r.stdout
r=subprocess.run(['/home/claude/asmb/asm6502','-t','atari800','-I','.','-s','-o','/tmp/t6/c.xex','aout.a'],capture_output=True,text=True,cwd=W)
assert r.returncode==0, r.stdout[-800:]
sym={}
for l in r.stdout.split('\n'):
    p=l.split()
    if len(p)>=2 and p[1].startswith('$'):
        try: sym[p[0].lower()]=int(p[1][1:],16)
        except: pass
m=load('/tmp/t6/c.xex'); mem=m.memory; mem[0x8e]=0; mem[0x8f]=0x40
mem[0x600:0x604]=[0x8d,0x00,0x07,0x60]            # sta $0700 ; rts
mem[0x610:0x61e]=[0x8d,0x01,0x07,0x8e,0x02,0x07,0x8c,0x03,0x07,0xa9,0x34,0xa2,0x12,0x60]
call(m,0x2000,5000000)
ck('Zap A',mem[0x700],5)
ck('Calc A,X,Y',[mem[0x701],mem[0x702],mem[0x703]],[0xe8,0x03,3])
ck('q3',uw(m,var(sym,2)),0x1234)
ck('string args',[mem[var(sym,0)],mem[var(sym,1)],uw(m,var(sym,3))],[ord('H'),ord('i'),1])
done()
