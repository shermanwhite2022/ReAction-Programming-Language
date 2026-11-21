# Action! compatibility audit (against the Action! 3.6 reference manual)

Checked: reserved words, operators and precedence, statements, routines, declarations, the Part VI library.
Test programs are in tests/audit (run.py compiles and assembles them), run time checks in tests/aud1-3.py.

## Matches Action!
- All 33 reserved words (case insensitive), `%` `!` `&` `#` `<>` `LSH` `RSH` `MOD` `@` `^`, `==op` shorthand
- Precedence levels (checked at run time: `a+b & c`, `a % b ! c`, `a ! b % c`, `a LSH b RSH 1`, unary minus)
- BYTE+BYTE wraps as a BYTE, any CARD makes a CARD, a wider value assigned to a BYTE keeps the low byte
- IF / ELSEIF / ELSE / FI, FOR with STEP (init, limit and step are taken once on entry), WHILE, DO..UNTIL..OD, EXIT
- DEFINE, INCLUDE, SET, MODULE; `SET TRACE=0` and `SET LIST=0` are accepted
- Declarations: `BYTE x=$E0` (address), `CARD i=[0]` (initial), `BYTE ARRAY a=[4 7 18]`, `CARD ARRAY j=$8000`,
  `BYTE ARRAY s="text"`, POINTER, TYPE records (also pointers to records), code blocks `[$40 'A b1 4+$A7]`,
  `PROC CIO=$E456(BYTE a,x)`, `PROC name=*()[bytes]`
- The Part VI library: every routine in the manual's list exists, with `device`, `EOF(8)`, `color`

## Changed during the audit
- `color` is now a library variable as in Action! (`color=1`).  The `Color(c)` procedure is gone (a name can't be both).
- `Break()` added (ends the program; there is no monitor to stop in).
- A call may pass fewer arguments than there are parameters (Action! allows it).  The missing ones are 0 here;
  Action! leaves them as they were last time.  Routines with default values still need their required arguments.
- FOR with a BYTE or CARD counter and a negative STEP ran past its limit (wrapped around); now it ends.
  A BYTE counter going past 255 now ends the loop too (Action! loops forever there).

## Differences that remain
- Arrays declared with a size can't be repointed: `b=a` and `nm="def"` are errors for them (arrays declared without a size
  and POINTERs can).  Action! arrays are pointers; here sized arrays have their own storage.
- The system `Error` routine can't be replaced (`Error=myhandler`); use TRY / CATCH / YRT instead.
- A routine must be declared before it is called, as in Action!.
- `*`, `/` and `MOD` on CARD values above 32767 are unsigned here (Action! gets them wrong; you chose unsigned).
- Atari only: player/missile graphics, display list interrupts and other hardware use work as Peek/Poke only.
- Not checked: Part VII (run time package, ROMmable code), the Toolkit, the Monitor and Editor, Appendix C error codes
  (the manual text I could read didn't include them), and the `PrintF` %H format (prints hex without `$`).
- The manual text I read was garbled in places and lacked Part IV 8.4 to 9 detail on forward references, so those
  rules come from the parts that I could read.
