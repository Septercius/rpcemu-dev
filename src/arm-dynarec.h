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

static int
opANDreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MUL */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]);
	} else {
		dest = GETADDR(RN) & shift2(opcode);
		arm_write_dest(opcode, dest);
	}
	return 0;
}

static int
opANDregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MULS */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]);
		arm_flags_logical(arm.reg[MULRD]);
	} else {
		lhs = GETADDR(RN);
		if (RD == 15) {
			arm_write_r15(opcode, lhs & shift2(opcode));
		} else {
			dest = lhs & shift(opcode);
			arm.reg[RD] = dest;
			arm_flags_logical(dest);
		}
	}
	return 0;
}

static int
opEORreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MLA */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
	} else {
		dest = GETADDR(RN) ^ shift2(opcode);
		arm_write_dest(opcode, dest);
	}
	return 0;
}

static int
opEORregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MLAS */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
		arm_flags_logical(arm.reg[MULRD]);
	} else {
		lhs = GETADDR(RN);
		if (RD == 15) {
			arm_write_r15(opcode, lhs ^ shift2(opcode));
		} else {
			dest = lhs ^ shift(opcode);
			arm.reg[RD] = dest;
			arm_flags_logical(dest);
		}
	}
	return 0;
}

static int
opSUBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opSUBregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs - rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sub(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opRSBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opRSBregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = rhs - lhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sub(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opADDreg(uint32_t opcode)
{
	uint32_t dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* UMULL */
		uint64_t mula = (uint64_t) arm.reg[MULRS];
		uint64_t mulb = (uint64_t) arm.reg[MULRM];
		uint64_t mulres = mula * mulb;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		return 0;
	}
	dest = GETADDR(RN) + shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opADDregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* UMULLS */
		uint64_t mula = (uint64_t) arm.reg[MULRS];
		uint64_t mulb = (uint64_t) arm.reg[MULRM];
		uint64_t mulres = mula * mulb;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return 0;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_add(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opADCreg(uint32_t opcode)
{
	uint32_t dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* UMLAL */
		uint64_t mula = (uint64_t) arm.reg[MULRS];
		uint64_t mulb = (uint64_t) arm.reg[MULRM];
		uint64_t current = ((uint64_t) arm.reg[MULRD] << 32) |
		                   arm.reg[MULRN];
		uint64_t mulres = (mula * mulb) + current;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		return 0;
	}
	dest = GETADDR(RN) + shift2(opcode) + CFSET;
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opADCregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* UMLALS */
		uint64_t mula = (uint64_t) arm.reg[MULRS];
		uint64_t mulb = (uint64_t) arm.reg[MULRM];
		uint64_t current = ((uint64_t) arm.reg[MULRD] << 32) |
		                   arm.reg[MULRN];
		uint64_t mulres = (mula * mulb) + current;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return 0;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs + rhs + CFSET;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_adc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opSBCreg(uint32_t opcode)
{
	uint32_t dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* SMULL */
		int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
		int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
		int64_t mulres = mula * mulb;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		return 0;
	}
	dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opSBCregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* SMULLS */
		int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
		int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
		int64_t mulres = mula * mulb;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return 0;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs - rhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sbc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opRSCreg(uint32_t opcode)
{
	uint32_t dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* SMLAL */
		int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
		int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
		int64_t current = ((int64_t) arm.reg[MULRD] << 32) |
		                  arm.reg[MULRN];
		int64_t mulres = (mula * mulb) + current;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		return 0;
	}
	dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opRSCregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
		/* SMLALS */
		int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
		int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
		int64_t current = ((int64_t) arm.reg[MULRD] << 32) |
		                  arm.reg[MULRN];
		int64_t mulres = (mula * mulb) + current;

		arm.reg[MULRN] = (uint32_t) mulres;
		arm.reg[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return 0;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = rhs - lhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sbc(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opSWPword(uint32_t opcode)
{
	uint32_t addr, dest, data;

	if ((opcode & 0xff0) == 0x90) {
		/* SWP */
		if (RD != 15) {
			addr = GETADDR(RN);
			data = GETREG(RM);
			dest = mem_read32(addr & ~3u);
			if (arm.event & 0x40) {
				return 1;
			}
			dest = arm_ldr_rotate(dest, addr);
			mem_write32(addr & ~3u, data);
			if (arm.event & 0x40) {
				return 1;
			}
			LOADREG(RD, dest);
		}
	} else if ((opcode & 0xf0fff) == 0xf0000) {
		/* MRS reg,CPSR */
		if (!ARM_MODE_32(arm.mode)) {
			arm.reg[16] = (arm.reg[15] & 0xf0000000) | (arm.reg[15] & 3);
			arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
		}
		arm.reg[RD] = arm.reg[16];
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opTSTreg(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TSTP reg */
		arm_compare_rd15(opcode, lhs & shift2(opcode));
	} else {
		arm_flags_logical(lhs & shift(opcode));
	}
	return 0;
}

static int
opMSRcreg(uint32_t opcode)
{
	if ((opcode & 0xf010) == 0xf000) {
		/* MSR CPSR,reg */
		arm_write_cpsr(opcode, arm.reg[RM]);
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opTEQreg(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TEQP reg */
		arm_compare_rd15(opcode, lhs ^ shift2(opcode));
	} else {
		arm_flags_logical(lhs ^ shift(opcode));
	}
	return 0;
}

static int
opSWPbyte(uint32_t opcode)
{
	uint32_t addr, dest, data;

	if ((opcode & 0xff0) == 0x90) {
		/* SWPB */
		if (RD != 15) {
			addr = GETADDR(RN);
			data = GETREG(RM);
			dest = mem_read8(addr);
			if (arm.event & 0x40) {
				return 1;
			}
			mem_write8(addr, data);
			if (arm.event & 0x40) {
				return 1;
			}
			LOADREG(RD, dest);
		}
	} else if ((opcode & 0xf0fff) == 0xf0000) {
		/* MRS reg,SPSR */
		arm.reg[RD] = arm_read_spsr();
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opCMPreg(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs - rhs;
	if (RD == 15) {
		/* CMPP reg */
		arm_compare_rd15(opcode, dest);
	} else {
		arm_flags_sub(lhs, rhs, dest);
	}
	return 0;
}

static int
opMSRsreg(uint32_t opcode)
{
	if ((opcode & 0xf010) == 0xf000) {
		/* MSR SPSR,reg */
		arm_write_spsr(opcode, arm.reg[RM]);
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opCMNreg(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		/* CMNP reg */
		arm_compare_rd15(opcode, dest);
	} else {
		arm_flags_add(lhs, rhs, dest);
	}
	return 0;
}

static int
opORRreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opORRregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs | shift2(opcode));
	} else {
		dest = lhs | shift(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opMOVreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opMOVregS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, shift2(opcode));
	} else {
		arm.reg[RD] = shift(opcode);
		arm_flags_logical(arm.reg[RD]);
	}
	return 0;
}

static int
opBICreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opBICregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & ~shift2(opcode));
	} else {
		dest = lhs & ~shift(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opMVNreg(uint32_t opcode)
{
	uint32_t dest;

	dest = ~shift2(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opMVNregS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, ~shift2(opcode));
	} else {
		arm.reg[RD] = ~shift(opcode);
		arm_flags_logical(arm.reg[RD]);
	}
	return 0;
}

static int
opANDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opANDimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & arm_imm(opcode));
	} else {
		dest = lhs & arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opEORimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) ^ arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opEORimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs ^ arm_imm(opcode));
	} else {
		dest = lhs ^ arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opSUBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opSUBimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs - rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm.reg[RD] = dest;
		arm_flags_sub(lhs, rhs, dest);
	}
	return 0;
}

static int
opRSBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opRSBimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = rhs - lhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sub(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opADDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opADDimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_add(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opADCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + arm_imm(opcode) + CFSET;
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opADCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs + rhs + CFSET;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_adc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opSBCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - arm_imm(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opSBCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs - rhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sbc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opRSCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opRSCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = rhs - lhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		arm_flags_sbc(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
	return 0;
}

static int
opTSTimm(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TSTP imm */
		arm_compare_rd15(opcode, lhs & arm_imm(opcode));
	} else {
		arm_flags_logical(lhs & arm_imm_cflag(opcode));
	}
	return 0;
}

static int
opMSRcimm(uint32_t opcode)
{
	if (RD == 15) {
		/* MSR CPSR,imm */
		arm_write_cpsr(opcode, arm_imm(opcode));
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opTEQimm(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TEQP imm */
		arm_compare_rd15(opcode, lhs ^ arm_imm(opcode));
	} else {
		arm_flags_logical(lhs ^ arm_imm_cflag(opcode));
	}
	return 0;
}

static int
opCMPimm(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs - rhs;
	if (RD == 15) {
		/* CMPP imm */
		arm_compare_rd15(opcode, dest);
	} else {
		arm_flags_sub(lhs, rhs, dest);
	}
	return 0;
}

static int
opMSRsimm(uint32_t opcode)
{
	if (RD == 15) {
		/* MSR SPSR,imm */
		arm_write_spsr(opcode, arm_imm(opcode));
	} else if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opCMNimm(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		/* CMNP imm */
		arm_compare_rd15(opcode, dest);
	} else {
		arm_flags_add(lhs, rhs, dest);
	}
	return 0;
}

static int
opORRimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opORRimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs | arm_imm(opcode));
	} else {
		dest = lhs | arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opMOVimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opMOVimmS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, arm_imm(opcode));
	} else {
		arm.reg[RD] = arm_imm_cflag(opcode);
		arm_flags_logical(arm.reg[RD]);
	}
	return 0;
}

