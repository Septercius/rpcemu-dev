#ifdef INARMC

static void opANDreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) /* MUL */
	{
		armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
	{
		dest = GETADDR(RN) & shift2(opcode);
		arm_write_dest(opcode, dest);
	}
	//inscount++; //r//inscount++;
}

static void opANDregS(uint32_t opcode)
{
	uint32_t templ;

	if ((opcode & 0xf0) == 0x90) /* MULS */
        {
		armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
		if (MULRD==MULRM) armregs[MULRD]=0;
		setzn(armregs[MULRD]);
	}
        else
	{
		if (RD==15)
		{
			templ=shift2(opcode);
			armregs[15]=(GETADDR(RN)&templ)+4;
			refillpipeline();
			if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
		}
		else
		{
			templ=shift(opcode);
			armregs[RD]=GETADDR(RN)&templ;
			setzn(armregs[RD]);
		}
	}
	//inscount++; //r//inscount++;
}

static void opEORreg(uint32_t opcode)
{
	uint32_t dest;

	if ((opcode & 0xf0) == 0x90) /* MLA */
	{
		armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
        {
		dest = GETADDR(RN) ^ shift2(opcode);
		arm_write_dest(opcode, dest);
        }
	//inscount++; //r//inscount++;
}

static void opEORregS(uint32_t opcode)
{
	uint32_t templ;

        if ((opcode & 0xf0) == 0x90) /* MLAS */
        {
                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
		if (MULRD==MULRM) armregs[MULRD]=0;
                setzn(armregs[MULRD]);
        }
        else
        {
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=(GETADDR(RN)^templ)+4;
                        refillpipeline();
                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                }
                else
                {
                        templ=shift(opcode);
                        armregs[RD]=GETADDR(RN)^templ;
                        setzn(armregs[RD]);
                }
        }
	//inscount++; //r//inscount++;
}

static void opSUBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opSUBregS(uint32_t opcode)
{
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(GETADDR(RN)-templ)+4;
                refillpipeline();
        }
        else
        {
                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                armregs[RD]=GETADDR(RN)-templ;
        }
        //inscount++; //r//inscount++;
}

