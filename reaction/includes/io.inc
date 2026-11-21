; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; io.inc - input/output primitives for the reaction compiler (6502asm syntax)
;
; The library IO.ACT (Print, PrintE, InputS, Open, Close ...) is written in reaction and
; calls these routines, which it declares with   PROC name=@LABEL(params).  They are called as
; any routine is: the arguments are pushed on the run time stack in order, and a FUNC leaves
; its value on the stack.  Only the routines at the bottom of this file are machine specific;
; which set is assembled depends on the assembler's TARGET symbol:
;
;   atari800            CIO (CIOV at $E456)
;   c64                 the KERNAL jump table ($FFBA..$FFE4)
;   appleii             the monitor (COUT, RDKEY, GETLN1): the keyboard and the screen only
;   prodos              the monitor for the console, and the ProDOS MLI ($BF00) for files
;   (no target)         as atari800
;
; Channels are numbered 0 to 7, as in Action!.  Channel 0 is the console (keyboard and screen),
; channels 1 to 7 are files or devices opened with IO_OPEN.
;
; Strings are length prefixed: byte 0 is the length, the characters follow.
;
; IOSTAT holds the status of the last operation in the Atari's CIO numbering: 1 is success, 136 is
; end of file, 128 and above is any other error (130 no such device, 170 file not found ...).  The
; other machines' errors are turned into the nearest CIO number.
;
; The calls that make a program stop (IO_EXIT) and the EOL character are machine specific too.
;
; Not tested on the real machines: only on an emulated CPU with stand-ins for the operating system.

; ---- shared by all machines --------------------------------------------------------------

IOSTAT  .bzero 1                ; status of the last operation
IOBYTE  .bzero 1                ; the byte being transferred
IOCH    .bzero 1                ; the channel of the operation
IOX     .bzero 1                ; scratch
IOMODE  .bzero 1                ; Open: the mode
IOAUX   .bzero 1                ; Open: aux2, XIO: aux2
IOAUX1  .bzero 1                ; XIO: aux1
IOCMD   .bzero 1                ; XIO: the command
IOMAX   .bzero 1                ; the longest line wanted
IOIDX   .bzero 1                ; index into a string
IOSRC   .bzero 1                ; IOCOPY: the index of the first character of the name to copy
IOSECT  .bzero 2                ; Note/Point: the sector
IOOFFS  .bzero 1                ; Note/Point: the offset in the sector
IOCNTW  .bzero 2                ; BGet/BPut: the count asked for
IODONE  .bzero 2                ; BGet/BPut: the count done
IOP2    .bzero 2                ; Rename: the new name
IONAME  .bzero 160              ; a file name being built (Rename: the old name, then the new)

IO_STAT jsr IOSTAT2             ; BYTE FUNC IOStat()
        jmp PUSHA
IOSTAT2 lda IOSTAT
        sta TMPA
        lda #0
        sta TMPA+1
        rts

; copy the name (TMPB points at the string) from its character IOSRC to IONAME, starting at
; IONAME+X.  At most 76 characters.  Returns X = the next free index.
IOCOPY  ldy #0
        lda (TMPB),y
        sta TMPC                ; the length
        cmp #77
        bcc IOCP1
        lda #76
        sta TMPC
IOCP1   ldy IOSRC
IOCP2   tya
        cmp TMPC
        beq IOCP3
        bcs IOCP9               ; past the end
IOCP3   lda (TMPB),y
        sta IONAME,x
        inx
        iny
        bne IOCP2
IOCP9   rts

; does the name (at TMPB) start with a device name, "D:" "D1:" "P:" ...?  A colon among the
; first four characters.  Carry set if so, with Y = the index of the colon.
IOHASDEV ldy #0
        lda (TMPB),y
        sta TMPC
        ldy #1
IOHD1   tya
        cmp TMPC
        beq IOHD2
        bcs IOHD9
IOHD2   lda (TMPB),y
        cmp #':'
        beq IOHDY
        iny
        cpy #5
        bcc IOHD1
IOHD9   clc
        rts
IOHDY   sec
        rts

        .if TARGET == TARGET_C64

; =========================================================================================
; Commodore 64: the KERNAL
; =========================================================================================

        .const KSETLFS $ffba
        .const KSETNAM $ffbd
        .const KOPEN   $ffc0
        .const KCLOSE  $ffc3
        .const KCHKIN  $ffc6
        .const KCHKOUT $ffc9
        .const KCLRCHN $ffcc
        .const KCHRIN  $ffcf
        .const KCHROUT $ffd2
        .const KREADST $ffb7
        .const EOLCHR  $0d

IOEOFP  .bzero 8                ; the end of the file was reported with the last byte
IOPL    .bzero 8                ; the bytes read or written so far on each channel: the low,
IOPM    .bzero 8                ; middle
IOPH    .bzero 8                ; and high byte (for Note)
IOSDV   .bzero 8                ; a file open for reading: its device (0: Point is not possible)
IOSLN   .bzero 8                ; and the length of its name,
IOSNM   .bzero 192              ; and the name, 24 characters at most, to open it again

; the KERNAL's own error numbers 0..9, as CIO errors
IOKERR  .byte 0,129,129,133,170,130,131,135,165,130

; a KERNAL call failed: A = its error number
IOSKERR cmp #10
        bcc IOSK1
        lda #0
IOSK1   tax
        lda IOKERR,x
        sta IOSTAT
        jmp KCLRCHN

; select channel IOCH for output: carry set if that failed
IOSELO  ldx IOCH
        beq IOSO9               ; the screen
        jsr KCHKOUT
        bcc IOSO9
        jsr IOSKERR
        sec
        rts
IOSO9   clc
        rts

; status after an output operation
IOEND   lda IOCH
        beq IOEND1              ; the screen: READST belongs to the last serial operation
        jsr KREADST
        beq IOEND1
        lda #144
        .byte $2c               ; bit: skips the next lda
IOEND1  lda #1
        sta IOSTAT
        jmp KCLRCHN

IO_PUT  jsr POPA                ; PROC IOPut(BYTE chan, byte)
        lda TMPA
        sta IOBYTE
        jmp IOPUT1
IO_PUTEOL lda #EOLCHR           ; PROC IOEOL(BYTE chan)
        sta IOBYTE
IOPUT1  jsr POPA
        lda TMPA
        sta IOCH
        jsr IOSELO
        bcs IOPUT9
        lda IOBYTE
        jsr KCHROUT
        jsr IOPINC
        jmp IOEND
IOPUT9  rts

IO_WRITE jsr POPA               ; PROC IOWrite(BYTE chan, BYTE ARRAY s)
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        ldy #0
        lda (TMPB),y
        beq IOWR9               ; an empty string
        sta IOMAX
        jsr IOSELO
        bcs IOWR9
        lda #0
        sta IOIDX
IOWR1   inc IOIDX
        ldy IOIDX
        lda (TMPB),y
        jsr KCHROUT
        jsr IOPINC
        lda IOIDX
        cmp IOMAX
        bcc IOWR1
        jmp IOEND
IOWR9   rts

; read a byte from channel IOCH: A = the byte, IOSTAT = the status
IOGETC  ldx IOCH
        beq IOGC1               ; the keyboard
        lda IOEOFP,x
        beq IOGC0
        lda #136                ; the end of the file came with the last byte
        sta IOSTAT
        lda #0
        rts
IOGC0   jsr KCHKIN
        bcc IOGC1
        jsr IOSKERR
        lda #0
        rts
