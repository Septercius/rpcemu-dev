/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef ARM_COMMON_H
#define ARM_COMMON_H

/* Functions in arm_common.c */
extern void arm_ldrh(uint32_t opcode);
extern void arm_ldrsh(uint32_t opcode);
extern void arm_ldrsb(uint32_t opcode);
extern void arm_strh(uint32_t opcode);
extern void arm_store_multiple(uint32_t opcode, uint32_t address, uint32_t writeback);
extern void arm_store_multiple_s(uint32_t opcode, uint32_t address, uint32_t writeback);
extern void arm_load_multiple(uint32_t opcode, uint32_t address, uint32_t writeback);
extern void arm_load_multiple_s(uint32_t opcode, uint32_t address, uint32_t writeback);
extern int opSWI(uint32_t opcode);

#define refillpipeline() blockend=1;

#define LOADREG(r, v) if (r == 15) { arm.reg[15] = (arm.reg[15] & ~arm.r15_mask) | (((v) + 4) & arm.r15_mask); refillpipeline(); } else arm.reg[r] = (v);

#define GETREG(r) ((r == 15) ? (arm.reg[15] + arm.r15_diff) : arm.reg[r])

/** Evaulate to non-zero if 'mode' is a 32-bit mode */
#define ARM_MODE_32(mode)	((mode) & 0x10)

/** Evaluate to non-zero if 'mode' is a privileged mode */
#define ARM_MODE_PRIV(mode)	((mode) & 0xf)

/// Evaluate to non-zero if 'mode' has a SPSR (i.e. not USR26/USR32/Sys32)
#define ARM_MODE_HAS_SPSR(mode)	(ARM_MODE_PRIV(mode) && ((mode) != 0x1f))

/// Only certain bits within CPSR/SPSR can be modified on real hardware
#define PSR_BITS_VALID	0xf00000df

/** A table used by MSR instructions to determine which fields can be modified
    within a PSR */
static const uint32_t msrlookup[16] = {
	0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
	0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
	0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
	0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff
};

/**
 * Perform a rotate-right operation on a 32-bit integer.
 *
 * @param x Value to rotate
 * @param n Number of bit positions to rotate by
 * @return Rotated value
 */
static inline uint32_t
rotate_right32(uint32_t x, uint32_t n)
{
	return (x >> n) | (x << (32 - n));
}

/**
 * Return the immediate operand in an opcode.
 *
 * It is encoded as an 8-bit constant rotated by twice the value of a 4-bit
 * constant.
 *
 * @param opcode Opcode of instruction being emulated
 * @return Value of immediate operand
 */
static inline uint32_t
arm_imm(uint32_t opcode)
{
	uint32_t val = opcode & 0xff;
	uint32_t amount = ((opcode >> 8) & 0xf) << 1;

	return rotate_right32(val, amount);
}

/**
 * Return the immediate operand in an opcode, and update the C flag.
 *
 * It is encoded as an 8-bit constant rotated by twice the value of a 4-bit
 * constant. The C flag will be updated if the rotate amount is non-zero.
 *
 * This is used by AND, EOR, TST, TEQ, ORR, MOV, BIC and MVN when the S flag
 * is set.
 *
 * @param opcode Opcode of instruction being emulated
 * @return Value of immediate operand
 */
static inline uint32_t
arm_imm_cflag(uint32_t opcode)
{
	uint32_t result = arm_imm(opcode);

	if (opcode & 0xf00) {
		if (result & 0x80000000) {
			arm.reg[cpsr] |= CFLAG;
		} else {
			arm.reg[cpsr] &= ~CFLAG;
		}
	}
	return result;
}

/**
 * Update the NZCV flags following an add instruction.
 *
 * @param op1 The left operand
 * @param op2 The right operand
 * @param result The result of the operation
 */
static inline void
arm_flags_add(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}
	flags |= result & NFLAG;
	if (result < op1) {
		flags |= CFLAG;
	}
	if ((op1 ^ result) & (op2 ^ result) & 0x80000000) {
		flags |= VFLAG;
	}
	arm.reg[cpsr] = (arm.reg[cpsr] & 0x0fffffff) | flags;
}

