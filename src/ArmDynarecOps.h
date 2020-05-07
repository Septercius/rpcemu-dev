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

static void
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
}

static void
opANDregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MULS */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]);
		setzn(arm.reg[MULRD]);
	} else {
		lhs = GETADDR(RN);
		if (RD == 15) {
			arm_write_r15(opcode, lhs & shift2(opcode));
		} else {
			dest = lhs & shift(opcode);
			arm.reg[RD] = dest;
			setzn(dest);
		}
	}
}

static void
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
}

static void
opEORregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	if ((opcode & 0xf0) == 0x90) {
		/* MLAS */
		arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
		    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
		setzn(arm.reg[MULRD]);
	} else {
		lhs = GETADDR(RN);
		if (RD == 15) {
			arm_write_r15(opcode, lhs ^ shift2(opcode));
		} else {
			dest = lhs ^ shift(opcode);
			arm.reg[RD] = dest;
			setzn(dest);
		}
	}
}

static void
opSUBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
opSUBregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs - rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsub(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opRSBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void
opRSBregS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = rhs - lhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsub(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
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
		return;
	}
	dest = GETADDR(RN) + shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
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
		return;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setadd(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
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
		return;
	}
	dest = GETADDR(RN) + shift2(opcode) + CFSET;
	arm_write_dest(opcode, dest);
}

static void
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
		return;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs + rhs + CFSET;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setadc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
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
		return;
	}
	dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void
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
		return;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = lhs - rhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsbc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
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
		return;
	}
	dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void
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
		return;
	}
	lhs = GETADDR(RN);
	rhs = shift2(opcode);
	dest = rhs - lhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsbc(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
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
			if (armirq & 0x40) {
				return 1;
			}
			dest = arm_ldr_rotate(dest, addr);
			mem_write32(addr & ~3u, data);
			if (armirq & 0x40) {
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
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static void
opTSTreg(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TSTP reg */
		arm_compare_rd15(opcode, lhs & shift2(opcode));
	} else {
		setzn(lhs & shift(opcode));
	}
}

static void
opMSRcreg(uint32_t opcode)
{
	if ((opcode & 0xf010) == 0xf000) {
		/* MSR CPSR,reg */
		arm_write_cpsr(opcode, arm.reg[RM]);
	} else if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
}

static void
opTEQreg(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TEQP reg */
		arm_compare_rd15(opcode, lhs ^ shift2(opcode));
	} else {
		setzn(lhs ^ shift(opcode));
	}
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
			if (armirq & 0x40) {
				return 1;
			}
			mem_write8(addr, data);
			if (armirq & 0x40) {
				return 1;
			}
			LOADREG(RD, dest);
		}
	} else if ((opcode & 0xf0fff) == 0xf0000) {
		/* MRS reg,SPSR */
		arm.reg[RD] = arm_read_spsr();
	} else if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
	return 0;
}

static void
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
		setsub(lhs, rhs, dest);
	}
}

static void
opMSRsreg(uint32_t opcode)
{
	if ((opcode & 0xf010) == 0xf000) {
		/* MSR SPSR,reg */
		arm_write_spsr(opcode, arm.reg[RM]);
	} else if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
}

static void
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
		setadd(lhs, rhs, dest);
	}
}

static void
opORRreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
opORRregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs | shift2(opcode));
	} else {
		dest = lhs | shift(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opMOVreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
opMOVregS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, shift2(opcode));
	} else {
		arm.reg[RD] = shift(opcode);
		setzn(arm.reg[RD]);
	}
}

static void
opBICreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
opBICregS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & ~shift2(opcode));
	} else {
		dest = lhs & ~shift(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opMVNreg(uint32_t opcode)
{
	uint32_t dest;

	dest = ~shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void
opMVNregS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, ~shift2(opcode));
	} else {
		arm.reg[RD] = ~shift(opcode);
		setzn(arm.reg[RD]);
	}
}

static void
opANDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opANDimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & arm_imm(opcode));
	} else {
		dest = lhs & arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opEORimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) ^ arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opEORimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs ^ arm_imm(opcode));
	} else {
		dest = lhs ^ arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opSUBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
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
		setsub(lhs, rhs, dest);
	}
}

static void
opRSBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void
opRSBimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = rhs - lhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsub(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opADDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opADDimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs + rhs;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setadd(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opADCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + arm_imm(opcode) + CFSET;
	arm_write_dest(opcode, dest);
}

static void
opADCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs + rhs + CFSET;
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setadc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opSBCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - arm_imm(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void
opSBCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = lhs - rhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsbc(lhs, rhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opRSCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void
opRSCimmS(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

	lhs = GETADDR(RN);
	rhs = arm_imm(opcode);
	dest = rhs - lhs - (CFSET ? 0 : 1);
	if (RD == 15) {
		arm_write_r15(opcode, dest);
	} else {
		setsbc(rhs, lhs, dest);
		arm.reg[RD] = dest;
	}
}

static void
opTSTimm(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TSTP imm */
		arm_compare_rd15(opcode, lhs & arm_imm(opcode));
	} else {
		setzn(lhs & arm_imm_cflag(opcode));
	}
}

static void
opMSRcimm(uint32_t opcode)
{
	if (RD == 15) {
		/* MSR CPSR,imm */
		arm_write_cpsr(opcode, arm_imm(opcode));
	} else if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
}

static void
opTEQimm(uint32_t opcode)
{
	uint32_t lhs;

	lhs = GETADDR(RN);
	if (RD == 15) {
		/* TEQP imm */
		arm_compare_rd15(opcode, lhs ^ arm_imm(opcode));
	} else {
		setzn(lhs ^ arm_imm_cflag(opcode));
	}
}

static void
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
		setsub(lhs, rhs, dest);
	}
}

