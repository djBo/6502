;
; 65c02 Main BIOS
;
			.65C02

RESET_ADDR	.EQU	$E000
MSGBASE		.EQU	$80

TEENSY		.EQU	$8000
TEENSY1		.EQU	$8800
TEENSY2		.EQU	$9000

LCD0		.EQU	$A000
LCD1		.EQU	LCD0+1


			.CODE
			.ORG RESET_ADDR

INIT
			CLD					; clear decimal mode
			LDX	#$FF			; empty stack
			TXS					; set the stack
;			JSR TINIT			; setup Teensy
			JSR LINIT			; setup LCD display
			JSR MEMTEST			; perform memory test

LOOP
			NOP
			JMP LOOP

NMI_HANDLER:
			PHA		; Save accumulator
			PHX		; Save X-register
			PHY		; Save Y-register

			PLY		; Restore Y-register
			PLX		; Restore X-register
			PLA		; Restore accumulator
			RTI		; Resume interrupted task

IRQ_HANDLER:
			PHA		; Save accumulator
			PHX		; Save X-register
			PHY		; Save Y-register

			PLY		; Restore Y-register
			PLX		; Restore X-register
			PLA		; Restore accumulator
			RTI		; Resume interrupted task

; *** Teensy initialisation
TINIT		LDY #$02			; Run 2 times
			LDX #$0F			; Load 0x0F in X
TINIT0		TXA					; Copy X to A
			STA TEENSY,X		; Store A at 0x8000 + X ?
			DEX					; Dec X
			BNE TINIT0			; Is X not zero?
			LDX #$0F
			DEY					; Dec Y
			BNE TINIT0			; Is X not zero?
			LDX #$0F
TINIT1		TXA
			STA TEENSY1,X
			DEX
			BNE TINIT1
			LDX #$0F
TINIT2		TXA
			STA TEENSY2,X
			DEX
			BNE TINIT2
			JSR LOOP

			.ORG $F000

; *** LCD initialisation
LINIT		LDX #$04			; do function set 4 times
LINIT0		LDA #$38			; function set: 8 bit, 2 lines, 5x7
			STA LCD0
			JSR LCDBUSY			; wait for busy flag to clear
			DEX
			BNE LINIT0
			LDA #$06			; entry mode set: increment, no shift
			STA LCD0
			JSR LCDBUSY
			LDA #$0E			; display on, cursor on, blink off
			STA LCD0
			JSR LCDBUSY
			LDA #$01			; clear display
			STA LCD0
			JSR LCDBUSY
			LDA #$80			; DDRAM address set: $00
			STA LCD0
			JSR LCDBUSY
			RTS
LINITMSG	.BYTE "LCD init done. ",0


; *** Memory test
MEMMSG1		.BYTE "Memory test: ",0
MEMMSG2		.BYTE "OK",0
MEMMSG3		.BYTE "fail at $",0
MEMTEST		LDX #$01			; X=high byte
			LDY #$FF			; Y=low byte
			LDA #MEMMSG1		; print message
			STA MSGBASE
			LDA #MEMMSG1/256
			STA MSGBASE+1
			JSR LCDSTRING

			JSR WRTDATA
			JMP LOOP

MEMLOOP		INY
			BNE MEMTST
			INX
			CPX #$80			; reached $8000? no more RAM...
			BNE MEMTST
			LDA #MEMMSG2		; print 'OK'
			STA MSGBASE
			LDA #MEMMSG2/256
			STA MSGBASE+1
			JSR LCDSTRING
			RTS
MEMTST		STX $0001
			LDA #$FF
			STA ($0000),Y
			CMP ($0000),Y
			BNE MEMFAIL
			LDA #$00
			STA ($0000),Y
			CMP ($0000),Y
			BNE MEMFAIL
			JMP MEMLOOP
MEMFAIL		STY $0000			; store high (x) and low (y) bytes of fail address
			STX $0001
			LDA #MEMMSG3		; print 'fail at $'
			STA MSGBASE
			LDA #MEMMSG3/256
			STA MSGBASE+1
			JSR LCDSTRING
			LDA $0001			; print high byte
			JSR LCDHEX
			LDA $0000			; print low byte
			JSR LCDHEX
MEMFAI1		LDA $0001			; display high byte on leds
			BEQ MEMFAI1
			RTS

; *** Wait for LCD busy bit to clear
LCDBUSY		PHA
LCDBUSY0	LDA LCD0			; read from LCD register 0
			AND #$80			; check bit 7 (busy)
			BNE LCDBUSY0
			PLA
			RTS

; *** Print character on LCD
LCDPRINT	STA LCD1
			JSR LCDBUSY
			LDA LCD0			; get current DDRAM address
			AND #$7F
			CMP #$14			; wrap from pos $13 (line 1 char 20)...
			BNE LCDPRINT0
			LDA #$C0			; ...to $40 (line 2 char 1)
			STA LCD0
			JSR LCDBUSY
LCDPRINT0	RTS