/**
 * Update the NZCV flags following a sub instruction.
 *
 * @param op1 The left operand
 * @param op2 The right operand
 * @param result The result of the operation
 */
static inline void
arm_flags_sub(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}
	flags |= result & NFLAG;
	if (result <= op1) {
		flags |= CFLAG;
	}
	if ((op1 ^ op2) & (op1 ^ result) & 0x80000000) {
		flags |= VFLAG;
	}
	arm.reg[cpsr] = (arm.reg[cpsr] & 0x0fffffff) | flags;
}

/**
 * Update the NZCV flags following an adc instruction.
 *
 * @param op1 The left operand
 * @param op2 The right operand
 * @param result The result of the operation
 */
static inline void
arm_flags_adc(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}
	flags |= result & NFLAG;
	if (((op1 & op2) | ((op1 | op2) & ~result)) & 0x80000000) {
		flags |= CFLAG;
	}
	if (((op1 ^ result) & (op2 ^ result)) & 0x80000000) {
		flags |= VFLAG;
	}
	arm.reg[cpsr] = (arm.reg[cpsr] & 0x0fffffff) | flags;
}

/**
 * Update the NZCV flags following a sbc instruction.
 *
 * @param op1 The left operand
 * @param op2 The right operand
 * @param result The result of the operation
 */
static inline void
arm_flags_sbc(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}
	flags |= result & NFLAG;
	if (((op1 & ~op2) | ((op1 | ~op2) & ~result)) & 0x80000000) {
		flags |= CFLAG;
	}
	if (((op1 ^ op2) & (op1 ^ result)) & 0x80000000) {
		flags |= VFLAG;
	}
	arm.reg[cpsr] = (arm.reg[cpsr] & 0x0fffffff) | flags;
}

/**
 * Update the N and Z flags following a logical or multiply instruction.
 *
 * The Z flag will be set if the result equals 0.
 * The N flag will be set if the result has bit 31 set.
 *
 * @param result The result of the operation
 */
static inline void
arm_flags_logical(uint32_t result)
{
	uint32_t flags;

	if (result == 0) {
		flags = ZFLAG;
	} else {
		flags = 0;
	}
	flags |= result & NFLAG;
	arm.reg[cpsr] = (arm.reg[cpsr] & 0x3fffffff) | flags;
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

	arm.reg[cpsr] = (arm.reg[cpsr] & 0x3fffffff) | flags;
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
		dest = ((dest + 4) & arm.r15_mask) | (arm.reg[15] & ~arm.r15_mask);
	}
	arm.reg[rd] = dest;
}

/**
 * Handle writes to R15 for data processing instructions with S flag set
 *
 * @param opcode Opcode of instruction being emulated
 * @param dest   Value for R15
 */
static inline void
arm_write_r15(uint32_t opcode, uint32_t dest)
{
	uint32_t mask;

	NOT_USED(opcode);

	if (ARM_MODE_32(arm.mode)) {
		/* In 32-bit mode, update all bits in R15 except 0 and 1 */
		mask = 0xfffffffc;
	} else if (ARM_MODE_PRIV(arm.mode)) {
		/* In 26-bit privileged mode, update all bits and flags */
		mask = 0xffffffff;
	} else {
		/* In 26-bit non-privileged mode, only update PC and NZCV */
		mask = 0xf3fffffc;
	}

	/* Write to R15 (adding 4 for pipelining) */
	arm.reg[15] = (arm.reg[15] & ~mask) | ((dest + 4) & mask);

	if (ARM_MODE_PRIV(arm.mode)) {
		/* In privileged mode, can change mode */

		if (ARM_MODE_32(arm.mode)) {
			/* Copy SPSR of current mode to CPSR */
			arm.reg[16] = arm.spsr[arm.mode & 0xf];
		}
		if ((arm.reg[cpsr] & arm.mmask) != arm.mode) {
			updatemode(arm.reg[cpsr] & arm.mmask);
		}
	}
}

/**
 * Implement compare instructions when Rd==15 (i.e. P flag is used).
 * The instructions are TSTP, TEQP, CMPP, and CMNP.
 *
 * @param opcode Opcode of instruction being emulated
 * @param dest   Value for PSR bits (if in 26-bit mode)
 */
