#include "rpcemu.h"

#ifdef DYNAREC
unsigned long readmeml(unsigned long a);
//void writememl(unsigned long a, unsigned long v);
/*RPCemu v0.6 by Tom Walker
  SA110 emulation
  Dynamic recompiling version!*/
int swiout=0;
int times8000=0;
unsigned long abortaddr,abortaddr2;
int twice=0;
unsigned long lastblockfunc;
int blockend;
#define STRONGARM

/*Preliminary FPA emulation. This works to an extent - !Draw works with it, !SICK
  seems to (FPA Whetstone scores are around 100x without), but !AMPlayer doesn't
  work, and GCC stuff tends to crash.*/
//#define FPA

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int inscounts[256];
//#include <allegro.h>

#include "rpcemu.h"
#include "hostfs.h"
//#include "codegen_x86.h"
#include "keyboard.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cp15.h"
#include "82c711.h"

/*unsigned long readmeml(unsigned long a)
{
        if (mmu) rpclog("readmeml slow %08X %07X\n",a,PC);
        if (vraddrl[(a)>>12]&1)
           return readmemfl(a);
        else
        {
                rpclog("Readmeml %08X %08X\n",a,PC);
                return *(unsigned long *)((a)+(vraddrl[(a)>>12]&~3));
        }
}*/

int r15diff;
int flyback;
//static int r11check=0;
static int oldmode;
static int fdci=0;
//static int out2=0;
//static int r8match=0;
static int mmask;
int r15mask;
int memmode;
int irq;
static int cycles;
int prefabort;
//static void refillpipeline(void);
static void refillpipeline2(void);
//static char bigs[256];
//static FILE *olog;
uint32_t rotatelookup[4096];
uint32_t inscount;
//static unsigned char cmosram[256];
int armirq=0;
uint32_t output=0;
int cpsr;
uint32_t *pcpsr;
unsigned char *pcpsrb;

uint32_t *usrregs[16],userregs[17],superregs[17],fiqregs[17],irqregs[17],abortregs[17],undefregs[17],systemregs[17];
uint32_t spsr[16];
uint32_t armregs[18];
uint32_t mode;
int databort;
uint32_t opcode,opcode2,opcode3;
int prog32;


#define USER       0
#define FIQ        1
#define IRQ        2
#define SUPERVISOR 3
#define ABORT      7
#define UNDEFINED  11
#define SYSTEM     15

#define NFSET ((armregs[cpsr]&0x80000000)?1:0)
#define ZFSET ((armregs[cpsr]&0x40000000)?1:0)
#define CFSET ((armregs[cpsr]&0x20000000)?1:0)
#define VFSET ((armregs[cpsr]&0x10000000)?1:0)

#define NFLAG 0x80000000
#define ZFLAG 0x40000000
#define CFLAG 0x20000000
#define VFLAG 0x10000000
#define IFLAG 0x08000000

//int RD;

#define GETADDR(r) ((r==15)?(armregs[15]&r15mask):armregs[r])
#define LOADREG(r,v) if (r==15) { armregs[15]=(armregs[15]&~r15mask)|(((v)+4)&r15mask); refillpipeline(); } else armregs[r]=(v);
#define GETREG(r) ((r==15) ? armregs[15]+4 : armregs[r])
#define LDRRESULT(a,v) ((a&3)?(v>>((a&3)<<3))|(v<<(((a&3)^3)<<3)):v)

uint32_t ins=0;

/*0=i/o, 1=all, 2=r/o, 3=os r/o, 4=super only, 5=read mem, write io*/
/*0=user, 1=os, 2=super*/
static const int modepritabler[3][6]=
{
        {0,1,1,0,0,1},
        {0,1,1,1,0,1},
        {0,1,1,1,1,1}
};
static const int modepritablew[3][6]=
{
        {0,1,0,0,0,0},
        {0,1,1,0,0,0},
        {0,1,1,1,1,0}
};

//static int osmode=0;

uint32_t pccache,*pccache2;

#ifdef PREFETCH
static void refillpipeline(void)
{
        uint32_t addr=(PC-4)>>2;
        if ((addr>>10)!=pccache)
        {
                pccache=addr>>10;
                pccache2=getpccache(addr<<2);
                if ((uint32_t)pccache2==0xFFFFFFFF)
                {
                        opcode2=opcode3=0xFFFFFFFF;
                        pccache=(uint32_t)pccache2;
                        return;
                }
        }
        opcode2=pccache2[addr];
        addr++;
        if (!(addr&0x3FF))
        {
                pccache=addr>>10;
                pccache2=getpccache(addr<<2);
                if ((uint32_t)pccache2==0xFFFFFFFF)
                {
                        opcode3=0xFFFFFFFF;
                        pccache=(uint32_t)pccache2;
                        return;
                }
        }
        opcode3=pccache2[addr];
}
#else
#define refillpipeline() blockend=1;
#endif

