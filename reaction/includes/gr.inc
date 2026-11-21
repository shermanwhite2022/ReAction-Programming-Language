; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; gr.inc - graphics and sound primitives (6502asm syntax)
;
; The library GRAPHICS.ACT (Graphics, Color, SetColor, Plot, Position, DrawTo, Locate, Fill, GrWidth ...)
; and SOUND.ACT (Sound, SndRst, Tone) call these routines, which they declare with
; PROC name=@GR_xxx(params) / @SN_xxx.  The arguments are on the run time stack, pushed in order (the
; last one is popped first), a FUNC leaves its value there.
;
; The machines differ a lot, so what each one has is described in GRAPHICS.ACT and SOUND.ACT.  A call the
; hardware can't do sets GRERRC to 146 ("not supported on this hardware"), a bad argument sets it to 132,
; and the ACT routine that called turns that into a call of Error.  On the Atari 800 an operating
; system error (141 for a position off the screen ...) goes there too.
;
; Not tested on the real machines: only on an emulated CPU with stand-ins for the hardware.

GRERRC  .bzero 1                ; 0, or the error to report
GRX     .bzero 2
GRY     .bzero 2
GRCOLV  .bzero 1                ; the current color
GRMD    .bzero 1                ; the mode given to Graphics
GRT     .bzero 1
GRT2    .bzero 1

GR_ERR  lda GRERRC              ; BYTE FUNC GrErr(): the error of the last call, which is forgotten
        ldx #0
        stx GRERRC
GRRET   sta TMPA                ; A = the value to give back
        lda #0
        sta TMPA+1
        jmp PUSHA

GRPOPXY jsr POPA                ; CARD x, CARD y popped into GRX, GRY
        lda TMPA
        sta GRY
        lda TMPA+1
        sta GRY+1
        jsr POPA
        lda TMPA
        sta GRX
        lda TMPA+1
        sta GRX+1
        rts

GRUNS   lda #146                ; not supported on this hardware
        sta GRERRC
        rts

GRPUSH0 lda #0                  ; a FUNC that has nothing to give back
        sta TMPA
        sta TMPA+1
        jmp PUSHA

GR_COLOR jsr POPA               ; PROC GrColor(BYTE c)
        lda TMPA
        sta GRCOLV
        rts

        .if TARGET == TARGET_C64