IOGC1   jsr KCHRIN
        sta IOBYTE
        lda #1
        sta IOSTAT
        lda IOCH
        beq IOGC9               ; the keyboard has no status
        jsr IOPINC
        jsr KREADST
        beq IOGC9
        pha
        and #$40
        beq IOGC2
        ldx IOCH
        lda #1
        sta IOEOFP,x
IOGC2   pla
        and #$bf
        beq IOGC9
        lda #144
        sta IOSTAT
IOGC9   jsr KCLRCHN
        lda IOBYTE
        rts

IO_GET  jsr POPA                ; BYTE FUNC IOGet(BYTE chan)
        lda TMPA
        sta IOCH
        jsr IOGETC
        sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

IO_LINE jsr POPA                ; PROC IOLine(BYTE chan, BYTE ARRAY s, BYTE max)
        lda TMPA
        sta IOMAX
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        lda #0
        sta IOIDX
IOLN1   lda IOIDX
        cmp IOMAX
        bcs IOLN9               ; the string is full
        jsr IOGETC
        ldx IOSTAT
        cpx #128
        bcs IOLN9               ; end of file or an error
        cmp #$0d
        beq IOLN9               ; end of the line
        inc IOIDX
        ldy IOIDX
        sta (TMPB),y
        jmp IOLN1
IOLN9   ldy #0
        lda IOIDX
        sta (TMPB),y
        rts

; PROC IOOpen(BYTE chan, BYTE ARRAY name, BYTE mode, BYTE aux2)
; Names: "D:FILE" (disk, unit 8; "D2:" is unit 9), "P:" the printer, "S:" and "E:" the screen,
; "K:" the keyboard.  A name with no device is on the disk.  Modes: 4 read, 6 directory,
; 8 write, 9 append.  The KERNAL reports a missing file only on the first read.
IO_OPEN jsr POPA
        lda TMPA
        sta IOAUX
        jsr POPA
        lda TMPA
        sta IOMODE
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        lda #8                  ; the device: the disk unless the name says otherwise
        sta IOX
        lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IOOP2
        sty IOIDX               ; the colon
        iny
        sty IOSRC               ; the name follows it
        ldy #1
        lda (TMPB),y            ; the device letter
        and #$df                ; upper case
        cmp #'D'
        bne IOOP3
        ldy #2
        lda (TMPB),y            ; D1: D2: ... the unit number
        cmp #'1'
        bcc IOOP2
        cmp #'5'
        bcs IOOP2
        sec
        sbc #'1'
        clc
        adc #8
        sta IOX
        jmp IOOP2
IOOP3   ldx #4
        cmp #'P'
        beq IOOP4
        ldx #3
        cmp #'S'
        beq IOOP4
        cmp #'E'
        beq IOOP4
        ldx #0
        cmp #'K'
        beq IOOP4
        lda #130                ; no such device
        sta IOSTAT
        rts
IOOP4   stx IOX
        lda #0
        sta IOIDX               ; no file name
        jmp IOOP8
IOOP2   ldx #0                  ; the disk: the name, then the type and direction
        lda IOMODE
        cmp #6
        bne IOOP5
        lda #'$'                ; the directory
        sta IONAME
        ldx #1
        jmp IOOP7
IOOP5   jsr IOCOPY
        lda #','
        sta IONAME,x
        inx
        lda #'S'
        sta IONAME,x
        inx
        lda #','
        sta IONAME,x
        inx
        lda IOMODE
        ldy #'R'
        cmp #4
        beq IOOP6
        ldy #'W'
        cmp #8
        beq IOOP6
        ldy #'A'
        cmp #9
        beq IOOP6
        lda #132                ; mode not supported
        sta IOSTAT
        rts
IOOP6   tya
        sta IONAME,x
        inx
IOOP7   stx IOIDX               ; the length of the name
IOOP8   lda IOCH                ; the logical file number
        ldx IOX                 ; the device
        ldy #0                  ; the secondary address: 0 for a device or a directory
        pha
        lda IOX
        cmp #8
        bcc IOOP9
        lda IOMODE
        cmp #6
        beq IOOP9
        lda IOCH                ; a disk file: 2 + the channel
        clc
        adc #2
        tay
IOOP9   pla
        jsr KSETLFS
        lda IOIDX
        ldx #<IONAME
        ldy #>IONAME
        jsr KSETNAM
        jsr KOPEN
        bcc IOOPA
        jmp IOSKERR
IOOPA   ldx IOCH
        lda #0
        sta IOEOFP,x
        sta IOPL,x
        sta IOPM,x
        sta IOPH,x
        sta IOSDV,x
        jsr IOSAVE
        lda #1
        sta IOSTAT
        rts

; a disk file for reading: keep what is needed to open it again (Point)
IOSAVE  lda IOX
        cmp #8
        bcc IOSV9
        lda IOMODE
        cmp #4
        bne IOSV9
        lda IOIDX
        cmp #25
        bcs IOSV9
        ldx IOCH
        sta IOSLN,x
        lda IOX
        sta IOSDV,x
        jsr IOSOFF
        ldy #0
IOSV1   cpy IOIDX
        bcs IOSV9
        lda IONAME,y
        sta IOSNM,x
        inx
        iny
        bne IOSV1
IOSV9   rts

; X = IOCH * 24, the start of the channel's saved name
IOSOFF  lda IOCH
        asl a
        asl a
        asl a
        sta TMPC
        asl a
        clc
        adc TMPC
        tax
        rts

; count a byte transferred on channel IOCH
IOPINC  ldx IOCH
        beq IOPI9
        inc IOPL,x
        bne IOPI9
        inc IOPM,x
        bne IOPI9
        inc IOPH,x
IOPI9   rts

IO_CLOSE jsr POPA               ; PROC IOClose(BYTE chan)
        lda TMPA
        tax
        lda #0
        sta IOEOFP,x
        txa
        jsr KCLOSE
        jsr KCLRCHN
        lda #1
        sta IOSTAT
        rts

IO_XIO  lda #146                ; XIO is an Atari call: not available
        sta IOSTAT
        lda #6
        sta IOIDX
IOXIO1  jsr POPA                ; the six arguments are thrown away
        dec IOIDX
        bne IOXIO1
        rts

; PROC IONote(BYTE chan, CARD POINTER sector, BYTE POINTER offset): the count of the bytes read
; or written so far, as the sector and the offset: position / 256 and position mod 256.
IO_NOTE jsr POPA
        lda TMPA
        sta TMPC
        lda TMPA+1
        sta TMPC+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        cmp #8
        bcs IONT8
        tax
        ldy #0
        lda IOPM,x
        sta (TMPB),y
        iny
        lda IOPH,x
        sta (TMPB),y
        ldy #0
        lda IOPL,x
        sta (TMPC),y
        lda #1
        sta IOSTAT
        rts
IONT8   lda #132
        sta IOSTAT
        rts

; PROC IOPoint(BYTE chan, CARD sector, BYTE offset): the disk has no seek for a sequential
; file, so the file is opened again and the bytes before the position are read and thrown away.
; Only for a file opened for reading, with a name of 24 characters at most.
IO_POINT jsr POPA
        lda TMPA
        sta IOOFFS
        jsr POPA
        lda TMPA
        sta IOSECT
        lda TMPA+1
        sta IOSECT+1
        jsr POPA
        lda TMPA
        sta IOCH
        beq IOPT8
        cmp #8
        bcs IOPT8
        tax
        lda IOSDV,x
        bne IOPT0
IOPT8   lda #132
        sta IOSTAT
        rts
IOPT0   sta IOX                 ; the device
        lda IOSLN,x
        sta IOIDX               ; the length of the name
        lda IOCH
        jsr KCLOSE
        jsr KCLRCHN
        jsr IOSOFF
        ldy #0