void updatemode(uint32_t m)
{
        uint32_t c,om=mode;
        oldmode=mode;
//        if (output) rpclog("Switch from mode %i to %i %07X\n",oldmode,m,PC);
//        if (PC==0x8E30) output=1;
//      if (output) rpclog("Update mode to %s mode %i %08X\n",(m&0x10)?"32-bit":"26-bit",m&15,PC);
//      timetolive=1000;
//      if (!m && PC==0x9FB8) output=2;
//      if (!m && PC==0x9FBC) output=2;
        usrregs[15]=&armregs[15];
        switch (mode&15) /*Store back registers*/
        {
            case USER:
                for (c=8;c<15;c++) userregs[c]=armregs[c];
                break;

            case IRQ:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                irqregs[0]=armregs[13];
                irqregs[1]=armregs[14];
                break;

            case FIQ:
                for (c=8;c<15;c++) fiqregs[c]=armregs[c];
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                superregs[0]=armregs[13];
                superregs[1]=armregs[14];
                break;

            case ABORT:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                abortregs[0]=armregs[13];
                abortregs[1]=armregs[14];
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                undefregs[0]=armregs[13];
                undefregs[1]=armregs[14];
                break;

            case SYSTEM:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                systemregs[0]=armregs[13];
                systemregs[1]=armregs[14];
                break;
        }
        mode=m;
        memmode=1;

        switch (m&15)
        {
            case USER:
                for (c=8;c<15;c++) armregs[c]=userregs[c];
                for (c=0;c<15;c++) usrregs[c]=&armregs[c];
                memmode=0;
                break;

            case IRQ:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=irqregs[0];
                armregs[14]=irqregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
            
            case FIQ:
                for (c=8;c<15;c++) armregs[c]=fiqregs[c];
                for (c=0;c<8;c++)  usrregs[c]=&armregs[c];
                for (c=8;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=superregs[0];
                armregs[14]=superregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
            
            case ABORT:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=abortregs[0];
                armregs[14]=abortregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=undefregs[0];
                armregs[14]=undefregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
 
            case SYSTEM:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=systemregs[0];
                armregs[14]=systemregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;

            default:
                error("Bad mode %i\n",mode);
                rpclog("%i : %07X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X  %08X %08X %08X - %08X %08X %08X %i R10=%08X R11=%08X R12=%08X %08X %08X %08X %08X\n",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[9],armregs[12],armregs[13],armregs[14],armregs[15],armregs[16],opcode,mode,armregs[10],armregs[11],armregs[12],spsr[mode&15],armregs[16],armregs[15],armregs[14]);
                dumpregs();
                exit(-1);
        }

        if (mode&16)
        {
                mmask=31;
                cpsr=16;
                pcpsr=&armregs[16];
//                printf("Now 32-bit mode %i %08X %i\n",mode&15,PC,ins);
                r15mask=0xFFFFFFFC;
                if (!(om&16))
                {
                        armregs[16]=(armregs[15]&0xF0000000)|mode;
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                        armregs[15]&=0x3FFFFFC;
  //                      if (output) printf("Switching to 32-bit mode : CPSR %08X\n",armregs[16]);
                }
        }
        else
        {
                mmask=3;
                cpsr=15;
                pcpsr=&armregs[15];
//                printf("Now 26-bit mode %i %08X %i\n",mode&15,PC,ins);
                r15mask=0x3FFFFFC;
                if (om&16)
                {
                        armregs[15]&=r15mask;
                        armregs[15]|=(mode&3);
                        armregs[15]|=(armregs[16]&0xF0000000);
                        armregs[15]|=((armregs[16]&0xC0)<<20);
//                        printf("R15 now %08X\n",armregs[15]);
                }
        }
        pcpsrb=((unsigned char *)pcpsr)+3;
}

int stmlookup[256];
//#define countbits(c) countbitstable[c]
int countbitstable[65536];
void resetarm(void)
{
        int c,d,exec = 0,data;
//        atexit(dumpregs);
        uint32_t rotval,rotamount;
        resetcodeblocks();
        for (c=0;c<256;c++) inscounts[c]=0;
        for (c=0;c<256;c++)
        {
                stmlookup[c]=0;
                for (d=0;d<8;d++)
                {
                        if (c&(1<<d)) stmlookup[c]+=4;
                }
        }
        for (c=0;c<65536;c++)
        {
                countbitstable[c]=0;
                for (d=0;d<16;d++)
                {
                        if (c&(1<<d)) countbitstable[c]+=4;
                }
        }
        r15mask=0x3FFFFFC;
//        if (!olog)
//           olog=fopen("armlog.txt","wt");
        pccache=0xFFFFFFFF;
        updatemode(SUPERVISOR);
        cpsr=15;
//        prog32=1;
        for (c=0;c<16;c++)
        {
                for (d=0;d<16;d++)
                {
                        armregs[15]=d<<28;
                        switch (c)
                        {
                                case 0:  /*EQ*/ exec=ZFSET; break;
                                case 1:  /*NE*/ exec=!ZFSET; break;
                                case 2:  /*CS*/ exec=CFSET; break;
                                case 3:  /*CC*/ exec=!CFSET; break;
                                case 4:  /*MI*/ exec=NFSET; break;
                                case 5:  /*PL*/ exec=!NFSET; break;
                                case 6:  /*VS*/ exec=VFSET; break;
                                case 7:  /*VC*/ exec=!VFSET; break;
                                case 8:  /*HI*/ exec=(CFSET && !ZFSET); break;
                                case 9:  /*LS*/ exec=(!CFSET || ZFSET); break;
                                case 10: /*GE*/ exec=(NFSET == VFSET); break;
                                case 11: /*LT*/ exec=(NFSET != VFSET); break;
                                case 12: /*GT*/ exec=(!ZFSET && (NFSET==VFSET)); break;
                                case 13: /*LE*/ exec=(ZFSET || (NFSET!=VFSET)); break;
                                case 14: /*AL*/ exec=1; break;
                                case 15: /*NV*/ exec=0; break;
                        }
                        flaglookup[c][d]=(unsigned char)exec;
                }
        }

        for (data=0;data<4096;data++)
        {
                rotval=data&0xFF;
                rotamount=((data>>8)&0xF)<<1;
                rotval=(rotval>>rotamount)|(rotval<<(32-rotamount));
                rotatelookup[data]=rotval;
        }

        armregs[15]=0x0C000008|3;
        armregs[16]=SUPERVISOR|0xD0;
        mode=SUPERVISOR;
        refillpipeline2();
        resetcp15();
        pccache=0xFFFFFFFF;
        if (model==3) r15diff=0;
        else          r15diff=4;
}

int indumpregs=0;
int insnum[256];

void dumpregs()
{
/*}*/
#if 1
        FILE *f;//,*ff;
        char s[1024];
        int c,d;//,d,e;
        //uint32_t templ;
/*        for (c=0;c<0x100;c++)
        {
                rpclog("Opcode %02X : %09i\n",c,inscounts[c]);
                insnum[c]=c;
        }
        for (c=0;c<0x100;c++)
        {
                for (d=0;d<0x100;d++)
                {
                        if (c>d)
                        {
                                if (inscounts[c]<inscounts[d])
                                {
                                        templ=inscounts[c];
                                        inscounts[c]=inscounts[d];
                                        inscounts[d]=templ;
                                        e=insnum[d];
                                        insnum[d]=insnum[c];
                                        insnum[c]=e;
                                }
                        }
                }
        }
        rpclog("\nOrdered :\n");
        for (c=0;c<0x100;c++)
            rpclog("Opcode %02X : %09i\n",insnum[c],inscounts[c]);*/
//        return;
        if (indumpregs) return;
        indumpregs=1;
//        f=fopen("ram.dmp","wb");
        sprintf(s,"R 0=%08X R 4=%08X R 8=%08X R12=%08X\nR 1=%08X R 5=%08X R 9=%08X R13=%08X\nR 2=%08X R 6=%08X R10=%08X R14=%08X\nR 3=%08X R 7=%08X R11=%08X R15=%08X\n%u %s\n%08X %08X %08X",armregs[0],armregs[4],armregs[8],armregs[12],armregs[1],armregs[5],armregs[9],armregs[13],armregs[2],armregs[6],armregs[10],armregs[14],armregs[3],armregs[7],armregs[11],armregs[15],ins,(mmu)?"MMU enabled":"MMU disabled",oldpc,oldpc2,oldpc3);
//        error("%s",s);
        rpclog("%s",s);
        printf("%s",s);
//        error("PC =%07X ins=%i R12f=%08X CPSR=%08X\n",PC,ins,fiqregs[12],armregs[16]);
//        fwrite(ram,0x400000,1,f);
//        fclose(f);
memmode=1;
/*        f=fopen("kernel.dmp","wb");
        for (c=0xC0000000;c<0xC0200000;c+=4096)
        {
                armirq=0;
                pccache2=getpccache(c);
//                rpclog("pccache %08X\n",pccache2);
                if (pccache2!=0xFFFFFFFF)
                {
//                        rpclog("Writing\n");
                        fwrite(&pccache2[c>>2],4096,1,f);
                }
        }
        fclose(f);
        f=fopen("bsdkernel.dmp","wb");
        for (c=0xF0000000;c<0xF0800000;c+=4096)
        {
                armirq=0;
                pccache2=getpccache(c);
//                rpclog("pccache %08X\n",pccache2);
                if (pccache2!=0xFFFFFFFF)
                {
//                        rpclog("Writing\n");
                        fwrite(&pccache2[c>>2],4096,1,f);
                }
                else
                {
                        for (d=0;d<4096;d++)
                            putc(0xFF,f);
                }
        }
        fclose(f);
        f=fopen("prog.dmp","wb");
        for (c=0x40000000;c<0x40200000;c+=4096)
        {
                armirq=0;
                pccache2=getpccache(c);
//                rpclog("pccache %08X\n",pccache2);
                if (pccache2!=0xFFFFFFFF)
                {
//                        rpclog("Writing\n");
                        fwrite(&pccache2[c>>2],4096,1,f);
                }
        }
        fclose(f);
        f=fopen("ram.dmp","wb");
        for (c=0x8000;c<0x70000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
        f=fopen("ram0.dmp","wb");
        for (c=0x0000;c<0x8000;c++)
            putc(readmemb(c),f);
        fclose(f);

  /*      f=fopen("ram10.dmp","wb");
        for (c=0x10000000;c<0x10800000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
/*        f=fopen("bsdk.dmp","wb");
        for (c=0xF0000000;c<0xF0800000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
/*        f=fopen("ram31.dmp","wb");
        for (c=(PC-0x1000);c<(PC+0x1000);c+=4)
        {
                templ=readmeml(c);
                putc(templ,f);
                putc(templ>>8,f);
                putc(templ>>16,f);
                putc(templ>>24,f);
        }
        fclose(f);
        f=fopen("ram32.dmp","wb");
        for (c=(PC&~0x3FF);c<((PC&~0x3FF)+0x400);c+=4)
        {
                templ=pccache2[c>>2];
                putc(templ,f);
                putc(templ>>8,f);
                putc(templ>>16,f);
                putc(templ>>24,f);
        }
        fclose(f);*/
        f=fopen("rambsd.dmp","wb");
        for (c=0x3D9000;c<0x3DF000;c++)
            putc(readmemb(c),f);
        fclose(f);
/*        f=fopen("ram0.dmp","wb");
        ff=fopen("ram02.dmp","wb");
        for (c=0;c<0x800;c+=4)
        {
                templ=readmeml(c);
                putc(templ,f);
                putc(templ>>8,f);
                putc(templ>>16,f);
                putc(templ>>24,f);
                putc(readmemb(c),ff);
                putc(readmemb(c+1),ff);
                putc(readmemb(c+2),ff);
                putc(readmemb(c+3),ff);
        }
        fclose(ff);
        fclose(f);
        f=fopen("program.dmp","wb");
        armirq=0;
//        databort=0;
        for (c=0;c<0x40000;c++)
        {
                putc(readmemb(c+0x8000),f);
//                if (databort) break;
        }
        fclose(f);*/
        indumpregs=0;
#endif
}

#define dumpregs()
unsigned char readmemfbr()
{
        return readmemb(armregs[0]);
}

unsigned long readmemflr()
{
        return readmeml(armregs[0]);
}

void writememfbr()
{
        writememb(armregs[0],armregs[1]);
}

void writememflr()
{
        writememl(armregs[0],armregs[1]);
}
#define checkneg(v) (v&0x80000000)
#define checkpos(v) !(v&0x80000000)

INLINING void setadd(uint32_t op1, uint32_t op2, uint32_t res)
{
/*        armregs[cpsr]&=0xFFFFFFF;
        if (!res)                           armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))             armregs[cpsr]|=NFLAG;
        if (res<op1)                        armregs[cpsr]|=CFLAG;
        if ((op1^res)&(op2^res)&0x80000000) armregs[cpsr]|=VFLAG;*/
        unsigned long temp=0;
        if (!res)                           temp=ZFLAG;
        else if (checkneg(res))             temp=NFLAG;
        if (res<op1)                        temp|=CFLAG;
        if ((op1^res)&(op2^res)&0x80000000) temp|=VFLAG;
        *pcpsr=((*pcpsr)&0xFFFFFFF)|(temp);
}

INLINING void setsub(uint32_t op1, uint32_t op2, uint32_t res)
{
/*        armregs[cpsr]&=0xFFFFFFF;
        if (!res)                           armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))             armregs[cpsr]|=NFLAG;
        if (res<=op1)                       armregs[cpsr]|=CFLAG;
        if ((op1^op2)&(op1^res)&0x80000000) armregs[cpsr]|=VFLAG;*/
        unsigned long temp=0;
        if (!res)                           temp=ZFLAG;
        else if (checkneg(res))             temp=NFLAG;
        if (res<=op1)                       temp|=CFLAG;
        if ((op1^op2)&(op1^res)&0x80000000) temp|=VFLAG;
        *pcpsr=((*pcpsr)&0xFFFFFFF)|(temp);
}

INLINING void setsbc(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[cpsr]&=0xFFFFFFF;
        if (!res)                           armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))             armregs[cpsr]|=NFLAG;
        if ((checkneg(op1) && checkpos(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkpos(op2) && checkpos(res)))  armregs[cpsr]|=CFLAG;
        if ((checkneg(op1) && checkpos(op2) && checkpos(res)) ||
            (checkpos(op1) && checkneg(op2) && checkneg(res)))
            armregs[cpsr]|=VFLAG;
}