; ---- C64: the VIC-II and the SID, which the program drives itself ----
;
; Graphics modes: 0 the text screen, 8 the 320 by 200 bitmap (2 colors per 8 by 8 cell), 15 the 160 by 200
; multicolor bitmap (4 colors, 3 of them per 4 by 8 cell).  Any other mode is "not supported".  The
; screen (bank 3 of the VIC, $C000 to $FFFF, which programs don't use) is at $C400 and the bitmap at $E000
; under the KERNAL ROM: it is written freely, and read with the ROM switched out for a moment.  Color registers: the 8 mode has 1 (the points), 2
; (the background of all cells) and 4 (the border); the 15 mode has 0, 1 and 2 (colors 1 to 3 of all
; cells) and 4 (the background and the border); text has 1 (the text), 2 (the background), 4 (the border).
; The Atari's hue and luminance become the nearest of the C64's 16 colors.

        .const GRBM    $e000
        .const GRSCR   $c400
        .const GRCPU   $01
        .const GRCRAM  $d800
        .const GRVIC11 $d011
        .const GRVIC16 $d016
        .const GRVIC18 $d018
        .const GRBORD  $d020
        .const GRBKGD  $d021
        .const GRCIA2  $dd00
        .const GRCLRSC $e544
        .const GRSID   $d400
        .const GRRASTR $d012

GRKIND  .bzero 1                ; 0 text, 1 the 320 by 200 bitmap, 2 the 160 by 200 one
GRREGS  .bzero 5                ; the C64 color of each color register
GRMSK   .bzero 1
GRVAL   .bzero 1

GRROWS  .word $e000,$e140,$e280,$e3c0,$e500,$e640,$e780,$e8c0,$ea00,$eb40,$ec80,$edc0,$ef00,$f040,$f180,$f2c0,$f400,$f540,$f680,$f7c0,$f900,$fa40,$fb80,$fcc0,$fe00
GRBIT   .byte $80,$40,$20,$10,$08,$04,$02,$01
GRMBIT  .byte $c0,$30,$0c,$03
GRMVAL  .byte $00,$55,$aa,$ff
GRMSH   .byte 6,4,2,0
GRWC    .word 40,320,160
GRHC    .byte 25,200,200
GRCC    .byte 2,2,4
GRHUE   .byte 0,8,8,2,10,4,4,6,6,14,3,3,5,13,7,8

GR_NATIVE lda #0                ; BYTE FUNC GrNative(): 0, lines and fills are drawn by GRAPHICS.ACT
        jmp GRRET

GRFILLP stx GRT                 ; A = the byte, TMPA = the address, X = the number of pages
        ldy #0
GRFP1   sta (TMPA),y
        iny
        bne GRFP1
        inc TMPA+1
        dec GRT
        bne GRFP1
        rts

GRPAINT lda GRKIND              ; the colors of the registers to all cells
        beq GRPA9
        cmp #2
        beq GRPA2
        lda GRREGS+1            ; 8: points in the high nibble, background in the low
        asl a
        asl a
        asl a
        asl a
        ora GRREGS+2
        ldx #4
        jsr GRPSCR
        lda GRREGS+4
        sta GRBORD
        lda #0
        sta GRBKGD
GRPA9   rts
GRPA2   lda GRREGS              ; 15: colors 1 and 2 on the screen, color 3 in the color RAM
        asl a
        asl a
        asl a
        asl a
        ora GRREGS+1
        ldx #4
        jsr GRPSCR
        lda #<GRCRAM
        sta TMPA
        lda #>GRCRAM
        sta TMPA+1
        lda GRREGS+2
        ldx #4
        jsr GRFILLP
        lda GRREGS+4
        sta GRBKGD
        sta GRBORD
        rts
GRPSCR  pha
        lda #<GRSCR
        sta TMPA
        lda #>GRSCR
        sta TMPA+1
        pla
        jmp GRFILLP

GR_MODE jsr POPA                ; PROC GrMode(BYTE mode)
        lda TMPA
        sta GRMD
        and #15
        beq GRM0
        cmp #8
        beq GRM8
        cmp #15
        beq GRMF
        jmp GRUNS
GRM0    lda #0
        sta GRKIND
        lda GRCIA2
        ora #3
        sta GRCIA2
        lda #$15
        sta GRVIC18
        lda #$1b
        sta GRVIC11
        lda #$c8
        sta GRVIC16
        lda GRMD
        and #$20
        bne GRM01
        jsr GRCLRSC
GRM01   lda #1
        sta GRCOLV
        rts
GRM8    lda #1
        sta GRKIND
        sta GRREGS+1
        lda #0
        sta GRREGS+2
        sta GRREGS+4
        jmp GRMSET
GRMF    lda #2
        sta GRKIND
        lda #8
        sta GRREGS
        lda #13
        sta GRREGS+1
        lda #6
        sta GRREGS+2
        lda #0
        sta GRREGS+4
GRMSET  lda GRCIA2              ; VIC bank 3, the bitmap at +$2000 and the screen at +$0400
        and #$fc
        sta GRCIA2
        lda #$18
        sta GRVIC18
        lda #$3b
        sta GRVIC11
        lda #$c8
        ldx GRKIND
        cpx #2
        bne GRMS1
        lda #$d8
GRMS1   sta GRVIC16
        lda GRMD
        and #$20
        bne GRMS3
        lda #<GRBM              ; clear the bitmap: 31 pages and 64 bytes
        sta TMPA
        lda #>GRBM
        sta TMPA+1
        lda #0
        ldx #31
        jsr GRFILLP
        ldy #63
        lda #0
GRMS2   sta (TMPA),y
        dey
        bpl GRMS2
GRMS3   jsr GRPAINT
        lda #1
        sta GRCOLV
        rts

; the C64 color for an Atari hue (GRT2) and luminance (GRT) in A
GRCMAP  lda GRT
        cmp #2
        bcs GRCM1
        lda #0                  ; very dark: black
        rts
GRCM1   ldy GRT2
        bne GRCM3
        cmp #6                  ; gray: dark gray, gray, light gray, white
        bcs GRCM2
        lda #11
        rts
GRCM2   cmp #10
        bcs GRCM4
        lda #12
        rts
GRCM4   cmp #14
        bcs GRCM5
        lda #15
        rts
GRCM5   lda #1
        rts
GRCM3   lda GRHUE,y
        rts

GR_SETCOL jsr POPA              ; PROC GrSetCol(BYTE reg, BYTE hue, BYTE lum)
        lda TMPA
        and #15
        sta GRT
        jsr POPA
        lda TMPA
        and #15
        sta GRT2
        jsr POPA
        ldx TMPA
        cpx #5
        bcc GRSC1
        lda #132
        sta GRERRC
        rts
GRSC1   jsr GRCMAP
        sta GRREGS,x
        lda GRKIND
        bne GRSC3
        cpx #1                  ; text: the text color, the background, the border
        bne GRSC2
        lda #<GRCRAM
        sta TMPA
        lda #>GRCRAM
        sta TMPA+1
        lda GRREGS+1
        ldx #4
        jmp GRFILLP
GRSC2   cpx #2
        bne GRSC4
        lda GRREGS+2
        sta GRBKGD
        rts
GRSC4   cpx #4
        bne GRSC5
        lda GRREGS+4
        sta GRBORD
GRSC5   rts
GRSC3   jmp GRPAINT

; is the point in GRX, GRY on the screen?  Carry set and GRERRC if not
GRRANGE lda GRKIND
        bne GRRG1
        lda #133                ; no bitmap screen: Graphics 8 or 15 first
        sta GRERRC
        sec
        rts
GRRG1   lda GRY+1
        bne GRRG9
        lda GRY
        cmp #200
        bcs GRRG9
        lda GRKIND
        cmp #2
        beq GRRG2
        lda GRX+1               ; 320 wide
        beq GRRG0
        cmp #1
        bne GRRG9
        lda GRX
        cmp #64
        bcs GRRG9
        clc
        rts
GRRG2   lda GRX+1               ; 160 wide
        bne GRRG9
        lda GRX
        cmp #160
        bcs GRRG9
GRRG0   clc
        rts
GRRG9   lda #141
        sta GRERRC
        sec
        rts

; the address of the byte with the point (TMPA), its mask and the pattern of the color
GRADDR  lda GRY
        lsr a
        lsr a
        lsr a
        asl a
        tax
        lda GRROWS,x
        sta TMPA
        lda GRROWS+1,x
        sta TMPA+1
        lda GRKIND
        cmp #2
        beq GRAD2
        lda GRX                 ; 320: the byte is x / 8, 8 bytes per cell
        and #$f8
        clc
        adc TMPA
        sta TMPA
        lda GRX+1
        adc TMPA+1
        sta TMPA+1
        lda GRX
        and #7
        tax
        lda GRBIT,x
        sta GRMSK
        lda GRCOLV
        and #1
        beq GRAD1
        lda #$ff
GRAD1   sta GRVAL
        jmp GRAD3
GRAD2   lda GRX                 ; 160: the byte is x / 4
        lsr a
        lsr a
        sta GRT
        lda #0
        sta GRT2
        asl GRT
        rol GRT2
        asl GRT
        rol GRT2
        asl GRT
        rol GRT2
        lda GRT
        clc
        adc TMPA
        sta TMPA
        lda GRT2
        adc TMPA+1
        sta TMPA+1
        lda GRX
        and #3
        tax
        lda GRMBIT,x
        sta GRMSK
        lda GRCOLV
        and #3
        tay
        lda GRMVAL,y
        sta GRVAL
GRAD3   lda GRY                 ; the line in the cell
        and #7
        clc
        adc TMPA
        sta TMPA
        bcc GRAD4
        inc TMPA+1
GRAD4   rts

GR_PIX  jsr GRPOPXY             ; PROC GrPix(CARD x, CARD y)
        jsr GRRANGE
        bcs GRPX9
        jsr GRADDR
        php
        sei
        lda GRCPU
        pha
        lda #$35                ; the KERNAL ROM out: the RAM under it can be read
        sta GRCPU
        ldy #0
        lda GRMSK
        eor #$ff
        and (TMPA),y
        sta GRT
        lda GRVAL
        and GRMSK
        ora GRT
        sta (TMPA),y
        pla
        sta GRCPU
        plp
GRPX9   rts

GR_LOC  jsr GRPOPXY             ; BYTE FUNC GrLoc(CARD x, CARD y)
        jsr GRRANGE
        bcc GRLC1
        jmp GRPUSH0
GRLC1   jsr GRADDR
        php
        sei
        lda GRCPU
        pha
        lda #$35
        sta GRCPU
        ldy #0
        lda (TMPA),y
        sta GRT
        pla
        sta GRCPU
        plp
        lda GRT
        and GRMSK
        sta GRT
        lda GRKIND
        cmp #2
        beq GRLC2
        lda GRT                 ; 0 or 1
        beq GRLC4
        lda #1
GRLC4   jmp GRRET
GRLC2   lda GRX
        and #3
        tax
        lda GRMSH,x
        tax
        lda GRT
GRLC3   cpx #0
        beq GRLC4
        lsr a
        dex
        jmp GRLC3

GR_POS  jsr GRPOPXY             ; the position is kept by GRAPHICS.ACT
        rts

GR_OSLINE jsr GRPOPXY
        jmp GRUNS
GR_OSFILL jsr GRPOPXY
        jmp GRUNS

GR_WIDTH lda GRKIND             ; CARD FUNC GrWidth()
        asl a
        tax
        lda GRWC,x
        sta TMPA
        lda GRWC+1,x
        sta TMPA+1
        jmp PUSHA
GR_HEIGHT ldx GRKIND
        lda GRHC,x
        jmp GRRET
GR_COLORS ldx GRKIND
        lda GRCC,x
        jmp GRRET

; ---- SID: 3 voices ----
;
; Sound(voice 0..2, pitch, dist, vol): pitch is the Atari's divisor (frequency 31960 / (pitch + 1), at most
; about 4000, for an NTSC machine), dist 0..14 picks the wave (noise for 0 and 8, pulse for 2, 4, 10, 12,
; sawtooth for 6, triangle for 14) and vol 0..15 is the sustain level of the voice.  A fourth voice is
; "not supported".

