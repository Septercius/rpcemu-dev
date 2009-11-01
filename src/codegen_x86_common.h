#ifndef CODEGEN_X86_COMMON_H
#define CODEGEN_X86_COMMON_H

#include <stddef.h>
#include <stdint.h>

/* x86 registers */
#define EAX	0
#define ECX	1
#define EDX	2
#define EBX	3
#define ESP	4
#define EBP	5
#define ESI	6
#define EDI	7

/* Basic x86 operations (bitfields for instruction encoding) */
#define X86_OP_ADD	0x00
#define X86_OP_OR	0x08
#define X86_OP_ADC	0x10	/* Add with carry */
#define X86_OP_SBB	0x18	/* Subtract with borrow */
#define X86_OP_AND	0x20
#define X86_OP_SUB	0x28
#define X86_OP_XOR	0x30
#define X86_OP_CMP	0x38

/* x86 Condition Codes (for conditional instructions) */
#define CC_C		0x2	/* Carry (CF=1) */
#define CC_NC		0x3	/* Not Carry (CF=0) */
#define CC_Z		0x4	/* Zero (ZF=1) */
#define CC_NZ		0x5	/* Not Zero (ZF=0) */
#define CC_S		0x8	/* Sign (SF=1) */
#define CC_NS		0x9	/* Not Sign (SF=0) */

#define CC_E		CC_Z	/* Equal */
#define CC_NE		CC_NZ	/* Not Equal */

#define CC_ALWAYS	-1	/* Unconditional (use with helper functions) */

/**
 * Store a 32-bit relative address at the current code generation position.
 * The offset is calculated relative to the current position.
 *
 * @param addr Pointer to the address to be stored relative to current position
 */
static inline void
addrel32(const void *addr)
{
	ptrdiff_t rel = ((const char *) addr) -
	                ((const char *) &rcodeblock[blockpoint2][codeblockpos]);

	addlong((uint32_t) (rel - 4));
}

#define gen_x86_call(addr)	addbyte(0xe8); addrel32(addr)
#define gen_x86_lahf()		addbyte(0x9f)
#define gen_x86_ret()		addbyte(0xc3)

/**
 * Generate a jump instruction (optionally conditional) where the destination
 * is not yet known because the jump is forward.
 *
 * The jump generated will have a 8-bit displacement allowing a range of
 * +/- 127 bytes, so this function should only be used when the jump will not
 * exceed that range.
 *
 * The jump must be completed by using the function gen_x86_jump_here() at the
 * destination of this jump.
 *
 * @param condition Jump condition (or CC_ALWAYS for unconditional)
 * @return Position of jump offset, which is passed to gen_x86_jump_here()
 */
static inline int
gen_x86_jump_forward(int condition)
{
	int jump_offset_pos;

	if (condition == CC_ALWAYS) {
		addbyte(0xeb);
	} else {
		addbyte(0x70 | condition);
	}
	jump_offset_pos = codeblockpos;
	codeblockpos++;

	return jump_offset_pos;
}

/**
 * Generate a jump instruction (optionally conditional) where the destination
 * is not yet known because the jump is forward.
 *
 * The jump generated will have a 32-bit displacement allowing a range of
 * +/- 2^31 bytes.
 *
 * The jump must be completed by using the function gen_x86_jump_here_long() at
 * the destination of this jump.
 *
 * @param condition Jump condition (or CC_ALWAYS for unconditional)
 * @return Position of jump offset, which is passed to gen_x86_jump_here_long()
 */
static inline int
gen_x86_jump_forward_long(int condition)
{
	int jump_offset_pos;

	if (condition == CC_ALWAYS) {
		addbyte(0xe9);
	} else {
		addbyte(0x0f);
		addbyte(0x80 | condition);
	}
	jump_offset_pos = codeblockpos;
	codeblockpos += 4;

	return jump_offset_pos;
}

/**
 * Complete a previous forward jump by making the destination of the jump the
 * current code generation position.
 *
 * The forward jump must have a 8-bit displacement.
 *
 * @param jump_offset_pos Position of jump offset obtained from
 *                        gen_x86_jump_forward()
 */
static inline void
gen_x86_jump_here(int jump_offset_pos)
{
	int rel = codeblockpos - jump_offset_pos;

	rcodeblock[blockpoint2][jump_offset_pos] = (uint8_t) (rel - 1);
}

/**
 * Complete a previous forward jump by making the destination of the jump the
 * current code generation position.
 *
 * The forward jump must have a 32-bit displacement.
 *
 * @param jump_offset_pos Position of jump offset obtained from
 *                        gen_x86_jump_forward_long()
 */
static inline void
gen_x86_jump_here_long(int jump_offset_pos)
{
	int rel = codeblockpos - jump_offset_pos;

	*((uint32_t *) &rcodeblock[blockpoint2][jump_offset_pos]) =
	    (uint32_t) (rel - 4);
}

/**
 * Generate a jump instruction to the given destination (specified as a
 * position within the current block). The jump can optionally be made
 * conditional. A more compact instruction encoding will be used if possible.
 *
 * @param condition Optional condition to be applied to the jump, or CC_ALWAYS
 *                  for unconditional jump
 * @param destination Destination for jump specified as a position within the
 *                    current block
 */
static inline void
gen_x86_jump(int condition, int destination)
{
	int rel = destination - codeblockpos;

	if (rel > -124 && rel < 124) {
		/* 8-bit signed displacement */
		if (condition == CC_ALWAYS) {
			addbyte(0xeb);
		} else {
			addbyte(0x70 | condition);
		}
		addbyte((uint8_t) ((destination - codeblockpos) - 1));
	} else {
		/* 32-bit signed displacement */
		if (condition == CC_ALWAYS) {
			addbyte(0xe9);
		} else {
			addbyte(0x0f);
			addbyte(0x80 | condition);
		}
		addlong((uint32_t) ((destination - codeblockpos) - 4));
	}
}

#endif /* CODEGEN_X86_COMMON_H */

