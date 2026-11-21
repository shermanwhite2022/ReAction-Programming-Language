; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; mdefs.inc - macros used by the reaction compiler (6502asm syntax)
;
; Macros only, no code or data, so this file can be included at the top.
; Run time stack: RUNSTK (zero page) points at the top item, the stack grows
; down, every item is 16 bits (low byte first).  TMPA is the work register.
; The routines these call are in rtl.inc.

; Zero page for the floating point routines (fp.inc), 47 bytes from FPZ.
; The Wozniak/Rankin code needs WX2,WM2,WX1,WM1,WE in exactly this order.
        .ifndef FPZ
        ; FPZ (the base of the floating point block) is set per machine in the program header
        .endif
        .map WSIGN  @ FPZ+0
        .map WX2    @ FPZ+1         ; EXP/MANT2 (4 bytes)
        .map WM2    @ FPZ+2
        .map WX1    @ FPZ+5         ; EXP/MANT1 (4 bytes): the accumulator
        .map WM1    @ FPZ+6
        .map WE     @ FPZ+9         ; scratch (4)
        .map WZ     @ FPZ+13
        .map WT     @ FPZ+17
        .map WSEXP  @ FPZ+21
        .map WINT   @ FPZ+25
        .map FU     @ FPZ+26        ; our scratch: four REALs...
        .map FY     @ FPZ+30
        .map FA     @ FPZ+34
        .map FB     @ FPZ+38
        .map FC     @ FPZ+42
        .map FD     @ FPZ+46
        .map FPTR   @ FPZ+50        ; polynomial table pointer
        .map FCNT   @ FPZ+52
        .map FSGN   @ FPZ+53
        .map FFLG   @ FPZ+54

        .macro PUSHI v          ; push a 16 bit constant
        lda #<@v
        sta TMPA
        lda #>@v
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro PUSH8 a          ; push a BYTE variable (zero extended)
        lda @a
        sta TMPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro PUSH16 a         ; push a CARD/INT variable
        lda @a
        sta TMPA
        lda @a+1
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro POP8 a           ; pop into a BYTE variable
        jsr POPA
        lda TMPA
        sta @a
        .endm

        .macro POP16 a          ; pop into a CARD/INT variable
        jsr POPA
        lda TMPA
        sta @a
        lda TMPA+1
        sta @a+1
        .endm


        .macro PUSHFLOAT e, m1, m2, m3      ; push a REAL constant (4 bytes)
        lda #@e
        sta WX1
        lda #@m1
        sta WM1
        lda #@m2
        sta WM1+1
        lda #@m3
        sta WM1+2
        jsr FPUSH1
        .endm

        .macro PUSHF a                      ; push a REAL variable
        lda @a
        sta WX1
        lda @a+1
        sta WM1
        lda @a+2
        sta WM1+1
        lda @a+3
        sta WM1+2
        jsr FPUSH1
        .endm

        .macro POPF a                       ; pop into a REAL variable
        jsr FPOP1
        lda WX1
        sta @a
        lda WM1
        sta @a+1
        lda WM1+1
        sta @a+2
        lda WM1+2
        sta @a+3
        .endm

; ---- through a pointer (a variable holding an address), at byte offset off -----
; The pointer is copied to TMPC (zero page).  off is 0 for p^ and the field
; offset for p.field.  The pointer operand can also be a label+offset (r.p^).

        .macro PUSHIND8 p, off              ; push the BYTE at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        ldy #@off
        lda (TMPC),y
        sta TMPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro PUSHIND16 p, off             ; push the CARD/INT at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        ldy #@off
        lda (TMPC),y
        sta TMPA
        iny
        lda (TMPC),y
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro PUSHINDF p, off              ; push the REAL at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        ldx #3
        ldy #@off+3
@lp     lda (TMPC),y
        sta WX1,x
        dey
        dex
        bpl @lp
        jsr FPUSH1
        .endm

        .macro POPIND8 p, off               ; pop into the BYTE at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        jsr POPA
        ldy #@off
        lda TMPA
        sta (TMPC),y
        .endm

        .macro POPIND16 p, off              ; pop into the CARD/INT at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        jsr POPA
        ldy #@off
        lda TMPA
        sta (TMPC),y
        iny
        lda TMPA+1
        sta (TMPC),y
        .endm

        .macro POPINDF p, off               ; pop into the REAL at p^+off
        lda @p
        sta TMPC
        lda @p+1
        sta TMPC+1
        jsr FPOP1
        ldx #3
        ldy #@off+3
@lp     lda WX1,x
        sta (TMPC),y
        dey
        dex
        bpl @lp
        .endm


; ---- control flow and routines -------------------------------------------------

        .macro JZ t                         ; pop; jump to t if it is zero (any distance)
        jsr POPA
        lda TMPA
        ora TMPA+1
        bne @ok
        jmp @t
