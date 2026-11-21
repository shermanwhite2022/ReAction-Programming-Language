; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; fp.inc - floating point support for the reaction compiler (6502asm syntax)
;
; NUMBER FORMAT (4 bytes, as in the Rankin/Wozniak routines): byte 0 is the
; exponent (power of two, +128), bytes 1-3 a 24 bit two's complement mantissa
; with the binary point after bit 6 of byte 1, so the value is
; mantissa/2^22 * 2^(exponent-128).  About 7 digits, range about 1e-38..1e38.
; 0.0 is all four bytes zero.  On the run time stack a REAL takes 4 bytes
; (two 16 bit slots) with byte 0 at the lowest address.
;
; THE WOZNIAK/RANKIN PART (everything below "BEGIN WOZ") is from Dr. Dobb's
; Journal, August 1976, pages 17-19, July 5 1976 listing, by R. Rankin and
; S. Wozniak: FADD FSUB FMUL FDIV FLOAT FIX LOG LOG10 EXP.  Checked by
; assembling it at the printed addresses: all 344 instructions match the
; printed bytes.  Changes: every label has a W prefix (WFADD, WLOG ...), the
; zero page locations are moved (see mdefs.inc, FPZ), the .org lines are gone,
; and Rankin's errata of Sept. 22 1976 is applied to LOG (without it LOG gave
; wrong results for arguments below 1).  Their error exits are BRK instructions
; (non-positive LOG argument, overflow).
;
; THE REST is new: stack interface, conversions, comparisons, sin cos tan
; atan ln log2 log10.  The routines the compiler calls (stack in, stack out):
;   FADDS FSUBS FMULS FDIVS FNEGS      two REALs -> REAL (one for FNEGS)
;   FEQS FNES FLTS FGTS FLES FGES      two REALs -> 1 or 0 (16 bit)
;   ITOFS   16 bit int on top -> REAL      ITOF2S  int under a REAL -> REAL
;   FTOIS   REAL -> 16 bit int, truncated toward zero, saturates at +-32767
;   FTOUS   REAL -> CARD (unsigned 16 bit), truncated toward zero, exact up to 65535
;   FSIN FCOS FTAN FATAN FLN FLOG2 FLOG10   REAL -> REAL (angles in radians)
; ACCURACY (measured against double precision): about 6 digits.  ln/log: error up
; to about 4e-6; atan about 3e-7; sin/cos about 4e-6 + 3e-7*|x| (the angle is
; reduced modulo 2 pi in 24 bit arithmetic, so large angles lose digits).
; SIN, COS and TAN return 0.0 when |x/2pi| >= 32768 (|x| above about 205000).


; 
; 
; NATURAL LOG OF MANT/EXP1 WITH RESULT IN MANT/EXP1
; 
WLOG     lda  WM1           ; 
        beq  WERROR        ; 
        bpl  WCONT         ; IF ARG>0 OK
WERROR   brk               ; ERROR ARG<=0
; 
WCONT    jsr  WSWAP         ; MOVE ARG TO EXP/MANT2
        ldx  #0            ; ERRATA 9/22/76: X = high byte of the exponent
        lda  WX2           ; HOLD EXPONENT
        ldy  #$80         ; 
        sty  WX2           ; SET EXPONENT 2 TO 0 ($80)
        eor  #$80         ; COMPLIMENT SIGN BIT OF ORIGINAL EXPONENT
        sta  WM1+1         ; SET EXPONENT INTO MANTISSA 1 FOR FLOAT
        bpl  *+3           ; ERRATA 9/22/76: is the exponent negative?
        dex                ; yes, high byte is $FF
        stx  WM1           ; upper byte of the exponent
        jsr  WFLOAT        ; CONVERT TO FLOATING POINT
        ldx  #3           ; 4 BYTE TRANSFERS
WSEXP1   lda  WX2,X         ; 
        sta  WZ,X          ; COPY MANTISSA TO Z
        lda  WX1,X         ; 
        sta  WSEXP,X       ; SAVE EXPONENT IN SEXP
        lda  WR22,X        ; LOAD EXP/MANT1 WITH SQRT(2)
        sta  WX1,X         ; 
        dex               ; 
        bpl  WSEXP1        ; 
        jsr  WFSUB         ; Z-SQRT(2)
        ldx  #3           ; 4 BYTE TRANSFER
WSAVET   lda  WX1,X         ; SAVE EXP/MANT1 AS T
        sta  WT,X          ; 
        lda  WZ,X          ; LOAD EXP/MANT1 WITH Z
        sta  WX1,X         ; 
        lda  WR22,X        ; LOAD EXP/MANT2 WITH SQRT(2)
        sta  WX2,X         ; 
        dex               ; 
        bpl  WSAVET        ; 
        jsr  WFADD         ; Z+SQRT(2)
        ldx  #3           ; 4 BYTE TRANSFER
WTM2     lda  WT,X          ; 
        sta  WX2,X         ; LOAD T INTO EXP/MANT2
        dex               ; 
        bpl  WTM2          ; 
        jsr  WFDIV         ; T=(Z-SQRT(2))/(Z+SQRT(2))
        ldx  #3           ; 4 BYTE TRANSFER
