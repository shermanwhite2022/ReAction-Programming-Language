; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; misc.inc - memory, block and random number routines (6502asm syntax)
;
; The Action! library's Peek, Poke, PeekC, PokeC, Zero, SetBlock, MoveBlock and Rand, for MISC.ACT, which
; declares them with   PROC name=@MS_xxx(params).  The arguments are on the run time stack, pushed in order
; (so the last one is popped first), a FUNC leaves its value there.  Nothing here depends on the machine.

; ---- Peek, Poke -----------------------------------------------------------------------

MS_PEEK jsr POPA                ; BYTE FUNC Peek(CARD addr)
        ldy #0
        lda (TMPA),y
        sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

MS_PEEKC jsr POPA               ; CARD FUNC PeekC(CARD addr): low byte first
        ldy #0
        lda (TMPA),y
        tax
        iny
        lda (TMPA),y
        sta TMPA+1
        stx TMPA
        jmp PUSHA

MS_POKE jsr POPA                ; PROC Poke(CARD addr, BYTE value)
        lda TMPA
        pha
        jsr POPA
        pla
        ldy #0
        sta (TMPA),y
        rts

MS_POKEC jsr POPA               ; PROC PokeC(CARD addr, CARD value)
        lda TMPA
        pha
        lda TMPA+1
        pha
        jsr POPA
        pla
        ldy #1
        sta (TMPA),y
        pla
        dey
        sta (TMPA),y
        rts

; ---- Zero, SetBlock -----------------------------------------------------------------

MSPOPN  jsr POPA                ; TMPC = a popped CARD
        lda TMPA
        sta TMPC
        lda TMPA+1
        sta TMPC+1
        rts

MSPOPD  jsr POPA                ; TMPB = a popped CARD
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        rts

MS_ZERO jsr MSPOPN              ; PROC Zero(BYTE ARRAY addr, CARD size)
        jsr MSPOPD
        lda #0
        jmp MSFILL

MS_SETBLK jsr POPA              ; PROC SetBlock(BYTE ARRAY addr, CARD size, BYTE value)
        lda TMPA
        pha
        jsr MSPOPN
        jsr MSPOPD
        pla
; fill TMPC bytes from (TMPB) with A
MSFILL  tax
        lda TMPC+1
        beq MSF2
        ldy #0
MSF1    txa                     ; whole pages
        sta (TMPB),y
        iny
        bne MSF1
        inc TMPB+1
        dec TMPC+1
        bne MSF1
MSF2    lda TMPC                ; the rest
        beq MSF9
        tay
MSF3    dey
        txa
        sta (TMPB),y
        cpy #0
        bne MSF3
MSF9    rts

; ---- MoveBlock: MEMCPY (rtl.inc) copies the right way round when the blocks overlap -------------

MS_MOVEBLK jsr MSPOPN           ; PROC MoveBlock(BYTE ARRAY dest, BYTE ARRAY source, CARD size)
        jsr POPA
        lda TMPA
        sta MSA
        lda TMPA+1
        sta MSA+1
        jsr MSPOPD
        lda MSA
        sta TMPA
        lda MSA+1
        sta TMPA+1
        jmp MEMCPY

; ---- Blit: copy a block of rows, as a game moves a picture over a screen ----------------------
; The source rows follow each other, width bytes apart; the destination rows are stride bytes apart.

MSBST   .bzero 2
MSBH    .bzero 1
MSBW    .bzero 1

MS_BLIT jsr POPA                ; PROC Blit(BYTE ARRAY dest, BYTE ARRAY source, BYTE width, BYTE height, CARD stride)
        lda TMPA
        sta MSBST
        lda TMPA+1
        sta MSBST+1
        jsr POPA
        lda TMPA
        sta MSBH
        jsr POPA
        lda TMPA
        sta MSBW
        jsr POPA                ; the source
        lda TMPA
        sta MSA
        lda TMPA+1
        sta MSA+1
        jsr MSPOPD              ; the destination, in TMPB
        lda MSA
        sta TMPA
        lda MSA+1
        sta TMPA+1
        lda MSBW
        beq MSBL9
        lda MSBH
        beq MSBL9
