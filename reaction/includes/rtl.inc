; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; rtl.inc - run time library for the reaction compiler (6502asm syntax)
;
; All values are 16 bit, signed where it matters.  A binary operator pops the
; right operand then the left one, and pushes the result.  Comparisons push 1
; or 0.  Division by zero is not trapped: the result is undefined.
; Not provided yet: EXP16S (the ** operator).

; ---- stack ---------------------------------------------------------------

PUSHA   sec                     ; push TMPA
        lda RUNSTK
        sbc #2
        sta RUNSTK
        bcs PUSHA1
        dec RUNSTK+1
PUSHA1  ldy #0
        lda TMPA
        sta (RUNSTK),y
        iny
        lda TMPA+1
        sta (RUNSTK),y
        rts

POPA    ldy #0                  ; pop into TMPA
        lda (RUNSTK),y
        sta TMPA
        iny
        lda (RUNSTK),y
        sta TMPA+1
        jmp POPADJ

POPB    ldy #0                  ; pop into TMPB
        lda (RUNSTK),y
        sta TMPB
        iny
        lda (RUNSTK),y
        sta TMPB+1

POPADJ  clc                     ; drop the item just read
        lda RUNSTK
        adc #2
        sta RUNSTK
        bcc POPADJ1
        inc RUNSTK+1
POPADJ1 rts

POP2    jsr POPB                ; TMPB = right operand, TMPA = left operand
        jmp POPA

; ---- arithmetic ----------------------------------------------------------

ADD16S  jsr POP2
        clc
        lda TMPA
        adc TMPB
        sta TMPA
        lda TMPA+1
        adc TMPB+1
        sta TMPA+1
        jmp PUSHA

SUB16S  jsr POP2
        sec
        lda TMPA
        sbc TMPB
        sta TMPA
        lda TMPA+1
        sbc TMPB+1
        sta TMPA+1
        jmp PUSHA

NEG16S  jsr POPA
        jsr NEGA
        jmp PUSHA

NEGA    sec                     ; TMPA = -TMPA
        lda #0
        sbc TMPA
        sta TMPA
        lda #0
        sbc TMPA+1
        sta TMPA+1
        rts

NEGB    sec                     ; TMPB = -TMPB
        lda #0
        sbc TMPB
        sta TMPB
        lda #0
        sbc TMPB+1
        sta TMPB+1
        rts

AND16S  jsr POP2
        lda TMPA
        and TMPB
        sta TMPA
        lda TMPA+1
        and TMPB+1
        sta TMPA+1
        jmp PUSHA

OR16S   jsr POP2
        lda TMPA
        ora TMPB
        sta TMPA
        lda TMPA+1
        ora TMPB+1
        sta TMPA+1
        jmp PUSHA

XOR16S  jsr POP2
        lda TMPA
        eor TMPB
        sta TMPA
        lda TMPA+1
        eor TMPB+1
        sta TMPA+1
        jmp PUSHA

LSH16S  jsr POP2                ; left << (right & 255)
        ldx TMPB
        beq LSHEND
LSHLP   asl TMPA
        rol TMPA+1
        dex
        bne LSHLP
LSHEND  jmp PUSHA

RSH16S  jsr POP2                ; left >> (right & 255), zero filled
        ldx TMPB
        beq RSHEND
RSHLP   lsr TMPA+1
        ror TMPA
        dex
        bne RSHLP
RSHEND  jmp PUSHA

MUL16S  jsr POP2                ; low 16 bits of the product
        lda #0
        sta TMPC
        sta TMPC+1
MULLP   lda TMPB                ; stop as soon as the multiplier is used up
        ora TMPB+1
        beq MULDN
        lsr TMPB+1
        ror TMPB
        bcc MULNO
        clc
        lda TMPC
        adc TMPA
        sta TMPC
        lda TMPC+1
        adc TMPA+1
        sta TMPC+1
MULNO   asl TMPA
        rol TMPA+1
        jmp MULLP
MULDN   lda TMPC
        sta TMPA
        lda TMPC+1
        sta TMPA+1
        jmp PUSHA

; 16 bit multiply, TMPA * TMPB -> TMPC (low 16 bits); destroys TMPA, TMPB
XMUL    lda #0
        sta TMPC
        sta TMPC+1
        ldx #16
XMULLP  lsr TMPB+1
        ror TMPB
        bcc XMULNO
        clc
        lda TMPC
        adc TMPA
        sta TMPC
        lda TMPC+1
        adc TMPA+1
        sta TMPC+1