INLINING void setadc(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[cpsr]&=0xFFFFFFF;
        if ((checkneg(op1) && checkneg(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkneg(op2) && checkpos(res)))  armregs[cpsr]|=CFLAG;
        if ((checkneg(op1) && checkneg(op2) && checkpos(res)) ||
            (checkpos(op1) && checkpos(op2) && checkneg(res)))
            armregs[cpsr]|=VFLAG;
        if (!res)                          armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))            armregs[cpsr]|=NFLAG;
}

#if 0
INLINING void setzn(uint32_t op)
{
/*        armregs[cpsr]&=0x3FFFFFFF;
        if (!op)               armregs[cpsr]|=ZFLAG;
        else if (checkneg(op)) armregs[cpsr]|=NFLAG;*/
        unsigned long temp=0;
        if (!op)               temp=ZFLAG;
        else if (checkneg(op)) temp=NFLAG;
        *pcpsr=((*pcpsr)&0x3FFFFFFF)|(temp);
}
#endif

#define setzn(op) templ=0; \
        if (!(op))               templ=ZFLAG; \
        if (checkneg((op))) templ=NFLAG; \
        *pcpsr=((*pcpsr)&0x3FFFFFFF)|(templ);
        
static char err2[512];

uint32_t shift3(uint32_t opcode)
{
        uint32_t shiftmode=opcode&0x60;//(opcode>>5)&3;
        uint32_t shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
        }
        temp=armregs[RM];
        if (shiftamount) armregs[cpsr]&=~CFLAG;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&1) armregs[cpsr]|=CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp<<(shiftamount-1))&0x80000000) armregs[cpsr]|=CFLAG;
                return temp<<shiftamount;

                case 0x20: /*LSR*/
                if (!shiftamount && !(opcode&0x10))
                {
                        shiftamount=32;
                }
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        else                 armregs[cpsr]&=~CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                return temp>>shiftamount;

                case 0x40: /*ASR*/
                if (!shiftamount)
                {
                        if (opcode&0x10) return temp;
                }
                if (shiftamount>=32 || !shiftamount)
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        else                 armregs[cpsr]&=~CFLAG;
                        if (temp&0x80000000) return 0xFFFFFFFF;
                        return 0;
                }
                if (((int)temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                return (int)temp>>shiftamount;

                default: /*ROR*/
                armregs[cpsr]&=~CFLAG;
                if (!shiftamount && !(opcode&0x10))
                {
                        if (temp&1) armregs[cpsr]|=CFLAG;
                        return (((cflag)?1:0)<<31)|(temp>>1);
                }
                if (!shiftamount)
                {
                        armregs[cpsr]|=cflag;
                        return temp;
                }
                if (!(shiftamount&0x1F))
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        return temp;
                }
                if (((temp>>shiftamount)|(temp<<(32-shiftamount)))&0x80000000) armregs[cpsr]|=CFLAG;
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;
        }
}