MSBL1   ldy #0
MSBL2   lda (TMPA),y
        sta (TMPB),y
        iny
        cpy MSBW
        bne MSBL2
        clc
        lda TMPA
        adc MSBW
        sta TMPA
        bcc MSBL3
        inc TMPA+1
MSBL3   clc
        lda TMPB
        adc MSBST
        sta TMPB
        lda TMPB+1
        adc MSBST+1
        sta TMPB+1
        dec MSBH
        bne MSBL1
MSBL9   rts

; ---- BlitXor --------------------------------------------------------------------------------
; PROC BlitXor(BYTE ARRAY dest, BYTE ARRAY source, BYTE width, BYTE height, CARD stride): like Blit, but each byte
; of the source is exclusive-or'd into the destination.  Doing it twice with the same arguments puts the
; destination back as it was, which is how a picture is moved without keeping what is behind it.

MSXST   .bzero 2
MSXH    .bzero 1
MSXW    .bzero 1
MSXA    .bzero 2
MS_BLITX jsr POPA               ; the stride
        lda TMPA
        sta MSXST
        lda TMPA+1
        sta MSXST+1
        jsr POPA
        lda TMPA
        sta MSXH
        jsr POPA
        lda TMPA
        sta MSXW
        jsr POPA                ; the source
        lda TMPA
        sta MSXA
        lda TMPA+1
        sta MSXA+1
        jsr MSPOPD              ; the destination, in TMPB
        lda MSXA
        sta TMPA
        lda MSXA+1
        sta TMPA+1
        lda MSXW
        beq MSXL9
        lda MSXH
        beq MSXL9
MSXL1   ldy #0
MSXL2   lda (TMPA),y
        eor (TMPB),y
        sta (TMPB),y
        iny
        cpy MSXW
        bne MSXL2
        clc
        lda TMPA
        adc MSXW
        sta TMPA
        bcc MSXL3
        inc TMPA+1
MSXL3   clc
        lda TMPB
        adc MSXST
        sta TMPB
        lda TMPB+1
        adc MSXST+1
        sta TMPB+1
        dec MSXH
        bne MSXL1
MSXL9   rts

; ---- Rand ---------------------------------------------------------------------------------
; A 16 bit xorshift generator (shifts 7, 9, 8: it visits all 65535 non-zero states), the high byte is the number.
; Every program starts from the same state: RandSeed(n) starts it somewhere else.

RNDSTEP lda RNDHI               ; x ^= x << 7
        lsr a
        lda RNDLO
        ror a
        tay
        lda #0
        ror a
        eor RNDLO
        sta RNDLO
        tya
        eor RNDHI
        sta RNDHI
        lda RNDHI               ; x ^= x >> 9
        lsr a
        eor RNDLO
        sta RNDLO
        eor RNDHI               ; x ^= x << 8
        sta RNDHI
        rts

MS_RAND jsr POPA                ; BYTE FUNC Rand(BYTE range): 0 .. range-1, or 0 .. 255 for a range of 0
        lda TMPA
        sta MSA
        jsr RNDSTEP
        ldx MSA
        beq MSR9
        sta MSA+1               ; (random * range) / 256
        lda #0
        ldx #8
        lsr MSA+1
MSR1    bcc MSR2
        clc
        adc MSA
MSR2    ror a
        ror MSA+1
        dex
        bne MSR1
MSR9    sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

MS_RSEED jsr POPA               ; PROC RandSeed(CARD seed)
        lda TMPA
        ora TMPA+1
        bne MSS1
        lda #1                  ; zero would stay zero
        sta TMPA
MSS1    lda TMPA
        sta RNDLO
        lda TMPA+1
        sta RNDHI
        rts

MSA     .bzero 2
RNDLO   .byte $e1
RNDHI   .byte $ac