XMULNO  asl TMPA
        rol TMPA+1
        dex
        bne XMULLP
        rts

; base ** exponent by repeated squaring: at most 16 squarings and 16 multiplies.
; EXP16S takes the exponent as unsigned (CARD, BYTE); EXP16I is for an INT exponent,
; where a negative exponent gives 0. n**0 = 1.
XB      .bzero 2
XE      .bzero 2
XR      .bzero 2

EXP16I  jsr POP2                ; TMPA = base, TMPB = signed exponent
        lda TMPB+1
        bpl EXPGO
        lda #0
        sta TMPA
        sta TMPA+1
        jmp PUSHA
EXP16S  jsr POP2                ; TMPA = base, TMPB = unsigned exponent
EXPGO
        lda TMPA
        sta XB
        lda TMPA+1
        sta XB+1
        lda TMPB
        sta XE
        lda TMPB+1
        sta XE+1
        lda #0
        sta XR+1
        lda #1
        sta XR
EXPLP   lda XE
        ora XE+1
        beq EXPDN               ; exponent used up
        lsr XE+1
        ror XE                  ; carry = the bit just shifted out
        bcc EXPSQ
        lda XR                  ; result = result * base
        sta TMPA
        lda XR+1
        sta TMPA+1
        lda XB
        sta TMPB
        lda XB+1
        sta TMPB+1
        jsr XMUL
        lda TMPC
        sta XR
        lda TMPC+1
        sta XR+1
EXPSQ   lda XB                  ; base = base * base
        sta TMPA
        sta TMPB
        lda XB+1
        sta TMPA+1
        sta TMPB+1
        jsr XMUL
        lda TMPC
        sta XB
        lda TMPC+1
        sta XB+1
        jmp EXPLP
EXPDN   lda XR
        sta TMPA
        lda XR+1
        sta TMPA+1
        jmp PUSHA

; unsigned TMPA / TMPB: quotient in TMPA, remainder in TMPC
UDIV16  lda #0
        sta TMPC
        sta TMPC+1
        ldx #16
        lda TMPA+1              ; a dividend under 256: its 8 leading bits are zero, skip them
        bne UDIVLP              ; (but a divide by 0 keeps its 16 steps: the quotient is 65535)
        lda TMPB
        ora TMPB+1
        beq UDIVLP
        lda TMPA
        sta TMPA+1
        lda #0
        sta TMPA
        ldx #8
UDIVLP  asl TMPA
        rol TMPA+1
        rol TMPC
        rol TMPC+1
        sec
        lda TMPC
        sbc TMPB
        tay
        lda TMPC+1
        sbc TMPB+1
        bcc UDIVNO
        sta TMPC+1
        sty TMPC
        inc TMPA
UDIVNO  dex
        bne UDIVLP
        rts

; pop operands, remember the signs, make both positive
SDIVPR  jsr POP2
        lda TMPA+1
        sta SGNR                ; remainder takes the sign of the dividend
        eor TMPB+1
        sta SGNQ                ; quotient is negative if the signs differ
        lda TMPA+1
        bpl SDIVP1
        jsr NEGA
SDIVP1  lda TMPB+1
        bpl SDIVP2
        jsr NEGB
SDIVP2  rts

DIV16S  jsr SDIVPR
        jsr UDIV16
        lda SGNQ
        bpl DIVEND
        jsr NEGA
DIVEND  jmp PUSHA

DIV16U  jsr POP2                ; unsigned
        jsr UDIV16
        jmp PUSHA

MOD16U  jsr POP2
        jsr UDIV16
        lda TMPC
        sta TMPA
        lda TMPC+1
        sta TMPA+1
        jmp PUSHA

MOD16S  jsr SDIVPR
        jsr UDIV16
        lda TMPC
        sta TMPA
        lda TMPC+1
        sta TMPA+1
        lda SGNR
        bpl MODEND
        jsr NEGA
MODEND  jmp PUSHA

; ---- comparisons: push 1 for true, 0 for false -----------------------------

SLTAB   sec                     ; TMPA = (TMPA < TMPB) signed
        lda TMPA
        sbc TMPB
        lda TMPA+1
        sbc TMPB+1
        bvc SLT1
        eor #$80
SLT1    bmi SLTYES
        lda #0
        beq SLTSET
SLTYES  lda #1
SLTSET  sta TMPA
        lda #0
        sta TMPA+1
        rts

ULTAB   lda TMPA                ; TMPA = (TMPA < TMPB) unsigned
        cmp TMPB
        lda TMPA+1
        sbc TMPB+1
        lda #0
        bcs ULT1
        lda #1