#define shift(o)  ((o&0xFF0)?shift3(o):armregs[RM])
#define shift2(o) ((o&0xFF0)?shift4(o):armregs[RM])
#define shift_ldrstr(o) shift2(o)
//#define shift_ldrstr(o) ((o&0xFF0)?shift_ldrstr2(o):armregs[RM])

//#if 0
unsigned shift5(unsigned opcode, unsigned shiftmode, unsigned shiftamount, uint32_t rm)
{
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        if (!shiftamount)    return rm;
                        return 0; /*shiftamount>=32*/

                        case 0x20: /*LSR*/
                        if (!shiftamount && (opcode&0x10)) return rm;
                        return 0; /*shiftamount>=32*/

                        case 0x40: /*ASR*/
                        if (!shiftamount && !(opcode&0x10)) shiftamount=32;
                        if (shiftamount>=32)
                        {
                                if (rm&0x80000000)
                                   return 0xFFFFFFFF;
                                return 0;
                        }
                        return (int)rm>>shiftamount;

                        case 0x60: /*ROR*/
                        if (!(opcode&0x10)) return (((CFSET)?1:0)<<31)|(rm>>1);
                        shiftamount&=0x1F;
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));

                        default:
                        sprintf(err2,"Shift2 mode %i amount %i\n",shiftmode,shiftamount);
                        error("%s",err2);
                        dumpregs();
                        exit(-1);
                }
}
//#endif

INLINING unsigned shift4(unsigned opcode)
{
        unsigned shiftmode=opcode&0x60;
        unsigned shiftamount=(opcode&0x10)?(armregs[(opcode>>8)&15]&0xFF):((opcode>>7)&31);
        uint32_t rm=armregs[RM];
//        if (((opcode>>8)&15)==15 && (opcode&0x10)) rpclog("Shift by R15!!\n");
        if ((shiftamount-1)>=31)
        {
                return shift5(opcode,shiftmode,shiftamount,rm);
        }
        else
        {
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm<<shiftamount;
                        case 0x20: /*LSR*/
                        return rm>>shiftamount;
                        case 0x40: /*ASR*/
                        return (int)rm>>shiftamount;
                        default: /*ROR*/
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));
                }
        }
}
#if 0
unsigned shift_ldrstr3(unsigned opcode, unsigned shiftmode, unsigned shiftamount, uint32_t rm)
{
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm;
                        case 0x20: /*LSR*/
                        return 0;
                        case 0x40: /*ASR*/
                        if (rm&0x80000000) return 0xFFFFFFFF;
                        return 0;
                        default: /*ROR*/
                        return (((*pcpsr)/*armregs[cpsr]*/&CFLAG)<<2)|(rm>>1);
//                        return (((CFSET)?1:0)<<31)|(rm>>1);
                }
}

INLINING unsigned shift_ldrstr2(unsigned opcode)
{
        unsigned shiftmode=opcode&0x60;
        unsigned shiftamount=(opcode>>7)&31;
        uint32_t rm=armregs[RM];
        if (!shiftamount)
        {
                return 0;
//                shift_ldrstr3(opcode,shiftmode,shiftamount,rm);
        }
        else
        {
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm<<shiftamount;
                        case 0x20: /*LSR*/
                        return rm>>shiftamount;
                        case 0x40: /*ASR*/
                        return (int)rm>>shiftamount;
                        default: /*ROR*/
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));
                }
        }
}
#endif
INLINING unsigned rotate(unsigned data)
{
        uint32_t rotval;
        rotval=rotatelookup[data&4095];
        if (/*data&0x100000 && */data&0xF00)
        {
//                (*pcpsrb)=((*pcpsrb)&~(CFLAG>>24))|(((rotval>>2)&CFLAG)>>24);
//                /*armregs[cpsr]*/(*pcpsrb)&=~(CFLAG>>24);
//                /*armregs[cpsr]*/(*pcpsrb)|=(((rotval>>2)&CFLAG)>>24);
                if (rotval&0x80000000) armregs[cpsr]|=CFLAG;
                else                   armregs[cpsr]&=~CFLAG;
        }
        return rotval;
}

static const int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

#define undefined() exception(UNDEFINED,8,4)
int hitu=0;

static void refillpipeline2()
{
        #ifdef PREFETCH
        uint32_t addr=PC-8;
        if ((addr>>12)!=pccache)
        {
                pccache=addr>>12;
                pccache2=getpccache(addr);
                if ((uint32_t)pccache2==0xFFFFFFFF)
                {
                        opcode2=opcode3=0xFFFFFFFF;
                        pccache=(uint32_t)pccache2;
                        return;
                }
        }
        opcode2=pccache2[addr>>2];
        addr+=4;
        if ((addr>>12)!=pccache)
        {
                pccache=addr>>12;
                pccache2=getpccache(addr);
                if ((uint32_t)pccache2==0xFFFFFFFF)
                {
                        opcode3=0xFFFFFFFF;
                        pccache=(uint32_t)pccache2;
                        return;
                }
        }
        opcode3=pccache2[addr>>2];
//        opcode2=readmeml(PC-8);
//        opcode3=readmeml(PC-4);
        #endif
        blockend=1;
}