; ---- the controllers: Paddle, PTrig, Stick, STrig --------------------------------------------
;
; The Atari numbers: Stick is 0..15 (bit 0 up, 1 down, 2 left, 3 right, a bit is 0 when the
; stick is pushed that way: 15 is centered), STrig and PTrig are 0 when the button is pressed,
; Paddle is the paddle's reading.  The Atari has the operating system's copies of the hardware
; (PADDL0.. at $270, STICK0.. at $278, PTRIG0.. at $27c, STRIG0.. at $284).  The C64 reads the
; CIA: Stick(0) is control port 2, Stick(1) is port 1 (which the keyboard shares), Paddle(0,1)
; and PTrig(0,1) are port 1, Paddle(2,3) and PTrig(2,3) are port 2.  The Apple II reads its paddles
; with the monitor's PREAD and the buttons at $c061..$c063: Stick(n) is paddles 2n and 2n+1 (a
; joystick) turned into the four directions, PTrig(n) is button n (0 to 2), STrig(n) button n (0, 1).
; A number the machine has no such control for sets MSBAD, and the ACT routines call Error(132).

MSBAD   .bzero 1
MSTMP   .bzero 1
MSX     .bzero 1
MSY     .bzero 1
MSSAV   .bzero 1
MSVAL   .bzero 1

MS_BAD  lda MSBAD               ; BYTE FUNC MSBad(): 1 if the last controller call had a bad number
MSRET   sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

MSBADR  ldx #1                  ; A = the value to give back
        stx MSBAD
        jmp MSRET

        .if TARGET == TARGET_C64

MSSEL   .byte $40,$80
MSBIT   .byte 4,8

MS_PADDLE jsr POPA              ; BYTE FUNC MSPaddle(BYTE n)
        lda #0
        sta MSBAD
        lda TMPA
        cmp #4
        bcc MSPD1
        lda #0
        jmp MSBADR
MSPD1   sta MSTMP
        lsr a
        tax
        php
        sei
        lda $dc00
        sta MSSAV
        and #$3f
        ora MSSEL,x             ; the paddles of port 1 or port 2
        sta $dc00
        ldx #0
MSPD2   dex                     ; the lines need about 500 microseconds
        bne MSPD2
MSPD3   dex
        bne MSPD3
        lda MSTMP
        and #1
        tax
        lda $d419,x             ; POTX or POTY
        sta MSVAL
        lda MSSAV
        sta $dc00
        plp
        lda MSVAL
        jmp MSRET

MS_PTRIG jsr POPA               ; BYTE FUNC MSPTrig(BYTE n): the fire button of a paddle
        lda #0
        sta MSBAD
        lda TMPA
        cmp #4
        bcc MSPT1
        lda #1
        jmp MSBADR
MSPT1   and #1
        tax
        lda TMPA
        cmp #2
        bcs MSPT2
        lda $dc01
        jmp MSPT3
MSPT2   lda $dc00
MSPT3   and MSBIT,x             ; left for the first paddle of a port, right for the second
        beq MSPT4
        lda #1
        jmp MSRET
MSPT4   lda #0
        jmp MSRET

MS_STICK jsr POPA               ; BYTE FUNC MSStick(BYTE n)
        lda #0
        sta MSBAD
        lda TMPA
        cmp #2
        bcc MSST1
        lda #15
        jmp MSBADR
MSST1   tax
        lda $dc00,x
        and #15
        jmp MSRET

MS_STRIG jsr POPA               ; BYTE FUNC MSSTrig(BYTE n)
        lda #0
        sta MSBAD
        lda TMPA
        cmp #2
        bcc MSSG1
        lda #1
        jmp MSBADR
MSSG1   tax
        lda $dc00,x
        and #16
        beq MSSG2
        lda #1
        jmp MSRET
MSSG2   lda #0
        jmp MSRET


; ---- time and the keyboard (MISC.ACT: Wait, Ticks, KeyHit) ----
; C64: the KERNAL's jiffy clock ($A0-$A2), one tick for each interrupt (a screen frame).