IOPT1   cpy IOIDX
        bcs IOPT2
        lda IOSNM,x
        sta IONAME,y
        inx
        iny
        bne IOPT1
IOPT2   lda IOCH
        ldx IOX
        ldy IOCH
        iny
        iny
        jsr KSETLFS
        lda IOIDX
        ldx #<IONAME
        ldy #>IONAME
        jsr KSETNAM
        jsr KOPEN
        bcc IOPT3
        jmp IOSKERR
IOPT3   ldx IOCH
        lda #0
        sta IOEOFP,x
        sta IOPL,x
        sta IOPM,x
        sta IOPH,x
        lda #1
        sta IOSTAT
IOPT4   ldx IOCH                ; read on while the count is below the position
        lda IOPH,x
        cmp IOSECT+1
        bcc IOPT5
        bne IOPT9
        lda IOPM,x
        cmp IOSECT
        bcc IOPT5
        bne IOPT9
        lda IOPL,x
        cmp IOOFFS
        bcs IOPT9
IOPT5   jsr IOGETC
        ldx IOSTAT
        cpx #128
        bcc IOPT4
IOPT9   rts

; CARD FUNC IOBGet(BYTE chan, BYTE ARRAY buf, CARD count), PROC IOBPut(BYTE chan, BYTE ARRAY buf, CARD count):
; one byte at a time here
IOBLK0  jsr POPA
        lda TMPA
        sta IOCNTW
        lda TMPA+1
        sta IOCNTW+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        lda #0
        sta IODONE
        sta IODONE+1
        lda #1
        sta IOSTAT
        rts

; carry set if IODONE has reached IOCNTW
IOBLDN  lda IODONE
        cmp IOCNTW
        bne IOBD9
        lda IODONE+1
        cmp IOCNTW+1
        beq IOBD8
IOBD9   clc
        rts
IOBD8   sec
        rts

; the next byte of the buffer
IOBLNX  inc TMPB
        bne IOBN1
        inc TMPB+1
IOBN1   inc IODONE
        bne IOBN2
        inc IODONE+1
IOBN2   rts

IO_BGET jsr IOBLK0
IOBG1   jsr IOBLDN
        bcs IOBG9
        jsr IOGETC
        ldx IOSTAT
        cpx #128
        bcs IOBG9
        ldy #0
        sta (TMPB),y
        jsr IOBLNX
        jmp IOBG1
IOBG9   lda IODONE
        sta TMPA
        lda IODONE+1
        sta TMPA+1
        jmp PUSHA

IO_BPUT jsr IOBLK0
        jsr IOSELO
        bcs IOBP9
IOBP1   jsr IOBLDN
        bcs IOBP8
        ldy #0
        lda (TMPB),y
        jsr KCHROUT
        jsr IOPINC
        jsr IOBLNX
        jmp IOBP1
IOBP8   jmp IOEND
IOBP9   rts

; The disk's own commands, sent on channel 15.  A = the number of the error, from the
; first two characters of its message.
; the unit of a name (D: D1: .. D4:, 8 and up) into IOX, and IOSRC to its first character;
; carry set if the device is not a disk
IOCDEV  lda #8
        sta IOX
        lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IOCD9
        iny
        sty IOSRC
        ldy #1
        lda (TMPB),y
        and #$df
        cmp #'D'
        bne IOCDE
        ldy #2
        lda (TMPB),y
        cmp #'1'
        bcc IOCD9
        cmp #'5'
        bcs IOCD9
        sec
        sbc #'1'
        clc
        adc #8
        sta IOX
IOCD9   clc
        rts
IOCDE   sec
        rts

; send IONAME (IOIDX characters) to unit IOX as a command, and read the answer
IOCMDG  lda #15
        ldx IOX
        ldy #15
        jsr KSETLFS
        lda IOIDX
        ldx #<IONAME
        ldy #>IONAME
        jsr KSETNAM
        jsr KOPEN
        bcc IOCG1
        jmp IOSKERR
IOCG1   ldx #15
        jsr KCHKIN
        jsr KCHRIN
        sta IOBYTE              ; the first digit of the error number
        jsr KCHRIN
        sta IOAUX               ; and the second
        jsr KCLRCHN
        lda #15
        jsr KCLOSE
        jsr KCLRCHN
        lda #1
        sta IOSTAT
        lda IOBYTE
        cmp #'0'
        bne IOCG2
        lda IOAUX
        cmp #'2'
        bcc IOCG9               ; 00 is no error, 01 is a count of files scratched
IOCG2   lda #144
        ldx IOBYTE
        cpx #'6'
        bne IOCG3
        ldx IOAUX
        cpx #'2'
        bne IOCG3
        lda #170                ; 62: file not found
IOCG3   sta IOSTAT
IOCG9   rts

IO_DELETE jsr POPA              ; PROC IODelete(BYTE ARRAY name): "S0:"
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCDEV
        bcs IODL8
        lda #'S'
        sta IONAME
        lda #'0'
        sta IONAME+1
        lda #':'
        sta IONAME+2
        ldx #3
        jsr IOCOPY
        stx IOIDX
        jmp IOCMDG
IODL8   lda #130
        sta IOSTAT
        rts

IO_RENAME jsr POPA              ; PROC IORename(BYTE ARRAY old, BYTE ARRAY new): "R0:new=old"
        lda TMPA
        sta IOP2
        lda TMPA+1
        sta IOP2+1
        jsr POPA
        lda TMPA
        sta TMPB
        sta IOCNTW
        lda TMPA+1
        sta TMPB+1
        sta IOCNTW+1
        jsr IOCDEV
        bcs IODL8
        lda IOSRC
        sta IOOFFS              ; where the old name starts
        lda #'R'
        sta IONAME
        lda #'0'
        sta IONAME+1
        lda #':'
        sta IONAME+2
        lda IOP2
        sta TMPB
        lda IOP2+1
        sta TMPB+1
        lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IORN1
        iny
        sty IOSRC               ; the new name has no device
IORN1   ldx #3
        jsr IOCOPY
        lda #'='
        sta IONAME,x
        inx
        stx IODONE
        lda IOCNTW
        sta TMPB
        lda IOCNTW+1
        sta TMPB+1
        lda IOOFFS
        sta IOSRC
        ldx IODONE
        jsr IOCOPY
        stx IOIDX
        jmp IOCMDG

IO_EXIT jmp $a474               ; back to BASIC

        .elif TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS

; =========================================================================================
; Apple II: the monitor, and for ProDOS the MLI
; =========================================================================================

        .const COUT    $fded
        .const RDKEY   $fd0c
        .const GETLN1  $fd6f
        .const EOLCHR  $0d

; the console: characters go out with bit 7 set
IOCOUT  ora #$80
        jsr COUT
        lda #1
        sta IOSTAT
        rts

        .if TARGET == TARGET_PRODOS

        .const MLI     $bf00

        .macro MLICALL cmd, plist
        jsr MLI
        .byte @cmd
        .word @plist
        .endm

IOREF   .bzero 8                ; the ProDOS reference number of each channel
IOBUFH  .bzero 8                ; the page of the channel's 1K buffer (0: none yet)

IOP_OPEN .byte 3
        .word IONAME
IOP_OBUF .word 0
IOP_OREF .byte 0
IOP_CRT .byte 7
        .word IONAME
        .byte $c3,$04           ; access, file type TXT
        .word 0                 ; aux type
        .byte 1                 ; a seedling file
        .word 0,0               ; date and time