static const uint32_t msrlookup[16]=
{
        0x00000000,0x000000FF,0x0000FF00,0x0000FFFF,
        0x00FF0000,0x00FF00FF,0x00FFFF00,0x00FFFFFF,
        0xFF000000,0xFF0000FF,0xFF00FF00,0xFF00FFFF,
        0xFFFF0000,0xFFFF00FF,0xFFFFFF00,0xFFFFFFFF
};

static void bad_opcode(uint32_t opcode) 
{
     error("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
     rpclog("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
     dumpregs();
     exit(EXIT_FAILURE);
}

void exception(int mmode, uint32_t address, int diff)
{
        uint32_t templ;
        unsigned char irq=0xC0;
//        rpclog("Exception %i %i %02X %08X %i %08X %08X\n",mode&16,prog32,armirq,&armregs[cpsr],cpsr,&armregs[16],pcpsr);
        if (mmode==SUPERVISOR) irq=0x80;
        if (mode&16)
        {
                templ=armregs[15]-diff;
                spsr[mmode]=armregs[16];
                updatemode(mmode|16);
                armregs[14]=templ;
                armregs[16]&=~0x1F;
                armregs[16]|=0x10|mmode|irq;
                armregs[15]=address;
                refillpipeline();
        }
        else if (prog32)
        {
                templ=armregs[15]-diff;
                updatemode(mmode|16);
                armregs[14]=templ&0x3FFFFFC;
                spsr[mmode]=(armregs[16]&~0x1F)|(templ&3);
                spsr[mmode]&=~0x10;
                armregs[16]|=irq;
                armregs[15]=address;
                refillpipeline();
        }
        else
        {
                templ=armregs[15]-diff;
                armregs[15]|=3;
                updatemode(SUPERVISOR);
                armregs[14]=templ;
                armregs[15]&=0xFC000003;
                armregs[15]|=((irq<<20)|address);
                refillpipeline();
        }
}
uint32_t andbefore,andmid,andafter,andpc;

#define INARMC
#include "ArmDynarecOps.h"
void opSWI(unsigned long opcode)
{
	inscount++; rinscount++;
        templ=opcode&0xDFFFF;
//        if ((templ&~0x1F)==0x404C0) rpclog("MIDI SWI %05X at %07X\n",templ,PC);
//        printf("SWI %08X at %07X\n",opcode,PC);
        if (mousehack && templ==7 && armregs[0]==0x15)
        {
                if (readmemb(armregs[1])==1)
                {
                        setmouseparams(armregs[1]);
                        return;
                }
                else if (readmemb(armregs[1])==4)
                {
                        getunbufmouse(armregs[1]);
                        return;
                }
                else if (readmemb(armregs[1])==3)
                {
                        setmousepos(armregs[1]);
                        return;
                }
                else if (readmemb(armregs[1])==5)
                {
                        setmousepos(armregs[1]);
                        return;
                }
                else
                   goto realswi;
        }
        else if (mousehack && templ==0x1C)
        {
                getosmouse();
                armregs[15]&=~VFLAG;
        }
        else if (templ == ARCEM_SWI_HOSTFS)
	{
		ARMul_State state;
		state.Reg = armregs;
	        hostfs(&state);
	}
	else
	{
                realswi:
                if (mousehack && templ==7 && armregs[0]==0x15 && readmemb(armregs[1])==0)
                   setpointer(armregs[1]);
                if (mousehack && templ==6 && armregs[0]==106)
                   osbyte106(armregs[1]);
                exception(SUPERVISOR,0xC,4);
        }
}
unsigned long startpc;
/*void writememl(unsigned long a, unsigned long v)
{
        if (vraddrl[(a)>>12]&3)
           writememfl(a,v);
        else
        {
                *(unsigned long *)((a)+vraddrl[(a)>>12])=v;
        }
}*/

void badopcode(unsigned long opcode)
{
        bad_opcode(opcode);
        exit(-1);
}

unsigned char validforskip[64]=
{
        1,        1,        1,        1,        1,       1,        1,       1,
        1,        1,        1,        1,        1,       1,        1,       1,
        0,        0,        0,        0,        0,       0,        0,       0,
        1,        1,        0,        0,        1,       1,        1,       1,
        
        0,        0,        1,        0,        1,       0,        1,       0,
        1,        0,        1,        0,        1,       0,        1,       0,
        0,        0,        0,        0,        0,       0,        0,       0,
        1,        0,        1,        0,        1,       0,        1,       0
};

typedef void (*OpFn)(unsigned long opcode);

int codeblockpos;
OpFn opcodes[256]=
{
	opANDreg, opANDregS,opEORreg, opEORregS,opSUBreg,opSUBregS,opRSBreg,opRSBregS,   //00
	opADDreg, opADDregS,opADCreg, opADCregS,opSBCreg,opSBCregS,opRSCreg,opRSCregS,   //08
	opSWPword,opTSTreg, opSWPbyte,opTEQreg, opMRSs,  opCMPreg, opMSRs,  opCMNreg,    //10
	opORRreg, opORRregS,opMOVreg, opMOVregS,opBICreg,opBICregS,opMVNreg,opMVNregS,   //18

	opANDimm, opANDimmS,opEORimm, opEORimmS,opSUBimm, opSUBimmS,opRSBimm, opRSBimmS, //20
	opADDimm, opADDimmS,opADCimm, opADCimmS,opSBCimm, opSBCimmS,opRSCimm, opRSCimmS, //28
	badopcode,opTSTimm, opMRSc,   opTEQimm, badopcode,opCMPimm, badopcode,opCMNimm,  //30
	opORRimm, opORRimmS,opMOVimm, opMOVimmS,opBICimm, opBICimmS,opMVNimm, opMVNimmS, //38

	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)badopcode,(OpFn)opLDRBT,   //40
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB4C, (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRBT,   //48
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //50
	(OpFn)opSTR,    (OpFn)opLDR59,  (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //58

	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)badopcode,(OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)badopcode, //60
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)badopcode,(OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)badopcode, //68
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //70
        (OpFn)opSTR,    (OpFn)opLDR79,  (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB7D, (OpFn)opSTRB,   (OpFn)opLDRB,    //78

	(OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMDS,  (OpFn)opLDMDS,  (OpFn)opSTMDS,  (OpFn)opLDMDS,   //80
	(OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMIS,  (OpFn)opLDMIS,  (OpFn)opSTMIS,  (OpFn)opLDMIS,   //88
	(OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMDS,  (OpFn)opLDMDS,  (OpFn)opSTMDS,  (OpFn)opLDMDS,   //90
	(OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMIS,  (OpFn)opLDMIS,  (OpFn)opSTMIS,  (OpFn)opLDMIS,   //98

	opB,	  opB,	    opB,      opB,      opB,      opB,      opB,      opB,       //A0
	opB,	  opB,	    opB,      opB,      opB,      opB,      opB,      opB,       //A8
	opBL,	  opBL,	    opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      //B0
	opBL,	  opBL,	    opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      //B8

	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //C0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //C8
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //D0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //D8

	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     //E0
	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     //E8
	opSWI,	  opSWI,    opSWI,    opSWI,	opSWI,	  opSWI,    opSWI,    opSWI,     //F0
	opSWI,	  opSWI,    opSWI,    opSWI,	opSWI,	  opSWI,    opSWI,    opSWI      //F8
};

void testit()
{
//opSWI(0x12345678);
        //asm("addl $4,0x12345678;");
}

int linecyc=0;
#ifdef __amd64__
#include "codegen_amd64.h" 
#else
        #if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32
        #include "codegen_x86.h"
        #else
                #error Fatal error : no recompiler available for your architecture
        #endif
#endif


int hasldrb[BLOCKS];
//int output=0;
int callcount=0;
void execarm(int cycs)
{
        //int target;
        int c;
        int hash;
        void (*gen_func)(void);
        int mod;
        uint32_t templ;//,templ2,addr,addr2,mask;
        //unsigned char temp;
//        int RD;
        cycles+=cycs;
        linecyc=256;
        while (cycles>0)
        {
//                cyccount+=200;
//                linecyc=200;
//                while (linecyc>0)
//                for (linecyc=0;linecyc<200;linecyc++)
                while (linecyc>=0)
                {
/*                        oldpc3=oldpc2;
                        oldpc2=oldpc;
                        oldpc=PC;
                        if (output) rpclog("PC now %07X\n",PC);
                        if (PC==0x397F4E0)
                        {
                                callcount++;
                                rpclog("Call 397F4E0 %i\n",callcount);
                                if (callcount==2)
                                {
                                        callcount=0;
                                }
                        }*/
/*                        if (armregs[15]==0x838282F7) *///output=1;
//                        if (PC==0x38203BC) rpclog("Hit 38203BC\n");
//                        /*if (output) */rpclog("New block - %08X %i %08X %08X %08X\n",PC,ins,iomd.t0c,iomd.t1c,armregs[6]);
//                        if (PC==0x38F797C) rpclog("Hit 38F797C at %i\n",ins);
//                        ins++;
//                        if (PC>=0x0300000 && PC<0x0400000) output=1;
//                        if ((PC&0xC0000000)==0x40000000) output=1;
//                        if (armirq&0xC0) { rpclog("Next instruction will abort!\n"); }
                        armirq&=~0xC0;
                        if (!isblockvalid(PC)) /*Interpret block*/
                        {
//                                if (PC==0x26F000) rpclog("Relocation - %08X %08X %08X  %08X\n",oldpc,oldpc2,oldpc3,armregs[2]);
//                                if (PC==0x397F510) rpclog("Hit it 0\n");
//                                if (PC==0x38071A8) rpclog("Hit 2t 0\n");
//                                mod=0;
//                                if (output) rpclog("Interpreting block %07X\n",PC);
                                blockend=0;
                                if ((PC>>12)!=pccache)
                                {
                                        pccache=PC>>12;
                                        pccache2=getpccache(PC);
                                        if ((uint32_t)pccache2==0xFFFFFFFF) { opcode=pccache=(uint32_t)pccache2; armirq|=0x80; }
                                        else                      opcode=pccache2[PC>>2];
                                }
//                                rpclog("%08X %08X %08X %08X %08X\n",pccache2,pccache2+(PC>>2),PC,0xFE3E0020+0x3816000,pccache);
                                while (!blockend && !(armirq&0xC0))
                                {
/*                                        if (PC==0xF0141814) { rpclog("Hit it %i\n",ins); exit(-1); }
                                        if (PC==0x2810A0 || PC==0x281000)
                                        {
                                                output=1;
                                                rpclog("output enabled..\n");
                                        }*/
                                        opcode=pccache2[PC>>2];
//                                        if (PC==0x3B952A0) { output=2; timetolive=100000; }
                                        if (output==3)/* || ((PC&0xC0000000)==0x40000000))*/ rpclog("%07X : %08X %08X %08X %08X  %08X %08X %08X %08X  %08X %08X %08X %08X  %08X %08X %08X %08X   %08X %08X %08X %08X %08X\n",PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15],opcode,oldpc,oldpc2,oldpc3,armregs[16]);
/*                                        if (timetolive)
                                        {
                                                timetolive--;
                                                if (!timetolive) output=0;
                                        }*/
                                                if ((opcode&0x0E000000)==0x0A000000) { /*if (output) rpclog("Block end on a\n");*/ blockend=1; } /*Always end block on branches*/
                                                if ((opcode&0x0F000000)==0x0F000000) { /*if (output) rpclog("Block end on b\n");*/ blockend=1; }/*And SWIs and copro stuff*/
                                                if (!(opcode&0xC000000) && (RD==15)) { /*if (output) rpclog("Block end on c\n");*/ blockend=1; }/*End if R15 can be modified*/
                                                if ((opcode&0x0E108000)==0x08108000) { /*if (output) rpclog("Block end on d\n");*/ blockend=1; }/*End if R15 reloaded from LDM*/
                                                if ((opcode&0x0C100000)==0x04100000 && (RD==15)) { /*if (output) rpclog("Block end on e\n");*/ blockend=1; }/*End if R15 reloaded from LDR*/
                                        if (flaglookup[opcode>>28][(*pcpsr)>>28])// && !(armirq&0x80))
                                           opcodes[(opcode>>20)&0xFF](opcode);
//                                                if ((opcode&0x0E000000)==0x0A000000) blockend=1; /*Always end block on branches*/
//                                                if ((opcode&0x0C000000)==0x0C000000) blockend=1; /*And SWIs and copro stuff*/
                                        oldpc3=oldpc2;
                                        oldpc2=oldpc;
                                        oldpc=PC;
                                        armregs[15]+=4;
                                        if (!((PC)&0xFFC)) blockend=1;
//                                        if (armirq) blockend=1;
                                        inscount++;
                                        rinscount++;
                                        #if 0
                                        ins++;
                                        if (output && ins==19820000/*23720000*/)
                                        {
                                                output=3;
                                                rpclog("logging now\n");
                                        }
                                        #endif
//                                        linecyc++;
                                }
                        linecyc--;
//                                linecyc+=
                        }
                        else
                        {
                                hash=HASH(PC);
/*                                if (pagedirty[PC>>9])
                                {
                                        pagedirty[PC>>9]=0;
                                        cacheclearpage(PC>>9);
                                }
                                else */if (codeblockpc[hash]==PC)
                                {
//                                        mod=1;
                                        /*if (output) */
//                                        if (PC>=0x6F000 && PC<0x70000) { rpclog("Calling block 0 %07X\n",PC); }
//rpclog("Calling block 0 %07X\n",PC);
startpc=armregs[15];
                                        templ=codeblocknum[hash];
//rpclog("Calling block 0 %i %07X\n",templ,PC);
                                        gen_func=(void *)(&rcodeblock[templ][BLOCKSTART]);
                                        if (hasldrb[templ])
                                        {
//                                                *((uint32_t *)0)=1;
                                                hasldrb[templ]=0;
                                        }
//                                        if (PC==0x397F510) rpclog("Hit it 1\n");
//                                        if (PC==0x38071A8) rpclog("Hit 2t 1\n");
//                                        gen_func=(void *)(&codeblock[blocks[templ]>>24][blocks[templ]&0xFFF][4]);
lastblockfunc=&rcodeblock[templ][BLOCKSTART];
                                        gen_func();
//                                        inscount+=codeinscount[0][hash];
//                                        rinscount+=codeinscount[hash];
                                        if (armirq&0x40) armregs[15]+=4;
                                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                                }
                                else
                                {
//                                        if (PC==0x397F510) { rpclog("Hit it 2\n"); output=1; }
//                                        if (PC==0x38071A8) rpclog("Hit 2t 2\n");
//                                        mod=2;
//                                        ins++;
//                                        oldpc=PC;
//if (ins==9683151) rpclog("Is new block\n");
//                                        rpclog("Is new block %07X %08X %08X %08X\n",PC,armregs[8],armregs[0],armregs[3]);
                                        blockend=0;
                                        if ((PC>>12)!=pccache)
                                        {
                                                pccache=PC>>12;
                                                pccache2=getpccache(PC);
                                                if ((uint32_t)pccache2==0xFFFFFFFF) { opcode=pccache=(uint32_t)pccache2; armirq|=0x80; }
                                                else                      opcode=pccache2[PC>>2];
                                        }
                                        if (!(armirq&0x80)) 
					{
						initcodeblock(PC);
						//printf("New block %08X %04X %08X\n",PC,hash,codeblockpc[hash]);
//codeblockpc[hash]=PC;
}
                                        c=0;
//                                        if (oldpc==0x38282E4) output=1;
//                                        if (PC>=0x6F000 && PC<0x70000) { rpclog("Rebuilding block %07X\n",PC); output=1; }
                                        while (!blockend && !(armirq&0xC0))
                                        {
                                                opcode=pccache2[PC>>2];
//                                                if (output) rpclog("%08X %08X %i\n",PC,opcode,blockend);
                                                if ((opcode>>28)==0xF) /*NV*/
                                                {
                                                        generatepcinc();
                                                }
                                                else
                                                {
                                                        #ifdef ABORTCHECKING
                                                        generateupdatepc();
                                                        #else
                                                        if ((opcode&0xE000000)==0x0000000/* && (RN==15 || RD==15 || RM==15 || !validforskip[(opcode>>20)&63])*/) generateupdatepc();
                                                        if ((opcode&0xE000000)==0x2000000/* && (RN==15 || RD==15 ||           !validforskip[(opcode>>20)&63])*/) generateupdatepc();
                                                        if ((opcode&0xC000000)==0x4000000 && (RN==15 || RD==15 || RM==15)) generateupdatepc();
                                                        if ((opcode&0xE000000)==0x8000000 && ((opcode&0x8000) || (RN==15))) generateupdatepc();
                                                        if ((opcode&0xF000000)>=0xA000000) generateupdatepc();
                                                        #endif
//                                                        if (((opcode+0x6000000)&0xF000000)>=0xA000000) generateupdatepc();
//                                                        generateupdatepc();
                                                        generatepcinc();
                                                        if ((opcode&0xE000000)==0xA000000) generateupdateinscount();
                                                        if ((opcode>>28)!=0xE) generateflagtestandbranch(opcode,pcpsr);//,flaglookup);
                                                        else                   lastflagchange=0;
                                                        generatecall((uint32_t)opcodes[(opcode>>20)&0xFF],opcode,pcpsr);
                                                        #ifdef ABORTCHECKING
                                                        if (((opcode+0x6000000)&0xF000000)>=0xA000000) generateirqtest();
                                                        #endif
//                                                        if ((opcode&0x0E000000)==0x0A000000) blockend=1; /*Always end block on branches*/
                                                        if ((opcode&0x0C000000)==0x0C000000) blockend=1; /*And SWIs and copro stuff*/
                                                        if (!(opcode&0xC000000) && (RD==15)) blockend=1; /*End if R15 can be modified*/
                                                        if ((opcode&0x0E108000)==0x08108000) blockend=1; /*End if R15 reloaded from LDM*/
                                                        if ((opcode&0x0C100000)==0x04100000 && (RD==15)) blockend=1; /*End if R15 reloaded from LDR*/
                                                        if (flaglookup[opcode>>28][(*pcpsr)>>28])// && !(armirq&0x80))
                                                           opcodes[(opcode>>20)&0xFF](opcode);
                                                }
                                                armregs[15]+=4;
                                                if (!((PC)&0xFFC))
                                                {
                                                        blockend=1;
/*                                                        pccache=PC>>12;
                                                        pccache2=getpccache(PC);
                                                        if ((uint32_t)pccache2==0xFFFFFFFF) { opcode=pccache=(uint32_t)pccache2; armirq|=0x80; blockend=1; rpclog("Abort!\n"); }
                                                        else                      opcode=pccache2[PC>>2];*/
                                                }
                                                //blockend=1;
//                                                inscount++;
                                                rinscount++;
                                                c++;
                                        }
//                                        if (output) rpclog("Block ended at %07X %i %02X\n",PC,c,armirq);
//                                        output=0;
                                        if (!(armirq&0x80)) endblock(c,pcpsr);
                                        else                removeblock();
/*                                        if (oldpc==0x38282E4)
                                        {
                                                dumplastblock();
                                                rpclog("%i instructions %i\n",c,codeblockpos);
                                        }*/
                                }
                        linecyc--;
                        }
/*                        if (timetolive)
                        {
                                timetolive--;
                                if (!timetolive)
                                   output=0;
                        }*/
//                        linecyc+=10;
//                        if (armirq&0xC0) rpclog("It's set...?\n");
                        if (/*databort|*/armirq&0xC3)//|prefabort)
                        {
/*                                if (mode&16)
                                {
                                        printf("32-bit Exception %i %i %i\n",databort,armirq,prefabort);
                                        dumpregs();
                                        exit(-1);
                                }
                                else*/
                                if (!(mode&16))
                                {
                                        armregs[16]&=~0xC0;
                                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                                }
//                                if (output) rpclog("Exception process - %i %i %i %08X %08X %i\n",databort,armirq,prefabort,armregs[15],armregs[16],inscount);
//                                if (out2) printf("PC at the moment %07X %i %i %02X %08X\n",PC,ins,mode,armregs[16]&0xC0,armregs[15]);
                                if (armirq&0xC0)
                                {
//                                        if (armirq&0xC0) rpclog("Will abort\n");
//                                        exception(ABORT,(armirq&0x80)?0x10:0x14,0);
//                                        armirq&=~0xC0;
//                                        #if 0
                                        if (armirq&0x80)//prefabort)       /*Prefetch abort*/
                                        {
/*                                                rpclog("Prefetch abort! gah! %i %i %07X %i\n",mod,inscount,PC,ins);
                                                dumpregs();
                                                exit(-1);*/
        /*                                        if (output)
                                                {
                                                        dumpregs();
                                                        exit(-1);
                                                }*/
        //                                        dumpregs();
                                                armregs[15]-=4;
                                                exception(ABORT,0x10,0);
                                                armregs[15]+=4;
                                                armirq&=~0xC0;
                                        }
                                        else if (armirq&0x40)//databort==1)     /*Data abort*/
                                        {
/*                                                rpclog("Data abort %08X %08X %08X %08X %08X %i  ",PC,armregs[15],armregs[14],opcode,armregs[cpsr],ins);
                                                getcp15fsr();
                                                dumpregs();
                                                exit(-1);*/
/*                                                output=3;
                                                twice++;
                                                if (twice==3)
                                                {
                                                        dumpregs();
                                                        exit(-1);
                                                }*/
//                                                rpclog("%08X %08X %08X %08X\n",andbefore,andmid,andafter,andpc);
        /*                                        if (abortaddr==abortaddr2 && PC==abortaddr && PC==0x40008C10)
                                                {
                                                        rpclog("Once...\n");
                                                        twice++;
                                                        if (twice==1)
                                                        {
                                                                output=1;
                                                        }
                                                        else
                                                        {
                                                                rpclog("Twice in the row!\n");
                                                                dumpregs();
                                                                exit(-1);
                                                        }
                                                }*/
//                                                abortaddr2=abortaddr;
//                                                abortaddr=PC;
        //                                        output=1;
        //                                        timetolive=500;
        //                                        icache=0;
        //                                        dumpregs();
        //                                        exit(-1);
                                                armregs[15]-=8;
//                                                rpclog("%02X ",armirq);
                                                exception(ABORT,0x14,-4);
//                                                rpclog("%02X\n",armirq);
                                                armregs[15]+=4;
                                                armirq&=~0xC0;
        //                                        rpclog("%08X ",armregs[14]);
          //                                      getcp15fsr();
                                        }
//                                        if (armirq&0xC0) rpclog("Have aborted, but still set?\n");
                                }
                                else if ((armirq&2) && !(armregs[16]&0x40)) /*FIQ*/
                                {
//                                        printf("FIQ %02X %02X\n",iomd.statf,iomd.maskf);
                                        armregs[15]-=4;
                                        exception(FIQ,0x20,0);
                                        armregs[15]+=4;
                                }
                                else if ((armirq&1) && !(armregs[16]&0x80)) /*IRQ*/
                                {
                                        armregs[15]-=4;
//                                        if (output) rpclog("IRQ %02X %02X %02X %02X %08X %08X %02X %08X\n",iomd.stata&iomd.maska,iomd.statb&iomd.maskb,iomd.statc&iomd.maskc,iomd.statd&iomd.maskd,PC,armregs[13],mode,irqregs[0]);
//                                        if (output) printf("IRQ %i %i\n",prog32,mode&16);
//                                        exception(IRQ,0x1C,0x80,0);
                                        if (mode&16)
                                        {
                                                templ=armregs[15];
                                                spsr[IRQ]=armregs[16];
                                                updatemode(IRQ|16);
                                                armregs[14]=templ;
                                                armregs[16]&=~0x1F;
                                                armregs[16]|=0x92;
                                                armregs[15]=0x00000001C;
                                                refillpipeline();
//                                                timetolive=500;
                                        }
                                        else if (prog32)
                                        {
                                                templ=armregs[15];
                                                updatemode(IRQ|16);
                                                armregs[14]=templ&0x3FFFFFC;
                                                spsr[IRQ]=(armregs[16]&~0x1F)|(templ&3);
                                                armregs[16]|=0x80;
                                                armregs[15]=0x00000001C;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=armregs[15];
                                                armregs[15]|=3;
                                                updatemode(IRQ);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000002;
                                                armregs[15]|=0x0800001C;
                                                refillpipeline();
                                        }
                                        armregs[15]+=4;
                                }
//                                if (armirq&0xC0) rpclog("Why is it _still_ set?\n");
                        }
//                        armirq=(armirq&0xCC)|((armirq>>2)&3);
//                        if (ins==3242) printf("%08X %08X\n",iomd.t0c,iomd.t1c);
                }
//                if (ins>=30000000) output=1;
/*                if (ins>=50000)
                {
                        dumpregs();
                        exit(-1);
                }*/
/*                if (ins>=30078620)
                {
                        dumpregs();
                        exit(-1);
                }*/
                linecyc+=256;
/*                iomd.t0c--;
                iomd.t1c--;
                if ((iomd.t0c<0) || (iomd.t1c<0)) updateiomdtimers();*/
                if (kcallback)
                {
                        kcallback--;
                        if (kcallback<=0)
                        {
                                kcallback=0;
                                keycallback();
                        }
                }
                if (mcallback)
                {
                        mcallback-=10;
                        if (mcallback<=0)
                        {
                                mcallback=0;
                                mscallback();
                        }
                }
                if (fdccallback)
                {
                        fdccallback-=50;
                        if (fdccallback<=0)
                        {
                                fdccallback=0;
                                callbackfdc();
                        }
                }
                if (idecallback)
                {
                        idecallback-=10;
                        if (idecallback<=0)
                        {
                                idecallback=0;
                                callbackide();
                        }
                }
                if (motoron)
                {
                        fdci--;
                        if (fdci<=0)
                        {
                                fdci=20000;
                                iomd.stata|=4;
                                updateirqs();
                        }
                }
                if (flyback)
                {
                        flyback--;
//                        if (!flyback) rpclog("Vsync low\n");
                }
                        if (delaygenirqleft)
                        {
                                if (!delaygenirq)
                                {
                                        delaygenirq=2;
                                }
                                delaygenirq--;
                                if (!delaygenirq)
                                {
                                        delaygenirqleft--;
                                        gentimerirq();
                                }
                        }
//                printf("T0 now %04X\n",iomd.t0c);
//                cyc=(oldcyc-cycles);
/*                if (soundbufferfull)
                {
                        updatesoundbuffer();
                }*/
                cycles-=1000;
        }
}
#endif
