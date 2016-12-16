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

/* FPA emulation
   Not enabled by default due to bugs */

#include <math.h>
#include "rpcemu.h"
#include "mem.h"
#include "arm.h"

#define UNDEFINED  11
#define undefined() exception(UNDEFINED,8,4)

static double fparegs[8] = {0.0}; /*No C variable type for 80-bit floating point, so use 64*/
static uint32_t fpsr = 0, fpcr = 0;

void resetfpa(void)
{
//        fpsr=0;
        fpsr=0x81000000; /*FPA system*/
        fpcr=0;
}

#define FD ((opcode>>12)&7)
#define FN ((opcode>>16)&7)

#define GETADDR(r) ((r == 15) ? (arm.reg[15] & arm.r15_mask) : arm.reg[r])

static inline void setsubf(double op1, double op2)
{
	arm.reg[cpsr] &= 0xfffffff;
	if (op1 == op2) arm.reg[cpsr] |= ZFLAG;
	if (op1 < op2)  arm.reg[cpsr] |= NFLAG;
	if (op1 >= op2) arm.reg[cpsr] |= CFLAG;
	// if ((op1^op2)&(op1^res)&0x80000000) arm.reg[cpsr]|=VFLAG;
}

static const double fconstants[8]={0.0,1.0,2.0,3.0,4.0,5.0,0.5,10.0};

static double convert80to64(uint32_t *temp)
{
        int tempi,len;
        double *tf2=(double *)&temp[4];
        temp[4]=temp[2]>>11;
        temp[4]|=(temp[1]<<21);
        temp[5]=(temp[1]&~0x80000000)>>11;
        tempi=(temp[0]&0x7FFF)-16383;
        len=((tempi>0)?tempi:-tempi)&0x3FF;
        tempi=((tempi>0)?len:-len)+1023;
        temp[5]|=(tempi<<20);
        temp[5]|=(temp[0]&0x80000000);
        return *tf2;
}

static void convert64to80(uint32_t *temp, double tf)
{
        int tempi;
        double *tf2=(double *)&temp[4];
        *tf2=tf;
        temp[0]=temp[5]&0x80000000;
        tempi=((temp[5]>>20)&0x7FF)-1023+16383;
        temp[0]|=(tempi&0x7FFF);
        temp[1]=(temp[5]&0xFFFFF)<<11;
        temp[1]|=((temp[4]>>21)&0x7FF);
        temp[2]=temp[4]<<11;
        if (temp[0]&0x7FFF) temp[1]|=0x80000000;
}
/*Instruction types :
  Opcodes Cx/Dx, CP1 - LDF/STF
  Opcodes Cx/Dx, CP2 - LFM/SFM
  Opcodes Ex, bit 4 clear - Data processing
  Opcodes Ex, bit 4 set   - Register transfer
  Opcodex Ex, bit 4 set, RD=15 - Compare*/