static int
opBICimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opBICimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & ~arm_imm(opcode));
	} else {
		dest = lhs & ~arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		arm_flags_logical(dest);
	}
	return 0;
}

static int
opMVNimm(uint32_t opcode)
{
	uint32_t dest;

	dest = ~arm_imm(opcode);
	arm_write_dest(opcode, dest);
	return 0;
}

static int
opMVNimmS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, ~arm_imm(opcode));
	} else {
		arm.reg[RD] = ~arm_imm_cflag(opcode);
		arm_flags_logical(arm.reg[RD]);
	}
	return 0;
}

static int
opSTRT(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	// Store with User mode privileges
	data = GETREG(RD);
	mem_user_write32(addr & ~3u, data);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	/* Writeback */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}
	addr += offset;
	arm.reg[RN] = addr;

	return (arm.event & 0x40);
}

static int
opLDRT(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	// Load with User mode privileges
	data = mem_user_read32(addr & ~3u);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	/* Rotate if load is unaligned */
	data = arm_ldr_rotate(data, addr);

	/* Writeback */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}
	addr += offset;
	arm.reg[RN] = addr;

	/* Check for Abort (before writing Rd) */
	if (arm.event & 0x40) {
		return 1;
	}

	/* Write Rd */
	LOADREG(RD, data);

	return 0;
}