SIDFRQ
        .word 65535,65535,65535,65535,65535,65535,65535,65535
        .word 58254,52428,47662,43690,40330,37449,34952,32768
        .word 30840,29127,27594,26214,24966,23831,22795,21845
        .word 20971,20165,19418,18724,18079,17476,16912,16384
        .word 15887,15420,14980,14563,14170,13797,13443,13107
        .word 12787,12483,12193,11916,11651,11397,11155,10923
        .word 10700,10486,10280,10082,9892,9709,9532,9362
        .word 9198,9039,8886,8738,8595,8456,8322,8192
        .word 8066,7944,7825,7710,7598,7490,7384,7282
        .word 7182,7085,6990,6898,6809,6722,6637,6554
        .word 6473,6394,6317,6241,6168,6096,6026,5958
        .word 5891,5825,5761,5699,5637,5577,5519,5461
        .word 5405,5350,5296,5243,5191,5140,5090,5041
        .word 4993,4946,4900,4854,4810,4766,4723,4681
        .word 4640,4599,4559,4520,4481,4443,4406,4369
        .word 4333,4297,4262,4228,4194,4161,4128,4096
        .word 4064,4033,4002,3972,3942,3913,3884,3855
        .word 3827,3799,3772,3745,3718,3692,3666,3641
        .word 3616,3591,3567,3542,3519,3495,3472,3449
        .word 3427,3404,3382,3361,3339,3318,3297,3277
        .word 3256,3236,3216,3197,3177,3158,3139,3121
        .word 3102,3084,3066,3048,3031,3013,2996,2979
        .word 2962,2945,2929,2913,2897,2881,2865,2849
        .word 2834,2819,2804,2789,2774,2759,2745,2731
        .word 2717,2702,2689,2675,2661,2648,2635,2621
        .word 2608,2595,2583,2570,2557,2545,2533,2521
        .word 2509,2497,2485,2473,2461,2450,2439,2427
        .word 2416,2405,2394,2383,2372,2362,2351,2341
        .word 2330,2320,2310,2299,2289,2279,2270,2260
        .word 2250,2241,2231,2222,2212,2203,2194,2185
        .word 2175,2166,2158,2149,2140,2131,2123,2114
        .word 2106,2097,2089,2080,2072,2064,2056,2048
