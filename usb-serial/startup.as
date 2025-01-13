
	; HI-TECH C for 8051 V9.03
	; Copyright (C) 1984-2005 HI-TECH Software

	; Auto-generated runtime startup code for final link stage.

	;
	; Compiler options:
	;
	; --rom=default,-0-fff --codeoffset=1000h -q --chip=c8051f320 --opt=all \
	; -g --asmlist -I. -UDEBUG -ULOGGING -UUSB_LOGGING --runtime=+keep \
	; -osicom.hex -msicom.map main.obj sicom.obj bootlib.as
	;


	; small model, internal stack

	global	_main, _exit, start
	global	__Lrbss, __Hrbss, __Lbss, __Hbss
	global	__Lrdata, __Hrdata, __Brdata
	global	__Lidata, __Hidata
	global	__Lirdata, __Hirdata, __Birdata
	global	__Lrbit, __Hrbit, __Brbit
	global	start1, powerup
	global	small_model, stack_internal

small_model	equ	1
stack_internal	equ	1

	global	__Ldata, __Hdata
	global	__Lconst, __Hconst
	global	__Lstrings, __Hstrings

	global	intlevel0, intlevel1

intlevel0:
intlevel1:

	psect	data,class=CODE
	psect	const,class=CODE
	psect	strings,class=CODE
	psect	code,class=CODE
	psect	text,class=CODE
	psect	bss,class=XDATA,space=1
	psect	heap,global,class=XDATA,space=1
	psect	nvram,global,class=XDATA,space=1
	psect	rbss,class=DATA,space=1,size=80h,limit=80h
	psect	rdata,class=DATA,size=80h,limit=80h
	psect	rbit,bit,class=BITSEG,space=1,size=80h,limit=80h
	psect	vectors,ovrld,class=CODE
start:	ljmp	powerup
	fnroot	start1			;setup call graph
	fncall	start1,_main
	fnconf	rbss,?a,?
	psect	text
start1:
	clr	a
	mov	psw,a
	mov	sp,#__Hidata

	; rbit psect zero size; nothing to clear

	; Clear rbss psect.

	mov	r0,#__Lrbss
	mov	r2,#__Hrbss-__Lrbss	;get size of internal bss
1:
	mov	@r0,a			;clear it
	inc	r0			;advance pointer
	djnz	r2,1b

	; idata psect zero size; nothing to clear

	; rdata psect zero size; nothing to copy

	; irdata psect zero size; nothing to copy

	; Clear external RAM (bss psect)

	mov	dptr,#__Lbss
	mov	r2,#__Hbss-__Lbss	;get size of external bss
	;no need to zero accumulator
3:
	movx	@dptr,a			;clear it
	inc	dptr			;advance XRAM pointer
	djnz	r2,3b

	lcall	_main			;call user main()
_exit:
	ljmp	start			;and loop back to start

	end	start