MS_WAIT jsr POPA                ; PROC MSWait(CARD frames)
MSW1    lda TMPA
        ora TMPA+1
        beq MSW9
        lda $a2
MSW2    cmp $a2
        beq MSW2
        lda TMPA
        bne MSW3
        dec TMPA+1
MSW3    dec TMPA
        jmp MSW1
MSW9    rts

MS_TICKS lda #0                 ; CARD FUNC MSTicks(): the low 16 bits of the jiffy clock
        sta MSBAD
        lda $a2
        sta TMPA
        lda $a1
        sta TMPA+1
        jmp PUSHA

MS_KEYHIT lda #0                ; BYTE FUNC KeyHit(): the key pressed, or 0
        sta MSBAD
        jsr $ffcc               ; CLRCHN: the keyboard
        jsr $ffe4               ; GETIN: 0 if no key
        jmp MSRET

        .elif TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS

        .const PREAD $fb1e

MS_PADDLE jsr POPA              ; BYTE FUNC MSPaddle(BYTE n)
        lda #0
        sta MSBAD
        lda TMPA
        cmp #4
        bcc MSPD1
        lda #0
        jmp MSBADR
MSPD1   tax
        jsr PREAD
        tya
        jmp MSRET

MS_PTRIG jsr POPA               ; BYTE FUNC MSPTrig(BYTE n): the buttons 0 to 2
        lda #0
        sta MSBAD
        lda TMPA
        cmp #3
        bcc MSPT1
        lda #1
        jmp MSBADR
MSPT1   tax
        lda $c061,x
        bmi MSPT2
        lda #1
        jmp MSRET
MSPT2   lda #0
        jmp MSRET

MS_STRIG jsr POPA               ; BYTE FUNC MSSTrig(BYTE n): the buttons 0 and 1
        lda #0
        sta MSBAD
        lda TMPA
        cmp #2
        bcc MSSG1
        lda #1
        jmp MSBADR
MSSG1   tax
        lda $c061,x
        bmi MSSG2
        lda #1
        jmp MSRET
MSSG2   lda #0
        jmp MSRET

MS_STICK jsr POPA               ; BYTE FUNC MSStick(BYTE n): two paddles as a joystick
        lda #0
        sta MSBAD
        lda TMPA
        cmp #2
        bcc MSST1
        lda #15
        jmp MSBADR
MSST1   asl a
        sta MSTMP
        tax
        jsr PREAD
        sty MSX                 ; across
        ldx MSTMP
        inx
        jsr PREAD
        sty MSY                 ; up and down
        lda #15
        ldx MSX
        cpx #64
        bcs MSST2
        and #$fb                ; left
MSST2   cpx #192
        bcc MSST3
        and #$f7                ; right
MSST3   ldx MSY
        cpx #64
        bcs MSST4
        and #$fe                ; up
MSST4   cpx #192
        bcc MSST5
        and #$fd                ; down
MSST5   jmp MSRET


; ---- time and the keyboard (MISC.ACT: Wait, Ticks, KeyHit) ----
; Apple II: no clock.  Wait counts processor cycles (17045 for each frame, as Tone does) and Ticks is not supported.

MS_WAIT jsr POPA                ; PROC MSWait(CARD frames)
MSW1    lda TMPA
        ora TMPA+1
        beq MSW9
        ldx #13                 ; 13 passes of 1285 cycles + 61 * 5 + the loop = about 17045 cycles
MSW2    ldy #0
MSW3    dey
        bne MSW3
        dex
        bne MSW2
        ldx #61
MSW4    dex
        bne MSW4
        lda TMPA
        bne MSW5
        dec TMPA+1
MSW5    dec TMPA
        jmp MSW1
MSW9    rts

MS_TICKS lda #0                 ; CARD FUNC MSTicks(): no clock
        jmp MSBADR

MS_KEYHIT lda #0                ; BYTE FUNC KeyHit(): the key pressed, or 0
        sta MSBAD
        lda $c000
        bpl MSKH9
        and #$7f
        bit $c010               ; clear the strobe
        jmp MSRET
