H	function	X,(X>>8)&0FF
L	function	X,X&0FF

	; .TITLE PNLPGM, 'SC/MP PROM PROGRAMMER'

; "PNLPGM" IS A PROGRAM THAT ALLOWS THE SC/MPP LCDS TO
; PROGRAM MM5204 OR MM4204 PROMS USING THE PANEL FOR ALL
; INPUTS FROM THE USER

; THE USER MUST ENTER THE STARTING HEX ADDRESS TO BE
; PROGRAMMED FROM OR COPIED TO INTO POINTER #1 BEFORE
; EXECUTING EACH ROUTINE.

; THE PROGRAM REQUIRES THAT THERE ARE 512 CONTINUOUS MEMORY
; LOCATIONS UPWARDS FROM THE CONTENTS OF POINTER #1.

; "PNLPGM" USES MEMORY ON THE LCDS MOTHERBOARD FOR TEMPORARY
; STORAGE.  THEREFORE, USER R/W MEM0RY CAN BE ANYHERE,
; EXCEPT AS SHOWN BELOW.

; "PNLPGM" CAN BE ASSEMBLED ANYWHERE THE USER HAS MEMORY
; SPACE EXCEPT IN THE FOLLOWING LOCATIONS:

;       X'8000 -- X'83FF      LOCATIONS OCCUPIED BY
;                             PROGRAMMER HARDWARE
;       X'7000 -- X'7FFF      LOCATIONS USED BY LCDS

; THE ENTRY POINTS ARE:

; NAME          ADDRESS
; ----          -------

; CKERSD        X'0001          CHECK PROM FOR ERASED
; WOCHK         X'0035          PRORGAM PROM WITHOUT CHECK
;                               FOR ERASED
; PROG          X'0048          PRORGAM PROM WITH CHECK FOR
;                               ERASED
; COPY          X'0101          COPY PROM INTO MEMORY
; VERIFY        X'012C          VERIFY PROM AGAINST MEMORY

; "PNLPGM" HALTS WHEN FINISHED OR ON ERrORS.  THE ERRROR
; HALTS ARE:

;  X'0035       PROM NOT ERASED
;  X'0085       PROM CANNOT BE PROGRAMMED
;  X'015A       PROM DOES NOT VERIFY

; THE "HALT INST" SWITCH ON THE LCDS MOTHERBOARD MUST BE
; IN THE "DEBUG" POSITION BEFORE EXECUTION

	;.PAGE	'POINTERS AND CONSTANTS'

$	=	0

P1	=	1
P2	=	2
P3	=	3

PPRGMR	=	08000		;PERIPHERAL ADDR. OF PROM
				; PROGPAMMER BOARD
VP	=	014		;ORDER CODE FOR PROG. VOLTAGE
RD	=	0C		;ORDER CODE FOR READ
VSS	=	04		;ORDER CODE FOR VSS,VBB,VDD
CS	=	01C		;ORDER CODE FOR CHIP SELECT
LOAD	=	0		;ORDER CODE TO LOAD DATA
CLK	=	1		;ORDER CODE TO CLOCK COUNTER
RDPRM	=	2		;ORDER CODE TO READ PROM
				; DATA
CLR	=	3		;ORDER CODE TO CLEAR COUNTER
ENBL	=	4		;ORDER CODE TO ENABLE CONTROL
				; LATCH
RAM	=	077D0		;RAM POINTER TO MOTHERBOARD
				; R/W MEMORY
HI	=	0		;TEMPORARY LOCATIONS IN RAM
LO	=	-1
SAVLO	=	-2
SAVHI	=	-3
HCNT	=	-4
DPLO	=	-5
DPHI	=	-6

	;.PAGE	'PROM ACCESS AND PROGRAAMMING ROUTINES'

; *** ROUTINE TO CHECK PROM FOR ERASED CONDITION ***

	NOP
CKERSD:
	LDI	L(ERASED)-1
	XPAL	P1
	LDI	H(ERASED)
	XPAH	P1
	XPPC	P1
	HALT			;HALT IF ERASED

; THIS SUBROUTINE IS ALSO CALLED BY "PROG" TO VERIFY THAT
; THE PROM IS EPASED.

ERASED:
	LDI	L(RAM)		;INIT RAM POINTER
	XPAL	P2
	LDI	H(RAM)
	XPAH	P2
ERAS2:
	LDI	H(PPRGMR)	;SET ADDR. OF PROGRAMMER
	XPAH	P3
	LDI	L(PPRGMR)
	XPAL	P3
	LDI	1		;SET UPPER COUNT
	ST	HI(P2)		;STACK IS EMPTY, USE AS PTR
	ST	RD(P3)		;SET READ MODE
	ST	CS(P3)		;SELECT PROM SOCKET
	LDI	0		;SET LOWER COUNT
	ST	LO(P2)
	ST	CLR(P3)		;CLEAR PROM COUNTER
	ST	LOAD(P3)	;CLEAR OATA LATCHES FOR READ
