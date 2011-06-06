#ifdef INARMC

static void opANDreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) /* MUL */
	{
		armregs[MULRD] = (MULRD == MULRM) ? 0 :
		    (armregs[MULRM] * armregs[MULRS]);
	}
	else
	{
		dest = GETADDR(RN) & shift2(opcode);
		arm_write_dest(opcode, dest);
	}
}

static void opANDregS(uint32_t opcode)
{
	uint32_t lhs, templ;

	if ((opcode & 0xf0) == 0x90) /* MULS */
	{
		armregs[MULRD] = (MULRD == MULRM) ? 0 :
		    (armregs[MULRM] * armregs[MULRS]);
		setzn(armregs[MULRD]);
	}
        else
	{
		lhs = GETADDR(RN);
		if (RD==15)
		{
			arm_write_r15(opcode, lhs & shift2(opcode));
		}
		else
		{
			templ=shift(opcode);
			armregs[RD] = lhs & templ;
			setzn(armregs[RD]);
		}
	}
}

static void opEORreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) /* MLA */
	{
		armregs[MULRD] = (MULRD == MULRM) ? 0 :
		    (armregs[MULRM] * armregs[MULRS]) + armregs[MULRN];
	}
	else
        {
		dest = GETADDR(RN) ^ shift2(opcode);
		arm_write_dest(opcode, dest);
        }
}

static void opEORregS(uint32_t opcode)
{
	uint32_t lhs, templ;

        if ((opcode & 0xf0) == 0x90) /* MLAS */
        {
		armregs[MULRD] = (MULRD == MULRM) ? 0 :
		    (armregs[MULRM] * armregs[MULRS]) + armregs[MULRN];
		setzn(armregs[MULRD]);
        }
        else
        {
                lhs = GETADDR(RN);
                if (RD==15)
                {
                        arm_write_r15(opcode, lhs ^ shift2(opcode));
                }
                else
                {
                        templ=shift(opcode);
                        armregs[RD] = lhs ^ templ;
                        setzn(armregs[RD]);
                }
        }
}

static void opSUBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opSUBregS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        templ=shift2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs - templ);
        }
        else
        {
                setsub(lhs, templ, lhs - templ);
                armregs[RD] = lhs - templ;
        }
}

static void opRSBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void opRSBregS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        templ=shift2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, templ - lhs);
        }
        else
        {
                setsub(templ, lhs, templ - lhs);
                armregs[RD] = templ - lhs;
        }
}

static void opADDreg(uint32_t opcode)
{
	uint32_t dest;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* UMULL */
	{
		uint64_t mula = (uint64_t) armregs[MULRS];
		uint64_t mulb = (uint64_t) armregs[MULRM];
		uint64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		return;
	}
#endif
	dest = GETADDR(RN) + shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opADDregS(uint32_t opcode)
{
	uint32_t lhs, templ;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* UMULLS */
	{
		uint64_t mula = (uint64_t) armregs[MULRS];
		uint64_t mulb = (uint64_t) armregs[MULRM];
		uint64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return;
	}
#endif
	lhs = GETADDR(RN);
	templ = shift2(opcode);
	if (RD == 15) {
		arm_write_r15(opcode, lhs + templ);
	} else {
		setadd(lhs, templ, lhs + templ);
		armregs[RD] = lhs + templ;
	}
}

static void opADCreg(uint32_t opcode)
{
	uint32_t dest;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* UMLAL */
	{
		uint64_t mula = (uint64_t) armregs[MULRS];
		uint64_t mulb = (uint64_t) armregs[MULRM];
		uint64_t current = ((uint64_t) armregs[MULRD] << 32) |
		                   armregs[MULRN];
		uint64_t mulres = (mula * mulb) + current;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		return;
	}
#endif
	dest = GETADDR(RN) + shift2(opcode) + CFSET;
	arm_write_dest(opcode, dest);
}

static void opADCregS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* UMLALS */
	{
		uint64_t mula = (uint64_t) armregs[MULRS];
		uint64_t mulb = (uint64_t) armregs[MULRM];
		uint64_t current = ((uint64_t) armregs[MULRD] << 32) |
		                   armregs[MULRN];
		uint64_t mulres = (mula * mulb) + current;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return;
	}
#endif
	lhs = GETADDR(RN);
	templ2 = CFSET;
	templ = shift2(opcode);
	if (RD == 15) {
		arm_write_r15(opcode, lhs + templ + templ2);
	} else {
		setadc(lhs, templ, lhs + templ + templ2);
		armregs[RD] = lhs + templ + templ2;
	}
}