IOP_RW  .byte 4                 ; READ and WRITE
IOP_RWR .byte 0
IOP_RWB .word 0
IOP_RWQ .word 0
IOP_RWX .word 0
IOP_CLS .byte 1
IOP_CLR .byte 0
IOP_EOF .byte 2                 ; GET_EOF, SET_EOF and SET_MARK
IOP_ER  .byte 0
IOP_EV  .byte 0,0,0
IOP_QUIT .byte 4
        .byte 0
        .word 0
        .byte 0
        .word 0
IOP_DES .byte 1                 ; DESTROY
        .word IONAME
IOP_REN .byte 2                 ; RENAME: the old path, then the new one
        .word IONAME
        .word IONAME+80

; an MLI error: A = its code, as a CIO number
IOMERR  cmp #$4c
        beq IOME1
        ldx #170                ; path or file not found
        cmp #$46
        beq IOME2
        cmp #$44
        beq IOME2
        ldx #130                ; no such volume
        cmp #$45
        beq IOME2
        ldx #165                ; bad name
        cmp #$40
        beq IOME2
        ldx #144
IOME2   stx IOSTAT
        rts
IOME1   lda #136
        sta IOSTAT
        rts

; the channel's 1K buffer comes from the heap, on a page boundary, the first time it is needed
IOALLOC ldx IOCH
        lda IOBUFH,x
        bne IOAL9
        lda HEAPPTR
        beq IOAL1
        lda #0
        sta HEAPPTR
        inc HEAPPTR+1
IOAL1   lda HEAPPTR+1
        sta IOBUFH,x
        clc
        adc #4
        sta HEAPPTR+1
IOAL9   rts

; load the READ/WRITE block for channel IOCH
IORWSET ldx IOCH
        lda IOREF,x
        sta IOP_RWR
        rts

        .elif TARGET == TARGET_APPLEII

; DOS 3.3 files, through the file manager: JSR $03DC gives the address of DOS's own parameter list
; (Y low, A high); it is filled in, then JSR $03D6 does the call (X=0 lets OPEN create a missing file).
; Each channel has 768 bytes from the heap: a data sector, a track/sector list sector, and the work area.
; Text files hold characters with bit 7 set: Put and Print set it, Get and InputS clear it.
; The position of a file is a 16 bit byte offset (so at most 64K): Note gives it as sector = high byte,
; offset = low byte, Point takes the same.  Files must be closed to be complete.

        .const DOSLOC  $03dc
        .const DOSFM   $03d6
        .const IODSLOT 6

IOBUFH  .bzero 8                ; the page of the channel's 768 bytes (0: none yet)
IOOPN   .bzero 8                ; 1: the channel has an open file
IOPOSL  .bzero 8                ; the position in the file, low
IOPOSH  .bzero 8                ; and high
IODRV   .bzero 1                ; the drive of the name being used
IOFMX   .bzero 1                ; the X register for the file manager
IOFM    .bzero 18               ; the parameter list, built here and copied to DOS's
IOFNM   .bzero 30               ; file names: 30 characters, bit 7 set, padded with spaces
IOFNM2  .bzero 30

; the file manager's return codes as CIO numbers
IOFERRT .byte 1,144,132,132,144,136,170,130,144,162,167

; the channel's 768 bytes come from the heap, on a page boundary, the first time they are needed
IOALLC3 ldx IOCH
        lda IOBUFH,x
        bne IOAL39
        lda HEAPPTR
        beq IOAL31
        lda #0
        sta HEAPPTR
        inc HEAPPTR+1
IOAL31  lda HEAPPTR+1
        sta IOBUFH,x
        clc
        adc #3
        sta HEAPPTR+1
IOAL39  rts

; is IOCH a channel with an open file?  Carry set (and IOSTAT) if not
IOCHK   ldx IOCH
        beq IOCK8
        cpx #8
        bcs IOCK8
        lda IOOPN,x
        beq IOCK9
        clc
        rts
IOCK8   lda #132
        sta IOSTAT
        sec
        rts
IOCK9   lda #133
        sta IOSTAT
        sec
        rts

; clear the parameter list and fill in what OPEN, DELETE and RENAME share
IOFSET  ldy #9
        lda #0
IOFS1   sta IOFM,y
        dey
        bpl IOFS1
        lda IODRV
        sta IOFM+5
        lda #IODSLOT
        sta IOFM+6
        lda #<IOFNM
        sta IOFM+8
        lda #>IOFNM
        sta IOFM+9
        lda #1
        sta IOFMX
        rts

; the call: the buffers of channel IOCH, the parameter list to DOS, the call, the results back.
; Returns Z set and IOSTAT=1 if all went well, else A = the code and IOSTAT its CIO number.
IOFMGO  ldx IOCH
        lda #0
        sta IOFM+$0c
        sta IOFM+$0e
        sta IOFM+$10
        lda IOBUFH,x
        sta IOFM+$11
        clc
        adc #1
        sta IOFM+$0f
        adc #1
        sta IOFM+$0d
        jsr DOSLOC
        sty TMPC
        sta TMPC+1
        ldy #17
IOFG1   lda IOFM,y
        sta (TMPC),y
        dey
        bpl IOFG1
        ldx IOFMX
        jsr DOSFM
        jsr DOSLOC
        sty TMPC
        sta TMPC+1
        ldy #17
IOFG2   lda (TMPC),y
        sta IOFM,y
        dey
        bpl IOFG2
        lda IOFM+$0a
        bne IOFG3
        lda #1
        sta IOSTAT
        lda #0
        rts
IOFG3   cmp #11
        bcc IOFG4
        lda #144
        sta IOSTAT
        jmp IOFG5
IOFG4   tax
        lda IOFERRT,x
        sta IOSTAT
IOFG5   lda IOFM+$0a
        rts

; the name at TMPB into the 30 character field IOFNM+X: a leading "D:" "D1:" "D2:" picks the drive
IOFNAME stx IOX
        lda #1
        sta IOSRC
        cpx #0
        bne IOFN0
        sta IODRV
IOFN0   jsr IOHASDEV
        bcc IOFN1
        cpy #3
        bne IOFN2
        dey
        lda (TMPB),y
        iny
        cmp #'1'
        beq IOFN3
        cmp #'2'
        bne IOFN2
IOFN3   and #3
        sta IODRV
IOFN2   iny
        sty IOSRC
IOFN1   ldx IOX
        ldy #30
        lda #$a0
IOFN4   sta IOFNM,x
        inx
        dey
        bne IOFN4
        ldy #0
        lda (TMPB),y
        sta TMPC
        ldy IOSRC
        ldx IOX
IOFN5   tya
        cmp TMPC
        beq IOFN6
        bcs IOFN9
IOFN6   lda (TMPB),y
        ora #$80
        cmp #$e1
        bcc IOFN7
        cmp #$fb
        bcs IOFN7
        and #$df                ; capitals
IOFN7   sta IOFNM,x
        inx
        iny
        txa
        sec
        sbc IOX
        cmp #30
        bcc IOFN5
IOFN9   rts

IOPINC  ldx IOCH
        inc IOPOSL,x
        bne IOPI9
        inc IOPOSH,x
IOPI9   rts

; one byte to the file: IOBYTE, with bit 7 set
IOPUTF  jsr IOCHK
        bcs IOPF9
        jsr IOFSET
        lda #4
        sta IOFM
        lda #1
        sta IOFM+1
        lda IOBYTE
        ora #$80
        sta IOFM+8
        jsr IOFMGO
        bne IOPF9
        jsr IOPINC
IOPF9   rts

