; Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
; forward references, backward branches, zero page vs absolute
	.org $0200
	.const zpvar $10
	.const scr $0400

start
	lda #$00		; a9 00
	sta zpvar		; 85 10        (zero page, already defined)
	sta later		; 8d 15 02     (forward reference is always absolute)
	ldx #$05		; a2 05
loop	dex			; ca
	bne loop		; d0 fd        (backward branch)
	jmp done		; 4c 19 02
	.byte 1,2,3		; 01 02 03
msg	.byte "hi"		; 68 69 00     (zero terminated)
later	.word $1234, msg	; 34 12 12 02
done	lda scr			; ad 00 04
	lda #>scr		; a9 04
	lda #<later		; a9 15
	rts			; 60