ELOOP:
	LD	RDPRM(P3)	;READ DATA OUT OF PROM
	JNZ	NOT		;DATA NOT ZERO? NOT ERASEO
	ST	CLK(P3)		;DATA OK, BUMP COUNTER
	DLD	LO(P2)		;DECREMENT LOWER COUNT
	JNZ	ELOOP		;NOT DONE YET
	DLD	HI(P2)		;DECREMENT UPPER COUNT
	JP	ELOOP		;NOT DONE YET
	XPPC	P1		;PROM IS ERASED
NOT:
	HALT			;HALT IF PROM NOT ERASED

; *** ENTRY POINT TO PPOGRAM PROM W/O CHECK FOR ERASED ***

WOCHK:
	CCL
	LDI	H(RAM)		;INIT RAM POINTER
	XPAH	P2
	LDI	L(RAM)
	XPAL	P2
	LDI	H(PPRGMR)
	XPAH	P3
	LDI	L(PPRGMR)
	XPAL	P3
	XPAH	P1
	ADI	2		;ADD 512 OFFSET
	XPAH	P1
	JMP	SET

; *** ENTRY POINT TO PROGRAM PROM W/CHECK FOR ERASED **

;     PROM IS PROGRAMMED FROM MOST SIGNIFICANT ADDRESS TO
;     LEAST SIGNIFICANT ADDRESS DUE TO INVERSION OF COUNTER

PROG:
	CCL
	LDI	L(RAM)
	XPAL	P2
	LDI	H(RAM)
	XPAH	P2
	XPAL	P1		;SAVE ADDRESS
	ST	SAVLO(P2)
	XPAH	P1
	ST	SAVHI(P2)
	LDI	L(ERAS2)-1	;CHECK PROM FOR ERASED
	XPAL	P1
	XPPC	P1
	LD	SAVLO(P2)
	XPAL	P1
	LD	SAVHI(P2)
	ADI	2		;ADD 512 OFFSET
	XPAH	P1
SET:	LDI	1
	ST	HI(P2)		;SET UPPER LOC COUNT
	LDI	0
	ST	LO(P2)		;SET LOWER LOC COUNT
	ST	CLR(P3)		;CLEAR PROM COUNTER
NXTLOC:
	LD	@-1(P1)		;GET DATA TO PROGRAM
	XAE			;SAVE IN EXTENSION REG.
THSLOC:
	LDI	H(PPROM)	;SET ADDR OF PROGRAMMING RTN
	XPAH	P1
	ST	SAVHI(P2)	;SAVE PTR ADDR
	LDI	L(PPROM)-1
	XPAL	P1
	ST	SAVLO(P2)
	LDI	-2		;SET COUNT
	ST	HCNT(P2)	;SAVE HIT COUNT IN RAM
GO:
	XPPC	P1
	XRE			;CHECK PROM DATA
	JZ	OK		;PPOM DATA CORRECT, DO X+5X
	DLD	HCNT(P2)	;DECREMENT HIT COUNT
	JNZ	GO		;CHECK FOR MAX HIT
NPROG:
	HALT			;HIT COUNT OVER MAX, BAD PROM
OK:
	CCL
	LDI	0
	ST	DPHI(P2)	;CLEAR UPPER D. P COUNT
	CAD	HCNT(P2)	;COMPLEMENT HIT COUNT
	ST	HCNT(P2)
	CCL			;CLEAR CARRY/LINK FLAG
	ADD	HCNT(P2)	;COMPUTE 5X
	ST	DPLO(P2)
	LDI	0
	ADD	DPHI(P2)
	ST	DPHI(P2)	;2X
	CCL
	LD	DPLO(P2)
	ADD	DPLO(P2)
	ST	DPLO(P2)
	LD	DPHI(P2)
	ADD	DPHI(P2)
	ST	DPHI(P2)	;4X
	CCL
	LD	HCNT(P2)
	ADD	DPLO(P2)
	ST	DPLO(P2)
	LDI	0
	ADD	DPHI(P2)
	ST	DPHI(P2)	;5X
PRLP:
	XPPC	P1		;PROGRAM PROM
	DLD	DPLO(P2)	;DECREMENT LOWER COUNT
	JNZ	PRLP		;NOT DONE
	DLD	DPHI(P2)	;DECREMENT UPPER COUNT
	JP	PRLP		;NOT DONE
	LD	SAVLO(P2)	;RESTORE P1
	XPAL	P1
	LD	SAVHI(P2)
	XPAH	P1