ULT1    sta TMPA
        lda #0
        sta TMPA+1
        rts

EQAB    lda TMPA                ; TMPA = (TMPA = TMPB)
        cmp TMPB
        bne EQNO
        lda TMPA+1
        cmp TMPB+1
        bne EQNO
        lda #1
        bne EQSET
EQNO    lda #0
EQSET   sta TMPA
        lda #0
        sta TMPA+1
        rts

SWAPAB  ldx TMPA                ; exchange TMPA and TMPB
        lda TMPB
        sta TMPA
        stx TMPB
        ldx TMPA+1
        lda TMPB+1
        sta TMPA+1
        stx TMPB+1
        rts

FLIP1   lda TMPA                ; invert a 0/1 result
        eor #1
        sta TMPA
        rts

LT16S   jsr POP2
        jsr SLTAB
        jmp PUSHA

GT16S   jsr POP2
        jsr SWAPAB
        jsr SLTAB
        jmp PUSHA

GE16S   jsr POP2
        jsr SLTAB
        jsr FLIP1
        jmp PUSHA

LE16S   jsr POP2
        jsr SWAPAB
        jsr SLTAB
        jsr FLIP1
        jmp PUSHA

LT16U   jsr POP2
        jsr ULTAB
        jmp PUSHA

GT16U   jsr POP2
        jsr SWAPAB
        jsr ULTAB
        jmp PUSHA

GE16U   jsr POP2
        jsr ULTAB
        jsr FLIP1
        jmp PUSHA

LE16U   jsr POP2
        jsr SWAPAB
        jsr ULTAB
        jsr FLIP1
        jmp PUSHA

EQ16S   jsr POP2
        jsr EQAB
        jmp PUSHA

NE16S   jsr POP2
        jsr EQAB
        jsr FLIP1
        jmp PUSHA

; ---- array element address: pop the index, TMPB = base, result in TMPC ---------

AIDX4   jsr POPA
        asl TMPA
        rol TMPA+1
        asl TMPA
        rol TMPA+1
        jmp AIDXC

AIDX2   jsr POPA
        asl TMPA
        rol TMPA+1
        jmp AIDXC

AIDX1   jsr POPA
AIDXC   clc
        lda TMPA
        adc TMPB
        sta TMPC
        lda TMPA+1
        adc TMPB+1
        sta TMPC+1
        rts

; ---- MEMCPY: copy TMPC bytes from (TMPA) to (TMPB); the areas may overlap -----------
; Copies forwards when the destination is below the source, else backwards.
MEMCPY  lda TMPC
        ora TMPC+1
        beq MCDONE              ; nothing to copy
        ldy #0
        lda TMPB+1              ; destination above source?
        cmp TMPA+1
        bne MCCMP
        lda TMPB
        cmp TMPA
MCCMP   bcc MCFWD               ; destination < source: forwards
        beq MCDONE              ; same place: nothing to do
        clc                     ; backwards: both pointers to the last byte + 1
        lda TMPA
        adc TMPC
        sta TMPA
        lda TMPA+1
        adc TMPC+1
        sta TMPA+1
        clc
        lda TMPB
        adc TMPC
        sta TMPB
        lda TMPB+1
        adc TMPC+1
        sta TMPB+1
MCBLP   lda TMPA
        bne MCB1
        dec TMPA+1
MCB1    dec TMPA
        lda TMPB
        bne MCB2
        dec TMPB+1
MCB2    dec TMPB
        lda (TMPA),y
        sta (TMPB),y
        lda TMPC
        bne MCB3
        dec TMPC+1
MCB3    dec TMPC
        lda TMPC
        ora TMPC+1
        bne MCBLP
MCDONE  rts
MCFWD   lda (TMPA),y
        sta (TMPB),y
        inc TMPA
        bne MCF1
        inc TMPA+1
MCF1    inc TMPB
        bne MCF2
        inc TMPB+1
MCF2    lda TMPC
        bne MCF3
        dec TMPC+1
MCF3    dec TMPC
        lda TMPC
        ora TMPC+1
        bne MCFWD
        rts

; ---- heap: NEW takes zeroed memory from HEAPPTR (set to SYSMEM at start-up) --------

HEAPPTR .bzero 2

NEWOBJ  jsr NEWMEM
        jmp PUSHA

VDISPATCH jmp (TMPA)            ; go to the routine address in TMPA