@ok
        .endm

        .macro DROP n                       ; discard n bytes from the run time stack
        clc
        lda RUNSTK
        adc #@n
        sta RUNSTK
        bcc @d
        inc RUNSTK+1
@d
        .endm

        .macro DUP16                        ; duplicate the top item
        jsr POPA
        jsr PUSHA
        jsr PUSHA
        .endm

        .macro SET16 a, v                   ; a = constant
        lda #<@v
        sta @a
        lda #>@v
        sta @a+1
        .endm

        .macro TRUNC8                       ; top item = top item and $ff
        jsr POPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro CASEEQ sel, v, t             ; jump to t if the 16 bit selector equals v
        lda @sel
        cmp #<@v
        bne @no
        lda @sel+1
        cmp #>@v
        bne @no
        jmp @t
@no
        .endm

; FOR loop: TMPA = the loop variable.  Leaves for ex when it has passed lim
; (signed): v > lim for a step >= 0, v < lim for a negative step.
        .macro FORTEST lim, stp, ex
        lda @stp+1
        bmi @dn
        sec
        lda @lim
        sbc TMPA
        lda @lim+1
        sbc TMPA+1
        bvc @u1
        eor #$80
@u1     bmi @xt
        bpl @go
@dn     sec
        lda TMPA
        sbc @lim
        lda TMPA+1
        sbc @lim+1
        bvc @d1
        eor #$80
@d1     bpl @go
@xt     jmp @ex
@go
        .endm

; the same for an unsigned loop variable: a step with its top bit set counts down
        .macro FORTESTU lim, stp, ex
        lda @stp+1
        bmi @dn
        lda @lim                ; up: leave when lim < v
        cmp TMPA
        lda @lim+1
        sbc TMPA+1
        bcc @xt
        bcs @go
@dn     lda TMPA                ; down: leave when v < lim
        cmp @lim
        lda TMPA+1
        sbc @lim+1
        bcs @go
@xt     jmp @ex
@go
        .endm

        .macro FORADD stp                   ; TMPA = TMPA + step
        clc
        lda TMPA
        adc @stp
        sta TMPA
        lda TMPA+1
        adc @stp+1
        sta TMPA+1
        .endm

; after FORADD for an unsigned loop variable: the loop ends when the counter would run past its limits (a step
; going down below 0, a step going up past 65535, or past 255 for a BYTE), the carry being the one from the add
        .macro FORCHKU stp, ex
        lda @stp+1
        bmi @dn
        bcs @xt
        bcc @go
@dn     bcs @go
@xt     jmp @ex
@go
        .endm

        .macro FORCHKB stp, ex
        lda @stp+1
        bmi @dn
        lda TMPA+1
        beq @go
        bne @xt
@dn     bcs @go
@xt     jmp @ex
@go
        .endm

; short FOR loops (unsigned variable, step 1): leave when lim < v; v+1 passing 255 (BYTE) or 65535 (CARD) leaves too
        .macro FORT1B lim, v, ex
        lda @lim
        cmp @v
        lda @lim+1
        sbc #0
        bcs @go
        jmp @ex
@go
        .endm

        .macro FORT1W lim, v, ex
        lda @lim
        cmp @v
        lda @lim+1
        sbc @v+1
        bcs @go
        jmp @ex
@go
        .endm

        .macro FORI1B v, ex
        inc @v
        bne @go
        jmp @ex
@go
        .endm

        .macro FORI1W v, ex
        inc @v
        bne @go
        inc @v+1
        bne @go
        jmp @ex
@go
        .endm

; ---- array elements.  D = the array's data is at the label, I = the label holds
; the address.  Get: index on the stack -> element.  Set: index, then value.

        .macro AGETD1 b
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX1
        ldy #0
        lda (TMPC),y
        sta TMPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETI1 b
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX1
        ldy #0
        lda (TMPC),y
        sta TMPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETD2 b
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX2
        ldy #0
        lda (TMPC),y
        sta TMPA
        iny
        lda (TMPC),y
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETI2 b
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX2
        ldy #0
        lda (TMPC),y
        sta TMPA
        iny
        lda (TMPC),y
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETD4 b
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda (TMPC),y
        sta WX1,x
        dey
        dex
        bpl @lp
        jsr FPUSH1
        .endm

        .macro AGETI4 b
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda (TMPC),y
        sta WX1,x
        dey
        dex
        bpl @lp
        jsr FPUSH1
        .endm

        .macro ASETD1 b
        jsr POPA
        lda TMPA
        sta FU
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX1
        ldy #0
        lda FU
        sta (TMPC),y
        .endm

        .macro ASETI1 b
        jsr POPA
        lda TMPA
        sta FU
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX1
        ldy #0
        lda FU
        sta (TMPC),y
        .endm

        .macro ASETD2 b
        jsr POPA
        lda TMPA
        sta FU
        lda TMPA+1
        sta FU+1
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX2
        ldy #0
        lda FU
        sta (TMPC),y
        iny
        lda FU+1
        sta (TMPC),y
        .endm

        .macro ASETI2 b
        jsr POPA
        lda TMPA
        sta FU
        lda TMPA+1
        sta FU+1
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX2
        ldy #0
        lda FU
        sta (TMPC),y
        iny
        lda FU+1
        sta (TMPC),y
        .endm

        .macro ASETD4 b
        jsr FPOP1
        lda #<@b
        sta TMPB
        lda #>@b
        sta TMPB+1
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda WX1,x
        sta (TMPC),y
        dey
        dex
        bpl @lp
        .endm

        .macro ASETI4 b
        jsr FPOP1
        lda @b
        sta TMPB
        lda @b+1
        sta TMPB+1
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda WX1,x
        sta (TMPC),y
        dey
        dex
        bpl @lp
        .endm

