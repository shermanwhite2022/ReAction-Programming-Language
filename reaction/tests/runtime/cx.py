# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# compile errors must give a non-zero exit status
import subprocess,sys
W='/home/claude/rx_work/emit2'
bad=0
for name,src in (('undeclared','PROC main()\n x=1\nRETURN\n'),('syntax','PROC main(\n')):
    open('/tmp/cx.act','w').write(src)
    r=subprocess.run([W+'/rc','/tmp/cx.act','-o','/tmp/cx.a'],capture_output=True,text=True,cwd=W)
    ok=r.returncode!=0; print(('ok  ' if ok else 'BAD ')+name,r.returncode); bad+=not ok
print('bad',bad); sys.exit(bad)
