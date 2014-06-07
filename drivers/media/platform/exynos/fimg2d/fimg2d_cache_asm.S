#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/asm-offsets.h>
#include <asm/assembler.h>
#include <asm/unwind.h>

/*
 * Inner cache maintenance opeations for the user's area
 * Copied from arch/arm/mm/cache-v7.S
 */


ENTRY(fimg2d_flush_cache_range)
	mrc	p15, 0, r3, c0, c0, 1		@ read ctr
	lsr	r3, r3, #16
	and	r3, r3, #0xf		@ cache line size encoding
	mov	r2, #4			@ bytes per word
	mov	r2, r2, lsl r3		@ actual cache line size
	add	r1, r0, r1
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c14, 1
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
2:
	b	1b
ENDPROC(fimg2d_flush_cache_range)
.pushsection __ex_table, "a"
	.long	1b, 2
.popsection


ENTRY(fimg2d_touch_range)
	add r1, r0, r1
	bic r0, r0, #0xFF
	bic r0, r0, #0xF00
1:
	ldr	r3, [r0]
	add	r0, r0, #4096
	cmp	r0, r1
	blo	1b
	mov	pc, lr
2:
	b	1b
ENDPROC(fimg2d_touch_range)
.pushsection __ex_table, "a"
	.long	1b, 2b
.popsection