WMIT     lda  WX1,X         ; 
        sta  WT,X          ; COPY EXP/MANT1 TO T AND
        sta  WX2,X         ; LOAD EXP/MANT2 WITH T
        dex               ; 
        bpl  WMIT          ; 
        jsr  WFMUL         ; T*T
        jsr  WSWAP         ; MOVE T*T TO EXP/MANT2
        ldx  #3           ; 4 BYTE TRANSFER
WMIC     lda  WC,X          ; 
        sta  WX1,X         ; LOAD EXP/MANT1 WITH C
        dex               ; 
        bpl  WMIC          ; 
        jsr  WFSUB         ; T*T-C
        ldx  #3           ; 4 BYTE TRANSFER
WM2MB    lda  WMB,X         ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH MB
        dex               ; 
        bpl  WM2MB         ; 
        jsr  WFDIV         ; MB/(T*T-C)
        ldx  #3           ; 
WM2A1    lda  WA1,X         ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH A1
        dex               ; 
        bpl  WM2A1         ; 
        jsr  WFADD         ; MB/(T*T-C)+A1
        ldx  #3           ; 4 BYTE TRANSFER
WM2T     lda  WT,X          ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH T
        dex               ; 
        bpl  WM2T          ; 
        jsr  WFMUL         ; (MB/(T*T-C)+A1)*T
        ldx  #3           ; 4 BYTE TRANSFER
WM2MHL   lda  WMHLF,X       ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH MHLF (.5)
        dex               ; 
        bpl  WM2MHL        ; 
        jsr  WFADD         ; +.5
        ldx  #3           ; 4 BYTE TRANSFER
WLDEXP   lda  WSEXP,X       ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH ORIGINAL EXPONENT
        dex               ; 
        bpl  WLDEXP        ; 
        jsr  WFADD         ; +EXPN
        ldx  #3           ; 4 BYTE TRANSFER
WMLE2    lda  WLE2,X        ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH LN(2)
        dex               ; 
        bpl  WMLE2         ; 
        jsr  WFMUL         ; *LN(2)
        rts               ; RETURN RESULT IN MANT/EXP1
; 
; COMMON LOG OF MANT/EXP1 RESULT IN MANT/EXP1
; 
WLOG10   jsr  WLOG          ; COMPUTE NATURAL LOG
        ldx  #3           ; 
WL10     lda  WLN10,X       ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH 1/LN(10)
        dex               ; 
        bpl  WL10          ; 
        jsr  WFMUL         ; LOG10(X)=LN(X)/LN(10)
        rts               ; 
; 
WLN10    .byte $7E,$6F  ; DCM 0.4342945
        .byte $2D,$ED  ; (continuation)
WR22     .byte $80,$5A  ; DCM 1.4142136
        .byte $82,$7A  ; (continuation)
WLE2     .byte $7F,$58  ; DCM 0.69314718
        .byte $B9,$0C  ; (continuation)
WA1      .byte $80,$52  ; DCM 1.2920074
        .byte $B0,$40  ; (continuation)
WMB      .byte $81,$AB  ; DCM -2.6398577
        .byte $86,$49  ; (continuation)
WC       .byte $80,$6A  ; DCM 1.6567626
        .byte $08,$66  ; (continuation)
WMHLF    .byte $7F,$40  ; DCM 0.5
        .byte $00,$00  ; (continuation)
; 
; 
; EXP OF MANT/EXP1 RESULT IN MANT/EXP1
; 
WEXP     ldx  #3           ; 4 BYTE TRANSFER
        lda  WL2E,X        ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH LOG BASE 2 OF E
        dex               ; 
        bpl  WEXP+2        ; 
        jsr  WFMUL         ; LOG2(3)*X
        ldx  #3           ; 4 BYTE TRANSFER
WFSA     lda  WX1,X         ; 
        sta  WZ,X          ; STORE EXP/MANT1 IN Z
        dex               ; 
        bpl  WFSA          ; SAVE Z=LN(2)*X
        jsr  WFIX          ; CONVERT CONTENTS OF EXP/MANT1 TO AN INTEGER
        lda  WM1+1         ; 
        sta  WINT          ; SAVE RESULT AS INT
        sec               ; SET CARRY FOR SUBTRACTION
        sbc  #124         ; INT-124
        lda  WM1           ; 
        sbc  #0           ; 
        bpl  WOVFLW        ; OVERFLOW INT>=124
        clc               ; CLEAR CARRY FOR ADD
        lda  WM1+1         ; 
        adc  #120         ; ADD 120 TO INT
        lda  WM1           ; 
        adc  #0           ; 
        bpl  WCONTIN       ; IF RESULT POSITIVE CONTINUE
        lda  #0           ; INT<-120 SET RESULT TO ZERO AND RETURN
        ldx  #3           ; 4 BYTE MOVE
WZERO    sta  WX1,X         ; SET EXP/MANT1 TO ZERO
        dex               ; 
        bpl  WZERO         ; 
        rts               ; RETURN