; one byte from the file in A, bit 7 cleared; IOSTAT 136 at the end
IOGETF  jsr IOCHK
        bcs IOGF8
        jsr IOFSET
        lda #3
        sta IOFM
        lda #1
        sta IOFM+1
        jsr IOFMGO
        bne IOGF8
        jsr IOPINC
        lda IOFM+8
        and #$7f
        rts
IOGF8   lda #0
        rts

; POSITION to the start of the file
IOPOS0  jsr IOFSET
        lda #$0a
        sta IOFM
        lda #0                  ; the record and the offset are all zero: the drive and slot are not wanted here
        sta IOFM+5
        sta IOFM+6
        jmp IOFMGO

; the open call, with the arguments already popped: IOCH, TMPB = the name, IOMODE
IOOPX2  jmp IOOPE
IOOPD   lda IOCH
        beq IOOPX2
        cmp #8
        bcs IOOPX2
        tax
        lda IOOPN,x
        beq IOOD1
        lda #129                ; open already
        sta IOSTAT
        rts
IOOD1   jsr IOALLC3
        ldx #0
        jsr IOFNAME
        lda IOMODE
        cmp #8
        bne IOOD2
        jsr IOFSET              ; writing: a file there already is deleted first
        lda #5
        sta IOFM
        jsr IOFMGO
        beq IOOD2
        cmp #6
        beq IOOD2
        rts
IOOD2   jsr IOFSET
        lda #1
        sta IOFM
        ldx #1                  ; read and update: the file must be there
        lda IOMODE
        cmp #4
        beq IOOD3
        cmp #12
        beq IOOD3
        ldx #0                  ; write and append: it is made if it is not
IOOD3   stx IOFMX
        jsr IOFMGO
        beq IOOD4
        rts
IOOD4   ldx IOCH
        lda #0
        sta IOPOSL,x
        sta IOPOSH,x
        jsr IOPOS0
        bne IOOD9
        ldx IOCH
        lda #1
        sta IOOPN,x
        lda IOMODE
        cmp #9
        bne IOOD9
IOOD5   jsr IOGETF              ; append: read to the end
        lda IOSTAT
        cmp #128
        bcc IOOD5
        cmp #136
        bne IOOD9
        lda #1
        sta IOSTAT
IOOD9   rts

IOCLSD  ldx IOCH
        beq IOCD9
        cpx #8
        bcs IOCD9
        lda IOOPN,x
        beq IOCD9
        jsr IOFSET
        lda #2
        sta IOFM
        jsr IOFMGO
        ldx IOCH
        lda #0
        sta IOOPN,x
        rts
IOCD9   lda #1
        sta IOSTAT
        rts

; Note: the arguments are in the order channel, sector pointer, offset pointer
IONOTD  jsr POPA
        lda TMPA
        sta TMPC
        lda TMPA+1
        sta TMPC+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        jsr IOCHK
        bcs IONTD9
        ldx IOCH
        ldy #0
        lda IOPOSL,x
        sta (TMPC),y
        lda IOPOSH,x
        sta (TMPB),y
        iny
        lda #0
        sta (TMPB),y
        lda #1
        sta IOSTAT
IONTD9  rts

IOPNTD  jsr POPA
        lda TMPA
        sta IOOFFS
        jsr POPA
        lda TMPA
        sta IOSECT
        lda TMPA+1
        sta IOSECT+1
        jsr POPA
        lda TMPA
        sta IOCH
        jsr IOCHK
        bcs IOPD9
        lda IOSECT+1
        bne IOPD8
        jsr IOFSET
        lda #$0a
        sta IOFM
        lda IOOFFS
        sta IOFM+4
        lda IOSECT
        sta IOFM+5
        jsr IOFMGO
        bne IOPD9
        ldx IOCH
        lda IOOFFS
        sta IOPOSL,x
        lda IOSECT
        sta IOPOSH,x
IOPD9   rts
IOPD8   lda #132                ; a position past 64K
        sta IOSTAT
        rts

; BGet and BPut: the arguments are channel, buffer, count; carry set if the channel is not a file
IOBLKD  jsr POPA
        lda TMPA
        sta IOCNTW
        lda TMPA+1
        sta IOCNTW+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        lda #0
        sta IODONE
        sta IODONE+1
        jmp IOCHK

IOBGTD  jsr IOBLKD
        bcs IOBGR
        lda IOCNTW
        ora IOCNTW+1
        beq IOBG0
        jsr IOFSET
        lda #3
        sta IOFM
        lda #2                  ; READ a range
        sta IOFM+1
        lda IOCNTW
        sta IOFM+6
        lda IOCNTW+1
        sta IOFM+7
        lda TMPB
        sta IOFM+8
        lda TMPB+1
        sta IOFM+9
        jsr IOFMGO
        beq IOBGOK
        cmp #5
        bne IOBGR
        ldx IOCH                ; the end of the file: what was read is the change of position
        sec
        lda IOFM+4
        sbc IOPOSL,x
        sta IODONE
        lda IOFM+5
        sbc IOPOSH,x
        sta IODONE+1
        lda IOFM+4
        sta IOPOSL,x
        lda IOFM+5
        sta IOPOSH,x
        jmp IOBGR
IOBGOK  jsr IOBADD
IOBG0   lda #1
        sta IOSTAT
IOBGR   lda IODONE
        sta TMPA
        lda IODONE+1
        sta TMPA+1
        jmp PUSHA

; IODONE = IOCNTW, and the position moves on by that
IOBADD  lda IOCNTW
        sta IODONE
        lda IOCNTW+1
        sta IODONE+1
        ldx IOCH
        clc
        lda IOPOSL,x
        adc IOCNTW
        sta IOPOSL,x
        lda IOPOSH,x
        adc IOCNTW+1
        sta IOPOSH,x
        rts

IOBPTD  jsr IOBLKD
        bcs IOBPR
        lda IOCNTW
        ora IOCNTW+1
        beq IOBP0
        jsr IOFSET
        lda #4
        sta IOFM
        lda #2                  ; WRITE a range: the count is one less than the length
        sta IOFM+1
        sec
        lda IOCNTW
        sbc #1
        sta IOFM+6
        lda IOCNTW+1
        sbc #0
        sta IOFM+7
        lda TMPB
        sta IOFM+8
        lda TMPB+1
        sta IOFM+9
        jsr IOFMGO
        bne IOBPR
        jsr IOBADD
IOBP0   lda #1
        sta IOSTAT
IOBPR   rts

IODELD  jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        lda #0
        sta IOCH
        jsr IOALLC3
        ldx #0
        jsr IOFNAME
        jsr IOFSET
        lda #5
        sta IOFM
        jmp IOFMGO

IORENMD jsr POPA
        lda TMPA
        sta IOP2
        lda TMPA+1
        sta IOP2+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        lda #0
        sta IOCH
        jsr IOALLC3
        ldx #0
        jsr IOFNAME
        lda IOP2
        sta TMPB
        lda IOP2+1
        sta TMPB+1
        ldx #30
        jsr IOFNAME
        jsr IOFSET
        lda #9
        sta IOFM
        lda #<IOFNM2
        sta IOFM+2
        lda #>IOFNM2
        sta IOFM+3
        jmp IOFMGO

        .endif

IO_PUT  jsr POPA                ; PROC IOPut(BYTE chan, byte)
        lda TMPA
        sta IOBYTE
        jmp IOPUT1
IO_PUTEOL lda #EOLCHR           ; PROC IOEOL(BYTE chan)
        sta IOBYTE
IOPUT1  jsr POPA
        lda TMPA
        sta IOCH
        bne IOPUT2
        lda IOBYTE              ; the console
        jmp IOCOUT
