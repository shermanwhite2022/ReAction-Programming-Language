# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
import subprocess
def comp(src):
    open('/tmp/t6/rec.act','w').write(src)
    r=subprocess.run(['/home/claude/rx_work/emit2/rc','/tmp/t6/rec.act','-o','/tmp/t6/rec.a'],capture_output=True,text=True)
    return r.returncode,r.stdout
bad=0
def ck(name,cond):
    global bad
    print(('ok  ' if cond else 'FAIL'),name)
    if not cond: bad+=1
# direct recursion
rc,o=comp('CARD n\nPROC f()\n n==+1\n IF n<10 THEN f() FI\nRETURN\nPROC main()\n f()\nRETURN\n')
ck('direct recursion rejected',rc==1 and 'recursion: f -> f' in o)
# FUNC in expression
rc,o=comp('CARD FUNC fact(CARD n)\nRETURN(n*fact(n-1))\nPROC main()\n fact(3)\nRETURN\n')
ck('recursive FUNC rejected',rc==1 and 'fact -> fact' in o)
# ordinary chain, same routine called twice (not a cycle)
rc,o=comp('CARD n\nPROC a()\n n==+1\nRETURN\nPROC b()\n a() a()\nRETURN\nPROC c()\n a() b()\nRETURN\nPROC main()\n c() b()\nRETURN\n')
ck('diamond calls are fine',rc==0 and 'ERR' not in o)
# virtual cycle: m calls this.n, subclass overrides n to call m
rc,o=comp('''CLASS base
 CARD x
 PROC n()
  x==+1
 RETURN
 PROC m()
  n()
 RETURN
SSALC
CLASS sub EXTENDS base
 PROC n()
  m()
 RETURN
SSALC
sub s
PROC main()
 s=NEW sub
 s.m()
RETURN
''')
ck('cycle through an override rejected',rc==1 and 'ERR! recursion' in o and 'method call' in o)
# virtual call with no cycle
rc,o=comp('''CLASS base
 CARD x
 PROC n()
  x==+1
 RETURN
 PROC m()
  n()
 RETURN
SSALC
CLASS sub EXTENDS base
 PROC n()
  x==+2
 RETURN
SSALC
sub s
PROC main()
 s=NEW sub
 s.m()
RETURN
''')
ck('virtual calls without a cycle are fine',rc==0 and 'ERR' not in o)
# sibling class overriding does not matter
rc,o=comp('''CLASS base
 PROC n()
 RETURN
SSALC
CLASS a EXTENDS base
 PROC m2()
 RETURN
 PROC n()
  m2()
 RETURN
SSALC
CLASS b EXTENDS base
 PROC go()
  n()
 RETURN
SSALC
b pb
PROC main()
 pb=NEW b
 pb.go()
RETURN
''')
ck('unrelated overrides are not edges',rc==0 and 'ERR' not in o)
# NEW inside the constructor of the same class
rc,o=comp('''CARD hold
CLASS node
 CARD v
 PROC init(CARD d)
  IF d>0 THEN
   hold=NEW node(d-1)
  FI
 RETURN
SSALC
node q
PROC main()
 q=NEW node(3)
RETURN
''')
ck('constructor creating its own class is recursion',rc==1 and 'ERR! recursion' in o)
# super chain is fine
rc,o=comp('''CLASS base
 CARD x
 PROC init()
  x=1
 RETURN
SSALC
CLASS sub EXTENDS base
 PROC init()
  super.init()
  x==+1
 RETURN
SSALC
sub s
PROC main()
 s=NEW sub
RETURN
''')
ck('super.init chain is fine',rc==0 and 'ERR' not in o)
print('bad',bad)