static inline void
arm_compare_rd15(uint32_t opcode, uint32_t dest)
{
	uint32_t mask;

	NOT_USED(opcode);

	if (ARM_MODE_32(arm.mode)) {
		/* In 32-bit mode */

		if (ARM_MODE_PRIV(arm.mode)) {
			/* Copy SPSR of current mode to CPSR */
			arm.reg[16] = arm.spsr[arm.mode & 0xf];
		}

	} else {
		/* In 26-bit mode */
		if (ARM_MODE_PRIV(arm.mode)) {
			/* In privileged mode update all PSR bits */
			mask = 0xfc000003;
		} else {
			/* In non-privileged mode only update NZCV flags */
			mask = 0xf0000000;
		}

		/* Write to PSR bits (within R15) */
		arm.reg[15] = (arm.reg[15] & ~mask) | (dest & mask);
	}

	/* Have we changed processor mode? */
	if ((arm.reg[cpsr] & arm.mmask) != arm.mode) {
		updatemode(arm.reg[cpsr] & arm.mmask);
	}
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
	if (!ARM_MODE_PRIV(arm.mode)) {
		opcode &= ~0x70000;
	}

	/* Look up which fields to write to CPSR */
	field_mask = msrlookup[(opcode >> 16) & 0xf] & PSR_BITS_VALID;

	/* Write to CPSR */
	arm.reg[16] = (arm.reg[16] & ~field_mask) | (value & field_mask);

	if (!ARM_MODE_32(arm.mode)) {
		/* In 26-bit mode */
		if (opcode & 0x80000) {
			/* Also update flags within R15 */
			arm.reg[15] = (arm.reg[15] & ~0xf0000000) |
			              (value & 0xf0000000);
		}

		if (opcode & 0x10000) {
			/* Also update mode and IRQ/FIQ bits within R15 */
			arm.reg[15] = (arm.reg[15] & ~0x0c000003) |
			              (value & 0x3) |
			              ((value & 0xc0) << 20);
		}
	}

	/* Have we changed processor mode? */
	if ((arm.reg[16] & 0x1f) != arm.mode) {
		updatemode(arm.reg[16] & 0x1f);
	}
}

/**
 * Handle reads from SPSR by MRS instruction.
 *
 * @return Value of SPSR (or CPSR if unavailable)
 */
static inline uint32_t
arm_read_spsr(void)
{
	if (ARM_MODE_HAS_SPSR(arm.mode)) {
		return arm.spsr[arm.mode & 0xf];
	} else {
		// Real hardware returns CPSR if the mode has no SPSR
		if (ARM_MODE_32(arm.mode)) {
			return arm.reg[16];
		} else {
			return (arm.reg[15] & 0xf0000000) |
			       ((arm.reg[15] >> 20) & 0xc0) |
			       (arm.reg[15] & 3);
		}
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

	// Only privileged modes have an SPSR (except Sys32)
	if (ARM_MODE_HAS_SPSR(arm.mode)) {
		/* Look up which fields to write to SPSR */
		field_mask = msrlookup[(opcode >> 16) & 0xf] & PSR_BITS_VALID;

		/* Write to SPSR for current mode */
		arm.spsr[arm.mode & 0xf] = (arm.spsr[arm.mode & 0xf] & ~field_mask) |
		                           (value & field_mask);
	}
}

/**
 * Handle unaligned LDR by rotating loaded value if necessary.
 *
 * @param value Value loaded
 * @param addr  Address from which the load was performed
 * @return Modified value (rotated if necessary)
 */
static inline uint32_t
arm_ldr_rotate(uint32_t value, uint32_t addr)
{
	uint32_t rotate = (addr & 3) * 8;

	return rotate_right32(value, rotate);
}

/**
 * Calculate the offset (size) of a LDM/STM transfer.
 *
 * This is 4 bytes for each register being transferred.
 *
 * @param opcode Opcode of instruction being emulated
 * @return Offset (size) of LDM/STM transfer
 */
static inline uint32_t
arm_ldm_stm_offset(uint32_t opcode)
{
	return (uint32_t) countbitstable[opcode & 0xffff];
}

#endif