static int
opSTRBT(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	// Store with User mode privileges
	data = GETREG(RD);
	mem_user_write8(addr, data);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	/* Writeback */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}
	addr += offset;
	arm.reg[RN] = addr;

	return (arm.event & 0x40);
}

static int
opLDRBT(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	// Load with User mode privileges
	data = mem_user_read8(addr);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	/* Writeback */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}
	addr += offset;
	arm.reg[RN] = addr;

	/* Check for Abort (before writing Rd) */
	if (arm.event & 0x40) {
		return 1;
	}

	/* Write Rd */
	LOADREG(RD, data);

	return 0;
}

static int
opSTR(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += offset;
	}

	/* Store */
	data = GETREG(RD);
	mem_write32(addr & ~3u, data);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += offset;
		arm.reg[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		arm.reg[RN] = addr;
	}

	return (arm.event & 0x40);
}

static int
opLDR(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += offset;
	}

	/* Load */
	data = mem_read32(addr & ~3u);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	/* Rotate if load is unaligned */
	data = arm_ldr_rotate(data, addr);

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += offset;
		arm.reg[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		arm.reg[RN] = addr;
	}

	/* Check for Abort (before writing Rd) */
	if (arm.event & 0x40) {
		return 1;
	}

	/* Write Rd */
	LOADREG(RD, data);

	return 0;
}

static int
opSTRB(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += offset;
	}

	/* Store */
	data = GETREG(RD);
	mem_write8(addr, data);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += offset;
		arm.reg[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		arm.reg[RN] = addr;
	}

	return (arm.event & 0x40);
}