static void opSBCreg(uint32_t opcode)
{
	uint32_t dest;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* SMULL */
	{
		int64_t mula = (int64_t) (int32_t) armregs[MULRS];
		int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
		int64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		return;
	}
#endif
	dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opSBCregS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* SMULLS */
	{
		int64_t mula = (int64_t) (int32_t) armregs[MULRS];
		int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
		int64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return;
	}
#endif
	lhs = GETADDR(RN);
	templ2 = (CFSET) ? 0 : 1;
	templ = shift2(opcode);
	if (RD == 15) {
		arm_write_r15(opcode, lhs - (templ + templ2));
	} else {
		setsbc(lhs, templ, lhs - (templ + templ2));
		armregs[RD] = lhs - (templ + templ2);
	}
}

static void opRSCreg(uint32_t opcode)
{
	uint32_t dest;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* SMLAL */
	{
		int64_t mula = (int64_t) (int32_t) armregs[MULRS];
		int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
		int64_t current = ((int64_t) armregs[MULRD] << 32) |
		                  armregs[MULRN];
		int64_t mulres = (mula * mulb) + current;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		return;
	}
#endif
	dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opRSCregS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* SMLALS */
	{
		int64_t mula = (int64_t) (int32_t) armregs[MULRS];
		int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
		int64_t current = ((int64_t) armregs[MULRD] << 32) |
		                  armregs[MULRN];
		int64_t mulres = (mula * mulb) + current;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
		return;
	}
#endif
	lhs = GETADDR(RN);
	templ2 = (CFSET) ? 0 : 1;
	templ = shift2(opcode);
	if (RD == 15) {
		arm_write_r15(opcode, templ - (lhs + templ2));
	} else {
		setsbc(templ, lhs, templ - (lhs + templ2));
		armregs[RD] = templ - (lhs + templ2);
	}
}

static void opSWPword(uint32_t opcode)
{
	uint32_t templ;

        if ((opcode&0xF0)==0x90)
        {
                uint32_t addr;
                addr=armregs[RN]&~3;
                templ=GETREG(RM);
                LOADREG(RD,readmeml(addr));
                writememl(addr,templ);
        }
        else if (!(opcode&0xFFF)) /*MRS CPSR*/
        {
                if (!ARM_MODE_32(mode)) {
                        armregs[16]=(armregs[15]&0xF0000000)|(armregs[15]&3);
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                }
                armregs[RD]=armregs[16];
        }
        else
        {
                undefined();
        }
}

static void opTSTreg(uint32_t opcode)
{
	uint32_t lhs;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                /* TSTP reg */
                arm_compare_rd15(opcode, lhs & shift2(opcode));
        } else {
                setzn(lhs & shift(opcode));
        }
}

static void opMSRcreg(uint32_t opcode)
{
	if ((RD == 15) && ((opcode & 0xff0) == 0)) {
		arm_write_cpsr(opcode, armregs[RM]);
	} else {
		bad_opcode(opcode);
	}
}

static void opTEQreg(uint32_t opcode)
{
	uint32_t lhs;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                /* TEQP reg */
                arm_compare_rd15(opcode, lhs ^ shift2(opcode));
        }
        else
        {
                setzn(lhs ^ shift(opcode));
        }
}

static void opSWPbyte(uint32_t opcode)
{
	uint32_t templ;

        if ((opcode&0xF0)==0x90) /* SWPB */
        {
                uint32_t addr;
                addr=armregs[RN];
                templ=GETREG(RM);
                LOADREG(RD,readmemb(addr));
                writememb(addr,templ);
        }
        else if (!(opcode&0xFFF)) /* MRS SPSR */
        {
                armregs[RD]=spsr[mode&15];
        }
        else
        {
		bad_opcode(opcode);
        }
}