SIDWAV  .byte $81,$41,$41,$21,$81,$41,$41,$11
SIDOFS  .byte 0,7,14

SN_RESET ldx #24                ; PROC SndRst(): everything silent, the volume up
        lda #0
SNR1    sta GRSID,x
        dex
        bpl SNR1
        lda #15
        sta GRSID+24
        rts

SN_SOUND jsr POPA               ; PROC SnSound(BYTE voice, BYTE pitch, BYTE dist, BYTE vol)
        lda TMPA
        and #15
        sta GRT                 ; the volume
        jsr POPA
        lda TMPA
        and #14
        lsr a
        tax
        lda SIDWAV,x
        sta GRT2                ; the wave with the gate on
        jsr POPA
        lda TMPA
        sta GRMSK               ; the pitch
        jsr POPA
        lda TMPA
        cmp #3
        bcc SNS1
        beq SNSU
        lda #132
        sta GRERRC
        rts
SNSU    jmp GRUNS
SNS1    tax
        lda #15
        sta GRSID+24            ; the volume of the SID is shared: all the way up
        lda SIDOFS,x
        tax                     ; X = the offset of the voice in the SID
        lda GRMSK
        asl a
        tay
        lda SIDFRQ,y
        sta GRSID,x
        lda SIDFRQ+1,y
        sta GRSID+1,x
        lda #0
        sta GRSID+2,x
        lda #8
        sta GRSID+3,x           ; a pulse width of one half
        lda #$00
        sta GRSID+5,x           ; no attack or decay
        lda GRT
        asl a
        asl a
        asl a
        asl a
        sta GRSID+6,x           ; the sustain is the volume
        lda GRT2
        ldy GRT
        bne SNS2
        and #$fe                ; volume 0: the gate off
SNS2    sta GRSID+4,x
        rts

SN_TONE jsr POPA                ; PROC SnTone(BYTE pitch, BYTE frames)
        lda TMPA
        sta GRT2                ; frames
        jsr POPA
        lda #15
        sta GRSID+24
        lda TMPA
        asl a
        tay
        lda SIDFRQ,y
        sta GRSID
        lda SIDFRQ+1,y
        sta GRSID+1
        lda #0
        sta GRSID+2
        sta GRSID+5
        lda #8
        sta GRSID+3
        lda #$80
        sta GRSID+6
        lda #$41
        sta GRSID+4
SNT1    lda GRT2
        beq SNT4
SNT2    lda GRRASTR             ; a frame: from raster line 255 to the next time it gets there
        cmp #$ff
        bne SNT2
SNT3    lda GRRASTR
        cmp #$ff
        beq SNT3
        dec GRT2
        jmp SNT1
SNT4    lda #$40                ; the gate off
        sta GRSID+4
        rts

        .elif TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS

; ---- Apple II: the hi-res screen, and the speaker ----
;
; Graphics modes: 0 the text screen, 8 the 280 by 192 hi-res screen (page 1, at $2000; with the four text
; lines at the bottom unless 16 is added, so 160 lines are shown); any other mode is "not supported".  A
; hi-res point is a bit, 7 to a byte; the color of a lone point depends on the column and on bit 7 of
; its byte, and two points side by side are white, as the hardware has it.  Color 0 clears a point, color
; 1 sets it with bit 7 clear (green in the odd columns, violet in the even ones), and color 2 or more
; sets it with bit 7 set (orange and blue).  SetColor can't be done: the colors are fixed.
; The Apple has no sound hardware but its speaker, a click each time $C030 is touched: Sound is
; "not supported", and Tone(pitch, frames) is clicks, timed with the processor's cycles.

        .const GRTXTCLR $c050
        .const GRTXTSET $c051
        .const GRMIXCLR $c052
        .const GRMIXSET $c053
        .const GRPAGE1  $c054
        .const GRHIRES  $c057
        .const GRSPKR   $c030
        .const GRHOME   $fc58
        .const GRHGR    $2000

GRKIND  .bzero 1                ; 0 text, 1 hi-res
GRLIM   .bzero 1                ; the number of lines: 192, or 160 with the text window
GRMSK   .bzero 1

GRABIT  .byte 1,2,4,8,16,32,64
GRH40   .byte 0,40,80

GR_NATIVE lda #0                ; BYTE FUNC GrNative(): 0, lines and fills are drawn by GRAPHICS.ACT
        jmp GRRET

GR_MODE jsr POPA                ; PROC GrMode(BYTE mode)
        lda TMPA
        sta GRMD
        and #15
        beq GRM0
        cmp #8
        beq GRM8
        jmp GRUNS
GRM0    lda #0
        sta GRKIND
        lda GRTXTSET
        lda GRMIXCLR
        lda GRMD
        and #$20
        bne GRM01
        jsr GRHOME
GRM01   lda #1
        sta GRCOLV
        rts
