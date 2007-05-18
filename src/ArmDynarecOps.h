#ifdef INARMC
uint32_t templ,templ2;
void opANDreg(unsigned long opcode)
{
	if (((opcode&0xE00090)==0x90)) /*MUL*/
	{
		armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
	{
		if (RD==15)
		{
			templ=shift2(opcode);
			armregs[15]=(((GETADDR(RN)&templ)+4)&r15mask)|(armregs[15]&~r15mask);
			refillpipeline();
		}
		else
		{
			templ=shift2(opcode);
			armregs[RD]=GETADDR(RN)&templ;
		}
	}
	//inscount++; //r//inscount++;
}

void opANDregS(unsigned long opcode)
{
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

void opEORreg(unsigned long opcode)
{
	if (((opcode&0xE000090)==0x90)) /*MLA*/
	{
		armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
		if (MULRD==MULRM) armregs[MULRD]=0;
	}
	else
        {
		if (RD==15)
		{
			templ=shift2(opcode);
			armregs[15]=(((GETADDR(RN)^templ)+4)&r15mask)|(armregs[15]&~r15mask);
			refillpipeline();
		}
		else
		{
			templ=shift2(opcode);
			armregs[RD]=GETADDR(RN)^templ;
		}
        }
	//inscount++; //r//inscount++;
}

void opEORregS(unsigned long opcode)
{
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

void opSUBreg(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(((GETADDR(RN)-templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=shift2(opcode);
                armregs[RD]=GETADDR(RN)-templ;
        }
	//inscount++; //r//inscount++;
}

void opSUBregS(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(GETADDR(RN)-templ)+4;
                refillpipeline();
        }
        else
        {
                templ=shift2(opcode);
                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                armregs[RD]=GETADDR(RN)-templ;
        }
        //inscount++; //r//inscount++;
}

void opRSBreg(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(((templ-GETADDR(RN))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=shift2(opcode);
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
}

void opRSBregS(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(templ-GETADDR(RN))+4;
                refillpipeline();
        }
        else
        {
                templ=shift2(opcode);
                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
}

void opADDreg(unsigned long opcode)
{
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
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=((GETADDR(RN)+templ+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                   armregs[RD]=GETADDR(RN)+shift2(opcode);
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opADDregS(unsigned long opcode)
{
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
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=GETADDR(RN)+templ+4;
                        refillpipeline();
                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                }
                else
                {
                        templ=shift2(opcode);
                        setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                        armregs[RD]=GETADDR(RN)+templ;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opADCreg(unsigned long opcode)
{
#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*Long MUL*/
	{
                error("Bad opcode %08X\n",opcode);
                exit(-1);
        }
        else
        {
#endif
                if (RD==15)
                {
                        templ2=CFSET;
                        templ=shift2(opcode);
                        armregs[15]=((GETADDR(RN)+templ+templ2+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        templ2=CFSET;
                        templ=shift2(opcode);
                        armregs[RD]=GETADDR(RN)+templ+templ2;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opADCregS(unsigned long opcode)
{
#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*Long MUL*/
	{
                error("Bad opcode %08X\n",opcode);
                exit(-1);
        }
        else
        {
#endif
                if (RD==15)
                {
                        templ2=CFSET;
                        templ=shift2(opcode);
                        armregs[15]=GETADDR(RN)+templ+templ2+4;
                        refillpipeline();
                }
                else
                {
                        templ2=CFSET;
                        templ=shift2(opcode);
                        setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                        armregs[RD]=GETADDR(RN)+templ+templ2;
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opSBCreg(unsigned long opcode)
{
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
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        templ=shift2(opcode);
                        armregs[RD]=GETADDR(RN)-(templ+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opSBCregS(unsigned long opcode)
{
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
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                        refillpipeline();
                }
                else
                {
                        templ=shift2(opcode);
                        setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                        armregs[RD]=GETADDR(RN)-(templ+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opRSCreg(unsigned long opcode)
{
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
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                        refillpipeline();
                }
                else
                {
                        templ=shift2(opcode);
                        armregs[RD]=templ-(GETADDR(RN)+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opRSCregS(unsigned long opcode)
{
#ifdef STRONGARM
	if (((opcode&0xE000090)==0x000090)) /*Long MUL*/
	{
                error("Bad opcode %08X\n",opcode);
                exit(-1);
        }
        else
        {
#endif
                templ2=(CFSET)?0:1;
                if (RD==15)
                {
                        templ=shift2(opcode);
                        armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                        refillpipeline();
                }
                else
                {
                        templ=shift2(opcode);
                        setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                        armregs[RD]=templ-(GETADDR(RN)+templ2);
                }
#ifdef STRONGARM
        }
#endif
	//inscount++; //r//inscount++;
}

void opSWPword(unsigned long opcode)
{
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

void opTSTreg(unsigned long opcode)
{
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

void opSWPbyte(unsigned long opcode)
{
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

void opTEQreg(unsigned long opcode)
{
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

void opMRSs(unsigned long opcode)
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

void opCMPreg(unsigned long opcode)
{
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

void opMSRs(unsigned long opcode)
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

void opCMNreg(unsigned long opcode)
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

void opORRreg(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(((GETADDR(RN)|templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=shift2(opcode);
                armregs[RD]=GETADDR(RN)|templ;
        }
	//inscount++; //r//inscount++;
}

void opORRregS(unsigned long opcode)
{
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

void opMOVreg(unsigned long opcode)
{
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|((shift2(opcode)+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=shift2(opcode);
	//inscount++; //r//inscount++;
}
void opMOVregS(unsigned long opcode)
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

void opBICreg(unsigned long opcode)
{
        if (RD==15)
        {
                templ=shift2(opcode);
                armregs[15]=(((GETADDR(RN)&~templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=shift2(opcode);
                armregs[RD]=GETADDR(RN)&~templ;
        }
	//inscount++; //r//inscount++;
}

void opBICregS(unsigned long opcode)
{
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

void opMVNreg(unsigned long opcode)
{
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(((~shift2(opcode))+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=~shift2(opcode);
	//inscount++; //r//inscount++;
}

void opMVNregS(unsigned long opcode)
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


void opANDimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)&templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)&templ;
        }
	//inscount++; //r//inscount++;
}

void opANDimmS(unsigned long opcode)
{
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

void opEORimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)^templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)^templ;
        }
	//inscount++; //r//inscount++;
}

void opEORimmS(unsigned long opcode)
{
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

void opSUBimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)-templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)-templ;
        }
	//inscount++; //r//inscount++;
}

void opSUBimmS(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                if (mode&16) armregs[16]=spsr[mode&15];
                armregs[15]=(GETADDR(RN)-templ)+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate2(opcode);
                templ2=GETADDR(RN);
                armregs[RD]=templ2-templ;
                setsub(templ2,templ,templ2-templ);
        }
	//inscount++; //r//inscount++;
}

void opRSBimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((templ-GETADDR(RN))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
}

void opRSBimmS(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(templ-GETADDR(RN))+4;
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                armregs[RD]=templ-GETADDR(RN);
        }
	//inscount++; //r//inscount++;
}

void opADDimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)+templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)+templ;
        }
	//inscount++; //r//inscount++;
}
void opADDimmS(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=GETADDR(RN)+templ+4;
                refillpipeline();
                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        }
        else
        {
                templ=rotate2(opcode);
                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                armregs[RD]=GETADDR(RN)+templ;
        }
	//inscount++; //r//inscount++;
}

void opADCimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ2=CFSET;
                templ=rotate2(opcode);
                armregs[15]=((GETADDR(RN)+templ+templ2+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ2=CFSET;
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)+templ+templ2;
        }
	//inscount++; //r//inscount++;
}
void opADCimmS(unsigned long opcode)
{
        if (RD==15)
        {
                templ2=CFSET;
                templ=rotate2(opcode);
                armregs[15]=GETADDR(RN)+templ+templ2+4;
                refillpipeline();
        }
        else
        {
                templ2=CFSET;
                templ=rotate2(opcode);
                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                armregs[RD]=GETADDR(RN)+templ+templ2;
        }
	//inscount++; //r//inscount++;
}

void opSBCimm(unsigned long opcode)
{
        templ2=(CFSET)?0:1;
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)-(templ+templ2);
        }
	//inscount++; //r//inscount++;
}
void opSBCimmS(unsigned long opcode)
{
        templ2=(CFSET)?0:1;
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                armregs[RD]=GETADDR(RN)-(templ+templ2);
        }
	//inscount++; //r//inscount++;
}

void opRSCimm(unsigned long opcode)
{
        templ2=(CFSET)?0:1;
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=templ-(GETADDR(RN)+templ2);
        }
	//inscount++; //r//inscount++;
}
void opRSCimmS(unsigned long opcode)
{
        templ2=(CFSET)?0:1;
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                armregs[RD]=templ-(GETADDR(RN)+templ2);
        }
	//inscount++; //r//inscount++;
}

void opTSTimm(unsigned long opcode)
{
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

void opMRSc(unsigned long opcode)
{
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

void opTEQimm(unsigned long opcode)
{
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

void opCMPimm(unsigned long opcode)
{
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

void opCMNimm(unsigned long opcode)
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

void opORRimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)|templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)|templ;
        }
	//inscount++; //r//inscount++;
}
void opORRimmS(unsigned long opcode)
{
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

void opMOVimm(unsigned long opcode)
{
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(rotate2(opcode)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=rotate2(opcode);
	//inscount++; //r//inscount++;
}
void opMOVimmS(unsigned long opcode)
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

void opBICimm(unsigned long opcode)
{
        if (RD==15)
        {
                templ=rotate2(opcode);
                armregs[15]=(((GETADDR(RN)&~templ)+4)&r15mask)|(armregs[15]&~r15mask);
                refillpipeline();
        }
        else
        {
                templ=rotate2(opcode);
                armregs[RD]=GETADDR(RN)&~templ;
        }
	//inscount++; //r//inscount++;
}
void opBICimmS(unsigned long opcode)
{
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
 
void opMVNimm(unsigned long opcode)
{
        if (RD==15)
        {
                armregs[15]=(armregs[15]&~r15mask)|(((~rotate2(opcode))+4)&r15mask);
                refillpipeline();
        }
        else
           armregs[RD]=~rotate2(opcode);
	//inscount++; //r//inscount++;
}
void opMVNimmS(unsigned long opcode)
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

uint32_t addr,addr2;

int opSTRT(unsigned long opcode)
{
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

int opLDRT(unsigned long opcode)
{
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

int opSTRBT(unsigned long opcode)
{
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

int opLDRBT(unsigned long opcode)
{
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

int opSTRB4C(unsigned long opcode)
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

int opLDR59(unsigned long opcode)
{
	//inscount++; //r//inscount++;
        addr=(GETADDR(RN)+(opcode&0xFFF));
        templ=readmeml(addr&~3);
        if (addr&3) templ=ldrresult(templ,addr);
        if (armirq&0x40) return 1;
        LOADREG(RD,templ);
        return 0;
}

int opLDR79(unsigned long opcode)
{
	//inscount++; //r//inscount++;
        addr=(GETADDR(RN)+shift_ldrstr(opcode));
        templ=readmeml(addr&~3);
        if (addr&3) templ=ldrresult(templ,addr);
        if (armirq&0x40) return 1;
        LOADREG(RD,templ);
        return 0;
}

int opLDRB7D(unsigned long opcode)
{
	//inscount++; //r//inscount++;
        addr=GETADDR(RN)+shift_ldrstr(opcode);
        templ=readmemb(addr);
        if (armirq&0x40) return 1;
        armregs[RD]=templ;
        return 0;
}

int opSTR(unsigned long opcode)
{
	//inscount++; //r//inscount++;
//        if (ins==6020942) rpclog("STR %08X %i\n",PC,blockend);
        if ((opcode&0x2000010)==0x2000010)
        {
                undefined();
                return 0;
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

int opLDR(unsigned long opcode)
{
	//inscount++; //r//inscount++;
        if ((opcode&0x2000010)==0x2000010)
        {
                undefined();
                return 0;
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

int opLDRB(unsigned long opcode)
{
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

int opSTRB(unsigned long opcode)
{
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

uint16_t mask;
int c;

int opSTMD(unsigned long opcode)
{
//        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        STMfirst();
//        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        STMall()
//        if (armirq&0x40) armregs[RN]=temp;
        if (!(armirq&0x40) && opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        return (armirq&0x40);
}

int opSTMI(unsigned long opcode)
{
//        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        STMfirst();
//        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        STMall();
//        if (armirq&0x40) armregs[RN]=temp;
        if (!(armirq&0x40) && opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        return (armirq&0x40);
}

int opSTMDS(unsigned long opcode)
{
//        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        STMfirstS();
//        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        STMallS()
//        if (armirq&0x40) armregs[RN]=temp;
        if (!(armirq&0x40) && opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        return (armirq&0x40);
}

int opSTMIS(unsigned long opcode)
{
//        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        STMfirstS();
//        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        STMallS();
//        if (armirq&0x40) armregs[RN]=temp;
        if (!(armirq&0x40) && opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        return (armirq&0x40);
}

int opLDMD(unsigned long opcode)
{
        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        LDMall();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

int opLDMI(unsigned long opcode)
{
        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        LDMall();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

int opLDMDS(unsigned long opcode)
{
        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=(armregs[RN]-countbits(opcode&0xFFFF))&~3;
        if (!(opcode&0x1000000)) addr+=4;
        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
        LDMallS();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}

int opLDMIS(unsigned long opcode)
{
        uint32_t temp=armregs[RN];
	//inscount++; //r//inscount++;
        addr=armregs[RN]&~3;
        if (opcode&0x1000000) addr+=4;
        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
        LDMallS();
        if (armirq&0x40) armregs[RN]=temp;
        return (armirq&0x40);
}


void opB(unsigned long opcode)
{
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

void opBL(unsigned long opcode)
{
	//inscount++; //r//inscount++;
        templ=(opcode&0xFFFFFF)<<2;
        if (templ&0x2000000) templ|=0xFC000000;
        armregs[14]=armregs[15]-4;
        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
        refillpipeline();
}


void opMCR(unsigned long opcode)
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

void opMRC(unsigned long opcode)
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

void opcopro(unsigned long opcode)
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