static void opRSBreg(uint32_t opcode)
{
	uint32_t dest;

	dest = shift2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void opRSBregS(uint32_t opcode)
{
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(templ-GETADDR(RN))+4;
                refillpipeline();
        }
        else
        {
                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
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
        }
        else
        {
#endif
		dest = GETADDR(RN) + shift2(opcode);
		arm_write_dest(opcode, dest);
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opADDregS(uint32_t opcode)
{
	uint32_t templ;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* UMULLS */
	{
		uint64_t mula = (uint64_t) armregs[MULRS];
		uint64_t mulb = (uint64_t) armregs[MULRM];
		uint64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
        }
        else
        {
#endif
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=GETADDR(RN)+templ+4;
                        refillpipeline();
                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                }
                else
                {
                        setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                        armregs[RD]=GETADDR(RN)+templ;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
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
        }
        else
        {
#endif
		dest = GETADDR(RN) + shift2(opcode) + CFSET;
		arm_write_dest(opcode, dest);
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opADCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

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
        }
        else
        {
#endif
                templ2=CFSET;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=GETADDR(RN)+templ+templ2+4;
                        refillpipeline();
                }
                else
                {
                        setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                        armregs[RD]=GETADDR(RN)+templ+templ2;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
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
        }
        else
        {
#endif
		dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
		arm_write_dest(opcode, dest);
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opSBCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

#ifdef STRONGARM
	if ((opcode & 0xf0) == 0x90) /* SMULLS */
	{
		int64_t mula = (int64_t) (int32_t) armregs[MULRS];
		int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
		int64_t mulres = mula * mulb;

		armregs[MULRN] = (uint32_t) mulres;
		armregs[MULRD] = (uint32_t) (mulres >> 32);
		arm_flags_long_multiply(mulres);
        }
        else
        {
#endif
                templ2=(CFSET)?0:1;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                        refillpipeline();
                }
                else
                {
                        setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                        armregs[RD]=GETADDR(RN)-(templ+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
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
        }
        else
        {
#endif
		dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
		arm_write_dest(opcode, dest);
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opRSCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

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
        }
        else
        {
#endif
                templ2=(CFSET)?0:1;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                        refillpipeline();
                }
                else
                {
                        setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                        armregs[RD]=templ-(GETADDR(RN)+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
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
                if (!(mode&16))
                {
                        armregs[16]=(armregs[15]&0xF0000000)|(armregs[15]&3);
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                }
                armregs[RD]=armregs[16];
        }
        else
        {
                undefined();
        }
	//inscount++; //r//inscount++;
}

static void opTSTreg(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                opcode&=~0x100000;
                templ=armregs[15]&0x3FFFFFC;
                armregs[15]=((GETADDR(RN)&shift2(opcode))&0xFC000003)|templ;
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
           setzn(GETADDR(RN)&shift(opcode));
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        if (RD==15)
        {
                opcode&=~0x100000;
                templ=armregs[15]&0x3FFFFFC;
                armregs[15]=((GETADDR(RN)^shift2(opcode))&0xFC000003)|templ;
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                setzn(GETADDR(RN)^shift(opcode));
        }
	//inscount++; //r//inscount++;
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
	//inscount++; //r//inscount++;
}

static void opCMPreg(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                opcode&=~0x100000;
                armregs[15]&=0x3FFFFFC;
                armregs[15]|=((GETADDR(RN)-shift2(opcode))&0xFC000003);
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=shift2(opcode);
                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
        }
	//inscount++; //r//inscount++;
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
        if (RD==15)
        {
                opcode&=~0x100000;
                armregs[15]&=0x3FFFFFC;
                armregs[15]|=((GETADDR(RN)+shift2(opcode))&0xFC000003);
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
           setadd(GETADDR(RN),shift2(opcode),GETADDR(RN)+shift2(opcode));
	//inscount++; //r//inscount++;
}

static void opORRreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opORRregS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(GETADDR(RN)|templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=shift(opcode);
                armregs[RD]=GETADDR(RN)|templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
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
                armregs[15]=shift2(opcode)+4;
                if (mode&0x10)
                   armregs[16]=spsr[mode&15];
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                armregs[RD]=shift(opcode);
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
}

static void opBICreg(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~shift2(opcode);
	arm_write_dest(opcode, dest);
}

static void opBICregS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(GETADDR(RN)&~templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=shift(opcode);
                armregs[RD]=GETADDR(RN)&~templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
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
                armregs[15]=(~shift2(opcode))+4;
                refillpipeline();
        }
        else
        {
                armregs[RD]=~shift(opcode);
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
}


static void opANDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opANDimmS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(GETADDR(RN)&templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD]=GETADDR(RN)&templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
}

static void opEORimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) ^ rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opEORimmS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(GETADDR(RN)^templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD]=GETADDR(RN)^templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
}

static void opSUBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opSUBimmS(uint32_t opcode)
{
	uint32_t templ, templ2;

        templ=rotate2(opcode);
        if (RD==15)
        {
                if (mode&16) armregs[16]=spsr[mode&15];
                armregs[15]=(GETADDR(RN)-templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ2=GETADDR(RN);
                armregs[RD]=templ2-templ;
                setsub(templ2,templ,templ2-templ);
        }
	//inscount++; //r//inscount++;
}

static void opRSBimm(uint32_t opcode)
{
	uint32_t dest;

	dest = rotate2(opcode) - GETADDR(RN);
	arm_write_dest(opcode, dest);
}

static void opRSBimmS(uint32_t opcode)
{
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(templ-GETADDR(RN))+4;
                refillpipeline();
        }
        else
        {
                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
}

static void opADDimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opADDimmS(uint32_t opcode)
{
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=GETADDR(RN)+templ+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                armregs[RD]=GETADDR(RN)+templ;
        }
	//inscount++; //r//inscount++;
}

static void opADCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) + rotate2(opcode) + CFSET;
	arm_write_dest(opcode, dest);
}

static void opADCimmS(uint32_t opcode)
{
	uint32_t templ, templ2;

        templ2=CFSET;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=GETADDR(RN)+templ+templ2+4;
                refillpipeline();
        }
        else
        {
                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                armregs[RD]=GETADDR(RN)+templ+templ2;
        }
	//inscount++; //r//inscount++;
}

static void opSBCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) - rotate2(opcode) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opSBCimmS(uint32_t opcode)
{
	uint32_t templ, templ2;

        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                refillpipeline();
        }
        else
        {
                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                armregs[RD]=GETADDR(RN)-(templ+templ2);
        }
	//inscount++; //r//inscount++;
}

static void opRSCimm(uint32_t opcode)
{
	uint32_t dest;

	dest = rotate2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
	arm_write_dest(opcode, dest);
}

static void opRSCimmS(uint32_t opcode)
{
	uint32_t templ, templ2;

        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                refillpipeline();
        }
        else
        {
                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                armregs[RD]=templ-(GETADDR(RN)+templ2);
        }
	//inscount++; //r//inscount++;
}

static void opTSTimm(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                opcode&=~0x100000;
                templ=armregs[15]&0x3FFFFFC;
                armregs[15]=((GETADDR(RN)&rotate2(opcode))&0xFC000003)|templ;
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                setzn(GETADDR(RN)&rotate(opcode));
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        if (RD==15)
        {
                opcode&=~0x100000;
                if (armregs[15]&3)
                {
                        templ=armregs[15]&0x3FFFFFC;
                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xFC000003)|templ;
                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                }
                else
                {
                        templ=armregs[15]&0x0FFFFFFF;
                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xF0000000)|templ;
                }
        }
        else
        {
                setzn(GETADDR(RN)^rotate(opcode));
        }
	//inscount++; //r//inscount++;
}