NEWMEM  lda HEAPPTR             ; TMPB = size -> TMPA = zeroed block
        sta TMPA
        sta TMPC
        lda HEAPPTR+1
        sta TMPA+1
        sta TMPC+1
        clc
        lda HEAPPTR
        adc TMPB
        sta HEAPPTR
        lda HEAPPTR+1
        adc TMPB+1
        sta HEAPPTR+1
        ldy #0
NEWZ    lda TMPB
        ora TMPB+1
        beq NEWDONE
        lda #0
        sta (TMPC),y
        inc TMPC
        bne NEWZ1
        inc TMPC+1
NEWZ1   lda TMPB
        bne NEWZ2
        dec TMPB+1
NEWZ2   dec TMPB
        jmp NEWZ
NEWDONE rts

; ---- TRY / CATCH / THROW -------------------------------------------------------------------
;
; TRY records a handler: where to go, and the two stack pointers.  The handlers form a list, up to
; TRYMAX deep; the one at the end is the nearest.  Leaving the TRY body takes the handler off
; (dec TRYSP, which the compiler emits).  A throw puts the stack pointers back, takes the handler
; off, leaves the error code in TRYCODE and jumps to the catch address.  Variables are static,
; so nothing else needs undoing.
;
;   TRYPUSH    A, X = the catch address.  Called by TRY.
;   TRYTHROW   the THROW statement: the code is on the run time stack.  With no handler it puts the
;              code back and returns, and the compiler's next instruction calls Error.
;   TRY_THROW  PROC Throw(BYTE code), for ACT code (the library's Error): with no handler it returns.

        .const TRYMAX 8

TRYSP   .bzero 1                ; how many handlers there are
TRYCODE .bzero 1                ; the code of the last throw
TRYPCL  .bzero 8
TRYPCH  .bzero 8
TRYRSL  .bzero 8
TRYRSH  .bzero 8
TRYHW   .bzero 8

TRYPUSH ldy TRYSP
        cpy #TRYMAX
        bcs TRYFULL
        sta TRYPCL,y
        txa
        sta TRYPCH,y
        lda RUNSTK
        sta TRYRSL,y
        lda RUNSTK+1
        sta TRYRSH,y
        tsx
        inx                     ; the stack as it was before the jsr to here
        inx
        txa
        sta TRYHW,y
        inc TRYSP
        rts
TRYFULL lda #254                ; too many TRYs nested: the nearest handler gets 254
        sta TMPA
        jmp TRYUNW

TRYTHROW jsr POPA
        lda TRYSP
        bne TRYUNW
        jmp PUSHA

TRY_THROW jsr POPA
        lda TRYSP
        bne TRYUNW
        rts

TRYUNW  lda TMPA
        sta TRYCODE
        dec TRYSP
        ldx TRYSP
        lda TRYRSL,x
        sta RUNSTK
        lda TRYRSH,x
        sta RUNSTK+1
        lda TRYPCL,x
        sta TMPB
        lda TRYPCH,x
        sta TMPB+1
        lda TRYHW,x
        tax
        txs
        jmp (TMPB)

; ---- STRING / ZTRING ------------------------------------------------------------------------
;
; A STRING is a length byte followed by the characters, a ZTRING the characters and a zero byte.
; A string expression is the address of one of them.  Flag bits:  1 = the source is a ZTRING,
; 2 = the destination (or the right operand) is a ZTRING,  4 = the left operand is already STRTMP.
;
;   STRSET   TMPA = source, TMPB = destination, A = its capacity, X = flags (1 source ZTRING,
;            2 destination ZTRING): copies at most that many characters
;   STRCAT2  A = flags: left and right operands are on the run time stack; the joined string is
;            built in STRTMP (a STRING of up to 255 characters) and its address is pushed
;   STRCMP   same operands: pushes -1, 0 or 1 comparing character by character, then by length
;   ZLEN     TMPA = a string address, A = flags (1 = ZTRING): returns the length in A

STRCAPV .bzero 1
STRFL   .bzero 1
STRN    .bzero 1
STRI    .bzero 1
STRSO   .bzero 1
STRDO   .bzero 1
STRL1   .bzero 1
STRL2   .bzero 1
STRRP   .bzero 2
STRSV   .bzero 1
STRTMP  .bzero 257

STRLEN  ldy #0                  ; length of the string at TMPA, A=1 for a ZTRING
        and #1
        bne STRLEN1
        lda (TMPA),y
        rts
STRLEN1 lda (TMPA),y
        beq STRLEN2
        iny
        bne STRLEN1
        dey                     ; no terminator within 255: 255
STRLEN2 tya
        rts

STRSET  sta STRCAPV
        stx STRFL
        txa
        and #1
        jsr STRLEN              ; A = source length
        cmp STRCAPV
        bcc STRSET1
        lda STRCAPV
STRSET1 sta STRN
        lda STRFL               ; offsets of the first character
        and #1
        eor #1
        sta STRSO
        lda STRFL
        and #2
        lsr
        eor #1
        sta STRDO
        lda #0
        sta STRI
STRSET2 lda STRI
        cmp STRN
        beq STRSET3
        clc
        adc STRSO
        tay
        lda (TMPA),y
        pha
        lda STRI
        clc
        adc STRDO
        tay
        pla
        sta (TMPB),y
        inc STRI
        jmp STRSET2
STRSET3 lda STRFL
        and #2
        bne STRSET4
        ldy #0                  ; STRING: the length goes first
        lda STRN
        sta (TMPB),y
        rts
STRSET4 ldy STRN                ; ZTRING: the zero goes last
        lda #0
        sta (TMPB),y
        rts

; the length of the string at TMPB, A=1 for a ZTRING
STRLENB and #1
        bne STRLENB1
        tay
        lda (TMPB),y
        rts
STRLENB1 ldy #0
STRLENB2 lda (TMPB),y
        beq STRLENB3
        iny
        bne STRLENB2
        dey
STRLENB3 tya
        rts

; STRCAT2: the operands are on the run time stack, A = flags
STRCAT2 sta STRFL
        sta STRSV
        jsr POPB
        jsr POPA
        lda STRFL
        and #4
        bne STRCAT3             ; the left operand is STRTMP already
        lda TMPB
        sta STRRP
        lda TMPB+1
        sta STRRP+1
        lda #<STRTMP
        sta TMPB
        lda #>STRTMP
        sta TMPB+1
        lda STRFL
        and #1
        tax
        lda #255
        jsr STRSET              ; STRTMP = left, as a STRING
        lda STRSV
        sta STRFL
        lda STRRP
        sta TMPB
        lda STRRP+1
        sta TMPB+1
STRCAT3 lda TMPB                ; TMPA = the right operand
        sta TMPA
        lda TMPB+1
        sta TMPA+1
        lda STRFL
        and #2
        lsr
        jsr STRLEN              ; A = its length
        sta STRN
        lda #255
        sec
        sbc STRTMP              ; the room that is left
        cmp STRN
        bcs STRCAT4
        sta STRN
STRCAT4 lda STRFL
        and #2
        lsr
        eor #1
        sta STRSO               ; where its characters start
        lda #0
        sta STRI
STRCAT5 lda STRI
        cmp STRN
        beq STRCAT6
        clc
        adc STRSO
        tay
        lda (TMPA),y
        pha
        lda STRTMP
        clc
        adc STRI
        tax
        pla
        sta STRTMP+1,x
        inc STRI
        jmp STRCAT5
STRCAT6 lda STRTMP
        clc
        adc STRN
        sta STRTMP
        lda #<STRTMP
        sta TMPA
        lda #>STRTMP
        sta TMPA+1
        jmp PUSHA

; STRCMP: the operands are on the run time stack, A = flags; pushes -1, 0 or 1
STRCMP  sta STRFL
        jsr POPB
        jsr POPA
        lda STRFL
        and #1
        jsr STRLEN
        sta STRL1
        lda STRFL
        and #2
        lsr
        jsr STRLENB
        sta STRL2
        lda STRFL
        and #1
        eor #1
        sta STRSO
        lda STRFL
        and #2
        lsr
        eor #1
        sta STRDO
        lda #0
        sta STRI
STRCMP1 lda STRI
        cmp STRL1
        beq STRCMP4             ; the left one ended
        cmp STRL2
        beq STRCMP5             ; the right one ended first
        clc
        adc STRSO
        tay
        lda (TMPA),y
        sta STRN
        lda STRI
        clc
        adc STRDO
        tay
        lda (TMPB),y
        cmp STRN
        beq STRCMP2
        bcc STRCMP5             ; right < left
        lda #$ff
        jmp STRCMP6
STRCMP2 inc STRI
        jmp STRCMP1
STRCMP4 cmp STRL2
        beq STRCMP7             ; the same length: equal
        lda #$ff                ; left is the shorter
        jmp STRCMP6
STRCMP5 lda #1
        jmp STRCMP6
STRCMP7 lda #0
STRCMP6 sta TMPA
        bpl STRCMP8
        lda #$ff
        jmp STRCMP9
STRCMP8 lda #0
STRCMP9 sta TMPA+1
        jmp PUSHA