IOPUT2
        .if TARGET == TARGET_PRODOS
        jsr IORWSET
        lda #<IOBYTE
        sta IOP_RWB
        lda #>IOBYTE
        sta IOP_RWB+1
        lda #1
        sta IOP_RWQ
        lda #0
        sta IOP_RWQ+1
        _MLICALL $cb, IOP_RW
        bcs IOPUT3
        lda #1
        sta IOSTAT
        rts
IOPUT3  jmp IOMERR
        .elif TARGET == TARGET_APPLEII
        jmp IOPUTF
        .else
        lda #130                ; files need ProDOS
        sta IOSTAT
        rts
        .endif

IO_WRITE jsr POPA               ; PROC IOWrite(BYTE chan, BYTE ARRAY s)
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        ldy #0
        lda (TMPB),y
        beq IOWR9               ; an empty string
        sta IOMAX
        lda IOCH
        bne IOWR2
        lda #0
        sta IOIDX               ; the console, a character at a time
IOWR1   inc IOIDX
        ldy IOIDX
        lda (TMPB),y
        jsr IOCOUT
        lda IOIDX
        cmp IOMAX
        bcc IOWR1
IOWR9   rts
IOWR2
        .if TARGET == TARGET_PRODOS
        jsr IORWSET             ; a file: all of it in one call
        clc
        lda TMPB
        adc #1
        sta IOP_RWB
        lda TMPB+1
        adc #0
        sta IOP_RWB+1
        lda IOMAX
        sta IOP_RWQ
        lda #0
        sta IOP_RWQ+1
        _MLICALL $cb, IOP_RW
        bcs IOWR3
        lda #1
        sta IOSTAT
        rts
IOWR3   jmp IOMERR
        .elif TARGET == TARGET_APPLEII
        lda #0
        sta IOIDX
IOWD1   inc IOIDX
        ldy IOIDX
        lda (TMPB),y
        sta IOBYTE
        jsr IOPUTF
        lda IOSTAT
        cmp #128
        bcs IOWD9
        lda IOIDX
        cmp IOMAX
        bcc IOWD1
IOWD9   rts
        .else
        lda #130
        sta IOSTAT
        rts
        .endif

; read a byte from channel IOCH: A = the byte, IOSTAT = the status
IOGETC  lda IOCH
        bne IOGC2
        jsr RDKEY               ; the console: the key is echoed
        and #$7f
        sta IOBYTE
        ora #$80
        jsr COUT
        lda #1
        sta IOSTAT
        lda IOBYTE
        rts
IOGC2
        .if TARGET == TARGET_PRODOS
        jsr IORWSET
        lda #<IOBYTE
        sta IOP_RWB
        lda #>IOBYTE
        sta IOP_RWB+1
        lda #1
        sta IOP_RWQ
        lda #0
        sta IOP_RWQ+1
        sta IOBYTE
        _MLICALL $ca, IOP_RW
        bcs IOGC3
        lda #1
        sta IOSTAT
        lda IOBYTE
        rts
IOGC3   jsr IOMERR
        lda #0
        rts
        .elif TARGET == TARGET_APPLEII
        jmp IOGETF
        .else
        lda #130
        sta IOSTAT
        lda #0
        rts
        .endif

IO_GET  jsr POPA                ; BYTE FUNC IOGet(BYTE chan)
        lda TMPA
        sta IOCH
        jsr IOGETC
        sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

IO_LINE jsr POPA                ; PROC IOLine(BYTE chan, BYTE ARRAY s, BYTE max)
        lda TMPA
        sta IOMAX
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        bne IOLN1
        jsr GETLN1              ; the console: a line, edited and echoed
        stx TMPC
        lda IOMAX
        cmp TMPC
        bcs IOLC1
        sta TMPC                ; no more than the string takes
IOLC1   ldy #0
IOLC2   cpy TMPC
        beq IOLC3
        lda $0200,y
        and #$7f
        iny
        sta (TMPB),y
        jmp IOLC2
IOLC3   lda TMPC
        ldy #0
        sta (TMPB),y
        lda #1
        sta IOSTAT
        rts
IOLN1   lda #0                  ; a file: a character at a time to the end of the line
        sta IOIDX
IOLN2   lda IOIDX
        cmp IOMAX
        bcs IOLN9
        jsr IOGETC
        ldx IOSTAT
        cpx #128
        bcs IOLN9
        and #$7f
        cmp #$0d
        beq IOLN9
        inc IOIDX
        ldy IOIDX
        sta (TMPB),y
        jmp IOLN2
IOLN9   ldy #0
        lda IOIDX
        sta (TMPB),y
        rts

; PROC IOOpen(BYTE chan, BYTE ARRAY name, BYTE mode, BYTE aux2)
; ProDOS files only.  Modes: 4 read, 8 write (the file is created or emptied), 9 append,
; 12 read and write.  A leading "D:" is dropped.
IO_OPEN jsr POPA
        lda TMPA
        sta IOAUX
        jsr POPA
        lda TMPA
        sta IOMODE
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        .if TARGET == TARGET_PRODOS
        lda IOCH
        beq IOOPX               ; channel 0 is the console
        cmp #8
        bcc IOOPY
IOOPX   jmp IOOPE
IOOPY   lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IOOP1
        iny
        sty IOSRC               ; skip the device name
IOOP1   ldx #1
        jsr IOCOPY
        txa
        sec
        sbc #1
        sta IONAME              ; ProDOS paths are length prefixed
        lda IOMODE
        cmp #8
        beq IOOP2
        cmp #9
        bne IOOP3
IOOP2   _MLICALL $c0, IOP_CRT   ; CREATE; it may exist already
IOOP3   jsr IOALLOC
        ldx IOCH
        lda #0
        sta IOP_OBUF
        lda IOBUFH,x
        sta IOP_OBUF+1
        _MLICALL $c8, IOP_OPEN
        bcs IOOPF
        ldx IOCH
        lda IOP_OREF
        sta IOREF,x
        sta IOP_ER
        lda #0
        sta IOP_EV
        sta IOP_EV+1
        sta IOP_EV+2
        lda IOMODE
        cmp #8
        bne IOOP4
        _MLICALL $d0, IOP_EOF   ; SET_EOF to 0: an empty file
        bcs IOOPF
        jmp IOOPG
IOOP4   cmp #9
        bne IOOPG
        _MLICALL $d1, IOP_EOF   ; GET_EOF
        bcs IOOPF
        _MLICALL $ce, IOP_EOF   ; SET_MARK to the end
        bcs IOOPF
IOOPG   lda #1
        sta IOSTAT
        rts
IOOPF   jmp IOMERR
        .elif TARGET == TARGET_APPLEII
        jmp IOOPD
        .endif
IOOPE   lda #130                ; files need ProDOS
        sta IOSTAT
        rts

IO_CLOSE jsr POPA               ; PROC IOClose(BYTE chan)
        lda TMPA
        sta IOCH
        .if TARGET == TARGET_PRODOS
        beq IOCL9
        tax
        lda IOREF,x
        sta IOP_CLR
        lda #0
        sta IOREF,x
        _MLICALL $cc, IOP_CLS
        bcs IOCL8
IOCL9   lda #1
        sta IOSTAT
        rts
IOCL8   jmp IOMERR
        .elif TARGET == TARGET_APPLEII
        jmp IOCLSD
        .else
        lda #1
        sta IOSTAT
        rts
        .endif

IO_XIO  lda #146                ; XIO is an Atari call: not available
        sta IOSTAT
        lda #6
        sta IOIDX
IOXIO1  jsr POPA                ; the six arguments are thrown away
        dec IOIDX
        bne IOXIO1
        rts