void fpaopcode(uint32_t opcode)
{
        uint32_t temp[6];
        double *tf,*tf2;
        float *tfs;
        double tempf;
        int len,tempi;
        uint32_t addr;
        tf=(double *)temp;
        tf2=(double *)&temp[4];
        tfs=(float *)temp;

        switch ((opcode>>24)&0xF)
        {
                case 0xC: case 0xD:
                if (opcode&0x100) /*LDF/STF*/
                {
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*Single*/
                                *tfs=(float)fparegs[FD];
                                temp[1]=temp[2]=0;
                                len=1;
//                                if (!(opcode&0x100000)) rpclog("Storing %08X %08X %08X %08X %f %f\n",addr,temp[0],temp[1],temp[2],fparegs[FD],*tfs);
                                break;
                                case 0x008000: /*Double*/
                                *tf=fparegs[FD];
                                temp[2]=0;
                                len=2;
                                break;
                                case 0x400000: /*Long*/
                                *tf2=fparegs[FD];
                                temp[0]=temp[5]&0x80000000;
                                tempi=((temp[5]>>20)&0x7FF)-1023+16383;
                                temp[0]|=(tempi&0x7FFF);
//                                temp[0]|=(temp[5]&0x7FFF0000)>>16;
                                temp[1]=(temp[5]&0xFFFFF)<<11;
                                temp[1]|=((temp[4]>>21)&0x7FF);
                                temp[2]=temp[4]<<11;
                                if (temp[0]&0x7FFF) temp[1]|=0x80000000;
                                len=3;
                                break;
                                default:
/*                                arm.reg[15]+=8;
                                undefined();
                                return;*/
                                fatal("Bad LDF/STF size %08X %08X\n", opcode & 0x408000, opcode);
                        }
//                        rpclog("Address %07X len %i\n",addr,len);
                        if (opcode&0x100000)
                        {
                                switch (len)
                                {
                                        case 1:
                                        temp[0] = mem_read32(addr);
                                        break;
                                        case 2:
                                        temp[1] = mem_read32(addr);
                                        temp[0] = mem_read32(addr + 4);
                                        break;
                                        case 3:
                                        temp[0] = mem_read32(addr);
                                        temp[1] = mem_read32(addr + 4);
                                        temp[2] = mem_read32(addr + 8);
                                        break;
                                }
                                switch (opcode&0x408000)
                                {
                                        case 0x000000: /*Single*/
                                        fparegs[FD]=(double)(*tfs);
//                                        rpclog("Loaded %f %f %i %08X %08X %08X %08X\n",*tfs,fparegs[FD],len,addr,temp[0],temp[1],temp[2]);
                                        break;
                                        case 0x008000: /*Double*/
                                        fparegs[FD]=*tf;
//                                        rpclog("F%i = %f\n",FD,(double)fparegs[FD]);
                                        break;

                                        case 0x400000: /*Long*/
                                        temp[4]=temp[2]>>11;
                                        temp[4]|=(temp[1]<<21);
                                        temp[5]=(temp[1]&~0x80000000)>>11;
                                        tempi=(temp[0]&0x7FFF)-16383;
                                        len=((tempi>0)?tempi:-tempi)&0x3FF;
                                        tempi=((tempi>0)?len:-len)+1023;
                                        temp[5]|=(tempi<<20);
//                                        temp[5]|=((temp[0]&0x7FFF)<<16);
                                        temp[5]|=(temp[0]&0x80000000);
                                        fparegs[FD]=*tf2;
//                                        fparegs[FD]=*tf;
//                                        rpclog("F%i = %f\n",FD,(double)fparegs[FD]);
                                        break;
                                }
                        }
                        else
                        {
                                switch (len)
                                {
                                        case 1:
                                        mem_write32(addr, temp[0]);
                                        break;
                                        case 2:
                                        mem_write32(addr, temp[1]);
                                        mem_write32(addr + 4, temp[0]);
                                        break;
                                        case 3:
                                        mem_write32(addr, temp[0]);
                                        mem_write32(addr + 4, temp[1]);
                                        mem_write32(addr + 8, temp[2]);
                                        break;
                                }
                        }
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode & 0x200000) arm.reg[RN] = addr;
                        return;
                }
                if (opcode&0x100000) /*LFM*/
                {
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
//                        rpclog("LFM from %08X %08X %07X\n",GETADDR(RN),addr,PC);
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*4 registers*/
                                temp[0] = mem_read32(addr);
                                temp[1] = mem_read32(addr + 4);
                                temp[2] = mem_read32(addr + 8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 12);
                                temp[1] = mem_read32(addr + 16);
                                temp[2] = mem_read32(addr + 20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 24);
                                temp[1] = mem_read32(addr + 28);
                                temp[2] = mem_read32(addr + 32);
                                fparegs[(FD+2)&7]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 36);
                                temp[1] = mem_read32(addr + 40);
                                temp[2] = mem_read32(addr + 44);
                                fparegs[(FD+3)&7]=convert80to64(&temp[0]);
                                break;
                                case 0x408000: /*3 registers*/
                                temp[0] = mem_read32(addr);
                                temp[1] = mem_read32(addr + 4);
                                temp[2] = mem_read32(addr + 8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 12);
                                temp[1] = mem_read32(addr + 16);
                                temp[2] = mem_read32(addr + 20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 24);
                                temp[1] = mem_read32(addr + 28);
                                temp[2] = mem_read32(addr + 32);
                                fparegs[(FD+2)&7]=convert80to64(&temp[0]);
                                break;
                                case 0x400000: /*2 registers*/
                                temp[0] = mem_read32(addr);
                                temp[1] = mem_read32(addr + 4);
                                temp[2] = mem_read32(addr + 8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0] = mem_read32(addr + 12);
                                temp[1] = mem_read32(addr + 16);
                                temp[2] = mem_read32(addr + 20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                break;
                                case 0x008000: /*1 register*/
                                temp[0] = mem_read32(addr);
                                temp[1] = mem_read32(addr + 4);
                                temp[2] = mem_read32(addr + 8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                break;

                                default:
                                rpclog("Bad number of registers to load %06X\n",opcode&0x408000);
                                dumpregs();
                                exit(-1);
                        }
//                        rpclog("Loaded %08X  %i  %f %f %f %f\n",opcode&0x408000,FD,fparegs[FD],fparegs[(FD+1)&7],fparegs[(FD+2)&7],fparegs[(FD+3)&7]);
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode & 0x200000) arm.reg[RN] = addr;
                        return;
                }
                else /*SFM*/
                {
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
//                        rpclog("SFM from %08X %08X %07X\n",GETADDR(RN),addr,PC);
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*4 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                mem_write32(addr, temp[0]);
                                mem_write32(addr + 4, temp[1]);
                                mem_write32(addr + 8, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                mem_write32(addr + 12, temp[0]);
                                mem_write32(addr + 16, temp[1]);
                                mem_write32(addr + 20, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+2)&7]);
                                mem_write32(addr + 24, temp[0]);
                                mem_write32(addr + 28, temp[1]);
                                mem_write32(addr + 32, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+3)&7]);
                                mem_write32(addr + 36, temp[0]);
                                mem_write32(addr + 40, temp[1]);
                                mem_write32(addr + 44, temp[2]);
                                break;
                                case 0x408000: /*3 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                mem_write32(addr, temp[0]);
                                mem_write32(addr + 4, temp[1]);
                                mem_write32(addr + 8, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                mem_write32(addr + 12, temp[0]);
                                mem_write32(addr + 16, temp[1]);
                                mem_write32(addr + 20, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+2)&7]);
                                mem_write32(addr + 24, temp[0]);
                                mem_write32(addr + 28, temp[1]);
                                mem_write32(addr + 32, temp[2]);
                                break;
                                case 0x400000: /*2 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                mem_write32(addr, temp[0]);
                                mem_write32(addr + 4, temp[1]);
                                mem_write32(addr + 8, temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                mem_write32(addr + 12, temp[0]);
                                mem_write32(addr + 16, temp[1]);
                                mem_write32(addr + 20, temp[2]);
                                break;
                                case 0x008000: /*1 register*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                mem_write32(addr, temp[0]);
                                mem_write32(addr + 4, temp[1]);
                                mem_write32(addr + 8, temp[2]);
                                break;
                                
                                default:
                                rpclog("Bad number of registers to store %06X\n",opcode&0x408000);
                                dumpregs();
                                exit(-1);
                        }
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode & 0x200000) arm.reg[RN] = addr;
                        return;
                }
                /*LFM/SFM*/
                fatal("SFM opcode %08X\n", opcode);
                return;
                case 0xE:
                if (opcode&0x10)
                {
                        if (RD==15 && opcode&0x100000) /*Compare*/
                        {
                                switch ((opcode>>21)&7)
                                {
                                        case 4: /*CMF*/
                                        case 6: /*CMFE*/
                                        if (opcode&8) tempf=fconstants[opcode&7];
                                        else          tempf=fparegs[opcode&7];
                                        setsubf(fparegs[FN],tempf);
                                        return;
                                }
                                fatal("Compare opcode %08X %i\n", opcode, (opcode >> 21) & 7);
                                return;
                        }
                        /*Register transfer*/
                        switch ((opcode>>20)&0xF)
                        {
                                case 0: /*FLT*/
                                fparegs[FN] = (double) (int32_t) arm.reg[RD];
                                return;
                                case 1: /*FIX*/
                                arm.reg[RD] = (uint32_t) (int32_t) fparegs[opcode & 7];
                                return;
                                case 2: /*WFS*/
                                fpsr = (arm.reg[RD] & 0xffffff) | (fpsr & 0xff000000);
                                return;
                                case 3: /*RFS*/
                                arm.reg[RD] = fpsr;
                                return;
                                case 4: /*WFC*/
                                fpcr = (fpcr & ~0xd00) | (arm.reg[RD] & 0xd00);
                                return;
                                case 5: /*RFC*/
                                arm.reg[RD] = fpcr;
                                return;
                        }
                        fatal("Register opcode %08X at %07X\n", opcode, PC);
                        return;
                }
                if (opcode&8) tempf=fconstants[opcode&7];
                else          tempf=fparegs[opcode&7];