static void
opMSRsimm(uint32_t opcode)
{
	if (RD == 15) {
		/* MSR SPSR,imm */
		arm_write_spsr(opcode, arm_imm(opcode));
	} else if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
}

static void
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
		setadd(lhs, rhs, dest);
	}
}

static void
opORRimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opORRimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs | arm_imm(opcode));
	} else {
		dest = lhs | arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opMOVimm(uint32_t opcode)
{
	uint32_t dest;

	dest = arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opMOVimmS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, arm_imm(opcode));
	} else {
		arm.reg[RD] = arm_imm_cflag(opcode);
		setzn(arm.reg[RD]);
	}
}

static void
opBICimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opBICimmS(uint32_t opcode)
{
	uint32_t lhs, dest;

	lhs = GETADDR(RN);
	if (RD == 15) {
		arm_write_r15(opcode, lhs & ~arm_imm(opcode));
	} else {
		dest = lhs & ~arm_imm_cflag(opcode);
		arm.reg[RD] = dest;
		setzn(dest);
	}
}

static void
opMVNimm(uint32_t opcode)
{
	uint32_t dest;

	dest = ~arm_imm(opcode);
	arm_write_dest(opcode, dest);
}

static void
opMVNimmS(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_r15(opcode, ~arm_imm(opcode));
	} else {
		arm.reg[RD] = ~arm_imm_cflag(opcode);
		setzn(arm.reg[RD]);
	}
}

static int
opSTRT(uint32_t opcode)
{
	uint32_t addr, data, offset, templ;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	data = GETREG(RD);
	mem_write32(addr & ~3u, data);
	memmode = templ;

	/* Check for Abort */
	if (arm.abort_base_restored && (armirq & 0x40)) {
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

	return (armirq & 0x40);
}

static int
opLDRT(uint32_t opcode)
{
	uint32_t addr, data, offset, templ;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	data = mem_read32(addr & ~3u);
	memmode = templ;

	/* Check for Abort */
	if (arm.abort_base_restored && (armirq & 0x40)) {
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
	if (armirq & 0x40) {
		return 1;
	}

	/* Write Rd */
	LOADREG(RD, data);

	return 0;
}

static int
opSTRBT(uint32_t opcode)
{
	uint32_t addr, data, offset, templ;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	data = GETREG(RD);
	mem_write8(addr, data);
	memmode = templ;

	/* Check for Abort */
	if (arm.abort_base_restored && (armirq & 0x40)) {
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

	return (armirq & 0x40);
}

static int
opLDRBT(uint32_t opcode)
{
	uint32_t addr, data, offset, templ;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	data = mem_read8(addr);
	memmode = templ;

	/* Check for Abort */
	if (arm.abort_base_restored && (armirq & 0x40)) {
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
	if (armirq & 0x40) {
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
		undefined();
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
	if (arm.abort_base_restored && (armirq & 0x40)) {
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

	return (armirq & 0x40);
}

static int
opLDR(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
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
	if (arm.abort_base_restored && (armirq & 0x40)) {
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
	if (armirq & 0x40) {
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
		undefined();
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
	if (arm.abort_base_restored && (armirq & 0x40)) {
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

	return (armirq & 0x40);
}

static int
opLDRB(uint32_t opcode)
{
	uint32_t addr, data, offset;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
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
	if (arm.abort_base_restored && (armirq & 0x40)) {
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
	if (armirq & 0x40) {
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
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
	return (armirq & 0x40);
}

static void
opB(uint32_t opcode)
{
	uint32_t offset;

	/* Extract offset bits, and sign-extend */
	offset = (opcode << 8);
	offset = (uint32_t) ((int32_t) offset >> 6);

	arm.reg[15] = ((arm.reg[15] + offset + 4) & arm.r15_mask) |
	              (arm.reg[15] & ~arm.r15_mask);
	blockend = 1;
}

static void
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
}

static void
opcopro(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100 || (opcode & 0xf00) == 0x200) {
		fpaopcode(opcode);
		return;
	}
#endif
	NOT_USED(opcode);

	undefined();
}

static void
opMCR(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100) {
		fpaopcode(opcode);
		return;
	}
#endif
	if ((opcode & 0xf10) == 0xf10) {
		cp15_write(RN, arm.reg[RD], opcode);
	} else {
		undefined();
	}
}

static void
opMRC(uint32_t opcode)
{
#ifdef FPA
	if ((opcode & 0xf00) == 0x100) {
		fpaopcode(opcode);
		return;
	}
#endif
	if ((opcode & 0xf10) == 0xf10) {
		if (RD == 15) {
			arm.reg[RD] = (arm.reg[RD] & arm.r15_mask) |
				      (cp15_read(RN) & ~arm.r15_mask);
		} else {
			arm.reg[RD] = cp15_read(RN);
		}
	} else {
		undefined();
	}
}

/**
 * This refers to the unallocated portions of the opcode space.
 */
static void
opUNALLOC(uint32_t opcode)
{
	NOT_USED(opcode);

	if (arm.arch_v4) {
		undefined();
	} else {
		arm_unpredictable(opcode);
	}
}

static int
opLDRH(uint32_t opcode)
{
	arm_ldrh(opcode);
	return (armirq & 0x40);
}

static int
opLDRSB(uint32_t opcode)
{
	arm_ldrsb(opcode);
	return (armirq & 0x40);
}

static int
opLDRSH(uint32_t opcode)
{
	arm_ldrsh(opcode);
	return (armirq & 0x40);
}

static int
opSTRH(uint32_t opcode)
{
	arm_strh(opcode);
	return (armirq & 0x40);
}