GRM8    lda #1
        sta GRKIND
        lda GRMD
        and #16
        beq GRM80
        lda #192
        jmp GRM81
GRM80   lda #160
GRM81   sta GRLIM
        lda GRMD
        and #$20
        bne GRM83
        lda #<GRHGR             ; clear the 8K of page 1
        sta TMPA
        lda #>GRHGR
        sta TMPA+1
        ldx #32
        lda #0
        ldy #0
GRM82   sta (TMPA),y
        iny
        bne GRM82
        inc TMPA+1
        dex
        bne GRM82
GRM83   lda GRTXTCLR
        lda GRHIRES
        lda GRPAGE1
        lda GRMD
        and #16
        beq GRM84
        lda GRMIXCLR
        jmp GRM85
GRM84   lda GRMIXSET
GRM85   lda #1
        sta GRCOLV
        rts

GR_SETCOL jsr POPA              ; PROC GrSetCol(BYTE reg, BYTE hue, BYTE lum)
        jsr POPA
        jsr POPA
        jmp GRUNS

; is the point in GRX, GRY on the screen?  Carry set and GRERRC if not
GRRANGE lda GRKIND
        bne GRRG1
        lda #133                ; no hi-res screen: Graphics 8 first
        sta GRERRC
        sec
        rts
GRRG1   lda GRY+1
        bne GRRG9
        lda GRY
        cmp GRLIM
        bcs GRRG9
        lda GRX+1
        beq GRRG0
        cmp #1
        bne GRRG9
        lda GRX
        cmp #24                 ; 280 wide
        bcs GRRG9
GRRG0   clc
        rts
GRRG9   lda #141
        sta GRERRC
        sec
        rts

; the address of the byte with the point in GRX, GRY (TMPA) and its bit mask (GRMSK)
GRADDR  lda GRY                 ; $2000 + (y and 7) * $400 + ((y / 8) and 7) * $80 + (y / 64) * 40
        and #7
        asl a
        asl a
        ora #$20
        sta TMPA+1
        lda GRY
        lsr a
        lsr a
        lsr a
        and #7
        sta GRT
        lsr a
        clc
        adc TMPA+1
        sta TMPA+1
        lda GRT
        and #1
        beq GRAD1
        lda #$80
GRAD1   sta TMPA
        lda GRY
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        tax
        lda GRH40,x
        clc
        adc TMPA
        sta TMPA
        bcc GRAD2
        inc TMPA+1
GRAD2   lda GRX                 ; x / 7 is the byte, x mod 7 the bit
        sta GRT
        ldx #0
        lda GRX+1
        beq GRAD3
        lda GRT                 ; 256 is 36 * 7 + 4
        clc
        adc #4
        sta GRT
        ldx #36
GRAD3   lda GRT
GRAD4   cmp #7
        bcc GRAD5
        sbc #7
        inx
        jmp GRAD4
GRAD5   tay                     ; Y = the bit, X = the byte
        lda GRABIT,y
        sta GRMSK
        txa
        clc
        adc TMPA
        sta TMPA
        bcc GRAD6
        inc TMPA+1
GRAD6   rts

GR_PIX  jsr GRPOPXY             ; PROC GrPix(CARD x, CARD y)
        jsr GRRANGE
        bcs GRPX9
        jsr GRADDR
        ldy #0
        lda GRCOLV
        bne GRPX1
        lda GRMSK               ; color 0: the point off
        eor #$ff
        and (TMPA),y
        sta (TMPA),y
        rts
GRPX1   cmp #1
        bne GRPX2
        lda (TMPA),y            ; color 1: on, bit 7 clear
        ora GRMSK
        and #$7f
        sta (TMPA),y
        rts
GRPX2   lda (TMPA),y            ; the others: on, bit 7 set
        ora GRMSK
        ora #$80
        sta (TMPA),y
GRPX9   rts

GR_LOC  jsr GRPOPXY             ; BYTE FUNC GrLoc(CARD x, CARD y): 0 off, 1 on, 2 on with bit 7 set
        jsr GRRANGE
        bcc GRLC1
        jmp GRPUSH0
GRLC1   jsr GRADDR
        ldy #0
        lda (TMPA),y
        tax
        and GRMSK
        beq GRLC2
        txa
        bmi GRLC3
        lda #1
        jmp GRRET
GRLC3   lda #2
GRLC2   jmp GRRET

GR_POS  jsr GRPOPXY             ; the position is kept by GRAPHICS.ACT
        rts

GR_OSLINE jsr GRPOPXY
        jmp GRUNS
GR_OSFILL jsr GRPOPXY
        jmp GRUNS

GR_WIDTH lda GRKIND             ; CARD FUNC GrWidth()
        beq GRW0
        lda #<280
        sta TMPA
        lda #>280
        sta TMPA+1
        jmp PUSHA
GRW0    lda #40
        jmp GRRET
GR_HEIGHT lda GRKIND
        beq GRHT0
        lda GRLIM
        jmp GRRET
GRHT0   lda #24
        jmp GRRET
GR_COLORS lda GRKIND
        beq GRCL0
        lda #3
        jmp GRRET
GRCL0   lda #2
        jmp GRRET

; ---- the speaker ----

SNREM   .bzero 3
SNCST   .bzero 2

SN_RESET rts                    ; PROC SndRst(): the speaker is quiet already

SN_SOUND jsr POPA               ; PROC SnSound(BYTE voice, BYTE pitch, BYTE dist, BYTE vol): not supported
        jsr POPA
        jsr POPA
        jsr POPA
        jmp GRUNS

