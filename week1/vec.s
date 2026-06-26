	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 26, 2
	.globl	_naive                          ; -- Begin function naive
	.p2align	2
_naive:                                 ; @naive
	.cfi_startproc
; %bb.0:
                                        ; kill: def $w0 killed $w0 def $x0
	cmp	w0, #1
	b.lt	LBB0_11
; %bb.1:
	mov	w8, w0
	cmp	w0, #4
	b.hs	LBB0_12
; %bb.2:
	ldr	s0, [x1]
	ldr	s1, [x2]
	ldr	s2, [x3]
	fmadd	s0, s0, s1, s2
	str	s0, [x3]
	cmp	w0, #1
	b.eq	LBB0_11
; %bb.3:
	ldr	s0, [x1]
	ldr	s1, [x2, #4]
	ldr	s2, [x3, #4]
	fmadd	s0, s0, s1, s2
	str	s0, [x3, #4]
	cmp	w0, #2
	b.eq	LBB0_5
; %bb.4:
	ldr	s0, [x1]
	ldr	s1, [x2, #8]
	ldr	s2, [x3, #8]
	fmadd	s0, s0, s1, s2
	str	s0, [x3, #8]
	cmp	w0, #1
	b.eq	LBB0_11
LBB0_5:
	add	x10, x2, x8, lsl #2
	ldr	s0, [x1, #4]
	ldr	s1, [x10]
	ldp	s2, s3, [x3]
	fmadd	s0, s0, s1, s2
	str	s0, [x3]
	ldr	s0, [x1, #4]
	ldr	s1, [x10, #4]
	fmadd	s0, s0, s1, s3
	str	s0, [x3, #4]
	cmp	w0, #2
	b.eq	LBB0_7
; %bb.6:
	ldr	s0, [x1, #4]
	ldr	s1, [x10, #8]
	ldp	s4, s2, [x3, #4]
	fmadd	s0, s0, s1, s2
	str	s0, [x3, #8]
	add	x9, x2, x8, lsl #3
	ldr	s1, [x1, #8]
	ldr	s2, [x9]
	ldr	s3, [x3]
	fmadd	s1, s1, s2, s3
	str	s1, [x3]
	ldr	s1, [x1, #8]
	ldr	s2, [x9, #4]
	fmadd	s1, s1, s2, s4
	str	s1, [x3, #4]
	ldr	s1, [x1, #8]
	ldr	s2, [x9, #8]
	fmadd	s0, s1, s2, s0
	str	s0, [x3, #8]
	cmp	w0, #1
	b.eq	LBB0_11
LBB0_7:
	lsl	x9, x8, #2
	add	x12, x1, x9
	add	x11, x3, x9
	ldr	s0, [x12]
	ldr	s1, [x2]
	ldp	s2, s3, [x11]
	fmadd	s0, s0, s1, s2
	str	s0, [x11]
	ldr	s0, [x12]
	ldr	s1, [x2, #4]
	fmadd	s0, s0, s1, s3
	str	s0, [x11, #4]
	cmp	w0, #2
	b.eq	LBB0_9
; %bb.8:
	ldr	s0, [x12]
	ldr	s1, [x2, #8]
	ldr	s2, [x11, #8]
	fmadd	s0, s0, s1, s2
	str	s0, [x11, #8]
LBB0_9:
	ldr	s0, [x12, #4]
	ldr	s1, [x10]
	ldp	s2, s3, [x11]
	fmadd	s0, s0, s1, s2
	str	s0, [x11]
	ldr	s0, [x12, #4]
	ldr	s1, [x10, #4]
	fmadd	s0, s0, s1, s3
	str	s0, [x11, #4]
	cmp	w0, #2
	b.eq	LBB0_11
; %bb.10:
	ldr	s0, [x12, #4]
	ldr	s1, [x10, #8]
	ldp	s3, s2, [x11, #4]
	fmadd	s0, s0, s1, s2
	str	s0, [x11, #8]
	lsl	x8, x8, #3
	add	x10, x2, x8
	ldr	s1, [x12, #8]
	ldr	s2, [x3, x9]
	ldr	s4, [x10]
	fmadd	s1, s1, s4, s2
	str	s1, [x3, x9]
	ldr	s1, [x12, #8]
	ldr	s2, [x10, #4]
	fmadd	s1, s1, s2, s3
	str	s1, [x11, #4]
	ldr	s1, [x12, #8]
	ldr	s2, [x10, #8]
	fmadd	s0, s1, s2, s0
	str	s0, [x11, #8]
	add	x11, x1, x8
	add	x8, x3, x8
	ldr	s0, [x11]
	ldr	s1, [x2]
	ldp	s2, s3, [x8]
	fmadd	s0, s0, s1, s2
	str	s0, [x8]
	ldr	s1, [x11]
	ldr	s2, [x2, #4]
	fmadd	s1, s1, s2, s3
	str	s1, [x8, #4]
	ldr	s2, [x11]
	ldr	s3, [x2, #8]
	ldr	s4, [x8, #8]
	fmadd	s2, s2, s3, s4
	str	s2, [x8, #8]
	add	x9, x2, x9
	ldr	s3, [x11, #4]
	ldr	s4, [x9]
	fmadd	s0, s3, s4, s0
	str	s0, [x8]
	ldr	s3, [x11, #4]
	ldr	s4, [x9, #4]
	fmadd	s1, s3, s4, s1
	str	s1, [x8, #4]
	ldr	s3, [x11, #4]
	ldr	s4, [x9, #8]
	fmadd	s2, s3, s4, s2
	str	s2, [x8, #8]
	ldr	s3, [x11, #8]
	ldr	s4, [x10]
	fmadd	s0, s3, s4, s0
	str	s0, [x8]
	ldr	s0, [x11, #8]
	ldr	s3, [x10, #4]
	fmadd	s0, s0, s3, s1
	str	s0, [x8, #4]
	ldr	s0, [x11, #8]
	ldr	s1, [x10, #8]
	fmadd	s0, s0, s1, s2
	str	s0, [x8, #8]
LBB0_11:
	ret
LBB0_12:
	stp	x24, x23, [sp, #-48]!           ; 16-byte Folded Spill
	stp	x22, x21, [sp, #16]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #32]             ; 16-byte Folded Spill
	.cfi_def_cfa_offset 48
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	mov	x9, #0                          ; =0x0
	umull	x10, w0, w0
	add	x10, x2, x10, lsl #2
	ubfiz	x11, x0, #2, #32
	and	x12, x8, #0x7ffffff0
	and	x13, x8, #0xc
	and	x14, x8, #0x7ffffffc
	add	x15, x3, #32
	lsl	x16, x8, #2
	add	x17, x2, #32
	neg	x4, x14
	mov	x5, x3
	b	LBB0_14
LBB0_13:                                ;   in Loop: Header=BB0_14 Depth=1
	add	x9, x9, #1
	add	x15, x15, x16
	add	x5, x5, x16
	cmp	x9, x8
	b.eq	LBB0_32
LBB0_14:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB0_18 Depth 2
                                        ;       Child Loop BB0_21 Depth 3
                                        ;       Child Loop BB0_25 Depth 3
                                        ;       Child Loop BB0_27 Depth 3
                                        ;     Child Loop BB0_29 Depth 2
                                        ;       Child Loop BB0_30 Depth 3
	mul	x19, x11, x9
	add	x6, x11, x19
	add	x20, x3, x6
	add	x21, x1, x6
	mul	x6, x9, x8
	add	x6, x1, x6, lsl #2
	add	x22, x3, x19
	cmp	x22, x10
	ccmp	x20, x2, #0, lo
	cset	w7, hi
	add	x19, x1, x19
	cmp	x19, x20
	ccmp	x22, x21, #2, lo
	b.lo	LBB0_28
; %bb.15:                               ;   in Loop: Header=BB0_14 Depth=1
	tbnz	w7, #0, LBB0_28
; %bb.16:                               ;   in Loop: Header=BB0_14 Depth=1
	mov	x7, #0                          ; =0x0
	mov	x19, x2
	mov	x20, x17
	b	LBB0_18
LBB0_17:                                ;   in Loop: Header=BB0_18 Depth=2
	add	x7, x7, #1
	add	x20, x20, x16
	add	x19, x19, x16
	cmp	x7, x8
	b.eq	LBB0_13
LBB0_18:                                ;   Parent Loop BB0_14 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB0_21 Depth 3
                                        ;       Child Loop BB0_25 Depth 3
                                        ;       Child Loop BB0_27 Depth 3
	add	x21, x6, x7, lsl #2
	cmp	w0, #16
	b.hs	LBB0_20
; %bb.19:                               ;   in Loop: Header=BB0_18 Depth=2
	mov	x23, #0                         ; =0x0
	b	LBB0_24
LBB0_20:                                ;   in Loop: Header=BB0_18 Depth=2
	ld1r.4s	{ v0 }, [x21]
	mov	x22, x20
	mov	x23, x15
	mov	x24, x12
LBB0_21:                                ;   Parent Loop BB0_14 Depth=1
                                        ;     Parent Loop BB0_18 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldp	q1, q2, [x22, #-32]
	ldp	q3, q4, [x22], #64
	ldp	q5, q6, [x23, #-32]
	ldp	q7, q16, [x23]
	fmla.4s	v5, v1, v0
	fmla.4s	v6, v2, v0
	fmla.4s	v7, v3, v0
	fmla.4s	v16, v4, v0
	stp	q5, q6, [x23, #-32]
	stp	q7, q16, [x23], #64
	subs	x24, x24, #16
	b.ne	LBB0_21
; %bb.22:                               ;   in Loop: Header=BB0_18 Depth=2
	cmp	x12, x8
	b.eq	LBB0_17
; %bb.23:                               ;   in Loop: Header=BB0_18 Depth=2
	mov	x23, x12
	mov	x22, x12
	cbz	x13, LBB0_27
LBB0_24:                                ;   in Loop: Header=BB0_18 Depth=2
	ld1r.4s	{ v0 }, [x21]
	lsl	x22, x23, #2
	add	x21, x19, x22
	add	x22, x5, x22
	add	x23, x4, x23
LBB0_25:                                ;   Parent Loop BB0_14 Depth=1
                                        ;     Parent Loop BB0_18 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x21], #16
	ldr	q2, [x22]
	fmla.4s	v2, v1, v0
	str	q2, [x22], #16
	adds	x23, x23, #4
	b.ne	LBB0_25
; %bb.26:                               ;   in Loop: Header=BB0_18 Depth=2
	mov	x22, x14
	cmp	x14, x8
	b.eq	LBB0_17
LBB0_27:                                ;   Parent Loop BB0_14 Depth=1
                                        ;     Parent Loop BB0_18 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	s0, [x6, x7, lsl #2]
	ldr	s1, [x19, x22, lsl #2]
	ldr	s2, [x5, x22, lsl #2]
	fmadd	s0, s0, s1, s2
	str	s0, [x5, x22, lsl #2]
	add	x22, x22, #1
	cmp	x8, x22
	b.ne	LBB0_27
	b	LBB0_17
LBB0_28:                                ;   in Loop: Header=BB0_14 Depth=1
	mov	x7, #0                          ; =0x0
	mov	x19, x2
LBB0_29:                                ;   Parent Loop BB0_14 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB0_30 Depth 3
	mov	x20, #0                         ; =0x0
LBB0_30:                                ;   Parent Loop BB0_14 Depth=1
                                        ;     Parent Loop BB0_29 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	s0, [x6, x7, lsl #2]
	ldr	s1, [x19, x20]
	ldr	s2, [x5, x20]
	fmadd	s0, s0, s1, s2
	str	s0, [x5, x20]
	add	x20, x20, #4
	cmp	x16, x20
	b.ne	LBB0_30
; %bb.31:                               ;   in Loop: Header=BB0_29 Depth=2
	add	x7, x7, #1
	add	x19, x19, x16
	cmp	x7, x8
	b.ne	LBB0_29
	b	LBB0_13
LBB0_32:
	ldp	x20, x19, [sp, #32]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp], #48             ; 16-byte Folded Reload
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_blocked                        ; -- Begin function blocked
	.p2align	2
_blocked:                               ; @blocked
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #384
	stp	x28, x27, [sp, #288]            ; 16-byte Folded Spill
	stp	x26, x25, [sp, #304]            ; 16-byte Folded Spill
	stp	x24, x23, [sp, #320]            ; 16-byte Folded Spill
	stp	x22, x21, [sp, #336]            ; 16-byte Folded Spill
	stp	x20, x19, [sp, #352]            ; 16-byte Folded Spill
	stp	x29, x30, [sp, #368]            ; 16-byte Folded Spill
	.cfi_def_cfa_offset 384
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	.cfi_offset w25, -72
	.cfi_offset w26, -80
	.cfi_offset w27, -88
	.cfi_offset w28, -96
	str	x4, [sp, #24]                   ; 8-byte Folded Spill
	str	x3, [sp, #64]                   ; 8-byte Folded Spill
                                        ; kill: def $w1 killed $w1 def $x1
                                        ; kill: def $w0 killed $w0 def $x0
	cmp	w0, #1
	b.lt	LBB1_34
; %bb.1:
	sxtw	x11, w1
	mov	w17, w0
	cmp	w1, #0
	b.le	LBB1_32
; %bb.2:
	mov	x9, #0                          ; =0x0
	mov	x14, #0                         ; =0x0
	mul	x8, x11, x17
	lsl	x12, x8, #2
	sbfiz	x8, x1, #2, #32
	str	x8, [sp, #168]                  ; 8-byte Folded Spill
	ubfiz	x8, x0, #2, #32
	str	x8, [sp, #232]                  ; 8-byte Folded Spill
	ldr	x8, [sp, #64]                   ; 8-byte Folded Reload
	add	x8, x8, #32
	str	x8, [sp, #16]                   ; 8-byte Folded Spill
	lsl	x8, x11, #2
	stp	x8, x11, [sp, #120]             ; 16-byte Folded Spill
	lsl	x16, x17, #2
	ldr	x8, [sp, #24]                   ; 8-byte Folded Reload
	add	x10, x8, #32
	str	x10, [sp, #72]                  ; 8-byte Folded Spill
	str	x8, [sp, #112]                  ; 8-byte Folded Spill
	str	x12, [sp, #40]                  ; 8-byte Folded Spill
	b	LBB1_4
LBB1_3:                                 ;   in Loop: Header=BB1_4 Depth=1
	ldr	x9, [sp, #32]                   ; 8-byte Folded Reload
	add	x9, x9, #1
	ldr	x8, [sp, #72]                   ; 8-byte Folded Reload
	add	x8, x8, x12
	str	x8, [sp, #72]                   ; 8-byte Folded Spill
	ldr	x8, [sp, #112]                  ; 8-byte Folded Reload
	add	x8, x8, x12
	str	x8, [sp, #112]                  ; 8-byte Folded Spill
	mov	x14, x1
	cmp	x1, x17
	b.ge	LBB1_34
LBB1_4:                                 ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB1_6 Depth 2
                                        ;       Child Loop BB1_8 Depth 3
                                        ;         Child Loop BB1_16 Depth 4
                                        ;           Child Loop BB1_21 Depth 5
                                        ;             Child Loop BB1_24 Depth 6
                                        ;             Child Loop BB1_28 Depth 6
                                        ;             Child Loop BB1_31 Depth 6
                                        ;           Child Loop BB1_17 Depth 5
                                        ;             Child Loop BB1_18 Depth 6
                                        ;         Child Loop BB1_9 Depth 4
                                        ;           Child Loop BB1_10 Depth 5
                                        ;             Child Loop BB1_11 Depth 6
	mov	x15, #0                         ; =0x0
	mov	x13, #0                         ; =0x0
	str	x9, [sp, #32]                   ; 8-byte Folded Spill
	mul	x8, x12, x9
	add	x9, x8, #4
	add	x1, x14, x11
	ldr	x10, [sp, #24]                  ; 8-byte Folded Reload
	add	x0, x10, x8
	add	x10, x10, x9
	stp	x10, x0, [sp, #136]             ; 16-byte Folded Spill
	add	x8, x2, x8
	str	x8, [sp, #56]                   ; 8-byte Folded Spill
	add	x8, x2, x9
	str	x8, [sp, #48]                   ; 8-byte Folded Spill
	ldr	x3, [sp, #64]                   ; 8-byte Folded Reload
	ldr	x0, [sp, #16]                   ; 8-byte Folded Reload
	str	x14, [sp, #176]                 ; 8-byte Folded Spill
	b	LBB1_6
LBB1_5:                                 ;   in Loop: Header=BB1_6 Depth=2
	ldp	x0, x15, [sp, #80]              ; 16-byte Folded Reload
	add	x15, x15, #1
	ldr	x12, [sp, #40]                  ; 8-byte Folded Reload
	add	x0, x0, x12
	add	x3, x3, x12
	mov	x13, x25
	cmp	x25, x17
	b.ge	LBB1_3
LBB1_6:                                 ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB1_8 Depth 3
                                        ;         Child Loop BB1_16 Depth 4
                                        ;           Child Loop BB1_21 Depth 5
                                        ;             Child Loop BB1_24 Depth 6
                                        ;             Child Loop BB1_28 Depth 6
                                        ;             Child Loop BB1_31 Depth 6
                                        ;           Child Loop BB1_17 Depth 5
                                        ;             Child Loop BB1_18 Depth 6
                                        ;         Child Loop BB1_9 Depth 4
                                        ;           Child Loop BB1_10 Depth 5
                                        ;             Child Loop BB1_11 Depth 6
	str	xzr, [sp, #224]                 ; 8-byte Folded Spill
	mov	x5, #0                          ; =0x0
	mov	x6, #0                          ; =0x0
	ldr	x8, [sp, #168]                  ; 8-byte Folded Reload
	mul	x8, x8, x15
	add	x9, x13, #1
	add	x25, x13, x11
	cmp	x25, x9
	csinc	x9, x25, x13, gt
	mul	x10, x15, x11
	mvn	x10, x10
	add	x9, x9, x10
	ldr	x10, [sp, #64]                  ; 8-byte Folded Reload
	madd	x12, x12, x15, x10
	ldr	x10, [sp, #232]                 ; 8-byte Folded Reload
	str	x12, [sp, #104]                 ; 8-byte Folded Spill
	madd	x10, x10, x9, x12
	add	x10, x10, #4
	stp	x15, x10, [sp, #88]             ; 16-byte Folded Spill
	ldr	x10, [sp, #56]                  ; 8-byte Folded Reload
	add	x10, x10, x8
	str	x10, [sp, #160]                 ; 8-byte Folded Spill
	ldr	x10, [sp, #48]                  ; 8-byte Folded Reload
	add	x8, x10, x8
	add	x8, x8, x9, lsl #2
	str	x8, [sp, #152]                  ; 8-byte Folded Spill
	mov	x23, x3
	ldr	x15, [sp, #112]                 ; 8-byte Folded Reload
	ldr	x8, [sp, #72]                   ; 8-byte Folded Reload
	str	x8, [sp, #216]                  ; 8-byte Folded Spill
	str	x0, [sp, #80]                   ; 8-byte Folded Spill
	str	x0, [sp, #280]                  ; 8-byte Folded Spill
	mov	w10, #1                         ; =0x1
	mov	x12, x11
	b	LBB1_8
LBB1_7:                                 ;   in Loop: Header=BB1_8 Depth=3
	ldp	x10, x5, [sp, #192]             ; 16-byte Folded Reload
	add	x5, x5, #1
	ldr	x11, [sp, #128]                 ; 8-byte Folded Reload
	ldp	x14, x12, [sp, #176]            ; 16-byte Folded Reload
	add	x12, x12, x11
	add	x10, x10, x11
	ldr	x8, [sp, #224]                  ; 8-byte Folded Reload
	sub	x8, x8, x11
	str	x8, [sp, #224]                  ; 8-byte Folded Spill
	ldr	x8, [sp, #120]                  ; 8-byte Folded Reload
	ldr	x9, [sp, #280]                  ; 8-byte Folded Reload
	add	x9, x9, x8
	str	x9, [sp, #280]                  ; 8-byte Folded Spill
	ldp	x15, x9, [sp, #208]             ; 16-byte Folded Reload
	add	x9, x9, x8
	str	x9, [sp, #216]                  ; 8-byte Folded Spill
	add	x15, x15, x8
	add	x23, x23, x8
	mov	x6, x20
	cmp	x20, x17
	b.ge	LBB1_5
LBB1_8:                                 ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ; =>    This Loop Header: Depth=3
                                        ;         Child Loop BB1_16 Depth 4
                                        ;           Child Loop BB1_21 Depth 5
                                        ;             Child Loop BB1_24 Depth 6
                                        ;             Child Loop BB1_28 Depth 6
                                        ;             Child Loop BB1_31 Depth 6
                                        ;           Child Loop BB1_17 Depth 5
                                        ;             Child Loop BB1_18 Depth 6
                                        ;         Child Loop BB1_9 Depth 4
                                        ;           Child Loop BB1_10 Depth 5
                                        ;             Child Loop BB1_11 Depth 6
	cmp	x12, x10
	stp	x12, x10, [sp, #184]            ; 16-byte Folded Spill
	csel	x9, x12, x10, gt
	add	x8, x6, #1
	add	x20, x6, x11
	cmp	x20, x8
	csinc	x8, x20, x6, gt
	mul	x10, x5, x11
	sub	x4, x8, x10
	ldr	x11, [sp, #168]                 ; 8-byte Folded Reload
	stp	x5, x15, [sp, #200]             ; 16-byte Folded Spill
	mul	x11, x11, x5
	mvn	x10, x10
	add	x8, x8, x10
	lsl	x12, x8, #2
	ldr	x8, [sp, #136]                  ; 8-byte Folded Reload
	add	x8, x8, x11
	add	x10, x8, x12
	ldr	x8, [sp, #144]                  ; 8-byte Folded Reload
	add	x0, x8, x11
	ldr	x8, [sp, #152]                  ; 8-byte Folded Reload
	stp	x0, x10, [sp, #264]             ; 16-byte Folded Spill
	cmp	x0, x8
	ldr	x8, [sp, #160]                  ; 8-byte Folded Reload
	ccmp	x8, x10, #2, lo
	cset	w8, lo
	str	w8, [sp, #260]                  ; 4-byte Folded Spill
	mov	x8, x15
	mov	x10, x14
	cmp	x4, #4
	b.hs	LBB1_14
LBB1_9:                                 ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ; =>      This Loop Header: Depth=4
                                        ;           Child Loop BB1_10 Depth 5
                                        ;             Child Loop BB1_11 Depth 6
	mul	x9, x10, x17
	add	x9, x2, x9, lsl #2
	mov	x11, x23
	mov	x12, x13
LBB1_10:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_9 Depth=4
                                        ; =>        This Loop Header: Depth=5
                                        ;             Child Loop BB1_11 Depth 6
	mov	x14, #0                         ; =0x0
LBB1_11:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_9 Depth=4
                                        ;           Parent Loop BB1_10 Depth=5
                                        ; =>          This Inner Loop Header: Depth=6
	ldr	s0, [x9, x12, lsl #2]
	ldr	s1, [x11, x14, lsl #2]
	ldr	s2, [x8, x14, lsl #2]
	fmadd	s0, s0, s1, s2
	str	s0, [x8, x14, lsl #2]
	add	x14, x14, #1
	add	x15, x6, x14
	cmp	x15, x20
	b.lt	LBB1_11
; %bb.12:                               ;   in Loop: Header=BB1_10 Depth=5
	add	x12, x12, #1
	add	x11, x11, x16
	cmp	x12, x25
	b.lt	LBB1_10
; %bb.13:                               ;   in Loop: Header=BB1_9 Depth=4
	add	x10, x10, #1
	add	x8, x8, x16
	cmp	x10, x1
	b.lt	LBB1_9
	b	LBB1_7
LBB1_14:                                ;   in Loop: Header=BB1_8 Depth=3
	mov	x7, #0                          ; =0x0
	ldr	x8, [sp, #224]                  ; 8-byte Folded Reload
	add	x8, x9, x8
	and	x9, x8, #0xfffffffffffffffc
	neg	x19, x9
	and	x10, x8, #0xfffffffffffffff0
	ldp	x8, x27, [sp, #104]             ; 16-byte Folded Reload
	add	x8, x8, x11
	str	x8, [sp, #248]                  ; 8-byte Folded Spill
	ldr	x8, [sp, #96]                   ; 8-byte Folded Reload
	add	x8, x8, x11
	add	x8, x8, x12
	str	x8, [sp, #240]                  ; 8-byte Folded Spill
	and	x15, x4, #0xfffffffffffffff0
	and	x22, x4, #0xc
	and	x26, x4, #0xfffffffffffffffc
	ldp	x0, x9, [sp, #208]              ; 16-byte Folded Reload
	ldr	x11, [sp, #176]                 ; 8-byte Folded Reload
	b	LBB1_16
LBB1_15:                                ;   in Loop: Header=BB1_16 Depth=4
	add	x11, x11, #1
	add	x7, x7, #1
	add	x9, x9, x16
	add	x27, x27, x16
	add	x0, x0, x16
	cmp	x11, x1
	mov	x17, x5
	mov	x2, x30
	mov	x3, x28
	b.ge	LBB1_7
LBB1_16:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ; =>      This Loop Header: Depth=4
                                        ;           Child Loop BB1_21 Depth 5
                                        ;             Child Loop BB1_24 Depth 6
                                        ;             Child Loop BB1_28 Depth 6
                                        ;             Child Loop BB1_31 Depth 6
                                        ;           Child Loop BB1_17 Depth 5
                                        ;             Child Loop BB1_18 Depth 6
	ldr	x8, [sp, #232]                  ; 8-byte Folded Reload
	mul	x12, x8, x7
	ldr	x8, [sp, #272]                  ; 8-byte Folded Reload
	add	x14, x8, x12
	mov	x5, x17
	mul	x8, x11, x17
	mov	x30, x2
	add	x8, x2, x8, lsl #2
	ldr	x17, [sp, #264]                 ; 8-byte Folded Reload
	add	x12, x17, x12
	ldr	x17, [sp, #240]                 ; 8-byte Folded Reload
	cmp	x12, x17
	ldr	x12, [sp, #248]                 ; 8-byte Folded Reload
	ccmp	x12, x14, #2, lo
	ldr	w12, [sp, #260]                 ; 4-byte Folded Reload
	csinc	w17, w12, wzr, hs
	mov	x28, x3
	ldr	x2, [sp, #280]                  ; 8-byte Folded Reload
	mov	x21, x13
	mov	x12, x23
	mov	x14, x13
	tbz	w17, #0, LBB1_21
LBB1_17:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ; =>        This Loop Header: Depth=5
                                        ;             Child Loop BB1_18 Depth 6
	mov	x17, #0                         ; =0x0
LBB1_18:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ;           Parent Loop BB1_17 Depth=5
                                        ; =>          This Inner Loop Header: Depth=6
	ldr	s0, [x8, x14, lsl #2]
	ldr	s1, [x12, x17, lsl #2]
	ldr	s2, [x0, x17, lsl #2]
	fmadd	s0, s0, s1, s2
	str	s0, [x0, x17, lsl #2]
	add	x17, x17, #1
	add	x2, x6, x17
	cmp	x2, x20
	b.lt	LBB1_18
; %bb.19:                               ;   in Loop: Header=BB1_17 Depth=5
	add	x14, x14, #1
	add	x12, x12, x16
	cmp	x14, x25
	b.lt	LBB1_17
	b	LBB1_15
LBB1_20:                                ;   in Loop: Header=BB1_21 Depth=5
	add	x21, x21, #1
	add	x2, x2, x16
	add	x3, x3, x16
	cmp	x21, x25
	b.ge	LBB1_15
LBB1_21:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ; =>        This Loop Header: Depth=5
                                        ;             Child Loop BB1_24 Depth 6
                                        ;             Child Loop BB1_28 Depth 6
                                        ;             Child Loop BB1_31 Depth 6
	add	x12, x8, x21, lsl #2
	cmp	x4, #16
	b.hs	LBB1_23
; %bb.22:                               ;   in Loop: Header=BB1_21 Depth=5
	mov	x14, #0                         ; =0x0
	b	LBB1_27
LBB1_23:                                ;   in Loop: Header=BB1_21 Depth=5
	ld1r.4s	{ v0 }, [x12]
	mov	x17, x9
	mov	x24, x2
	mov	x14, x10
LBB1_24:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ;           Parent Loop BB1_21 Depth=5
                                        ; =>          This Inner Loop Header: Depth=6
	ldp	q1, q2, [x24, #-32]
	ldp	q3, q4, [x24], #64
	ldp	q5, q6, [x17, #-32]
	ldp	q7, q16, [x17]
	fmla.4s	v5, v1, v0
	fmla.4s	v6, v2, v0
	fmla.4s	v7, v3, v0
	fmla.4s	v16, v4, v0
	stp	q5, q6, [x17, #-32]
	stp	q7, q16, [x17], #64
	subs	x14, x14, #16
	b.ne	LBB1_24
; %bb.25:                               ;   in Loop: Header=BB1_21 Depth=5
	cmp	x4, x15
	b.eq	LBB1_20
; %bb.26:                               ;   in Loop: Header=BB1_21 Depth=5
	mov	x14, x15
	mov	x17, x15
	cbz	x22, LBB1_30
LBB1_27:                                ;   in Loop: Header=BB1_21 Depth=5
	ld1r.4s	{ v0 }, [x12]
	add	x12, x19, x14
	add	x14, x6, x14
	lsl	x17, x14, #2
	add	x14, x27, x17
	add	x17, x3, x17
LBB1_28:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ;           Parent Loop BB1_21 Depth=5
                                        ; =>          This Inner Loop Header: Depth=6
	ldr	q1, [x17], #16
	ldr	q2, [x14]
	fmla.4s	v2, v1, v0
	str	q2, [x14], #16
	adds	x12, x12, #4
	b.ne	LBB1_28
; %bb.29:                               ;   in Loop: Header=BB1_21 Depth=5
	mov	x17, x26
	cmp	x4, x26
	b.eq	LBB1_20
LBB1_30:                                ;   in Loop: Header=BB1_21 Depth=5
	add	x12, x6, x17
LBB1_31:                                ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_6 Depth=2
                                        ;       Parent Loop BB1_8 Depth=3
                                        ;         Parent Loop BB1_16 Depth=4
                                        ;           Parent Loop BB1_21 Depth=5
                                        ; =>          This Inner Loop Header: Depth=6
	ldr	s0, [x8, x21, lsl #2]
	ldr	s1, [x3, x12, lsl #2]
	ldr	s2, [x27, x12, lsl #2]
	fmadd	s0, s0, s1, s2
	str	s0, [x27, x12, lsl #2]
	add	x12, x12, #1
	cmp	x12, x20
	b.lt	LBB1_31
	b	LBB1_20
LBB1_32:
	mov	x8, #0                          ; =0x0
LBB1_33:                                ; =>This Inner Loop Header: Depth=1
	add	x8, x8, x11
	cmp	x8, x17
	b.lt	LBB1_33
LBB1_34:
	ldp	x29, x30, [sp, #368]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #352]            ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #336]            ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #320]            ; 16-byte Folded Reload
	ldp	x26, x25, [sp, #304]            ; 16-byte Folded Reload
	ldp	x28, x27, [sp, #288]            ; 16-byte Folded Reload
	add	sp, sp, #384
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_vectorized                     ; -- Begin function vectorized
	.p2align	2
_vectorized:                            ; @vectorized
	.cfi_startproc
; %bb.0:
                                        ; kill: def $w0 killed $w0 def $x0
	cmp	w0, #1
	b.lt	LBB2_7
; %bb.1:
	mov	x8, #0                          ; =0x0
	ubfiz	x9, x0, #2, #32
	mov	w10, w0
LBB2_2:                                 ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB2_3 Depth 2
                                        ;       Child Loop BB2_4 Depth 3
	mov	x11, #0                         ; =0x0
	mul	x12, x8, x10
	add	x12, x1, x12, lsl #2
	mov	x13, x2
LBB2_3:                                 ;   Parent Loop BB2_2 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB2_4 Depth 3
	mov	x14, #0                         ; =0x0
	ldr	s0, [x12, x11, lsl #2]
	mov	x15, x13
	mov	x16, x3
LBB2_4:                                 ;   Parent Loop BB2_2 Depth=1
                                        ;     Parent Loop BB2_3 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x15], #16
	ldr	q2, [x16]
	fmla.4s	v2, v1, v0[0]
	str	q2, [x16], #16
	add	x14, x14, #4
	cmp	x14, x10
	b.lo	LBB2_4
; %bb.5:                                ;   in Loop: Header=BB2_3 Depth=2
	add	x11, x11, #1
	add	x13, x13, x9
	cmp	x11, x10
	b.ne	LBB2_3
; %bb.6:                                ;   in Loop: Header=BB2_2 Depth=1
	add	x8, x8, #1
	add	x3, x3, x9
	cmp	x8, x10
	b.ne	LBB2_2
LBB2_7:
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_cmp_double                     ; -- Begin function cmp_double
	.p2align	2
_cmp_double:                            ; @cmp_double
	.cfi_startproc
; %bb.0:
	ldr	d0, [x0]
	ldr	d1, [x1]
	fcmp	d0, d1
	cset	w8, gt
	cset	w9, mi
	sub	w0, w8, w9
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_main                           ; -- Begin function main
	.p2align	2
_main:                                  ; @main
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #128
	stp	d11, d10, [sp, #32]             ; 16-byte Folded Spill
	stp	d9, d8, [sp, #48]               ; 16-byte Folded Spill
	stp	x24, x23, [sp, #64]             ; 16-byte Folded Spill
	stp	x22, x21, [sp, #80]             ; 16-byte Folded Spill
	stp	x20, x19, [sp, #96]             ; 16-byte Folded Spill
	stp	x29, x30, [sp, #112]            ; 16-byte Folded Spill
	add	x29, sp, #112
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	.cfi_offset w21, -40
	.cfi_offset w22, -48
	.cfi_offset w23, -56
	.cfi_offset w24, -64
	.cfi_offset b8, -72
	.cfi_offset b9, -80
	.cfi_offset b10, -88
	.cfi_offset b11, -96
	mov	w0, #16777216                   ; =0x1000000
	bl	_malloc
	mov	x19, x0
	mov	w0, #16777216                   ; =0x1000000
	bl	_malloc
	mov	x20, x0
	mov	w0, #16777216                   ; =0x1000000
	bl	_malloc
	cbz	x19, LBB4_40
; %bb.1:
	cbz	x20, LBB4_40
; %bb.2:
	mov	x21, x0
	cbz	x0, LBB4_40
; %bb.3:
	mov	x8, #0                          ; =0x0
	mov	x9, #0                          ; =0x0
	mov	w10, #1                         ; =0x1
	mov	w11, #3                         ; =0x3
	mov	w12, #2                         ; =0x2
	mov	w13, #51367                     ; =0xc8a7
	movk	w13, #56679, lsl #16
	mov	w14, #-74                       ; =0xffffffb6
	mov	w15, #34079                     ; =0x851f
	movk	w15, #20971, lsl #16
	mov	w16, #-100                      ; =0xffffff9c
	mov	w17, #74                        ; =0x4a
LBB4_4:                                 ; =>This Inner Loop Header: Depth=1
	ubfx	x0, x9, #1, #31
	umull	x0, w0, w13
	lsr	x0, x0, #37
	mov	w1, w9
	umull	x1, w1, w15
	lsr	x1, x1, #37
	lsr	w2, w10, #1
	umull	x2, w2, w13
	lsr	x2, x2, #37
	lsr	w3, w11, #1
	umull	x3, w3, w13
	lsr	x3, x3, #37
	lsr	w4, w12, #1
	umull	x4, w4, w13
	lsr	x4, x4, #37
	madd	w4, w4, w14, w9
	madd	w1, w1, w16, w9
	add	w5, w1, #1
	add	w6, w1, #2
	add	w7, w1, #3
	ucvtf	s0, w1
	ucvtf	s1, w5
	ucvtf	s2, w6
	ucvtf	s3, w7
	add	x1, x19, x8
	stp	s0, s1, [x1]
	madd	w0, w0, w14, w9
	madd	w2, w2, w14, w9
	add	w2, w2, #1
	add	w4, w4, #2
	msub	w3, w3, w17, w9
	ucvtf	s0, w0
	ucvtf	s1, w2
	add	w0, w3, #3
	ucvtf	s4, w4
	ucvtf	s5, w0
	stp	s2, s3, [x1, #8]
	add	x0, x20, x8
	stp	s0, s1, [x0]
	add	x9, x9, #4
	add	w10, w10, #4
	stp	s4, s5, [x0, #8]
	add	x8, x8, #16
	add	w11, w11, #4
	add	w12, w12, #4
	cmp	x9, #1024, lsl #12              ; =4194304
	b.ne	LBB4_4
; %bb.5:
	mov	w0, #80                         ; =0x50
	bl	_malloc
	mov	x22, x0
	mov	x0, x21
	mov	w1, #16777216                   ; =0x1000000
	bl	_bzero
	mov	x8, #0                          ; =0x0
	mov	x9, x21
LBB4_6:                                 ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB4_7 Depth 2
                                        ;       Child Loop BB4_8 Depth 3
	mov	x10, #0                         ; =0x0
	add	x11, x19, x8, lsl #13
	mov	x12, x20
LBB4_7:                                 ;   Parent Loop BB4_6 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB4_8 Depth 3
	ldr	s0, [x11, x10, lsl #2]
	mov	x13, #-4                        ; =0xfffffffffffffffc
	mov	x14, x12
	mov	x15, x9
LBB4_8:                                 ;   Parent Loop BB4_6 Depth=1
                                        ;     Parent Loop BB4_7 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x14], #16
	ldr	q2, [x15]
	fmla.4s	v2, v1, v0[0]
	str	q2, [x15], #16
	add	x13, x13, #4
	cmp	x13, #2044
	b.lo	LBB4_8
; %bb.9:                                ;   in Loop: Header=BB4_7 Depth=2
	add	x10, x10, #1
	add	x12, x12, #2, lsl #12           ; =8192
	cmp	x10, #2048
	b.ne	LBB4_7
; %bb.10:                               ;   in Loop: Header=BB4_6 Depth=1
	add	x8, x8, #1
	add	x9, x9, #2, lsl #12             ; =8192
	cmp	x8, #2048
	b.ne	LBB4_6
; %bb.11:
	mov	x8, #0                          ; =0x0
	mov	x9, x21
LBB4_12:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB4_13 Depth 2
                                        ;       Child Loop BB4_14 Depth 3
	mov	x10, #0                         ; =0x0
	add	x11, x19, x8, lsl #13
	mov	x12, x20
LBB4_13:                                ;   Parent Loop BB4_12 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB4_14 Depth 3
	ldr	s0, [x11, x10, lsl #2]
	mov	x13, #-4                        ; =0xfffffffffffffffc
	mov	x14, x12
	mov	x15, x9
LBB4_14:                                ;   Parent Loop BB4_12 Depth=1
                                        ;     Parent Loop BB4_13 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x14], #16
	ldr	q2, [x15]
	fmla.4s	v2, v1, v0[0]
	str	q2, [x15], #16
	add	x13, x13, #4
	cmp	x13, #2044
	b.lo	LBB4_14
; %bb.15:                               ;   in Loop: Header=BB4_13 Depth=2
	add	x10, x10, #1
	add	x12, x12, #2, lsl #12           ; =8192
	cmp	x10, #2048
	b.ne	LBB4_13
; %bb.16:                               ;   in Loop: Header=BB4_12 Depth=1
	add	x8, x8, #1
	add	x9, x9, #2, lsl #12             ; =8192
	cmp	x8, #2048
	b.ne	LBB4_12
; %bb.17:
	mov	x8, #0                          ; =0x0
	mov	x9, x21
LBB4_18:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB4_19 Depth 2
                                        ;       Child Loop BB4_20 Depth 3
	mov	x10, #0                         ; =0x0
	add	x11, x19, x8, lsl #13
	mov	x12, x20
LBB4_19:                                ;   Parent Loop BB4_18 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB4_20 Depth 3
	ldr	s0, [x11, x10, lsl #2]
	mov	x13, #-4                        ; =0xfffffffffffffffc
	mov	x14, x12
	mov	x15, x9
LBB4_20:                                ;   Parent Loop BB4_18 Depth=1
                                        ;     Parent Loop BB4_19 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x14], #16
	ldr	q2, [x15]
	fmla.4s	v2, v1, v0[0]
	str	q2, [x15], #16
	add	x13, x13, #4
	cmp	x13, #2044
	b.lo	LBB4_20
; %bb.21:                               ;   in Loop: Header=BB4_19 Depth=2
	add	x10, x10, #1
	add	x12, x12, #2, lsl #12           ; =8192
	cmp	x10, #2048
	b.ne	LBB4_19
; %bb.22:                               ;   in Loop: Header=BB4_18 Depth=1
	add	x8, x8, #1
	add	x9, x9, #2, lsl #12             ; =8192
	cmp	x8, #2048
	b.ne	LBB4_18
; %bb.23:
	mov	x8, #0                          ; =0x0
	mov	x9, x21
LBB4_24:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB4_25 Depth 2
                                        ;       Child Loop BB4_26 Depth 3
	mov	x10, #0                         ; =0x0
	add	x11, x19, x8, lsl #13
	mov	x12, x20
LBB4_25:                                ;   Parent Loop BB4_24 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB4_26 Depth 3
	ldr	s0, [x11, x10, lsl #2]
	mov	x13, #-4                        ; =0xfffffffffffffffc
	mov	x14, x12
	mov	x15, x9
LBB4_26:                                ;   Parent Loop BB4_24 Depth=1
                                        ;     Parent Loop BB4_25 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	ldr	q1, [x14], #16
	ldr	q2, [x15]
	fmla.4s	v2, v1, v0[0]
	str	q2, [x15], #16
	add	x13, x13, #4
	cmp	x13, #2044
	b.lo	LBB4_26
; %bb.27:                               ;   in Loop: Header=BB4_25 Depth=2
	add	x10, x10, #1
	add	x12, x12, #2, lsl #12           ; =8192
	cmp	x10, #2048
	b.ne	LBB4_25
; %bb.28:                               ;   in Loop: Header=BB4_24 Depth=1
	add	x8, x8, #1
	add	x9, x9, #2, lsl #12             ; =8192
	cmp	x8, #2048
	b.ne	LBB4_24
; %bb.29:
	mov	x23, #0                         ; =0x0
	mov	x8, #54933                      ; =0xd695
	movk	x8, #59430, lsl #16
	movk	x8, #11787, lsl #32
	movk	x8, #15889, lsl #48
	fmov	d8, x8
LBB4_30:                                ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB4_31 Depth 2
                                        ;       Child Loop BB4_32 Depth 3
                                        ;         Child Loop BB4_33 Depth 4
	mov	x0, x21
	mov	w1, #16777216                   ; =0x1000000
	bl	_bzero
	add	x1, sp, #16
	mov	w0, #6                          ; =0x6
	bl	_clock_gettime
	mov	x9, #0                          ; =0x0
	ldr	x8, [sp, #16]
	ldr	d0, [sp, #24]
	scvtf	d0, d0
	mov	x10, x21
LBB4_31:                                ;   Parent Loop BB4_30 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB4_32 Depth 3
                                        ;         Child Loop BB4_33 Depth 4
	mov	x11, #0                         ; =0x0
	add	x12, x19, x9, lsl #13
	mov	x13, x20
LBB4_32:                                ;   Parent Loop BB4_30 Depth=1
                                        ;     Parent Loop BB4_31 Depth=2
                                        ; =>    This Loop Header: Depth=3
                                        ;         Child Loop BB4_33 Depth 4
	ldr	s1, [x12, x11, lsl #2]
	mov	x14, #-4                        ; =0xfffffffffffffffc
	mov	x15, x13
	mov	x16, x10
LBB4_33:                                ;   Parent Loop BB4_30 Depth=1
                                        ;     Parent Loop BB4_31 Depth=2
                                        ;       Parent Loop BB4_32 Depth=3
                                        ; =>      This Inner Loop Header: Depth=4
	ldr	q2, [x15], #16
	ldr	q3, [x16]
	fmla.4s	v3, v2, v1[0]
	str	q3, [x16], #16
	add	x14, x14, #4
	cmp	x14, #2044
	b.lo	LBB4_33
; %bb.34:                               ;   in Loop: Header=BB4_32 Depth=3
	add	x11, x11, #1
	add	x13, x13, #2, lsl #12           ; =8192
	cmp	x11, #2048
	b.ne	LBB4_32
; %bb.35:                               ;   in Loop: Header=BB4_31 Depth=2
	add	x9, x9, #1
	add	x10, x10, #2, lsl #12           ; =8192
	cmp	x9, #2048
	b.ne	LBB4_31
; %bb.36:                               ;   in Loop: Header=BB4_30 Depth=1
	scvtf	d1, x8
	fmadd	d9, d0, d8, d1
	add	x1, sp, #16
	mov	w0, #6                          ; =0x6
	bl	_clock_gettime
	ldp	d0, d1, [sp, #16]
	scvtf	d0, d0
	scvtf	d1, d1
	fmadd	d0, d1, d8, d0
	fsub	d0, d0, d9
	str	d0, [x22, x23, lsl #3]
	add	x23, x23, #1
	cmp	x23, #10
	b.ne	LBB4_30
; %bb.37:
	add	x8, x21, #32
	movi	d8, #0000000000000000
	mov	w9, #4194304                    ; =0x400000
LBB4_38:                                ; =>This Inner Loop Header: Depth=1
	ldp	q0, q1, [x8, #-32]
	ldp	q2, q3, [x8], #64
	fcvtl2	v4.2d, v0.4s
	mov	d5, v4[1]
	fcvtl	v0.2d, v0.2s
	mov	d6, v0[1]
	fcvtl2	v7.2d, v1.4s
	mov	d16, v7[1]
	fcvtl	v1.2d, v1.2s
	mov	d17, v1[1]
	fcvtl2	v18.2d, v2.4s
	mov	d19, v18[1]
	fcvtl	v2.2d, v2.2s
	mov	d20, v2[1]
	fcvtl2	v21.2d, v3.4s
	mov	d22, v21[1]
	fcvtl	v3.2d, v3.2s
	mov	d23, v3[1]
	fadd	d0, d8, d0
	fadd	d0, d0, d6
	fadd	d0, d0, d4
	fadd	d0, d0, d5
	fadd	d0, d0, d1
	fadd	d0, d0, d17
	fadd	d0, d0, d7
	fadd	d0, d0, d16
	fadd	d0, d0, d2
	fadd	d0, d0, d20
	fadd	d0, d0, d18
	fadd	d0, d0, d19
	fadd	d0, d0, d3
	fadd	d0, d0, d23
	fadd	d0, d0, d21
	fadd	d8, d0, d22
	subs	x9, x9, #16
	b.ne	LBB4_38
; %bb.39:
Lloh0:
	adrp	x3, _cmp_double@PAGE
Lloh1:
	add	x3, x3, _cmp_double@PAGEOFF
	mov	x0, x22
	mov	w1, #10                         ; =0xa
	mov	w2, #8                          ; =0x8
	bl	_qsort
	ldr	d9, [x22, #40]
	mov	x8, #279275953455104            ; =0xfe0000000000
	movk	x8, #16911, lsl #48
	fmov	d0, x8
	fdiv	d0, d0, d9
	mov	x8, #225833675390976            ; =0xcd6500000000
	movk	x8, #16845, lsl #48
	fmov	d1, x8
	fdiv	d10, d0, d1
	ldr	d0, [x22, #56]
	ldr	d1, [x22, #16]
	fsub	d11, d0, d1
	str	d8, [sp]
Lloh2:
	adrp	x22, l_.str.1@PAGE
Lloh3:
	add	x22, x22, l_.str.1@PAGEOFF
	mov	x0, x22
	bl	_printf
	mov	w8, #2048                       ; =0x800
	str	x8, [sp]
Lloh4:
	adrp	x0, l_.str.2@PAGE
Lloh5:
	add	x0, x0, l_.str.2@PAGEOFF
	bl	_printf
	str	d9, [sp]
Lloh6:
	adrp	x0, l_.str.3@PAGE
Lloh7:
	add	x0, x0, l_.str.3@PAGEOFF
	bl	_printf
	str	d11, [sp]
Lloh8:
	adrp	x0, l_.str.4@PAGE
Lloh9:
	add	x0, x0, l_.str.4@PAGEOFF
	bl	_printf
	str	d10, [sp]
Lloh10:
	adrp	x0, l_.str.5@PAGE
Lloh11:
	add	x0, x0, l_.str.5@PAGEOFF
	bl	_printf
	mov	x8, #158329674399744            ; =0x900000000000
	movk	x8, #16534, lsl #48
	fmov	d0, x8
	fdiv	d0, d10, d0
	mov	x8, #4636737291354636288        ; =0x4059000000000000
	fmov	d1, x8
	fmul	d0, d0, d1
	str	d0, [sp]
Lloh12:
	adrp	x0, l_.str.6@PAGE
Lloh13:
	add	x0, x0, l_.str.6@PAGEOFF
	bl	_printf
	str	d8, [sp]
	mov	x0, x22
	bl	_printf
	mov	x0, x19
	bl	_free
	mov	x0, x20
	bl	_free
	mov	x0, x21
	bl	_free
	mov	w0, #0                          ; =0x0
	b	LBB4_41
LBB4_40:
Lloh14:
	adrp	x0, l_str@PAGE
Lloh15:
	add	x0, x0, l_str@PAGEOFF
	bl	_puts
	mov	w0, #1                          ; =0x1
LBB4_41:
	ldp	x29, x30, [sp, #112]            ; 16-byte Folded Reload
	ldp	x20, x19, [sp, #96]             ; 16-byte Folded Reload
	ldp	x22, x21, [sp, #80]             ; 16-byte Folded Reload
	ldp	x24, x23, [sp, #64]             ; 16-byte Folded Reload
	ldp	d9, d8, [sp, #48]               ; 16-byte Folded Reload
	ldp	d11, d10, [sp, #32]             ; 16-byte Folded Reload
	add	sp, sp, #128
	ret
	.loh AdrpAdd	Lloh12, Lloh13
	.loh AdrpAdd	Lloh10, Lloh11
	.loh AdrpAdd	Lloh8, Lloh9
	.loh AdrpAdd	Lloh6, Lloh7
	.loh AdrpAdd	Lloh4, Lloh5
	.loh AdrpAdd	Lloh2, Lloh3
	.loh AdrpAdd	Lloh0, Lloh1
	.loh AdrpAdd	Lloh14, Lloh15
	.cfi_endproc
                                        ; -- End function
	.section	__TEXT,__cstring,cstring_literals
l_.str.1:                               ; @.str.1
	.asciz	"checksum: %f\n"

l_.str.2:                               ; @.str.2
	.asciz	"n = %d\n"

l_.str.3:                               ; @.str.3
	.asciz	"median time: %f s\n"

l_.str.4:                               ; @.str.4
	.asciz	"IQR: %f s\n"

l_.str.5:                               ; @.str.5
	.asciz	"GFLOP/s: %f\n"

l_.str.6:                               ; @.str.6
	.asciz	"%% of peak: %f\n"

l_str:                                  ; @str
	.asciz	"error"

.subsections_via_symbols
