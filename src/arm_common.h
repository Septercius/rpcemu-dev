#ifndef ARM_COMMON_H
#define ARM_COMMON_H

/** Evaulate to non-zero if 'mode' is a 32-bit mode */
#define ARM_MODE_32(mode)	((mode) & 0x10)

/** Evaluate to non-zero if 'mode' is a privileged mode */
#define ARM_MODE_PRIV(mode)	((mode) & 0xf)

#define checkneg(v)	(v & 0x80000000)
#define checkpos(v)	(!(v & 0x80000000))

/** A table used by MSR instructions to determine which fields can be modified
    within a PSR */
static const uint32_t msrlookup[16] = {
	0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
	0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
	0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
	0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff
};

static inline void
setadd(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags = 0;

	if (result == 0) {
		flags = ZFLAG;
	} else if (checkneg(result)) {
		flags = NFLAG;
	}
	if (result < op1) {
		flags |= CFLAG;
	}
	if ((op1 ^ result) & (op2 ^ result) & 0x80000000) {
		flags |= VFLAG;
	}
	*pcpsr = ((*pcpsr) & 0x0fffffff) | flags;
}

static inline void
setsub(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags = 0;

	if (result == 0) {
		flags = ZFLAG;
	} else if (checkneg(result)) {
		flags = NFLAG;
	}
	if (result <= op1) {
		flags |= CFLAG;
	}
	if ((op1 ^ op2) & (op1 ^ result) & 0x80000000) {
		flags |= VFLAG;
	}
	*pcpsr = ((*pcpsr) & 0x0fffffff) | flags;
}

static inline void
setsbc(uint32_t op1, uint32_t op2, uint32_t result)
{
	armregs[cpsr] &= ~0xf0000000;

	if (result == 0) {
		armregs[cpsr] |= ZFLAG;
	} else if (checkneg(result)) {
		armregs[cpsr] |= NFLAG;
	}
	if ((checkneg(op1) && checkpos(op2)) ||
	    (checkneg(op1) && checkpos(result)) ||
	    (checkpos(op2) && checkpos(result)))
	{
		armregs[cpsr] |= CFLAG;
	}
	if ((checkneg(op1) && checkpos(op2) && checkpos(result)) ||
	    (checkpos(op1) && checkneg(op2) && checkneg(result)))
	{
		armregs[cpsr] |= VFLAG;
	}
}

static inline void
setadc(uint32_t op1, uint32_t op2, uint32_t result)
{
	armregs[cpsr] &= ~0xf0000000;

	if (result == 0) {
		armregs[cpsr] |= ZFLAG;
	} else if (checkneg(result)) {
		armregs[cpsr] |= NFLAG;
	}
	if ((checkneg(op1) && checkneg(op2)) ||
	    (checkneg(op1) && checkpos(result)) ||
	    (checkneg(op2) && checkpos(result)))
	{
		armregs[cpsr] |= CFLAG;
	}
	if ((checkneg(op1) && checkneg(op2) && checkpos(result)) ||
	    (checkpos(op1) && checkpos(op2) && checkneg(result)))
	{
		armregs[cpsr] |= VFLAG;
	}
}

static inline void
setzn(uint32_t op)
{
	uint32_t flags;

	if (op == 0) {
		flags = ZFLAG;
	} else if (checkneg(op)) {
		flags = NFLAG;
	} else {
		flags = 0;
	}
	*pcpsr = flags | ((*pcpsr) & 0x3fffffff);
}

/**
 * Update the N and Z flags following a long multiply instruction.
 *
 * The Z flag will be set if the result equals 0.
 * The N flag will be set if the result has bit 63 set.
 *
 * @param result The result of the long multiply instruction
 */
static inline void
arm_flags_long_multiply(uint64_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}

	/* N flag set if bit 63 of result is set.
	   N flag in CPSR is bit 31, so shift down by 32 */
	flags |= (((uint32_t) (result >> 32)) & NFLAG);

	*pcpsr = ((*pcpsr) & 0x3fffffff) | flags;
}

/**
 * Handle writes to Data Processing destination register with S flag clear
 *
 * @param opcode Opcode of instruction being emulated
 * @param dest   Value for destination register
 */
static inline void
arm_write_dest(uint32_t opcode, uint32_t dest)
{
	uint32_t rd = RD;

	if (rd == 15) {
		dest = ((dest + 4) & r15mask) | (armregs[15] & ~r15mask);
		refillpipeline();
	}
	armregs[rd] = dest;
}

/**
 * Handle writes to CPSR by MSR instruction
 *
 * Takes into account User/Privileged, and 26/32-bit modes. Handles change of
 * processor mode if necessary.
 *
 * @param opcode Opcode of instruction being emulated
 * @param value  Value for CPSR
 */
static inline void
arm_write_cpsr(uint32_t opcode, uint32_t value)
{
	uint32_t field_mask;

	/* User mode can only change flags, so remove other fields from
	   mask within 'opcode' */
	if (!ARM_MODE_PRIV(mode)) {
		opcode &= ~0x70000;
	}

	/* Look up which fields to write to CPSR */
	field_mask = msrlookup[(opcode >> 16) & 0xf];

	/* Write to CPSR */
	armregs[16] = (armregs[16] & ~field_mask) | (value & field_mask);

	if (!ARM_MODE_32(mode)) {
		/* In 26-bit mode */
		if (opcode & 0x80000) {
			/* Also update flags within R15 */
			armregs[15] = (armregs[15] & ~0xf0000000) |
			              (value & 0xf0000000);
		}

		if (opcode & 0x10000) {
			/* Also update mode and IRQ/FIQ bits within R15 */
			armregs[15] = (armregs[15] & ~0x0c000003) |
			              (value & 0x3) |
			              ((value & 0xc0) << 20);
		}
	}

	/* Have we changed processor mode? */
	if ((armregs[16] & 0x1f) != mode) {
		updatemode(armregs[16] & 0x1f);
	}
}

/**
 * Handle writes to SPSR by MSR instruction
 *
 * Takes into account User/Privileged modes.
 *
 * @param opcode Opcode of instruction being emulated
 * @param value  Value for SPSR
 */
static inline void
arm_write_spsr(uint32_t opcode, uint32_t value)
{
	uint32_t field_mask;

	/* Only privileged modes have an SPSR */
	if (ARM_MODE_PRIV(mode)) {
		/* Look up which fields to write to SPSR */
		field_mask = msrlookup[(opcode >> 16) & 0xf];

		/* Write to SPSR for current mode */
		spsr[mode & 0xf] = (spsr[mode & 0xf] & ~field_mask) |
		                   (value & field_mask);
	}
}

#endif