; ---- array fields of records (reaction extension) --------------------------------
; RBASED: TMPB = address of label+off.   RBASEP: TMPB = (pointer at p) + off.
; AGETT / ASETT: the array's base is already in TMPB.   PUSHFA: push the address.

        .macro RBASED b, off
        clc
        lda #<@b
        adc #<@off
        sta TMPB
        lda #>@b
        adc #>@off
        sta TMPB+1
        .endm

        .macro RBASEP p, off
        clc
        lda @p
        adc #<@off
        sta TMPB
        lda @p+1
        adc #>@off
        sta TMPB+1
        .endm

        .macro PUSHFAD b, off
        clc
        lda #<@b
        adc #<@off
        sta TMPA
        lda #>@b
        adc #>@off
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro PUSHFAP p, off
        clc
        lda @p
        adc #<@off
        sta TMPA
        lda @p+1
        adc #>@off
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETT1
        jsr AIDX1
        ldy #0
        lda (TMPC),y
        sta TMPA
        lda #0
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETT2
        jsr AIDX2
        ldy #0
        lda (TMPC),y
        sta TMPA
        iny
        lda (TMPC),y
        sta TMPA+1
        jsr PUSHA
        .endm

        .macro AGETT4
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda (TMPC),y
        sta WX1,x
        dey
        dex
        bpl @lp
        jsr FPUSH1
        .endm

        .macro ASETT1
        jsr POPA
        lda TMPA
        sta FU
        jsr AIDX1
        ldy #0
        lda FU
        sta (TMPC),y
        .endm

        .macro ASETT2
        jsr POPA
        lda TMPA
        sta FU
        lda TMPA+1
        sta FU+1
        jsr AIDX2
        ldy #0
        lda FU
        sta (TMPC),y
        iny
        lda FU+1
        sta (TMPC),y
        .endm

        .macro ASETT4
        jsr FPOP1
        jsr AIDX4
        ldx #3
        ldy #3
@lp     lda WX1,x
        sta (TMPC),y
        dey
        dex
        bpl @lp
        .endm

; ---- NEW: allocate n zeroed bytes from the heap, push the address ------------------
        .macro NEW n
        lda #<@n
        sta TMPB
        lda #>@n
        sta TMPB+1
        jsr NEWOBJ
        .endm

        .macro NEWC n, vt                   ; the same, for a class: store the method table pointer
        lda #<@n
        sta TMPB
        lda #>@n
        sta TMPB+1
        jsr NEWMEM
        ldy #0
        lda #<@vt
        sta (TMPA),y
        iny
        lda #>@vt
        sta (TMPA),y
        jsr PUSHA
        .endm

; call a method through the instance's method table: the instance is on the
; run time stack b bytes below the top (under the other arguments), s = slot*2
        .macro VCALL b, s
        ldy #@b
        lda (RUNSTK),y
        sta TMPC
        iny
        lda (RUNSTK),y
        sta TMPC+1
        ldy #0
        lda (TMPC),y
        sta TMPB
        iny
        lda (TMPC),y
        sta TMPB+1
        ldy #@s
        lda (TMPB),y
        sta TMPA
        iny
        lda (TMPB),y
        sta TMPA+1
        jsr VDISPATCH
        .endm

        .macro COPYREC dst, n               ; copy n bytes from the address on the stack to dst
        jsr POPA
        lda #<@dst
        sta TMPB
        lda #>@dst
        sta TMPB+1
        lda #<@n
        sta TMPC
        lda #>@n
        sta TMPC+1
        jsr MEMCPY
        .endm

        .macro COPYIND p, n                 ; copy n bytes from the address on the stack to where p points
        jsr POPA
        lda @p
        sta TMPB
        lda @p+1
        sta TMPB+1
        lda #<@n
        sta TMPC
        lda #>@n
        sta TMPC+1
        jsr MEMCPY
        .endm