; *** Print 2 digit hex number on LCD
LCDHEX		PHA
			LSR A
			LSR A
			LSR A
			LSR A
			ORA #$30
			JSR LCDPRINT
			PLA
			PHA
			AND #$0F
			JSR LCDPRINT
			PLA
			RTS
	
; *** Print string on LCD
LCDSTRING	PHA				; save A, Y to stack
			TYA
			PHA
			LDY #$00
LCDSTR0		LDA (MSGBASE),Y
			BEQ LCDSTR1
			JSR LCDPRINT
			INY
			BNE LCDSTR0
LCDSTR1		PLA				; restore A, Y
			TAY
			PLA
			RTS










;WRTDATA: Fill fifteen memory locations with unique, incremental
;values, where each address uses a unique address line.
WRTDATA:	LDX #$0F
			STX $4000     ;$4000 = #$0F  (for test of A14)
			DEX
			STX $2000     ;$2000 = #$0E  (for test of A13)
			DEX
			STX $1000     ;$1000 = #$0D  (for test of A12)
			DEX
			STX $0800     ;$0800 = #$0C  (for test of A11)
			DEX
			STX $0400     ;$0400 = #$0B  (for test of A10)
			DEX
			STX $0200     ;$0200 = #$0A  (for test of A9)
			DEX
			STX $0100     ;$0100 = #$09  (for test of A8)
			DEX
			STX $0080     ;$0080 = #$08  (for test of A7)
			DEX
			STX $0040     ;$0040 = #$07  (for test of A6)
			DEX
			STX $0020     ;$0020 = #$06  (for test of A5)
			DEX
			STX $0010     ;$0010 = #$05  (for test of A4)
			DEX
			STX $0008     ;$0008 = #$04  (for test of A3)
			DEX
			STX $0004     ;$0004 = #$03  (for test of A2)
			DEX
			STX $0002     ;$0002 = #$02  (for test of A1)
			DEX
			STX $0001     ;$0001 = #$01  (for test of A0)

;RAMTEST1: Check address lines A0-A7.
;Routine is broken into RAMTEST1 and RAMTEST2 due to the limited range
;of the relative branch instruction BNE.
RAMTEST1:	CPX $0001
			BNE FOUNDBAD  ;A0 bad (not = #$01)?
			INX
			CPX $0002
			BNE FOUNDBAD  ;A1 bad (not = #$02)?
			INX
			CPX $0004
			BNE FOUNDBAD  ;A2 bad (not = #$03)?
			INX
			CPX $0008
			BNE FOUNDBAD  ;A3 bad (not = #$04)?
			INX
			CPX $0010
			BNE FOUNDBAD  ;A4 bad (not = #$05)?
			INX
			CPX $0020
			BNE FOUNDBAD  ;A5 bad (not = #$06)?
			INX
			CPX $0040
			BNE FOUNDBAD  ;A6 bad (not = #$07)?
			INX
			CPX $0080     ;A7 bad (not = #$08)?
			BNE FOUNDBAD
			JMP RAMTEST2

;FOUNDBAD: Called by either RAMTEST1 or RAMTEST2 if
;an error was detected.  This is the jump to the error handler.
FOUNDBAD:	JMP FAILURE

;RAMTEST2: Tests address lines A8-A14
RAMTEST2:	INX
			CPX $0100
			BNE FOUNDBAD  ;A8 bad (not = #$09)?
			INX
			CPX $0200
			BNE FOUNDBAD  ;A9 bad (not = #$0A)?
			INX
			CPX $0400
			BNE FOUNDBAD  ;A10 bad (not = #$0B)?
			INX
			CPX $0800
			BNE FOUNDBAD  ;A11 bad (not = #$0C)?
			INX
			CPX $1000
			BNE FOUNDBAD  ;A12 bad (not = #$0D)?
			INX
			CPX $2000
			BNE FOUNDBAD  ;A13 bad (not = #$0E)?
			INX
			CPX $4000
			BNE FOUNDBAD  ;A14 bad (not = #$0E)?

;SUCCESS: Program completed with no errors detected.
;
SUCCESS:	LDX #$00		;Optional: Clean up the test bytes by filling
			STX $01			;with #$00.
			STX $02
			STX $04
			STX $08
			STX $10
			STX $20
			STX $40
			STX $80
			STX $0100
			STX $0200
			STX $0400
			STX $0800
			STX $1000
			STX $2000
			STX $4000
			LDA #MEMMSG2		; print 'OK'
			STA MSGBASE
			LDA #MEMMSG2/256
			STA MSGBASE+1
			JSR LCDSTRING
			RTS

FAILURE:	DEX           ;An error was detected.
;	   .
;	   .            The address line under test (Axx) at the
;	   .            time of failure is in the X register.
			
			TXA			; print high byte
			JSR LCDHEX
			RTS

			.ORG $FFFA
			.WORD	NMI_HANDLER
			.WORD	RESET_ADDR
			.WORD	IRQ_HANDLER
.END