static void opCMPimm(uint32_t opcode)
{
	uint32_t templ, templ2;

        if (RD==15)
        {
                opcode&=~0x100000;
                armregs[15]&=0x3FFFFFC;
                armregs[15]|=((GETADDR(RN)-rotate2(opcode))&0xFC000003);
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate2(opcode);
                templ2=GETADDR(RN);
                setsub(templ2,templ,templ2-templ);
        }
	//inscount++; //r//inscount++;
}

static void opCMNimm(uint32_t opcode)
{
        if (RD==15)
        {
                opcode&=~0x100000;
                armregs[15]&=0x3FFFFFC;
                armregs[15]|=((GETADDR(RN)+rotate2(opcode))&0xFC000003);
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
           setadd(GETADDR(RN),rotate2(opcode),GETADDR(RN)+rotate2(opcode));
	//inscount++; //r//inscount++;
}

static void opORRimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) | rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opORRimmS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=rotate2(opcode);
                if (armregs[15]&3) armregs[15]=(GETADDR(RN)|templ)+4;
                else               armregs[15]=(((GETADDR(RN)|templ)+4)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD]=GETADDR(RN)|templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
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
                armregs[15]=rotate2(opcode)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                armregs[RD]=rotate(opcode);
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
}

static void opBICimm(uint32_t opcode)
{
	uint32_t dest;

	dest = GETADDR(RN) & ~rotate2(opcode);
	arm_write_dest(opcode, dest);
}

static void opBICimmS(uint32_t opcode)
{
	uint32_t templ;

        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(GETADDR(RN)&~templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate(opcode);
                armregs[RD]=GETADDR(RN)&~templ;
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
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
                armregs[15]=(~rotate2(opcode))+4;
                refillpipeline();
        }
        else
        {
                armregs[RD]=~rotate(opcode);
                setzn(armregs[RD]);
        }
	//inscount++; //r//inscount++;
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

static int opLDRB7D(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr;

	//inscount++; //r//inscount++;
        addr=GETADDR(RN)+shift_ldrstr(opcode);
        templ=readmemb(addr);
        if (armirq&0x40) return 1;
        armregs[RD]=templ;
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

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)    addr+=addr2;
        templ=readmemb(addr);
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        armregs[RD]=templ;
        return 0;
}



#define STMfirst()      mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        if (c==15) { writememl(addr,armregs[c]+4); } \
                                        else       { writememl(addr,armregs[c]); } \
                                        addr+=4; \
                                        cycles--; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMall()        for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        writememl(addr,armregs[c]); \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                writememl(addr,armregs[15]+4); \
                                cycles--; \
                        }

#define STMfirstS()     mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        if (c==15) { writememl(addr,armregs[c]+4); } \
                                        else       { writememl(addr,*usrregs[c]); } \
                                        addr+=4; \
                                        cycles--; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMallS()       for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        writememl(addr,*usrregs[c]); \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                writememl(addr,armregs[15]+4); \
                                cycles--; \
                        }

#define LDMall()        mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        armregs[c]=readmeml(addr); \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask); \
                                cycles--; \
                                refillpipeline(); \
                        }

#define LDMallS()       mask=1; \
                        if (opcode&0x8000) \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr&0xC)) cycles--; \
                                                armregs[c]=readmeml(addr); \
                                                addr+=4; \
                                                cycles--; \
                                        } \
                                        mask<<=1; \
                                } \
                                if (!(addr&0xC)) cycles--; \
                                if ((armregs[15]&3) || (mode&16)) armregs[15]=(readmeml(addr)+4); \
                                else                              armregs[15]=(armregs[15]&0x0C000003)|((readmeml(addr)+4)&0xF3FFFFFC); \
                                if (mode&16) armregs[cpsr]=spsr[mode&15]; \
                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask); \
                                cycles--; \
                                refillpipeline(); \
                        } \
                        else \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr&0xC)) cycles--; \
                                                *usrregs[c]=readmeml(addr); \
                                                addr+=4; \
                                                cycles--; \
                                        } \
                                        mask<<=1; \
                                } \
                        }

static int opSTMD(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        STMfirst();
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        STMall()
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opSTMI(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        STMfirst();
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        STMall();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opSTMDS(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        STMfirstS();
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        STMallS()
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opSTMIS(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        STMfirstS();
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        STMallS();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opLDMD(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        LDMall();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opLDMI(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        LDMall();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opLDMDS(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        LDMallS();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

static int opLDMIS(uint32_t opcode)
{
        uint32_t temp=armregs[RN];
	uint32_t addr;
	uint16_t mask;
	int c;

	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        LDMallS();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
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
	//inscount++; //r//inscount++;
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
	//inscount++; //r//inscount++;
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
	//inscount++; //r//inscount++;
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