static int
opLDRB(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		arm_exception_undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		offset = shift_ldrstr(opcode);
	} else {
		offset = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		offset = -offset;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += offset;
	}

	/* Load */
	data = mem_read8(addr);

	/* Check for Abort */
	if (arm.abort_base_restored && (arm.event & 0x40)) {
		return 1;
	}

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += offset;
		arm.reg[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		arm.reg[RN] = addr;
	}

	/* Check for Abort (before writing Rd) */
	if (arm.event & 0x40) {
		return 1;
	}

	/* Write Rd */
	LOADREG(RD, data);

	return 0;
}

static int
opSTMD(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN] - offset;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_store_multiple(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opSTMI(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN];
	writeback = addr + offset;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_store_multiple(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opSTMDS(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN] - offset;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_store_multiple_s(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opSTMIS(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN];
	writeback = addr + offset;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_store_multiple_s(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opLDMD(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN] - offset;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_load_multiple(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opLDMI(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN];
	writeback = addr + offset;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_load_multiple(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opLDMDS(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN] - offset;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_load_multiple_s(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opLDMIS(uint32_t opcode)
{
	uint32_t addr, offset, writeback;

	offset = arm_ldm_stm_offset(opcode);
	addr = arm.reg[RN];
	writeback = addr + offset;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_load_multiple_s(opcode, addr, writeback);
	return (arm.event & 0x40);
}

static int
opB(uint32_t opcode)
{
	uint32_t offset;

	/* Extract offset bits, and sign-extend */
	offset = (opcode << 8);
	offset = (uint32_t) ((int32_t) offset >> 6);

	arm.reg[15] = ((arm.reg[15] + offset + 4) & arm.r15_mask) |
	              (arm.reg[15] & ~arm.r15_mask);
	blockend = 1;
	return 0;
}

static int
opBL(uint32_t opcode)
{
	uint32_t offset;

	/* Extract offset bits, and sign-extend */
	offset = (opcode << 8);
	offset = (uint32_t) ((int32_t) offset >> 6);

	arm.reg[14] = arm.reg[15] - 4;
	arm.reg[15] = ((arm.reg[15] + offset + 4) & arm.r15_mask) |
	              (arm.reg[15] & ~arm.r15_mask);
	refillpipeline();
	return 0;
}

static int
opcopro(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100 || (opcode & 0xf00) == 0x200) {
		fpaopcode(opcode);
		return 0;
	}
#endif
	NOT_USED(opcode);

	arm_exception_undefined();
	return 0;
}

static int
opMCR(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100) {
		fpaopcode(opcode);
		return 0;
	}
#endif
	if ((opcode & 0xf10) == 0xf10 && ARM_MODE_PRIV(arm.mode)) {
		cp15_write(opcode, arm.reg[RD]);
	} else {
		arm_exception_undefined();
	}
	return 0;
}

static int
opMRC(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100) {
		fpaopcode(opcode);
		return 0;
	}
#endif
	if ((opcode & 0xf10) == 0xf10 && ARM_MODE_PRIV(arm.mode)) {
		if (RD == 15) {
			arm.reg[RD] = (arm.reg[RD] & arm.r15_mask) |
				      (cp15_read(opcode) & ~arm.r15_mask);
		} else {
			arm.reg[RD] = cp15_read(opcode);
		}
	} else {
		arm_exception_undefined();
	}
	return 0;
}

/**
 * This refers to the unallocated portions of the opcode space.
 */
static int
opUNALLOC(uint32_t opcode)
{
	NOT_USED(opcode);

	if (arm.arch_v4) {
		arm_exception_undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static int
opLDRH(uint32_t opcode)
{
	arm_ldrh(opcode);
	return (arm.event & 0x40);
}

static int
opLDRSB(uint32_t opcode)
{
	arm_ldrsb(opcode);
	return (arm.event & 0x40);
}

static int
opLDRSH(uint32_t opcode)
{
	arm_ldrsh(opcode);
	return (arm.event & 0x40);
}

static int
opSTRH(uint32_t opcode)
{
	arm_strh(opcode);
	return (arm.event & 0x40);
}