; 
WOVFLW   brk               ; OVERFLOW
; 
WCONTIN  jsr  WFLOAT        ; FLOAT INT
        ldx  #3           ; 
WENTD    lda  WZ,X          ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH Z
        dex               ; 
        bpl  WENTD         ; 
        jsr  WFSUB         ; Z*Z-FLOAT(INT)
        ldx  #3           ; 4 BYTE MOVE
WZSAV    lda  WX1,X         ; 
        sta  WZ,X          ; SAVE EXP/MANT1 IN Z
        sta  WX2,X         ; COPY EXP/MANT1 TO EXP/MANT2
        dex               ; 
        bpl  WZSAV         ; 
        jsr  WFMUL         ; Z*Z
        ldx  #3           ; 4 BYTE MOVE
WLA2     lda  WA2,X         ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH A2
        lda  WX1,X         ; 
        sta  WSEXP,X       ; SAVE EXP/MANT1 AS SEXP
        dex               ; 
        bpl  WLA2          ; 
        jsr  WFADD         ; Z*Z+A2
        ldx  #3           ; 4 BYTE MOVE
WLB2     lda  WB2,X         ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH B2
        dex               ; 
        bpl  WLB2          ; 
        jsr  WFDIV         ; T=B/(Z*Z+A2)
        ldx  #3           ; 4 BYTE MOVE
WDLOAD   lda  WX1,X         ; 
        sta  WT,X          ; SAVE EXP/MANT1 AS T
        lda  WC2,X         ; 
        sta  WX1,X         ; LOAD EXP/MANT1 WITH C2
        lda  WSEXP,X       ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH SEXP
        dex               ; 
        bpl  WDLOAD        ; 
        jsr  WFMUL         ; Z*Z*C2
        jsr  WSWAP         ; MOVE EXP/MANT1 TO EXP/MANT2
        ldx  #3           ; 4 BYTE TRANSFER
WLTMP    lda  WT,X          ; 
        sta  WX1,X         ; LOAD EXP/MANT1 WITH T
        dex               ; 
        bpl  WLTMP         ; 
        jsr  WFSUB         ; C2*Z*Z-B2/(Z*Z+A2)
        ldx  #3           ; 4 BYTE TRANSFER
WLDD     lda  WD,X          ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH D
        dex               ; 
        bpl  WLDD          ; 
        jsr  WFADD         ; D+C2*Z*Z-B2/(Z*Z+A2)
        jsr  WSWAP         ; MOVE EXP/MANT1 TO EXP/MANT2
        ldx  #3           ; 4 BYTE TRANSFER
WLFA     lda  WZ,X          ; 
        sta  WX1,X         ; LOAD EXP/MANT1 WITH Z
        dex               ; 
        bpl  WLFA          ; 
        jsr  WFSUB         ; -Z+D+C2*Z*Z-B2/(Z*Z+A2)
        ldx  #3           ; 4 BYTE TRANSFER
WLF3     lda  WZ,X          ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH Z
        dex               ; 
        bpl  WLF3          ; 
        jsr  WFDIV         ; Z/(**** )
        ldx  #3           ; 4 BYTE TRANSFER