static void opCMPreg(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

        lhs = GETADDR(RN);
        rhs = shift2(opcode);
        dest = lhs - rhs;
        if (RD==15)
        {
                /* CMPP reg */
                arm_compare_rd15(opcode, dest);
        }
        else
        {
                setsub(lhs, rhs, dest);
        }
}

static void opMSRsreg(uint32_t opcode)
{
	if ((RD == 15) && ((opcode & 0xff0) == 0)) {
		/* MSR SPSR, reg */
		arm_write_spsr(opcode, armregs[RM]);
	} else {
		bad_opcode(opcode);
	}
}

static void opCMNreg(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

        lhs = GETADDR(RN);
        rhs = shift2(opcode);
        dest = lhs + rhs;
        if (RD==15)
        {
                /* CMNP reg */
                arm_compare_rd15(opcode, dest);
        } else {
                setadd(lhs, rhs, dest);
        }
}

static void opORRreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opORRregS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs | shift2(opcode));
        }
        else
        {
                templ=shift(opcode);
                armregs[RD] = lhs | templ;
                setzn(armregs[RD]);
        }
}

static void opMOVreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opMOVregS(uint32_t opcode)
{
        if (RD==15)
        {
                arm_write_r15(opcode, shift2(opcode));
        }
        else
        {
                armregs[RD]=shift(opcode);
                setzn(armregs[RD]);
        }
}

static void opBICreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opBICregS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs & ~shift2(opcode));
        }
        else
        {
                templ=shift(opcode);
                armregs[RD] = lhs & ~templ;
                setzn(armregs[RD]);
        }
}

static void opMVNreg(uint32_t opcode)
{
	uint32_t dest;

	dest = ~shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opMVNregS(uint32_t opcode)
{
        if (RD==15)
        {
                arm_write_r15(opcode, ~shift2(opcode));
        }
        else
        {
                armregs[RD]=~shift(opcode);
                setzn(armregs[RD]);
        }
}


static void opANDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opANDimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs & rotate2(opcode));
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD] = lhs & templ;
                setzn(armregs[RD]);
        }
}

static void opEORimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) ^ rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opEORimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs ^ rotate2(opcode));
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD] = lhs ^ templ;
                setzn(armregs[RD]);
        }
}

static void opSUBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opSUBimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs - templ);
        }
        else
        {
                armregs[RD] = lhs - templ;
                setsub(lhs, templ, lhs - templ);
        }
}

static void opRSBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = rotate2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void opRSBimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, templ - lhs);
        }
        else
        {
                setsub(templ, lhs, templ - lhs);
                armregs[RD] = templ - lhs;
        }
}

static void opADDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opADDimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs + templ);
        }
        else
        {
                setadd(lhs, templ, lhs + templ);
                armregs[RD] = lhs + templ;
        }
}

static void opADCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + rotate2(opcode) + CFSET;
	arm_write_dest(opcode, dest);
}

static void opADCimmS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

        lhs = GETADDR(RN);
        templ2=CFSET;
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs + templ + templ2);
        }
        else
        {
                setadc(lhs, templ, lhs + templ + templ2);
                armregs[RD] = lhs + templ + templ2;
        }
}

static void opSBCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - rotate2(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opSBCimmS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

        lhs = GETADDR(RN);
        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs - (templ + templ2));
        }
        else
        {
                setsbc(lhs, templ, lhs - (templ + templ2));
                armregs[RD] = lhs - (templ + templ2);
        }
}

static void opRSCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = rotate2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opRSCimmS(uint32_t opcode)
{
	uint32_t lhs, templ, templ2;

        lhs = GETADDR(RN);
        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                arm_write_r15(opcode, templ - (lhs + templ2));
        }
        else
        {
                setsbc(templ, lhs, templ - (lhs + templ2));
                armregs[RD] = templ - (lhs + templ2);
        }
}

static void opTSTimm(uint32_t opcode)
{
	uint32_t lhs;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                /* TSTP imm */
                arm_compare_rd15(opcode, lhs & rotate2(opcode));
        }
        else
        {
                setzn(lhs & rotate(opcode));
        }
}

