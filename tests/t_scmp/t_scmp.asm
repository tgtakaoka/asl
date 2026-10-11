	cpu	sc/mp
	page	0
	relaxed on

ptr0	reg	p0
ptr1	equ	1
ptr2	equ	p2
ptr3	equ	p3
ereg	reg	e
pcnt	equ	pc

        lde			; 40
        xae			; 01
        ane			; 50
        ore			; 58
        xre			; 60
        dae			; 68
        ade			; 70
        cae			; 78

        sio			; 19
        sr			; 1C
        srl			; 1D
        rr			; 1E
        rrl			; 1F

        halt			; 00
        ccl			; 02
        scl			; 03
        dint			; 04
        ien			; 05
        csa			; 06
        cas			; 07
        nop			; 08

        ldi	0x12		; C4 12
        ani	0x23		; D4 23
        ori	0x34		; DC 34
        xri	0x45		; E4 45
        dai	0x56		; EC 56
        adi	0x67		; F4 67
        cai	0x78		; FC 78
        dly	0x89		; 8F 89

        xpal	pc		; 30
        xpah	p2		; 36 ; it is valid to use just
	xpah	2		; 36 ; the register # instead of Pn
        xppc	p1		; 3D
	xppc	1		; 3D

	ld	e(pc)		; C0 80 ; pointer register is PC
        st	@e(p2)		; CE 80
	st	@ereg(2)	; CE 80
	ld	-127(pc)	; C0 81
	expect	440
	ld	-128(pc)	; C0 80
	endexpect
	ld	-127(p1)	; C1 81
	expect	440
	ld	-128(p2)	; C2 80 ; ...but not on P1...P3
	endexpect
	expect	1445
	ld	@-127(pc)	; no auto-increment with PC
	endexpect
        and	10(p1)		; D1 0A
	and	10(1)		; D1 0A
        or	@-20(p3)	; DF EC
	or	@-20(3)		; DF EC
        xor	vari		; E0 FF
vari:	dad	-30(p2)		; EA E2
	dad	-30(2)		; EA E2
	add	@40(p1)		; F5 28
	add	@40(1)		; F5 28
	add	@x'28'(p1)	; F5 28
	add	@x'28(p1)	; F5 28
        cad	vari		; F8 F3

	jmp	vari		; 90 F0
        jp	10(p2)		; 96 0A
	jp	10(2)		; 96 0A
        jz	vari		; 98 EA
        jnz	vari		; 9C E8

        ild	vari		; A8 E7
        dld	-5(p2)		; BA FB
	dld	-5(2)		; BA FB

;        org     0xfff
;        ldi     0x20

	org	384

	ld	$-126		; C0 81
	expect	1330
	ld	$-127
	endexpect

	expect	1330		; would result in a displacement
	ld	$-128		; of -129, which is out of range
	endexpect

;-----------------------------------------------------
; standard Intel/MASM-style pseudo instructions

	include "../t_dx/t_dn.inc"
	include "../t_dx/t_db.inc"
	include "../t_dx/t_dw.inc"
	include "../t_dx/t_dd.inc"
	include "../t_dx/t_dq.inc"
	include "../t_dx/t_dt.inc"
	include "../t_dx/t_do.inc"