WLD12    lda  WMHLF,X       ; 
        sta  WX2,X         ; LOAD EXP/MANT2 WITH .5
        dex               ; 
        bpl  WLD12         ; 
        jsr  WFADD         ; +Z/(***)+.5
        sec               ; ADD INT TO EXPONENT WITH CARRY SET
        lda  WINT          ; TO MULTIPLY BY
        adc  WX1           ; 2**(INT+1)
        sta  WX1           ; RETURN RESULT TO EXPONENT
        rts               ; RETURN ANS=(.5+Z/(-Z+D+C2*Z*Z-B2/(Z*Z+A2))*2**(INT+1)
WL2E     .byte $80,$5C  ; DCM 1.4426950409
        .byte $55,$1E  ; (continuation)
WA2      .byte $86,$57  ; DCM 87.417497202
        .byte $6A,$E1  ; (continuation)
WB2      .byte $89,$4D  ; DCM 617.9722695
        .byte $3F,$1D  ; (continuation)
WC2      .byte $7B,$46  ; DCM .03465735903
        .byte $FA,$70  ; (continuation)
WD       .byte $83,$4F  ; DCM 9.9545957821
        .byte $A3,$03  ; (continuation)
; 
; 
; BASIC FLOATING POINT ROUTINES
; 
WADD     clc               ; CLEAR CARRY
        ldx  #$02         ; INDEX FOR 3-BYTE ADD
WADD1    lda  WM1,X         ; 
        adc  WM2,X         ; ADD A BYTE OF MANT2 TO MANT1
        sta  WM1,X         ; 
        dex               ; ADVANCE INDEX TO NEXT MORE SIGNIF.BYTE
        bpl  WADD1         ; LOOP UNTIL DONE.
        rts               ; RETURN
WMD1     asl  WSIGN         ; CLEAR LSB OF SIGN
        jsr  WABSWAP       ; ABS VAL OF MANT1, THEN SWAP MANT2
WABSWAP  bit  WM1           ; MANT1 NEG?
        bpl  WABSWP1       ; NO,SWAP WITH MANT2 AND RETURN
        jsr  WFCOMPL       ; YES, COMPLIMENT IT.
        inc  WSIGN         ; INCR SIGN, COMPLEMENTING LSB
WABSWP1  sec               ; SET CARRY FOR RETURN TO MUL/DIV
; 
; SWAP EXP/MANT1 WITH EXP/MANT2
; 
WSWAP    ldx  #$04         ; INDEX FOR 4-BYTE SWAP.
WSWAP1   sty  WE-1,X        ; 
        lda  WX1-1,X       ; SWAP A BYTE OF EXP/MANT1 WITH
        ldy  WX2-1,X       ; EXP/MANT2 AND LEAVEA COPY OF
        sty  WX1-1,X       ; MANT1 IN E(3BYTES). E+3 USED.
        sta  WX2-1,X       ; 
        dex               ; ADVANCE INDEX TO NEXT BYTE
        bne  WSWAP1        ; LOOP UNTIL DONE.
        rts               ; 
; 
; 
; 
; CONVERT 16 BIT INTEGER IN M1(HIGH) AND M1+1(LOW) TO F.P.
; RESULT IN EXP/MANT1.  EXP/MANT2 UNEFFECTED
; 
; 
WFLOAT   lda  #$8E         ; 
        sta  WX1           ; SET EXPN TO 14 DEC
        lda  #0           ; CLEAR LOW ORDER BYTE
        sta  WM1+2         ; 
        beq  WNORM         ; NORMALIZE RESULT
WNORM1   dec  WX1           ; DECREMENT EXP1
        asl  WM1+2         ; 
        rol  WM1+1         ; SHIFT MANT1 (3 BYTES) LEFT
        rol  WM1           ; 
WNORM    lda  WM1           ; HIGH ORDER MANT1 BYTE
        asl               ; UPPER TWO BITS UNEQUAL?
        eor  WM1           ; 
        bmi  WRTS1         ; YES,RETURN WITH MANT1 NORMALIZED
        lda  WX1           ; EXP1 ZERO?
        bne  WNORM1        ; NO, CONTINUE NORMALIZING
WRTS1    rts               ; RETURN
; 
; 
; EXP/MANT2-EXP/MANT1 RESULT IN EXP/MANT1
; 
WFSUB    jsr  WFCOMPL       ; CMPL MANT1 CLEARS CARRY UNLESS ZERO
WSWPALG  jsr  WALGNSW       ; RIGHT SHIFT MANT1 OR SWAP WITH MANT2 ON CARRY
; 
; ADD EXP/MANT1 AND EXP/MANT2 RESULT IN EXP/MANT1
; 
WFADD    lda  WX2           ; 
        cmp  WX1           ; COMPARE EXP1 WITH EXP2
        bne  WSWPALG       ; IF UNEQUAL, SWAP ADDENDS OR ALIGN MANTISSAS
        jsr  WADD          ; ADD ALIGNED MANTISSAS
WADDEND  bvc  WNORM         ; NO OVERFLOW, NORMALIZE RESULTS
        bvs  WRTLOG        ; OV: SHIFT MANT1 RIGHT. NOTE CARRY IS CORRECT SIGN
WALGNSW  bcc  WSWAP         ; SWAP IF CARRY CLEAR, ELSE SHIFT RIGHT ARITH.
WRTAR    lda  WM1           ; SIGN OF MANT1 INTO CARRY FOR
        asl               ; RIGHT ARITH SHIFT
WRTLOG   inc  WX1           ; INCR EXP1 TO COMPENSATE FOR RT SHIFT
        beq  WOVFL         ; EXP1 OUT OF RANGE.
WRTLOG1  ldx  #$FA         ; INDEX FOR 6 BYTE RIGHT SHIFT
WROR1    lda  #$80         ; 
        bcs  WROR2         ; 
        asl               ; 
WROR2    lsr  WE+3,X        ; SIMULATE ROR E+3,X
        ora  WE+3,X        ; 
        sta  WE+3,X        ; 
        inx               ; NEXT BYTE OF SHIFT
        bne  WROR1         ; LOOP UNTIL DONE
        rts               ; RETURN
; 
; 
; EXP/MANT1 X EXP/MANT2 RESULT IN EXP/MANT1
; 
WFMUL    jsr  WMD1          ; ABS. VAL OF MANT1, MANT2
        adc  WX1           ; ADD EXP1 TO EXP2 FOR PRODUCT EXPONENT
        jsr  WMD2          ; CHECK PRODUCT EXP AND PREPARE FOR MUL
        clc               ; CLEAR CARRY
WMUL1    jsr  WRTLOG1       ; MANT1 AND E RIGHT.(PRODUCT AND MPLIER)
        bcc  WMUL2         ; IF CARRY CLEAR, SKIP PARTIAL PRODUCT
        jsr  WADD          ; ADD MULTIPLICAN TO PRODUCT
WMUL2    dey               ; NEXT MUL ITERATION
        bpl  WMUL1         ; LOOP UNTIL DONE
WMDEND   lsr  WSIGN         ; TEST SIGN (EVEN/ODD)
WNORMX   bcc  WNORM         ; IF EXEN, NORMALIZE PRODUCT, ELSE COMPLEMENT
WFCOMPL  sec               ; SET CARRY FOR SUBTRACT
        ldx  #$03         ; INDEX FOR 3 BYTE SUBTRACTION
WCOMPL1  lda  #$00         ; CLEAR A
        sbc  WX1,X         ; SUBTRACT BYTE OF EXP1
        sta  WX1,X         ; RESTORE IT
        dex               ; NEXT MORE SIGNIFICANT BYTE
        bne  WCOMPL1       ; LOOP UNTIL DONE
        beq  WADDEND       ; NORMALIZE (OR SHIFT RIGHT IF OVERFLOW)
; 
; 
; EXP/MANT2 / EXP/MANT1 RESULT IN EXP/MANT1
; 
WFDIV    jsr  WMD1          ; TAKE ABS VAL OF MANT1, MANT2
        sbc  WX1           ; SUBTRACT EXP1 FROM EXP2
        jsr  WMD2          ; SAVE AS QUOTIENT EXP
WDIV1    sec               ; SET CARRY FOR SUBTRACT
        ldx  #$02         ; INDEX FOR 3-BYTE INSTRUCTION
WDIV2    lda  WM2,X         ; 
        sbc  WE,X          ; SUBTRACT A BYTE OF E FROM MANT2
        pha               ; SAVE ON STACK
        dex               ; NEXT MORE SIGNIF BYTE
        bpl  WDIV2         ; LOOP UNTIL DONE
        ldx  #$FD         ; INDEX FOR 3-BYTE CONDITIONAL MOVE
WDIV3    pla               ; PULL A BYTE OF DIFFERENCE OFF STACK
        bcc  WDIV4         ; IF MANT2<E THEN DONT RESTORE MANT2
        sta  WM2+3,X       ; 
WDIV4    inx               ; NEXT LESS SIGNIF BYTE
        bne  WDIV3         ; LOOP UNTIL DONE
        rol  WM1+2         ; 
        rol  WM1+1         ; ROLL QUOTIENT LEFT, CARRY INTO LSB
        rol  WM1           ; 
        asl  WM2+2         ; 
        rol  WM2+1         ; SHIFT DIVIDEND LEFT
        rol  WM2           ; 
        bcs  WOVFL         ; OVERFLOW IS DUE TO UNNORMALIZED DIVISOR
        dey               ; NEXT DIVIDE ITERATION
        bne  WDIV1         ; LOOP UNTIL DONE 23 ITERATIONS
        beq  WMDEND        ; NORMALIZE QUOTIENT AND CORRECT SIGN
WMD2     stx  WM1+2         ; 
        stx  WM1+1         ; CLR MANT1 (3 BYTES) FOR MUL/DIV
        stx  WM1           ; 
        bcs  WOVCHK        ; IF EXP CALC SET CARRY, CHECK FOR OVFL
        bmi  WMD3          ; IF NEG NO UNDERFLOW
        pla               ; POP ONE
        pla               ; RETURN LEVEL
        bcc  WNORMX        ; CLEAR X1 AND RETURN
WMD3     eor  #$80         ; COMPLIMENT SIGN BIT OF EXP
        sta  WX1           ; STORE IT
        ldy  #$17         ; COUNT FOR 24 MUL OR 23 DIV ITERATIONS
        rts               ; RETURN
WOVCHK   bpl  WMD3          ; IF POS EXP THEN NO OVERFLOW
WOVFL    brk               ; 
; 
; 
; CONVERT EXP/MANT1 TO INTEGER IN M1 (HIGH) AND M1+1(LOW)
; EXP/MANT2 UNEFFECTED
; 
        jsr  WRTAR         ; SHIFT MANT1 RT AND INCREMENT EXPNT
WFIX     lda  WX1           ; CHECK EXPONENT
        cmp  #$8E         ; IS EXPONENT 14?
        bne  WFIX-3        ; NO, SHIFT
WRTRN    rts               ; RETURN


; ===========================================================================
; END WOZ.  Everything from here on is new.
; ===========================================================================

        .macro FMOVE dst, src       ; copy a 4 byte real
        lda @src
        sta @dst
        lda @src+1
        sta @dst+1
        lda @src+2
        sta @dst+2
        lda @src+3
        sta @dst+3
        .endm

        .macro FSUBC c              ; X1 := X1 - constant (clobbers X2)
        _FMOVE WX2, WX1
        _FMOVE WX1, @c
        jsr FPSUB
        .endm

        .macro FSETPOLY tab, n      ; point the polynomial evaluator at a table
        lda #<@tab
        sta FPTR
        lda #>@tab
        sta FPTR+1
        lda #@n
        sta FCNT
        .endm

; ---- stack interface -------------------------------------------------------

FPUSH1  sec                     ; push X1 (4 bytes)
        lda RUNSTK
        sbc #4
        sta RUNSTK
        bcs FPUSH1A
        dec RUNSTK+1
FPUSH1A ldy #3
FPUSH1B lda WX1,y
        sta (RUNSTK),y
        dey
        bpl FPUSH1B
        rts

FPOP1   ldy #3                  ; pop into X1
FPOP1A  lda (RUNSTK),y
        sta WX1,y
        dey
        bpl FPOP1A
        jmp FPDROP

FPOP2   ldy #3                  ; pop into X2
FPOP2A  lda (RUNSTK),y
        sta WX2,y
        dey
        bpl FPOP2A

FPDROP  clc
        lda RUNSTK
        adc #4
        sta RUNSTK
        bcc FPDROP1
        inc RUNSTK+1
FPDROP1 rts

FPOP12  jsr FPOP1               ; right operand -> X1, left operand -> X2
        jmp FPOP2

; X1 := X2 - X1, but exactly 0.0 when they are the same number (WFSUB leaves a
; tiny nonzero result then)
FPSUB   ldx #3
FPSUB1  lda WX1,x
        cmp WX2,x
        bne FPSUB2
        dex
        bpl FPSUB1
        inx
        stx WX1
        stx WM1
        stx WM1+1
        stx WM1+2
        rts
FPSUB2  jmp WFSUB

; ---- arithmetic: left op right, right was on top of the stack --------------

FADDS   jsr FPOP12
        jsr WFADD
        jmp FPUSH1

FSUBS   jsr FPOP12              ; X2 - X1
        jsr FPSUB
        jmp FPUSH1

FMULS   jsr FPOP12
        jsr WFMUL
        jmp FPUSH1

FDIVS   jsr FPOP12              ; X2 / X1
        jsr WFDIV
        jmp FPUSH1

FNEGS   jsr FPOP1
        jsr WFCOMPL
        jmp FPUSH1

; ---- comparisons: push 1 or 0 ----------------------------------------------

FCMPD   jsr FPOP12              ; X1 = left - right
        jmp FPSUB

FBOOL   sta TMPA                ; A = 0 or 1 -> push as 16 bit
        lda #0
        sta TMPA+1
        jmp PUSHA

FLTS    jsr FCMPD
        lda #0
        ldx WM1
        bpl FBOOL               ; left-right >= 0: false
        lda #1
        bne FBOOL

FGES    jsr FCMPD
        lda #0
        ldx WM1
        bmi FBOOL
        lda #1
        bne FBOOL

FEQS    jsr FCMPD
        lda #0
        ldx WM1
        bne FBOOL
        ldx WM1+1
        bne FBOOL
        ldx WM1+2
        bne FBOOL
        lda #1
        bne FBOOL

FNES    jsr FCMPD
        lda #1
        ldx WM1
        bne FBOOL
        ldx WM1+1
        bne FBOOL
        ldx WM1+2
        bne FBOOL
        lda #0
        beq FBOOL

FGTS    jsr FCMPD               ; true if left-right > 0
        lda #0
        ldx WM1
        bmi FBOOL               ; negative: false
        bne FGTS1               ; positive mantissa: true
        ldx WM1+1
        bne FGTS1
        ldx WM1+2
        beq FBOOL               ; zero: false
FGTS1   lda #1
        bne FBOOL

FLES    jsr FCMPD               ; true if left-right <= 0
        lda #1
        ldx WM1
        bmi FBOOL               ; negative: true
        bne FLES1               ; positive: false
        ldx WM1+1
        bne FLES1
        ldx WM1+2
        bne FLES1
        beq FBOOL               ; zero: true
FLES1   lda #0
        beq FBOOL

; ---- conversions -------------------------------------------------------------

ITOFS   jsr POPA                ; 16 bit int -> REAL
        lda TMPA+1
        sta WM1
        lda TMPA
        sta WM1+1
        jsr WFLOAT
        jmp FPUSH1

UTOFS   jsr POPA                ; unsigned 16 bit -> REAL
        lda #0
        sta WM1
        lda TMPA+1
        sta WM1+1
        lda TMPA
        sta WM1+2
        lda #$96                ; 22: the value sits in the low 16 bits of the mantissa
        sta WX1
        jsr WNORM
        jmp FPUSH1

UTOF2S  jsr FPOP1               ; an unsigned int sits under the REAL on top
        _FMOVE FC, WX1
        jsr UTOFS
        _FMOVE WX1, FC
        jmp FPUSH1

ITOF2S  jsr FPOP1               ; a 16 bit int sits under the REAL on top
        _FMOVE FC, WX1
        jsr ITOFS
        _FMOVE WX1, FC
        jmp FPUSH1

FTOIS   jsr FPOP1               ; REAL -> 16 bit int
        lda WX1
        cmp #$8F                ; |x| >= 32768: saturate
        bcc FTOI1
        lda WM1
        bmi FTOIN
        lda #$7F
        sta TMPA+1
        lda #$FF
        bne FTOIP
FTOIN   lda #$80
        sta TMPA+1
        lda #0
FTOIP   sta TMPA
        jmp PUSHA
FTOI1   lda WM1
        bpl FTOI2
        jsr WFCOMPL             ; negative: work on |x| then negate
        jsr WFIX
        sec
        lda #0
        sbc WM1+1
        sta TMPA
        lda #0
        sbc WM1
        sta TMPA+1
        jmp PUSHA
FTOI2   jsr WFIX
        lda WM1+1
        sta TMPA
        lda WM1
        sta TMPA+1
        jmp PUSHA

; FTOUS: REAL -> unsigned 16 bit (CARD), truncated toward zero.  0..65535 is exact,
; above that it saturates at 65535, and a negative below -32768 gives 0.  A small
; negative value wraps as a CARD does (-300.0 -> 65236), the same as FTOIS then CARD.
FTOUS   jsr FPOP1
        lda WX1
        cmp #$8F
        bcc FTOI1               ; |x| < 32768: same as FTOIS
        lda WM1
        bmi FTOUN               ; a large negative number
        lda WX1
        cmp #$8F
        bne FTOUP               ; 65536 or more
        asl WM1+2               ; 32768..65535: the mantissa shifted one more place
        rol WM1+1
        rol WM1
        lda WM1+1
        sta TMPA
        lda WM1
        sta TMPA+1
        jmp PUSHA
FTOUP   lda #$FF
        sta TMPA
        sta TMPA+1
        jmp PUSHA
FTOUN   lda #0
        sta TMPA
        sta TMPA+1
        jmp PUSHA

; ---- polynomial: X1 = c[0]; then repeat X1 = X1*FU + c[i] ------------------
; FPTR points at the table of FCNT coefficients (highest power first)

FPOLY   ldy #3
FPOLY0  lda (FPTR),y
        sta WX1,y
        dey
        bpl FPOLY0
        jsr FPTR4
FPOLY1  dec FCNT
        beq FPOLYX
        _FMOVE WX2, FU
        jsr WFMUL
        ldy #3
FPOLY2  lda (FPTR),y
        sta WX2,y
        dey
        bpl FPOLY2
        jsr FPTR4
        jsr WFADD
        jmp FPOLY1
FPOLYX  rts

FPTR4   clc
        lda FPTR
        adc #4
        sta FPTR
        bcc FPTR41
        inc FPTR+1
FPTR41  rts

; ---- sin / cos: X1 = angle in turns (radians / 2pi) -------------------------

FPSINT  lda WX1                 ; |t| >= 32768: give up, return 0.0
        cmp #$8F
        bcc FPSIN1
        lda #0
        sta WX1
        sta WM1
        sta WM1+1
        sta WM1+2
        rts
FPSIN1  _FMOVE FA, WX1          ; t
        jsr WFIX                ; floor(t)
        jsr WFLOAT
        _FMOVE WX2, FA
        jsr WFSUB               ; X1 = t - floor(t), 0 <= f < 1
        lda #0
        sta FSGN
        _FMOVE FB, WX1
        _FSUBC HALF
        lda WM1
        bmi FPSIN2              ; f < 0.5
        lda #1                  ; f >= 0.5: sin(2 pi f) = -sin(2 pi (f-0.5))
        sta FSGN
        bne FPSIN3
FPSIN2  _FMOVE WX1, FB
FPSIN3  _FMOVE FB, WX1
        _FSUBC QUART
        lda WM1
        bmi FPSIN4              ; f < 0.25
        _FMOVE WX2, QUART       ; f >= 0.25: use 0.5 - f = 0.25 - (f-0.25)
        jsr WFSUB
        jmp FPSIN5
FPSIN4  _FMOVE WX1, FB
FPSIN5  _FMOVE WX2, FOUR       ; y = 4 f, 0 <= y <= 1
        jsr WFMUL
        _FMOVE FY, WX1
        _FMOVE WX2, WX1
        jsr WFMUL               ; u = y*y
        _FMOVE FU, WX1
        _FSETPOLY SINTAB, 6
        jsr FPOLY
        _FMOVE WX2, FY
        jsr WFMUL               ; sin = y * poly(u)
        lda FSGN
        beq FPSINX
        jsr WFCOMPL
FPSINX  rts

FPSIN   _FMOVE WX2, INV2PI      ; X1 = x radians
        jsr WFMUL
        jmp FPSINT

FPCOS   _FMOVE WX2, INV2PI
        jsr WFMUL
        _FMOVE WX2, QUART       ; cos x = sin(x + pi/2) = sin turns + 0.25
        jsr WFADD
        jmp FPSINT

FPTAN   _FMOVE FC, WX1
        jsr FPSIN
        _FMOVE FD, WX1
        _FMOVE WX1, FC
        jsr FPCOS
        _FMOVE WX2, FD
        jmp WFDIV               ; sin / cos

; ---- atan -----------------------------------------------------------------------

FPATAN  lda #0
        sta FSGN
        sta FFLG
        lda WM1
        bpl FPAT1
        inc FSGN                ; remember the sign, work on |x|
        jsr WFCOMPL
FPAT1   _FMOVE FA, WX1
        _FSUBC ONE
        lda WM1
        bmi FPAT2               ; |x| < 1
        _FMOVE WX1, FA          ; |x| >= 1: x = 1/x, remember (bit 0)
        _FMOVE WX2, ONE
        jsr WFDIV
        lda #1
        sta FFLG
        jmp FPAT3
FPAT2   _FMOVE WX1, FA
FPAT3   _FMOVE FA, WX1
        _FSUBC T15
        lda WM1
        bpl FPAT3A              ; x >= tan(15 degrees)
        jmp FPAT4               ; x < tan(15 degrees)
FPAT3A
        _FMOVE WX1, FA          ; z = (sqrt3 x - 1) / (sqrt3 + x), remember (bit 1)
        _FMOVE WX2, SQRT3
        jsr WFMUL
        _FSUBC ONE
        _FMOVE FB, WX1
        _FMOVE WX1, FA
        _FMOVE WX2, SQRT3
        jsr WFADD
        _FMOVE WX2, FB
        jsr WFDIV
        lda FFLG
        ora #2
        sta FFLG
        jmp FPAT5
FPAT4   _FMOVE WX1, FA
FPAT5   _FMOVE FY, WX1
        _FMOVE WX2, WX1
        jsr WFMUL               ; u = z*z
        _FMOVE FU, WX1
        _FSETPOLY ATANTAB, 6
        jsr FPOLY
        _FMOVE WX2, FY
        jsr WFMUL               ; atan z = z * poly(z*z)
        lda FFLG
        and #2
        beq FPAT6
        _FMOVE WX2, PI6         ; + pi/6
        jsr WFADD
FPAT6   lda FFLG
        and #1
        beq FPAT7
        _FMOVE WX2, PI2         ; pi/2 - r
        jsr WFSUB
FPAT7   lda FSGN
        beq FPAT8
        jsr WFCOMPL
FPAT8   rts

; ---- logarithms (the Woz LOG and LOG10 take and return X1) -----------------------

FPLOG2  jsr WLOG
        _FMOVE WX2, INVLN2
        jmp WFMUL

; ---- stack versions of the functions the compiler calls ---------------------------

FSIN    jsr FPOP1
        jsr FPSIN
        jmp FPUSH1

FCOS    jsr FPOP1
        jsr FPCOS
        jmp FPUSH1

FTAN    jsr FPOP1
        jsr FPTAN
        jmp FPUSH1

FATAN   jsr FPOP1
        jsr FPATAN
        jmp FPUSH1

FLN     jsr FPOP1
        jsr WLOG
        jmp FPUSH1

FLOG2   jsr FPOP1
        jsr FPLOG2
        jmp FPUSH1

FLOG10  jsr FPOP1
        jsr WLOG10
        jmp FPUSH1

; ---- constants -------------------------------------------------------------------

INV2PI    .byte $7D,$51,$7C,$C2   ; 1/(2 pi)
HALF      .byte $7F,$40,$00,$00   ; 0.5
QUART     .byte $7E,$40,$00,$00   ; 0.25
ONE       .byte $80,$40,$00,$00   ; 1.0
FOUR      .byte $82,$40,$00,$00   ; 4.0
SQRT3     .byte $80,$6E,$D9,$EC   ; sqrt(3)
T15       .byte $7E,$44,$98,$51   ; tan(15 degrees)
PI6       .byte $7F,$43,$05,$49   ; pi/6
PI2       .byte $80,$64,$87,$ED   ; pi/2
INVLN2    .byte $80,$5C,$55,$1E   ; 1/ln(2)
; sin(pi/2 y) = y * (a0 + a1 u + ... + a5 u^5), u = y*y, a_k = (-1)^k (pi/2)^(2k+1)/(2k+1)!   (a5 first)
SINTAB
          .byte $6D,$87,$3E,$2C   ; a5 = -3.59884324e-06
          .byte $73,$54,$1E,$0D   ; a4 = 0.000160441185
          .byte $78,$B3,$4B,$4D   ; a3 = -0.00468175414
          .byte $7C,$51,$9A,$F2   ; a2 = 0.0796926262
          .byte $7F,$AD,$51,$0C   ; a1 = -0.645964098
          .byte $80,$64,$87,$ED   ; a0 = 1.57079633
; atan z = z * (b0 + b1 u + ... + b5 u^5), u = z*z, b_k = (-1)^k/(2k+1)   (b5 first)
ATANTAB
          .byte $7C,$A2,$E8,$BA   ; b5 = -0.0909090909
          .byte $7C,$71,$C7,$1C   ; b4 = 0.111111111
          .byte $7D,$B6,$DB,$6E   ; b3 = -0.142857143
          .byte $7D,$66,$66,$66   ; b2 = 0.2
          .byte $7E,$AA,$AA,$AB   ; b1 = -0.333333333
          .byte $80,$40,$00,$00   ; b0 = 1