MSKH9   lda #0
        jmp MSRET

        .else

MS_PADDLE jsr POPA              ; BYTE FUNC MSPaddle(BYTE n): PADDL0..7
        lda #0
        sta MSBAD
        lda TMPA
        cmp #8
        bcc MSPD1
        lda #0
        jmp MSBADR
MSPD1   tax
        lda $0270,x
        jmp MSRET

MS_PTRIG jsr POPA               ; BYTE FUNC MSPTrig(BYTE n): PTRIG0..7
        lda #0
        sta MSBAD
        lda TMPA
        cmp #8
        bcc MSPT1
        lda #1
        jmp MSBADR
MSPT1   tax
        lda $027c,x
        and #1
        jmp MSRET

MS_STICK jsr POPA               ; BYTE FUNC MSStick(BYTE n): STICK0..3
        lda #0
        sta MSBAD
        lda TMPA
        cmp #4
        bcc MSST1
        lda #15
        jmp MSBADR
MSST1   tax
        lda $0278,x
        and #15
        jmp MSRET

MS_STRIG jsr POPA               ; BYTE FUNC MSSTrig(BYTE n): STRIG0..3
        lda #0
        sta MSBAD
        lda TMPA
        cmp #4
        bcc MSSG1
        lda #1
        jmp MSBADR
MSSG1   tax
        lda $0284,x
        and #1
        jmp MSRET


; ---- time and the keyboard (MISC.ACT: Wait, Ticks, KeyHit) ----
; Atari: RTCLOK ($12-$14) counts frames; KeyHit reads CH ($02FC, 255 when no key) and translates the key with its own table.

MS_WAIT jsr POPA                ; PROC MSWait(CARD frames)
MSW1    lda TMPA
        ora TMPA+1
        beq MSW9
        lda $14
MSW2    cmp $14
        beq MSW2
        lda TMPA
        bne MSW3
        dec TMPA+1
MSW3    dec TMPA
        jmp MSW1
MSW9    rts

MS_TICKS lda #0                 ; CARD FUNC MSTicks(): the low 16 bits of RTCLOK
        sta MSBAD
        lda $14
        sta TMPA
        lda $13
        sta TMPA+1
        jmp PUSHA

MS_KEYHIT lda #0                ; BYTE FUNC KeyHit(): the key pressed (ATASCII), or 0
        sta MSBAD
        ldy $02fc
        cpy #$ff
        beq MSKH9
        ldx #$ff
        stx $02fc
        tya
        and #$3f                ; the key
        tax
        lda MSKEYS,x            ; its character
        beq MSKH9
        cpy #$80                ; the control key: a letter becomes its control code
        bcc MSKH2
        cmp #$61
        bcc MSKH9
        cmp #$7b
        bcs MSKH9
        and #$1f
        jmp MSRET
MSKH2   cpy #$40                ; the shift key: a letter becomes upper case
        bcc MSKH3
        cmp #$61
        bcc MSKH3
        cmp #$7b
        bcs MSKH3
        and #$df
MSKH3   jmp MSRET
MSKH9   lda #0
        jmp MSRET

; the Atari's key codes 0 to 63 and their characters (ATASCII), without shift or control.  Not the operating
; system's own table (a minimal emulated OS may not have one); a shifted symbol comes out as the unshifted one.
MSKEYS  .byte $6c,$6a,$3b,0,0,$6b,$2b,$2a,$6f,0,$70,$75,$9b,$69,$2d,$3d
        .byte $76,0,$63,0,0,$62,$78,$7a,$34,0,$33,$36,$1b,$35,$32,$31
        .byte $2c,$20,$2e,$6e,0,$6d,$2f,0,$72,0,$65,$79,$7f,$74,$77,$71
        .byte $39,0,$30,$37,$7e,$38,$3c,$3e,$66,$68,$64,0,0,$67,$73,$61

        .endif