//                rpclog("Data %08X %06X\n",opcode,opcode&0xF08000);
//                rpclog("F%i F%i F%i\n",FD,FN,opcode&7);
                if ((opcode&0x8000) && ((opcode&0xF08000)>=0x508000) && ((opcode&0xF08000)<0xE08000))
                {
                        arm.reg[15] += 4;
                        undefined();
                        return;
                }
                switch (opcode&0xF08000)
                {
                        case 0x000000: /*ADF*/
//                        rpclog("ADF %f+%f=",fparegs[FN],tempf);
                        fparegs[FD]=fparegs[FN]+tempf;
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x100000: /*MUF*/
                        case 0x900000: /*FML*/
//                        rpclog("MUF %f*%f=",fparegs[FN],tempf);
                        fparegs[FD]=fparegs[FN]*tempf;
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x200000: /*SUF*/
//                        rpclog("SUF %f-%f=",fparegs[FN],tempf);
                        fparegs[FD]=fparegs[FN]-tempf;
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x300000: /*RSF*/
//                        rpclog("SUF %f-%f=",fparegs[FN],tempf);
                        fparegs[FD]=tempf-fparegs[FN];
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x400000: /*DVF*/
                        case 0xA00000: /*FDV*/
//                        rpclog("DVF %f/%f=",fparegs[FN],tempf);
                        fparegs[FD]=fparegs[FN]/tempf;
//                        rpclog("%f  %07X\n",fparegs[RD],PC);
                        return;
                        case 0x008000: /*MVF*/
//                        rpclog("MVF %f=\n",tempf);
                        fparegs[FD]=tempf;
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x108000: /*MNF*/
//                        rpclog("MNF %f=\n",tempf);
                        fparegs[FD]=-tempf;
//                        rpclog("%f\n",fparegs[RD]);
                        return;
                        case 0x208000: /*ABS*/
                        fparegs[FD]=fabs(tempf);
                        return;
                        case 0x408000: /*SQT*/
                        fparegs[FD]=sqrt(tempf);
                        return;
                        case 0x508000: /*LOG*/
                        fparegs[FD]=log10(tempf);
                        return;
                        case 0x608000: /*LGN*/
                        fparegs[FD]=log(tempf);
                        return;
                        case 0x708000: /*EXP*/
                        fparegs[FD]=exp(tempf);
                        return;
                        case 0x808000: /*SIN*/
//                        rpclog("SIN of %f is ",tempf);
                        fparegs[FD]=sin(tempf);
//                        rpclog("%f\n",fparegs[FD]);
                        return;
                        case 0x908000: /*COS*/
                        fparegs[FD]=cos(tempf);
                        return;
                        case 0xA08000: /*TAN*/
                        fparegs[FD]=tan(tempf);
                        return;
                        case 0xB08000: /*ASN*/
                        fparegs[FD]=asin(tempf);
                        return;
                        case 0xC08000: /*ACS*/
                        fparegs[FD]=acos(tempf);
                        return;
                        case 0xD08000: /*ATN*/
                        fparegs[FD]=atan(tempf);
                        return;
                }
                /*Data processing*/
                fatal("Bad data opcode %08X %06X\n", opcode, opcode & 0xF08000);
                return;
        }
}