static void opMSRcimm(uint32_t opcode)
{
	if (RD == 15) {
		arm_write_cpsr(opcode, rotate2(opcode));
	} else {
		bad_opcode(opcode);
	}
}

static void opTEQimm(uint32_t opcode)
{
	uint32_t lhs;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                /* TEQP imm */
                arm_compare_rd15(opcode, lhs ^ rotate2(opcode));
        }
        else
        {
                setzn(lhs ^ rotate(opcode));
        }
}

static void opCMPimm(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

        lhs = GETADDR(RN);
        rhs = rotate2(opcode);
        dest = lhs - rhs;
        if (RD==15)
        {
                /* CMPP imm */
                arm_compare_rd15(opcode, dest);
        }
        else
        {
                setsub(lhs, rhs, dest);
        }
}

static void opCMNimm(uint32_t opcode)
{
	uint32_t lhs, rhs, dest;

        lhs = GETADDR(RN);
        rhs = rotate2(opcode);
        dest = lhs + rhs;
        if (RD==15)
        {
                /* CMNP imm */
                arm_compare_rd15(opcode, dest);
        } else {
                setadd(lhs, rhs, dest);
        }
}

static void opORRimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opORRimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs | rotate2(opcode));
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD] = lhs | templ;
                setzn(armregs[RD]);
        }
}

static void opMOVimm(uint32_t opcode)
{
	uint32_t dest;

	dest = rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opMOVimmS(uint32_t opcode)
{
        if (RD==15)
        {
                arm_write_r15(opcode, rotate2(opcode));
        }
        else
        {
                armregs[RD]=rotate(opcode);
                setzn(armregs[RD]);
        }
}

static void opBICimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opBICimmS(uint32_t opcode)
{
	uint32_t lhs, templ;

        lhs = GETADDR(RN);
        if (RD==15)
        {
                arm_write_r15(opcode, lhs & ~rotate2(opcode));
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD] = lhs & ~templ;
                setzn(armregs[RD]);
        }
}
 
static void opMVNimm(uint32_t opcode)
{
	uint32_t dest;

	dest = ~rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opMVNimmS(uint32_t opcode)
{
        if (RD==15)
        {
                arm_write_r15(opcode, ~rotate2(opcode));
        }
        else
        {
                armregs[RD]=~rotate(opcode);
                setzn(armregs[RD]);
        }
}

static int opSTRT(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	writememl(addr & ~3, armregs[RD]);
	memmode = templ;

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	/* Writeback */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}
	addr += addr2;
	armregs[RN] = addr;

	return 0;
}

static int opLDRT(uint32_t opcode)
{
	uint32_t templ, templ2;
	uint32_t addr, addr2;

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	templ2 = readmeml(addr & ~3);
	memmode = templ;

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	/* Rotate if load is unaligned */
	if (addr & 3) {
		templ2 = ldrresult(templ2, addr);
	}

	/* Writeback */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}
	addr += addr2;
	armregs[RN] = addr;

	/* Write Rd */
	LOADREG(RD, templ2);

	return 0;
}

static int opSTRBT(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	writememb(addr, armregs[RD]);
	memmode = templ;

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	/* Writeback */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}
	addr += addr2;
	armregs[RN] = addr;

	return 0;
}

static int opLDRBT(uint32_t opcode)
{
	uint32_t templ, templ2;
	uint32_t addr, addr2;

	addr = GETADDR(RN);

	/* Temporarily switch to user permissions */
	templ = memmode;
	memmode = 0;
	templ2 = readmemb(addr);
	memmode = templ;

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	/* Writeback */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}
	addr += addr2;
	armregs[RN] = addr;

	/* Write Rd */
	LOADREG(RD, templ2);

	return 0;
}

static int opSTR(uint32_t opcode)
{
	uint32_t addr, addr2, value;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += addr2;
	}

	/* Store */
	value = armregs[RD];
	if (RD == 15) {
		value += r15diff;
	}
	writememl(addr & ~3, value);

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += addr2;
		armregs[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		armregs[RN] = addr;
	}

	return 0;
}