UPDATE:
	ST	CLK(P3)		;BUMP PROM COUNTER
	DLD	LO(P2)		;DECREMENT LOWER LOC COUNT
	JNZ	NXTLOC		;NOT DONE
	DLD	HI(P2)		;DECREMENT UPPER LOC COUNT
	JP	NXTLOC		;NOT DONE
	HALT			;HALT WHEN DONE
PPROM:
	LDI	0
	ST	RD(P3)		;TURN OFF READ MODE AND
	ST	CS(P3)		; CHIP SELECT
	LDE
	ST	LOAD(P3)	;SEND DATA TO PROGRAMMER
	LDI	1
	ST	VSS(P3)		;TURN ON VSS VOLTAGE
	LDI	07F		;WAIT 500 MICPOSECONDS
	DLY	0
	ST	VP(P3)		;TURN ON PROGRAM PULSE
	LDI	0FF
	DLY	0		;DELAY 1 MS
	LDI	0
	ST	VP(P3)		;TURN OFF VP
	LDI	20		;WAIT 100 MICROSECONDS
	DLY	0
	LDI	0
	ST	VSS(P3)		;TURN OFF VSS VOLTAGE
	LDI	20
	DLY	0
	ST	RD(P3)		;SET UP READ MODE
	ST	CS(P3)		;SELECT PROM SOCKET
	LDI	0
	ST	LOAD(P3)	;CLEAR DATA LATCHES
	LD	RDPRM(P3)	;READ DATA FROM PROM
	XPPC	P1
	JMP	PPROM

; *** COPY PROM TO RANGE IN MEMORY ***

COPY:
	CCL
	LDI	H(RAM)
	XPAH	P2
	LDI	L(RAM)
	XPAL	P2		;INIT RAM POINTER
	XPAL	P1
	ST	SAVLO(P2)
	XPAH	P1
	ADI	2		;ADD 512 OFFSET
	ST	SAVHI(P2)
	JS	P1,SETRD	;SET READ MODE
	LD	SAVHI(P2)	;RETRIEVE ADDRESS
	XPAH	P1
	LD	SAVLO(P2)
	XPAL	P1
CPLOOP:
	LD	RDPRM(P3)	;GET PROM DATA
	ST	@-1(P1)		;STORE INTO MENORY
	ST	CLK(P3)		;BUMP PROM COUNTER
	DLD	LO(P2)		;DECREMENT LOC COUNTER LOW
	JNZ	CPLOOP
	DLD	HI(P2)		;DECREMENT LOC COUNTER HIGH
	JP	CPLOOP		;NOT DONE
	HALT			;HALT WHEN DONE

; *** VERIFY PROM AGAINST RANGE IN MEMORY ***

VERIFY:
	CCL
	LDI	H(RAM)
	XPAH	P2
	LDI	L(RAM)
	XPAL	P2		;INIT RAM POINTER
	XPAL	P1
	ST	SAVLO(P2)
	XPAH	P1
	ADI	2		;ADD 512 OFFSET
	ST	SAVHI(P2)
	JS	P1,SETRD	;SET READ MODE
	LD	SAVHI(P2)	;RETRIEVE ADDRESS
	XPAH	P1
	LD	SAVLO(P2)
	XPAL	P1
VLOOP:
	LD	RDPRM(P3)	;GET DATA FROM PROM
	XOR	@-1(P1)		;COMPARE AGAINST MEMORY DATA
	JNZ	NOVFY		;DOES NOT VERIFY
	ST	CLK(P3)		;BUMP PROM COUNTER
	DLD	LO(P2)		;DECREMENT LOC COUNTER LOW
	JNZ	VLOOP		;NOT DONE
	DLD	HI(P2)		;DECREMENT LOC COUNTER HIGH
	JP	VLOOP		;NOT DONE
	HALT			;HALT WHEN DONE
NOVFY:
	HALT			;HALT ON ERROR
SETRD:
	LDI	H(PPRGMR)	;PUT ADDR. OF PROGRAMMER
	XPAH	P3		; IN P3
	LDI	L(PPRGMR)
	XPAL	P3
	LDI	1		;SET UPPER LOC COUNTER
	ST	HI(P2)
	ST	RD(P3)		;SET READ MODE
	ST	CS(P3)		;SELECT PROM SOCKET
	LDI	0
	ST	LO(P2)
	ST	CLR(P3)		;CLEAR PROM COUNTER
	ST	LOAD(P3)	;CLEAR PROM DATA LATCHES
	XPPC	P1

	END