; Tone: a half wave takes 16 * (pitch + 1) cycles, as the pitch gives 31960 / (pitch + 1) Hz at 1.02 MHz (the
; Atari's), and the frames are 17045 cycles each; the time left is counted down in cycles.  The two highest
; pitches (0, 1) come out lower than they should, the loop being too long for them.
SN_TONE jsr POPA                ; PROC SnTone(BYTE pitch, BYTE frames)
        lda TMPA
        sta GRT2                ; frames
        jsr POPA
        lda TMPA
        sta GRT                 ; pitch
        lda GRT                 ; the delay loop of 16 cycles runs M = pitch - 1 times (at least once), since the
        cmp #2                  ; rest of a half wave takes 37 cycles, which makes 16 * (pitch + 1) in all
        bcs SNTA
        lda #2
SNTA    sec
        sbc #1
        sta GRT
        lda #0                  ; the cost of a half wave: 16 * M + 37
        sta SNCST+1
        lda GRT
        sta SNCST
        ldx #4
SNT01   asl SNCST
        rol SNCST+1
        dex
        bne SNT01
        lda SNCST
        clc
        adc #37
        sta SNCST
        bcc SNT02
        inc SNCST+1
SNT02   lda #0                  ; the time to fill: frames * 17045
        sta SNREM
        sta SNREM+1
        sta SNREM+2
        ldx GRT2
        beq SNT9
SNT03   txa
        beq SNT1
        lda SNREM
        clc
        adc #<17045
        sta SNREM
        lda SNREM+1
        adc #>17045
        sta SNREM+1
        bcc SNT04
        inc SNREM+2
SNT04   dex
        jmp SNT03
SNT1    lda GRSPKR              ; a half wave
        ldx GRT
SNT2    ldy #2
SNT3    dey
        bne SNT3
        dex
        bne SNT2
        sec
        lda SNREM
        sbc SNCST
        sta SNREM
        lda SNREM+1
        sbc SNCST+1
        sta SNREM+1
        lda SNREM+2
        sbc #0
        sta SNREM+2
        bcs SNT1
SNT9    rts

        .else

; ---- Atari 800 (and no target): the operating system's screen handler on IOCB 6, and POKEY ----
;
; Graphics opens "S:" on IOCB 6 as the OS does for GRAPHICS n (the mode is n and 15, bit 4 means no text
; window, bit 5 does not clear the screen).  Plot puts a character on it, DrawTo and Fill are XIO 17 and 18,
; Locate reads a character.  Each call sets the cursor (ROWCRS, COLCRS) and its old position (OLDROW,
; OLDCOL, where a line starts) first.

        .const GRCIOV  $e456
        .const GRICCOM $0342
        .const GRICBAL $0344
        .const GRICBAH $0345
        .const GRICBLL $0348
        .const GRICBLH $0349
        .const GRICAX1 $034a
        .const GRICAX2 $034b
        .const GRIOCB  $60
        .const GRROW   $54
        .const GRCOLC  $55
        .const GROLDR  $5a
        .const GROLDC  $5b
        .const GRATACHR $02fb
        .const GRFILDAT $02fd
        .const GRPALET $02c4
        .const GRRTCLK $14

GRSN    .byte "S:",$9b

GR_NATIVE lda #1                ; BYTE FUNC GrNative(): 1, the operating system draws lines and fills
        jmp GRRET

GRCHK   cpy #128                ; after a CIO call: remember an error
        bcc GRCHK9
        sty GRERRC
GRCHK9  rts

GR_MODE jsr POPA                ; PROC GrMode(BYTE mode)
        lda TMPA
        sta GRMD
        ldx #GRIOCB
        lda #12                 ; close what was there
        sta GRICCOM,x
        jsr GRCIOV
        ldx #GRIOCB
        lda #3                  ; open "S:"
        sta GRICCOM,x
        lda #<GRSN
        sta GRICBAL,x
        lda #>GRSN
        sta GRICBAH,x
        lda GRMD
        and #$30
        ora #$0c
        sta GRICAX1,x
        lda GRMD
        and #$0f
        sta GRICAX2,x
        lda #0
        sta GRICBLL,x
        sta GRICBLH,x
        jsr GRCIOV
        jsr GRCHK
        lda #1
        sta GRCOLV
        lda GRERRC
        bne GRMD9
        jsr GRFULL              ; a full screen mode: see below
GRMD9   rts

; ---- full screen (mode + 16) ------------------------------------------------------------------
; Bit 4 of the mode means "no text window", and the screen should have all the lines of the mode.  Some
; operating systems (the Atari 800 ROM in the emulator that this was tested with) gave the mode its text
; window anyway: the display list ended after 80 lines of mode 7 with a 4 line text window, though the
; memory for 96 lines was there, so the last lines of the screen were never shown.  After the OS opens the
; screen this looks at its display list.  If the lines of the mode (the ANTIC mode of the lines at the
; start) are fewer than the mode's full number, a new display list is built in GRDLB: the same blank lines
; and start address, the lines of the mode all the way down (with a new load of the address at a 4K
; boundary, as ANTIC needs it), and a jump back to the start.  The screen is cleared line by line unless
; bit 5 of the mode says not to.  A display list that is complete already is not touched.  Only the modes
; 1 to 11 and 15 are done.  The operating system still believes in its own size of the screen, so Plot and
; DrawTo below the OS's last line may report error 141; the direct writes into the screen memory work.

GRFX    .bzero 1                ; the table index of the mode
GRCNT   .bzero 1
GRIDX   .bzero 1
GRNEED  .bzero 1                ; the display list byte of a line with a load of the address
GRNOCL  .bzero 1
GRSTA   .bzero 2                ; the address of the line
GRANT   .byte 6,7,8,9,10,11,13,15,15,15,15,14   ; the ANTIC mode of the Atari modes 1 to 11, and 15
GRNLN   .byte 24,12,24,48,48,96,96,192,192,192,192,192
GRBPL   .byte 20,20,10,10,20,20,40,40,40,40,40,40       ; the bytes in a line
GRDLB   .bzero 511              ; room for a page aligned display list

GRFULL  lda GRMD
        and #16
        bne GRFLA
        rts                     ; with the text window: nothing to do
GRFLA   lda GRMD
        and #15
        bne GRFLB
        rts
GRFLB   cmp #12
        bcc GRFL1
        cmp #15
        bne GRFL9
        ldx #11
        jmp GRFL2
GRFL1   tax
        dex
GRFL2   stx GRFX
        lda GRANT,x
        ora #$40
        sta GRNEED
        lda $0230               ; SDLSTL: the display list the OS made
        sta TMPB
        lda $0231
        sta TMPB+1
        ldy #0
GRFL3   lda (TMPB),y            ; the blank lines
        cmp #$70
        bne GRFL4
        iny
        bne GRFL3
        rts
GRFL4   cmp GRNEED              ; the first line must be the mode's, with the address
        bne GRFL9
        iny
        lda (TMPB),y
        sta GRSTA
        iny
        lda (TMPB),y
        sta GRSTA+1
        iny
        lda #1
        sta GRCNT
GRFL5   lda (TMPB),y
        cmp #$41                ; the jump back: the end
        beq GRFL8
        cmp GRNEED
        bne GRFL6
        inc GRCNT               ; a line of the mode with a new address (the 4K boundary)
        iny
        iny
        iny
        bne GRFL5
        rts
GRFL6   ldx GRFX
        eor GRANT,x
        bne GRFL7
        inc GRCNT               ; a line of the mode
        iny
        bne GRFL5
        rts
GRFL7   lda (TMPB),y            ; some other line, the text window: skip it (3 bytes with an address)
        and #$40
        beq GRFL7A
        iny
        iny
GRFL7A  iny
        bne GRFL5
        rts
GRFL8   ldx GRFX
        lda GRCNT
        cmp GRNLN,x
        bcc GRBUILD             ; fewer lines than the mode has: build the whole list
GRFL9   rts

GRBUILD lda GRMD
        and #$20
        sta GRNOCL
        lda #<GRDLB             ; a page inside GRDLB (a display list may not cross 1K)
        beq GRB0
        lda #>GRDLB
        clc
        adc #1
        jmp GRB1
GRB0    lda #>GRDLB
GRB1    sta TMPB+1
        lda #0
        sta TMPB
        ldy #0
        lda #$70
        sta (TMPB),y
        iny
        sta (TMPB),y
        iny
        sta (TMPB),y
        iny
        lda GRNEED
        sta (TMPB),y
        iny
        lda GRSTA
        sta (TMPB),y
        iny
        lda GRSTA+1
        sta (TMPB),y
        iny
        sty GRIDX
        jsr GRCLRLN
        lda #1
        sta GRCNT
GRB2    ldx GRFX
        lda GRCNT
        cmp GRNLN,x
        bcs GRB6
        inc GRCNT
        clc                     ; the next line starts a line further on
        lda GRSTA
        adc GRBPL,x
        sta GRSTA
        bcc GRB3
        inc GRSTA+1
GRB3    lda GRSTA+1             ; does the line cross a 4K boundary?
        and #$0f
        cmp #$0f
        bne GRB4
        lda GRSTA
        clc
        adc GRBPL,x
        bcc GRB4                ; no: it ends before the boundary
        lda GRSTA+1             ; yes: it starts at the boundary, with a new load of the address
        and #$f0
        clc
        adc #$10
        sta GRSTA+1
        lda #0
        sta GRSTA
        ldy GRIDX
        lda GRNEED
        sta (TMPB),y
        iny
        lda GRSTA
        sta (TMPB),y
        iny
        lda GRSTA+1
        sta (TMPB),y
        iny
        jmp GRB5
GRB4    ldy GRIDX
        lda GRANT,x
        sta (TMPB),y
        iny
GRB5    sty GRIDX
        jsr GRCLRLN
        jmp GRB2
GRB6    ldy GRIDX               ; the jump back to the start
        lda #$41
        sta (TMPB),y
        iny
        lda #0
        sta (TMPB),y
        iny
        lda TMPB+1
        sta (TMPB),y
        lda TMPB+1              ; the display list is the new one
        sta $0231
        lda #0
        sta $0230
        rts

GRCLRLN lda GRNOCL              ; clear the line that starts at GRSTA
        bne GRCL9
        lda GRSTA
        sta TMPC
        lda GRSTA+1
        sta TMPC+1
        ldx GRFX
        ldy GRBPL,x
        lda #0
GRCL1   dey
        sta (TMPC),y
        bne GRCL1
GRCL9   rts


GRPOSN  lda GRX                 ; the cursor and the old cursor to GRX, GRY
        sta GRCOLC
        sta GROLDC
        lda GRX+1
        sta GRCOLC+1
        sta GROLDC+1
        lda GRY
        sta GRROW
        sta GROLDR
        rts

GR_POS  jsr GRPOPXY             ; PROC GrPos(CARD x, CARD y)
        jmp GRPOSN

GR_PIX  jsr GRPOPXY             ; PROC GrPix(CARD x, CARD y): plot a point in the current color
        jsr GRPOSN
        ldx #GRIOCB
        lda #11
        sta GRICCOM,x
        lda #0
        sta GRICBLL,x
        sta GRICBLH,x
        lda GRCOLV
        jsr GRCIOV
        jmp GRCHK

GR_LOC  jsr GRPOPXY             ; BYTE FUNC GrLoc(CARD x, CARD y): the color of a point
        jsr GRPOSN
        ldx #GRIOCB
        lda #7
        sta GRICCOM,x
        lda #0
        sta GRICBLL,x
        sta GRICBLH,x
        jsr GRCIOV
        sta GRT
        jsr GRCHK
        lda GRT
        jmp GRRET

GRXIO   ldx #GRIOCB             ; A = the XIO command, line or fill, to the position in GRX, GRY
        sta GRICCOM,x
        lda GRX
        sta GRCOLC
        lda GRX+1
        sta GRCOLC+1
        lda GRY
        sta GRROW
        lda GRCOLV
        sta GRATACHR
        sta GRFILDAT
        lda #12
        sta GRICAX1,x
        lda #0
        sta GRICAX2,x
        sta GRICBLL,x
        sta GRICBLH,x
        jsr GRCIOV
        jsr GRCHK
        lda GRCOLC              ; the old position is now this one
        sta GROLDC
        lda GRCOLC+1
        sta GROLDC+1
        lda GRROW
        sta GROLDR
        rts

GR_OSLINE jsr GRPOPXY           ; PROC GrOSLine(CARD x, CARD y): XIO 17 from the old position
        lda #17
        jmp GRXIO

GR_OSFILL jsr GRPOPXY           ; PROC GrOSFill(CARD x, CARD y): XIO 18
        lda #18
        jmp GRXIO

GR_SETCOL jsr POPA              ; PROC GrSetCol(BYTE reg, BYTE hue, BYTE lum)
        lda TMPA
        and #15
        sta GRT
        jsr POPA
        lda TMPA
        asl a
        asl a
        asl a
        asl a
        ora GRT
        sta GRT
        jsr POPA
        ldx TMPA
        cpx #5
        bcc GRSC1
        lda #132
        sta GRERRC
        rts
GRSC1   lda GRT
        sta GRPALET,x
        rts

; size of the screen in the mode: the modes 0 to 15, with the text window (bit 4 clear) and without
GRWID   .word 40,20,20,40,80,80,160,160,320,80,80,80,40,40,160,160
GRHGT   .byte 24,24,12,20,40,40,80,80,160,160,160,160,24,12,160,160
GRHGTF  .byte 24,24,12,24,48,48,96,96,192,192,192,192,24,12,192,192
GRCLR   .byte 2,5,5,4,2,4,2,4,2,16,9,16,5,5,2,4

GR_WIDTH lda GRMD               ; CARD FUNC GrWidth()
        and #15
        asl a
        tax
        lda GRWID,x
        sta TMPA
        lda GRWID+1,x
        sta TMPA+1
        jmp PUSHA

GR_HEIGHT lda GRMD              ; CARD FUNC GrHeight()
        and #15
        tax
        lda GRMD
        and #16
        bne GRH1
        lda GRHGT,x
        jmp GRRET
GRH1    lda GRHGTF,x
        jmp GRRET

GR_COLORS lda GRMD              ; BYTE FUNC GrColors()
        and #15
        tax
        lda GRCLR,x
        jmp GRRET

; ---- POKEY: 4 voices, AUDF at $D200 + 2 * voice, AUDC (distortion * 16 + volume) next to it ----

        .const GRAUDF  $d200
        .const GRAUDCT $d208
        .const GRSKCTL $d20f

SN_RESET ldx #7                 ; PROC SndRst(): silence, and the sound hardware in its normal state
        lda #0
SNR1    sta GRAUDF,x
        dex
        bpl SNR1
        sta GRAUDCT
        lda #3
        sta GRSKCTL
        rts

SN_SOUND jsr POPA               ; PROC SnSound(BYTE voice, BYTE pitch, BYTE dist, BYTE vol)
        lda TMPA
        and #15
        sta GRT                 ; the volume
        jsr POPA
        lda TMPA
        asl a
        asl a
        asl a
        asl a
        ora GRT
        sta GRT                 ; AUDC
        jsr POPA
        lda TMPA
        sta GRT2                ; AUDF
        jsr POPA
        lda TMPA
        cmp #4
        bcc SNS1
        lda #132
        sta GRERRC
        rts
SNS1    asl a
        tax
        lda GRT2
        sta GRAUDF,x
        lda GRT
        sta GRAUDF+1,x
        rts

SN_TONE jsr POPA                ; PROC SnTone(BYTE pitch, BYTE frames): a pure tone, then quiet again
        lda TMPA
        sta GRT2                ; frames
        jsr POPA
        lda TMPA
        sta GRAUDF
        lda #$a8
        sta GRAUDF+1
SNT1    lda GRT2
        beq SNT3
        lda GRRTCLK
        sta GRT
SNT2    lda GRRTCLK
        cmp GRT
        beq SNT2
        dec GRT2
        jmp SNT1
SNT3    lda #0
        sta GRAUDF+1
        rts

        .endif