; A call that needs ProDOS, on a machine that has not: A = the number of arguments to drop
IONOFS  sta IOIDX
        lda #130
        sta IOSTAT
IONOF1  jsr POPA
        dec IOIDX
        bne IONOF1
        rts

; PROC IONote(BYTE chan, CARD POINTER sector, BYTE POINTER offset)
; PROC IOPoint(BYTE chan, CARD sector, BYTE offset)
; The ProDOS file mark, as MARK / 256 (the sector) and MARK mod 256 (the offset).
IO_NOTE
        .if TARGET == TARGET_PRODOS
        jsr POPA
        lda TMPA
        sta TMPC
        lda TMPA+1
        sta TMPC+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOCH
        beq IONT8
        cmp #8
        bcs IONT8
        tax
        lda IOREF,x
        sta IOP_ER
        _MLICALL $cf, IOP_EOF   ; GET_MARK
        bcs IONT7
        ldy #0
        lda IOP_EV+1
        sta (TMPB),y
        iny
        lda IOP_EV+2
        sta (TMPB),y
        ldy #0
        lda IOP_EV
        sta (TMPC),y
        lda #1
        sta IOSTAT
        rts
IONT7   jmp IOMERR
IONT8   lda #132                ; not a file
        sta IOSTAT
        rts
        .elif TARGET == TARGET_APPLEII
        jmp IONOTD
        .else
        lda #3
        jmp IONOFS
        .endif

IO_POINT
        .if TARGET == TARGET_PRODOS
        jsr POPA
        lda TMPA
        sta IOP_EV
        jsr POPA
        lda TMPA
        sta IOP_EV+1
        lda TMPA+1
        sta IOP_EV+2
        jsr POPA
        lda TMPA
        sta IOCH
        beq IOPT8
        cmp #8
        bcs IOPT8
        tax
        lda IOREF,x
        sta IOP_ER
        _MLICALL $ce, IOP_EOF   ; SET_MARK
        bcs IOPT7
        lda #1
        sta IOSTAT
        rts
IOPT7   jmp IOMERR
IOPT8   lda #132
        sta IOSTAT
        rts
        .elif TARGET == TARGET_APPLEII
        jmp IOPNTD
        .else
        lda #3
        jmp IONOFS
        .endif

; CARD FUNC IOBGet(BYTE chan, BYTE ARRAY buf, CARD count), PROC IOBPut(BYTE chan, BYTE ARRAY buf, CARD count)
IO_BGET
        .if TARGET == TARGET_PRODOS
        jsr IOBLK0
        bcs IOBGR
        _MLICALL $ca, IOP_RW    ; READ
        jsr IOBLF
IOBGR   lda IODONE
        sta TMPA
        lda IODONE+1
        sta TMPA+1
        jmp PUSHA
IO_BPUT jsr IOBLK0
        bcs IOBPR
        _MLICALL $cb, IOP_RW    ; WRITE
        jsr IOBLF
IOBPR   rts

; the arguments: carry set if the channel is not a file
IOBLK0  jsr POPA
        lda TMPA
        sta IOP_RWQ
        lda TMPA+1
        sta IOP_RWQ+1
        jsr POPA
        lda TMPA
        sta IOP_RWB
        lda TMPA+1
        sta IOP_RWB+1
        jsr POPA
        lda TMPA
        sta IOCH
        lda #0
        sta IODONE
        sta IODONE+1
        lda IOCH
        beq IOBK8
        cmp #8
        bcs IOBK8
        jsr IORWSET
        clc
        rts
IOBK8   lda #132
        sta IOSTAT
        sec
        rts

; after the MLI: carry and A as it left them
IOBLF   php
        pha
        lda IOP_RWX
        sta IODONE
        lda IOP_RWX+1
        sta IODONE+1
        pla
        plp
        bcs IOBLF2
        lda #1
        sta IOSTAT
        rts
IOBLF2  jmp IOMERR
        .elif TARGET == TARGET_APPLEII
        jmp IOBGTD
IO_BPUT jmp IOBPTD
        .else
        lda #3
        jsr IONOFS
        lda #0
        sta TMPA
        sta TMPA+1
        jmp PUSHA
IO_BPUT lda #3
        jmp IONOFS
        .endif

; PROC IODelete(BYTE ARRAY name), PROC IORename(BYTE ARRAY old, BYTE ARRAY new)
IO_DELETE
        .if TARGET == TARGET_PRODOS
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        ldx #1
        jsr IOPNAM
        _MLICALL $c1, IOP_DES   ; DESTROY
        bcs IODL8
        lda #1
        sta IOSTAT
        rts
IODL8   jmp IOMERR

IO_RENAME jsr POPA
        lda TMPA
        sta IOP2
        lda TMPA+1
        sta IOP2+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        ldx #1
        jsr IOPNAM
        lda IOP2
        sta TMPB
        lda IOP2+1
        sta TMPB+1
        ldx #81
        jsr IOPNAM
        _MLICALL $c2, IOP_REN   ; RENAME
        bcs IODL8
        lda #1
        sta IOSTAT
        rts

; a ProDOS path from the string at TMPB, without a device, at IONAME+X, its length in front
IOPNAM  stx IOX
        lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IOPN1
        iny
        sty IOSRC
IOPN1   ldx IOX
        jsr IOCOPY
        txa
        sec
        sbc IOX
        ldx IOX
        dex
        sta IONAME,x
        rts
        .elif TARGET == TARGET_APPLEII
        jmp IODELD
IO_RENAME jmp IORENMD
        .else
        lda #1
        jmp IONOFS
IO_RENAME lda #2
        jmp IONOFS
        .endif

IO_EXIT
        .if TARGET == TARGET_PRODOS
        _MLICALL $65, IOP_QUIT  ; QUIT
        .else
        jmp $03d0               ; DOS 3.3 warm start
        .endif
        rts

        .else

; =========================================================================================
; Atari 800 (and the default): CIO
; =========================================================================================

        .const CIOV    $e456
        .const ICCOM   $0342
        .const ICBAL   $0344
        .const ICBAH   $0345
        .const ICBLL   $0348
        .const ICBLH   $0349
        .const ICAX1   $034a
        .const ICAX2   $034b
        .const ICAX3   $034c
        .const ICAX4   $034d
        .const ICAX5   $034e
        .const EOLCHR  $9b

; X = the channel number * 16 (the IOCB offset); pops the channel
IOCHX   jsr POPA
        lda TMPA
        asl a
        asl a
        asl a
        asl a
        tax
        rts

IO_PUT  jsr POPA                ; PROC IOPut(BYTE chan, byte)
        lda TMPA
        sta TMPB
        jmp IOPUT1
IO_PUTEOL lda #EOLCHR           ; PROC IOEOL(BYTE chan)
        sta TMPB
IOPUT1  jsr IOCHX
        lda #11                 ; PUTCHR; a length of 0 means the byte is in A
        sta ICCOM,x
        lda #0
        sta ICBLL,x
        sta ICBLH,x
        lda TMPB
        jsr CIOV
        sty IOSTAT
        rts

IO_WRITE jsr POPA               ; PROC IOWrite(BYTE chan, BYTE ARRAY s)
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCHX
        ldy #0
        lda (TMPB),y
        beq IOWR9               ; an empty string
        sta ICBLL,x
        lda #0
        sta ICBLH,x
        clc
        lda TMPB
        adc #1
        sta ICBAL,x
        lda TMPB+1
        adc #0
        sta ICBAH,x
        lda #11                 ; PUTCHR with a buffer: the whole string
        sta ICCOM,x
        jsr CIOV
        sty IOSTAT
