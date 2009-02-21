#ifdef INARMC

static void opANDreg(uint32_t opcode)
{
	uint32_t templ;

	if (((opcode&0xE00090)==0x90)) /*MUL*/
	{
		armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
	{
		templ=shift2(opcode);
		if (RD==15)
		{
			armregs[15]=(((GETADDR(RN)&templ)+4)&r15mask)|(armregs[15]&~r15mask);
			refillpipeline();
		}
		else
		{
			armregs[RD]=GETADDR(RN)&templ;
		}
	}
	//inscount++; //r//inscount++;
}

static void opANDregS(uint32_t opcode)
{
	uint32_t templ;

	if (((opcode&0xE000090)==0x90)) /*MULS*/
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
	uint32_t templ;

	if (((opcode&0xE000090)==0x90)) /*MLA*/
	{
		armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
        {
		templ=shift2(opcode);
		if (RD==15)
		{
			armregs[15]=(((GETADDR(RN)^templ)+4)&r15mask)|(armregs[15]&~r15mask);
			refillpipeline();
		}
		else
		{
			armregs[RD]=GETADDR(RN)^templ;
		}
        }
	//inscount++; //r//inscount++;
}

static void opEORregS(uint32_t opcode)
{
	uint32_t templ;

        if (((opcode&0xE000090)==0x90)) /*MLAS*/
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)-templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                armregs[RD]=GETADDR(RN)-templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(((templ-GETADDR(RN))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

#ifdef STRONGARM
	if ((opcode&0x90)==0x90) /*MULL*/
	{
                uint64_t mula,mulb,mulres;
                mula=(uint64_t)(uint32_t)armregs[MULRS];
                mulb=(uint64_t)(uint32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
        }
        else
        {
#endif
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=((GETADDR(RN)+templ+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                   armregs[RD]=GETADDR(RN)+templ;
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opADDregS(uint32_t opcode)
{
	uint32_t templ;

#ifdef STRONGARM
	if (((opcode&0x90)==0x90)) /*MULLS*/
	{
                uint64_t mula,mulb,mulres;
                mula=(uint64_t)(uint32_t)armregs[MULRS];
                mulb=(uint64_t)(uint32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                armregs[cpsr]&=~0xC0000000;
                if (!(armregs[MULRN]|armregs[MULRD])) armregs[cpsr]|=ZFLAG;
                if (armregs[MULRD]&0x80000000) armregs[cpsr]|=NFLAG;
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
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*UMLAL*/
	{
                uint64_t mula,mulb,mulres;
                uint32_t addr,addr2;
                addr=armregs[MULRN];
                addr2=armregs[MULRD];
                mula=(uint64_t)(uint32_t)armregs[MULRS];
                mulb=(uint64_t)(uint32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                if ((armregs[MULRN]+addr)<armregs[MULRN]) armregs[MULRD]++;
                armregs[MULRN]+=addr;
                armregs[MULRD]+=addr2;
        }
        else
        {
#endif
                templ2=CFSET;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=((GETADDR(RN)+templ+templ2+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        armregs[RD]=GETADDR(RN)+templ+templ2;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opADCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*UMLALS*/
	{
                uint64_t mula,mulb,mulres;
                uint32_t addr,addr2;
                addr=armregs[MULRN];
                addr2=armregs[MULRD];
                mula=(uint64_t)(uint32_t)armregs[MULRS];
                mulb=(uint64_t)(uint32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                if ((armregs[MULRN]+addr)<armregs[MULRN]) armregs[MULRD]++;
                armregs[MULRN]+=addr;
                armregs[MULRD]+=addr2;
                armregs[cpsr]&=~0xC0000000;
                if (!(armregs[MULRN]|armregs[MULRD])) armregs[cpsr]|=ZFLAG;
                if (armregs[MULRD]&0x80000000) armregs[cpsr]|=NFLAG;
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
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*SMULL*/
	{
                int64_t mula,mulb,mulres;
                mula=(int64_t)(int32_t)armregs[MULRS];
                mulb=(int64_t)(int32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
        }
        else
        {
#endif
                templ2=(CFSET)?0:1;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        armregs[RD]=GETADDR(RN)-(templ+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opSBCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*SMULLS*/
	{
                int64_t mula,mulb,mulres;
                mula=(int64_t)(int32_t)armregs[MULRS];
                mulb=(int64_t)(int32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                armregs[cpsr]&=~0xC0000000;
                if (!(armregs[MULRN]|armregs[MULRD])) armregs[cpsr]|=ZFLAG;
                if (armregs[MULRD]&0x80000000) armregs[cpsr]|=NFLAG;
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
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*SMLAL*/
	{
                int64_t mula,mulb,mulres;
                uint32_t addr,addr2;
                addr=armregs[MULRN];
                addr2=armregs[MULRD];
                mula=(int64_t)(int32_t)armregs[MULRS];
                mulb=(int64_t)(int32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                if ((armregs[MULRN]+addr)<armregs[MULRN]) armregs[MULRD]++;
                armregs[MULRN]+=addr;
                armregs[MULRD]+=addr2;
        }
        else
        {
#endif
                templ2=(CFSET)?0:1;
                templ=shift2(opcode);
                if (RD==15)
                {
                        armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        armregs[RD]=templ-(GETADDR(RN)+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

static void opRSCregS(uint32_t opcode)
{
	uint32_t templ, templ2;

#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*SMLALS*/
	{
                int64_t mula,mulb,mulres;
                uint32_t addr,addr2;
                addr=armregs[MULRN];
                addr2=armregs[MULRD];
                mula=(int64_t)(int32_t)armregs[MULRS];
                mulb=(int64_t)(int32_t)armregs[MULRM];
                mulres=mula*mulb;
                armregs[MULRN]=mulres&0xFFFFFFFF;
                armregs[MULRD]=mulres>>32;
                if ((armregs[MULRN]+addr)<armregs[MULRN]) armregs[MULRD]++;
                armregs[MULRN]+=addr;
                armregs[MULRD]+=addr2;
                armregs[cpsr]&=~0xC0000000;
                if (!(armregs[MULRN]|armregs[MULRD])) armregs[cpsr]|=ZFLAG;
                if (armregs[MULRD]&0x80000000) armregs[cpsr]|=NFLAG;
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

static void opSWPbyte(uint32_t opcode)
{
	uint32_t templ;

        if ((opcode&0xF0)==0x90)
        {
                uint32_t addr;
                addr=armregs[RN];
                templ=GETREG(RM);
                LOADREG(RD,readmemb(addr));
                writememb(addr,templ);
        }
        else if (!(opcode&0xFF0)) /*MSR CPSR*/
        {
//                temp=armregs[16];
                armregs[16]&=~msrlookup[(opcode>>16)&0xF];
                armregs[16]|=(armregs[RM]&msrlookup[(opcode>>16)&0xF]);
                templ=armregs[16];
                if (opcode&0x10000)
                {
                        updatemode(armregs[16]&0x1F);
                        if (!(mode&16)) {
                           armregs[15]=(armregs[15]&~3)|(armregs[16]&3);
                           armregs[15]=(armregs[15]&~0xC000000)|((armregs[16]&0xC0) << 20);
                        }
                }
                armregs[16]=templ;
        }
        else
        {
		bad_opcode(opcode);
        }
	//inscount++; //r//inscount++;
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

static void opMRSs(uint32_t opcode)
{
        if (!(opcode&0xFFF)) /*MRS SPSR*/
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

static void opMSRs(uint32_t opcode)
{
        if (!(opcode&0xFF0)) /*MSR SPSR*/
        {
//                temp=spsr[mode&15];
                spsr[mode&15]&=~msrlookup[(opcode>>16)&0xF];
                spsr[mode&15]|=(armregs[RM]&msrlookup[(opcode>>16)&0xF]);
        }
        else
        {
		bad_opcode(opcode);
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)|templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)|templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|((templ+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=templ;
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)&~templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)&~templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=shift2(opcode);
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(((~templ)+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=~templ;
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)&templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)&templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)^templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)^templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)-templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)-templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((templ-GETADDR(RN))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)+templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)+templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ, templ2;

        templ2=CFSET;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=((GETADDR(RN)+templ+templ2+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)+templ+templ2;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ, templ2;

        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)-(templ+templ2);
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ, templ2;

        templ2=(CFSET)?0:1;
        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=templ-(GETADDR(RN)+templ2);
        }
	//inscount++; //r//inscount++;
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

static void opMRSc(uint32_t opcode)
{
	uint32_t templ;

	templ = rotate2(opcode);
					if (mode & 0xF) {
						if (opcode & 0x10000) {
							armregs[16] = (armregs[16] & ~ 0xFF) | (templ & 0xFF);
							if ((mode & 0x10) == 0) {
								armregs[15] = (armregs[15] & ~ 0x3) | (templ & 0x3);
								armregs[15] = (armregs[15] & ~ 0xC000000) | ((templ & 0xC0) << 20);
							}
						}
						if (opcode & 0x20000)
							armregs[16] = (armregs[16] & ~ 0xFF00) | (templ & 0xFF00);
						if (opcode & 0x40000)
							armregs[16] = (armregs[16] & ~ 0xFF0000) | (templ & 0xFF0000);
					}

                                        if (opcode & 0x80000) {
						armregs[16] = (armregs[16] & ~ 0xFF000000) | (templ & 0xFF000000);
						if ((mode & 0x10) == 0) {
							armregs[15] = (armregs[15] & ~ 0xF0000000) | (templ & 0xF0000000);
						}
					}

//					if (output)
//						rpclog("%08x - %08x\n", armregs[15], armregs[16]);

					if ((armregs[16] & 0x1F) != mode) {
//						if (output)
//							rpclog("changing mode to %02x (was %02x)\n", armregs[16] & 0x1F, mode);

						updatemode(armregs[16] & 0x1F);
					}
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)|templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)|templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(templ&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=templ;
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(((GETADDR(RN)&~templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                armregs[RD]=GETADDR(RN)&~templ;
        }
	//inscount++; //r//inscount++;
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
	uint32_t templ;

        templ=rotate2(opcode);
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(((~templ)+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=~templ;
	//inscount++; //r//inscount++;
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

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        addr&=~3;
        templ=memmode;
        memmode=0;
        writememl(addr,armregs[RD]);
        memmode=templ;
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        return 0;
}

static int opLDRT(uint32_t opcode)
{
	uint32_t templ, templ2;
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        templ=memmode;
        memmode=0;
        templ2=readmeml(addr&~3);
        memmode=templ;
        if (armirq&0x40) return 1;
        if (addr&3) templ2=ldrresult(templ2,addr);
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        LOADREG(RD,templ2);
        return 0;
}

static int opSTRBT(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        templ=memmode;
        memmode=0;
        writememb(addr,armregs[RD]);
        memmode=templ;
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        return 0;
}

static int opLDRBT(uint32_t opcode)
{
	uint32_t templ, templ2;
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        templ=memmode;
        memmode=0;
        templ2=readmemb(addr);
        memmode=templ;
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        LOADREG(RD,templ2);
        return 0;
}

static int opSTRB46(uint32_t opcode)
{
	//inscount++; //r//inscount++;
	writememb(armregs[RN],armregs[RD]);
	if (armirq&0x40)
        {
                rpclog("Data abort! %07X\n",PC);
                return 1;
        }
        armregs[RN]-=(opcode&0xFFF);
        return 0;
}

static int opSTRB4C(uint32_t opcode)
{
	//inscount++; //r//inscount++;
	writememb(armregs[RN],armregs[RD]);
	if (armirq&0x40)
        {
                rpclog("Data abort! %07X\n",PC);
                return 1;
        }
        armregs[RN]+=(opcode&0xFFF);
        return 0;
}

static int opLDR59(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr;

	//inscount++; //r//inscount++;
        addr=(GETADDR(RN)+(opcode&0xFFF));
        templ=readmeml(addr&~3);
        if (addr&3) templ=ldrresult(templ,addr);
        if (armirq&0x40) return 1;
        LOADREG(RD,templ);
        return 0;
}

static int opLDR79(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr;

	//inscount++; //r//inscount++;
        addr=(GETADDR(RN)+shift_ldrstr(opcode));
        templ=readmeml(addr&~3);
        if (addr&3) templ=ldrresult(templ,addr);
        if (armirq&0x40) return 1;
        LOADREG(RD,templ);
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
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
//        if (ins==6020942) rpclog("STR %08X %i\n",PC,blockend);
        if ((opcode&0x2000010)==0x2000010)
        {
                undefined();
                return 1;
        }
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        addr&=~3;
        if (RD==15) { writememl(addr,armregs[RD]+r15diff); }
        else        { writememl(addr,armregs[RD]); }
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        return 0;
//        if (ins==6020942) rpclog("STR complete %i\n",blockend);
}

static int opLDR(uint32_t opcode)
{
	uint32_t templ;
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
        if ((opcode&0x2000010)==0x2000010)
        {
                undefined();
                return 1;
        }
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        templ=readmeml(addr&~3);
        if (addr&3) templ=ldrresult(templ,addr);
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
        LOADREG(RD,templ);
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

static int opSTRB(uint32_t opcode)
{
	uint32_t addr, addr2;

	//inscount++; //r//inscount++;
        addr=GETADDR(RN);
        if (opcode&0x2000000) addr2=shift_ldrstr(opcode);
        else                  addr2=opcode&0xFFF;
        if (!(opcode&0x800000))  addr2=-addr2;
        if (opcode&0x1000000)
           addr+=addr2;
        writememb(addr,armregs[RD]);
        if (armirq&0x40) return 1;
        if (!(opcode&0x1000000))
        {
                addr+=addr2;
                armregs[RN]=addr;
        }
        else if (opcode&0x200000) armregs[RN]=addr;
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

	//inscount++; //r//inscount++;
        templ=(opcode&0xFFFFFF)<<2;
        if (templ&0x2000000) templ|=0xFC000000;
        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
#ifdef PREFETCH
        templ=(PC-4)>>2;
        if ((templ>>10)!=pccache)
        {
                pccache=templ>>10;
                pccache2=getpccache(templ<<2);
                if ((uint32_t)pccache2==0xFFFFFFFF) pccache=(uint32_t)pccache2;
                else                                opcode2=pccache2[templ];
        }
        else opcode2=pccache2[templ];
        templ++;
        if (!(templ&0x3FF) || (uint32_t)pccache2==0xFFFFFFFF)
        {
                pccache=templ>>10;
                pccache2=getpccache(templ<<2);
                if ((uint32_t)pccache2==0xFFFFFFFF) pccache=(uint32_t)pccache2;
                else                                opcode3=pccache2[templ];
        }
        else opcode3=pccache2[templ];
#endif
        blockend=1;
}

static void opBL(uint32_t opcode)
{
	uint32_t templ;

	//inscount++; //r//inscount++;
        templ=(opcode&0xFFFFFF)<<2;
        if (templ&0x2000000) templ|=0xFC000000;
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