static int opLDR(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += addr2;
	}

	/* Load */
	templ = readmeml(addr & ~3);

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	/* Rotate if load is unaligned */
	if (addr & 3) {
		templ = ldrresult(templ, addr);
	}

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += addr2;
		armregs[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		armregs[RN] = addr;
	}

	/* Write Rd */
	LOADREG(RD, templ);

	return 0;
}

static int opSTRB(uint32_t opcode)
{
	uint32_t addr, addr2;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += addr2;
	}

	/* Store */
	writememb(addr, armregs[RD]);

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += addr2;
		armregs[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		armregs[RN] = addr;
	}

	return 0;
}

static int opLDRB(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	if ((opcode & 0x2000010) == 0x2000010) {
		undefined();
		return 0;
	}

	addr = GETADDR(RN);

	/* Calculate offset */
	if (opcode & 0x2000000) {
		addr2 = shift_ldrstr(opcode);
	} else {
		addr2 = opcode & 0xfff;
	}
	if (!(opcode & 0x800000)) {
		addr2 = -addr2;
	}

	/* Pre-indexed */
	if (opcode & 0x1000000) {
		addr += addr2;
	}

	/* Load */
	templ = readmemb(addr);

	/* Check for Abort */
	if (armirq & 0x40)
		return 1;

	if (!(opcode & 0x1000000)) {
		/* Post-indexed */
		addr += addr2;
		armregs[RN] = addr;
	} else if (opcode & 0x200000) {
		/* Pre-indexed with writeback */
		armregs[RN] = addr;
	}

	/* Write Rd */
	LOADREG(RD, templ);

	return 0;
}

static int opSTMD(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN] - templ;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_store_multiple(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opSTMI(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN];
	writeback = addr + templ;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_store_multiple(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opSTMDS(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN] - templ;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_store_multiple_s(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opSTMIS(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN];
	writeback = addr + templ;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_store_multiple_s(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opLDMD(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN] - templ;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_load_multiple(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opLDMI(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN];
	writeback = addr + templ;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_load_multiple(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opLDMDS(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN] - templ;
	writeback = addr;
	if (!(opcode & (1 << 24))) {
		/* Decrement After */
		addr += 4;
	}
	arm_load_multiple_s(opcode, addr, writeback);
	return (armirq & 0x40);
}

static int opLDMIS(uint32_t opcode)
{
	uint32_t templ, addr, writeback;

	templ = countbits(opcode & 0xffff);
	addr = armregs[RN];
	writeback = addr + templ;
	if (opcode & (1 << 24)) {
		/* Increment Before */
		addr += 4;
	}
	arm_load_multiple_s(opcode, addr, writeback);
	return (armirq & 0x40);
}


static void opB(uint32_t opcode)
{
	uint32_t templ;

	/* Extract offset bits, and sign-extend */
	templ = (opcode << 8);
	templ = (uint32_t) ((int32_t) templ >> 6);

        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
        blockend=1;
}

static void opBL(uint32_t opcode)
{
	uint32_t templ;

	/* Extract offset bits, and sign-extend */
	templ = (opcode << 8);
	templ = (uint32_t) ((int32_t) templ >> 6);

        armregs[14]=armregs[15]-4;
        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
        refillpipeline();
}


static void opMCR(uint32_t opcode)
{
#ifdef FPA
        if (MULRS==1)
        {
                fpaopcode(opcode);
        }
        else
#endif
        if (MULRS==15 && (opcode&0x10))
        {
                writecp15(RN,armregs[RD],opcode);
        }
        else
        {
                undefined();
        }
}

static void opMRC(uint32_t opcode)
{
#ifdef FPA
        if (MULRS==1)
        {
                fpaopcode(opcode);
        }
        else
#endif
        if (MULRS==15 && (opcode&0x10))
        {
                if (RD==15) armregs[RD]=(armregs[RD]&r15mask)|(readcp15(RN)&~r15mask);
                else        armregs[RD]=readcp15(RN);
        }
        else
        {
                undefined();
        }
}

static void opcopro(uint32_t opcode)
{
#ifdef FPA
        if ((opcode&0xF00)==0x100 || (opcode&0xF00)==0x200)
           fpaopcode(opcode);
        else
        {
                undefined();
        }
#else
        undefined();
#endif
}

#endif