IOWR9   rts

IO_GET  jsr IOCHX               ; BYTE FUNC IOGet(BYTE chan)
        lda #7                  ; GETCHR
        sta ICCOM,x
        lda #0
        sta ICBLL,x
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        sta TMPA
        lda #0
        sta TMPA+1
        jmp PUSHA

IO_LINE jsr POPA                ; PROC IOLine(BYTE chan, BYTE ARRAY s, BYTE max)
        lda TMPA
        sta IOMAX
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCHX
        lda #5                  ; GETREC: up to the end of the line
        sta ICCOM,x
        clc
        lda TMPB
        adc #1
        sta ICBAL,x
        lda TMPB+1
        adc #0
        sta ICBAH,x
        lda IOMAX
        sta ICBLL,x
        lda #0
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        lda ICBLL,x             ; the characters transferred, with the EOL
        beq IOLN9
        tay
        lda (TMPB),y
        cmp #EOLCHR
        bne IOLN2
        dey                     ; not part of the string
IOLN2   tya
IOLN9   ldy #0
        sta (TMPB),y
        rts

; PROC IOOpen(BYTE chan, BYTE ARRAY name, BYTE mode, BYTE aux2)
; "D:" is put in front of a name that has no device.  The name goes to CIO ended by an EOL.
IO_OPEN jsr POPA
        lda TMPA
        sta IOAUX
        jsr POPA
        lda TMPA
        sta IOMODE
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCHX
        stx IOX
        lda #1
        sta IOSRC
        jsr IOHASDEV
        ldx #0
        bcs IOOP1
        lda #'D'
        sta IONAME
        lda #':'
        sta IONAME+1
        ldx #2
IOOP1   jsr IOCOPY
        lda #EOLCHR
        sta IONAME,x
        ldx IOX
        lda #3                  ; OPEN
        sta ICCOM,x
        lda #<IONAME
        sta ICBAL,x
        lda #>IONAME
        sta ICBAH,x
        lda IOMODE
        sta ICAX1,x
        lda IOAUX
        sta ICAX2,x
        lda #0
        sta ICBLL,x
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        rts

IO_CLOSE jsr IOCHX              ; PROC IOClose(BYTE chan)
        lda #12                 ; CLOSE
        sta ICCOM,x
        jsr CIOV
        sty IOSTAT
        rts

; PROC IOXIO(BYTE chan, BYTE zero, BYTE cmd, BYTE aux1, BYTE aux2, BYTE ARRAY name)
IO_XIO  jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr POPA
        lda TMPA
        sta IOAUX
        jsr POPA
        lda TMPA
        sta IOAUX1
        jsr POPA
        lda TMPA
        sta IOCMD
        jsr POPA                ; the dummy parameter
        jsr IOCHX
        stx IOX
        lda #1
        sta IOSRC
        ldx #0
        jsr IOCOPY
        lda #EOLCHR
        sta IONAME,x
        ldx IOX
        lda IOCMD
        sta ICCOM,x
        lda #<IONAME
        sta ICBAL,x
        lda #>IONAME
        sta ICBAH,x
        lda IOAUX1
        sta ICAX1,x
        lda IOAUX
        sta ICAX2,x
        lda #0
        sta ICBLL,x
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        rts

; PROC IONote(BYTE chan, CARD POINTER sector, BYTE POINTER offset)
; The position of the next byte, as the disk handler gives it: a sector and an offset in it.
IO_NOTE jsr POPA
        lda TMPA
        sta TMPC
        lda TMPA+1
        sta TMPC+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCHX
        lda #38                 ; NOTE
        sta ICCOM,x
        jsr CIOV
        sty IOSTAT
        cpy #128
        bcs IONT9
        ldy #0
        lda ICAX3,x
        sta (TMPB),y
        iny
        lda ICAX4,x
        sta (TMPB),y
        ldy #0
        lda ICAX5,x
        sta (TMPC),y
IONT9   rts

; PROC IOPoint(BYTE chan, CARD sector, BYTE offset): back to a noted position
IO_POINT jsr POPA
        lda TMPA
        sta IOOFFS
        jsr POPA
        lda TMPA
        sta IOSECT
        lda TMPA+1
        sta IOSECT+1
        jsr IOCHX
        lda IOSECT
        sta ICAX3,x
        lda IOSECT+1
        sta ICAX4,x
        lda IOOFFS
        sta ICAX5,x
        lda #37                 ; POINT
        sta ICCOM,x
        jsr CIOV
        sty IOSTAT
        rts

; CARD FUNC IOBGet(BYTE chan, BYTE ARRAY buf, CARD count): count bytes in one call; returns
; the number read.  PROC IOBPut(BYTE chan, BYTE ARRAY buf, CARD count) writes them.
IO_BGET lda #7                  ; GETCHR
        sta IOCMD
        jsr IOBLK
        lda IODONE
        sta TMPA
        lda IODONE+1
        sta TMPA+1
        jmp PUSHA
IO_BPUT lda #11                 ; PUTCHR
        sta IOCMD
IOBLK   jsr POPA
        lda TMPA
        sta IOCNTW
        lda TMPA+1
        sta IOCNTW+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        jsr IOCHX
        lda #0
        sta IODONE
        sta IODONE+1
        lda #1
        sta IOSTAT
        lda IOCNTW
        ora IOCNTW+1
        beq IOBL9               ; a length of 0 would mean one byte in A
        lda IOCMD
        sta ICCOM,x
        lda TMPB
        sta ICBAL,x
        lda TMPB+1
        sta ICBAH,x
        lda IOCNTW
        sta ICBLL,x
        lda IOCNTW+1
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        lda ICBLL,x
        sta IODONE
        lda ICBLH,x
        sta IODONE+1
IOBL9   rts

; "D:" and then the name, for the file commands: X = the next free index of IONAME
IOXNAM  lda #1
        sta IOSRC
        jsr IOHASDEV
        ldx #0
        bcs IOXN1
        lda #'D'
        sta IONAME
        lda #':'
        sta IONAME+1
        ldx #2
IOXN1   jmp IOCOPY

; the command IOCMD on the name in IONAME up to X, which is ended here
IOXGO   lda #EOLCHR
        sta IONAME,x
        ldx #$70                ; IOCB 7 is kept for this
        lda IOCMD
        sta ICCOM,x
        lda #<IONAME
        sta ICBAL,x
        lda #>IONAME
        sta ICBAH,x
        lda #0
        sta ICAX1,x
        sta ICAX2,x
        sta ICBLL,x
        sta ICBLH,x
        jsr CIOV
        sty IOSTAT
        rts

IO_DELETE jsr POPA              ; PROC IODelete(BYTE ARRAY name)
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        lda #33
        sta IOCMD
        jsr IOXNAM
        jmp IOXGO

IO_RENAME jsr POPA              ; PROC IORename(BYTE ARRAY old, BYTE ARRAY new)
        lda TMPA
        sta IOP2
        lda TMPA+1
        sta IOP2+1
        jsr POPA
        lda TMPA
        sta TMPB
        lda TMPA+1
        sta TMPB+1
        lda #32
        sta IOCMD
        jsr IOXNAM
        lda #','
        sta IONAME,x
        inx
        stx IOX
        lda IOP2
        sta TMPB
        lda IOP2+1
        sta TMPB+1
        lda #1
        sta IOSRC
        jsr IOHASDEV
        bcc IORN1
        iny
        sty IOSRC               ; the new name has no device
IORN1   ldx IOX
        jsr IOCOPY
        jmp IOXGO

IO_EXIT jmp (10)                ; DOSVEC: back to DOS

        .endif
