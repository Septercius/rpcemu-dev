/*RPCemu v0.3 by Tom Walker
  ARM6/7 emulation*/
  
/*There are bits and pieces of StrongARM emulation in this file. All of the
  extra instructions have been identified, but only some of the long multiplication
  instructions have been defined (enough to get AMPlayer working in SA mode).
  Due to this, it has been defined out for now. Uncomment the following line to
  emulate what is there for now*/
//#define STRONGARM

#include <stdio.h>
#include <string.h>

#include "rpc.h"
#include "hostfs.h"

#include <allegro.h>

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
static void refillpipeline(void);
static void refillpipeline2(void);
//static char bigs[256];
//static FILE *olog;
static unsigned char flaglookup[16][16];
static unsigned long rotatelookup[4096];
static int timetolive = 0;
uint32_t inscount;
//static unsigned char cmosram[256];
int armirq=0;
uint32_t output=0;
static int cpsr;

uint32_t *usrregs[16],userregs[17],superregs[17],fiqregs[17],irqregs[17],abortregs[17],undefregs[17],systemregs[17];
uint32_t spsr[16];
uint32_t armregs[17];
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

#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define MULRD ((opcode>>16)&0xF)
#define MULRN ((opcode>>12)&0xF)
#define MULRS ((opcode>>8)&0xF)
#define MULRM (opcode&0xF)

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

void updatemode(uint32_t m)
{
        uint32_t c,om=mode;
        oldmode=mode;
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
//                printf("Now 32-bit mode %i %08X %i\n",mode&15,PC,ins);
                r15mask=0xFFFFFFFC;
                if (!(om&16))
                {
                        armregs[16]=(armregs[15]&0xF0000000)|mode;
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                        armregs[15]&=0x3FFFFFC;
                        if (output) printf("Switching to 32-bit mode : CPSR %08X\n",armregs[16]);
                }
        }
        else
        {
                mmask=3;
                cpsr=15;
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
}

uint32_t pccache,*pccache2;
void resetarm(void)
{
        int c,d,exec = 0,data;
        uint32_t rotval,rotamount;
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
                        flaglookup[c][d]=exec;
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
void dumpregs()
{
        FILE *f;
        char s[1024];
        int c;
        if (indumpregs) return;
        indumpregs=1;
        f=fopen("ram.dmp","wb");
        sprintf(s,"R 0=%08X R 4=%08X R 8=%08X R12=%08X\nR 1=%08X R 5=%08X R 9=%08X R13=%08X\nR 2=%08X R 6=%08X R10=%08X R14=%08X\nR 3=%08X R 7=%08X R11=%08X R15=%08X\n%i %s",armregs[0],armregs[4],armregs[8],armregs[12],armregs[1],armregs[5],armregs[9],armregs[13],armregs[2],armregs[6],armregs[10],armregs[14],armregs[3],armregs[7],armregs[11],armregs[15],ins,(mmu)?"MMU enabled":"MMU disabled");
        error("%s",s);
        error("PC =%07X ins=%i R12f=%08X CPSR=%08X\n",PC,ins,fiqregs[12],armregs[16]);
        fwrite(ram,0x400000,1,f);
        fclose(f);
        f=fopen("ram2.dmp","wb");
        for (c=0x21B2000;c<0x21B2800;c++)
            putc(readmemb(c),f);
        fclose(f);
        f=fopen("ram1c0.dmp","wb");
        for (c=0x1C01C00;c<0x1C02000;c++)
            putc(readmemb(c),f);
        fclose(f);
        f=fopen("ram0.dmp","wb");
        for (c=0;c<0x1C0;c++)
            putc(readmemb(c),f);
        fclose(f);
        f=fopen("program.dmp","wb");
        databort=0;
        for (c=0;c<0x78000;c++)
        {
                putc(readmemb(c),f);
                if (databort) break;
        }
        fclose(f);
        indumpregs=0;
}

int loadrom()
{
        FILE *f;
//        int c;
        memset(ram,0,4*1024*1024);
        #ifdef ALTSET
        f=fopen("rom1","rb");
        if (!f) return -2;
        fread(rom,0x100000,1,f);
        fclose(f);
        f=fopen("rom2","rb");
        if (!f) return -2;
        fread(&rom[0x40000],0x100000,1,f);
        fclose(f);
        f=fopen("rom3","rb");
        if (!f) return -2;
        fread(&rom[0x80000],0x100000,1,f);
        fclose(f);
        f=fopen("rom4","rb");
        if (!f) return -2;
        fread(&rom[0xC0000],0x100000,1,f);
        fclose(f);
        #else
        f=fopen("ic24.rom","rb");
        if (!f) return -2;
        fread(rom,0x100000,1,f);
        fclose(f);
        f=fopen("ic25.rom","rb");
        if (!f) return -2;
        fread(&rom[0x40000],0x100000,1,f);
        fclose(f);
        f=fopen("ic26.rom","rb");
        if (!f) return -2;
        fread(&rom[0x80000],0x100000,1,f);
        fclose(f);
        f=fopen("ic27.rom","rb");
        if (!f) return -2;
        fread(&rom[0xC0000],0x100000,1,f);
        fclose(f);
        #endif
        return 0;
}

#define checkneg(v) (v&0x80000000)
#define checkpos(v) !(v&0x80000000)

static inline void setadd(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[cpsr]&=0xFFFFFFF;
        if (!res)                           armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))             armregs[cpsr]|=NFLAG;
        if (res<op1)                        armregs[cpsr]|=CFLAG;
        if ((op1^res)&(op2^res)&0x80000000) armregs[cpsr]|=VFLAG;
}

static inline void setsub(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[cpsr]&=0xFFFFFFF;
        if (!res)                           armregs[cpsr]|=ZFLAG;
        else if (checkneg(res))             armregs[cpsr]|=NFLAG;
        if (res<=op1)                       armregs[cpsr]|=CFLAG;
        if ((op1^op2)&(op1^res)&0x80000000) armregs[cpsr]|=VFLAG;
}

static inline void setsbc(uint32_t op1, uint32_t op2, uint32_t res)
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

static inline void setadc(uint32_t op1, uint32_t op2, uint32_t res)
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

static inline void setzn(uint32_t op)
{
        armregs[cpsr]&=0x3FFFFFFF;
        if (!op)               armregs[cpsr]|=ZFLAG;
        else if (checkneg(op)) armregs[cpsr]|=NFLAG;
}
static char err2[512];

#if 0
inline unsigned long shift3s(unsigned long opcode)
{
        unsigned long rmreg=armregs[RM];
        int shiftamount;
        int cflag;
        if (opcode&0x10) shiftamount=armregs[(opcode>>8)&15]&0xFF;
        else             shiftamount=(opcode>>7)&0x1F;
        switch ((opcode>>5)&3)
        {
                case 0: /*LSL*/
                if ((rmreg<<(shiftamount-1))&0x80000000) armregs[cpsr]|=CFLAG;
                else                                     armregs[cpsr]&=~CFLAG;
                return rmreg<<shiftamount;
                case 1: /*LSR*/
                if (shiftamount)
                {
                        if ((rmreg>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                        else                            armregs[cpsr]&=~CFLAG;
                        return rmreg>>shiftamount;
                }
                if (rmreg&0x80000000) armregs[cpsr]|=CFLAG;
                else                  armregs[cpsr]&=~CFLAG;
                return 0;
                case 2: /*ASR*/
                if (!shiftamount)
                {
                        if (rmreg&0x80000000) armregs[cpsr]|=CFLAG;
                        else                  armregs[cpsr]&=~CFLAG;
                        return (rmreg&0x80000000)?0xFFFFFFFF:0;
                }
                if (((int)rmreg>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                else                                 armregs[cpsr]&=~CFLAG;
                return (int)rmreg>>shiftamount;
                case 3: /*ROR*/
                if (shiftamount)
                {
                        if ((rmreg>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                        else                            armregs[cpsr]&=~CFLAG;
                        return (rmreg>>shiftamount)|(rmreg<<(32-shiftamount));
                }
                cflag=(armregs[cpsr]&CFLAG)?0x80000000:1;
                if (rmreg&1) armregs[cpsr]|=CFLAG;
                else         armregs[cpsr]&=~CFLAG;
                return (rmreg>>1)|cflag;
        }
}
#endif

static inline uint32_t shift3(uint32_t opcode)
{
        uint32_t shiftmode=(opcode>>5)&3;
        uint32_t shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
                cycles--;
        }
        temp=armregs[RM];
//        if (RM==15)        temp+=4;
        if (opcode&0x100000 && shiftamount) armregs[cpsr]&=~CFLAG;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&1 && opcode&0x100000) armregs[cpsr]|=CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if (opcode&0x100000)
                {
                        if ((temp<<(shiftamount-1))&0x80000000) armregs[cpsr]|=CFLAG;
                }
                return temp<<shiftamount;

                case 1: /*LSR*/
                if (!shiftamount && !(opcode&0x10))
                {
                        shiftamount=32;
                }
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&0x80000000 && opcode&0x100000) armregs[cpsr]|=CFLAG;
                        else if (opcode&0x100000)               armregs[cpsr]&=~CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if (opcode&0x100000)
                {
                        if ((temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                }
                return temp>>shiftamount;

                case 2: /*ASR*/
                if (!shiftamount)
                {
                        if (opcode&0x10) return temp;
                }
                if (shiftamount>=32 || !shiftamount)
                {
                        if (temp&0x80000000 && opcode&0x100000) armregs[cpsr]|=CFLAG;
                        else if (opcode&0x100000)               armregs[cpsr]&=~CFLAG;
                        if (temp&0x80000000) return 0xFFFFFFFF;
                        return 0;
                }
                if (opcode&0x100000)
                {
                        if (((int)temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                }
                return (int)temp>>shiftamount;

                case 3: /*ROR*/
                if (opcode&0x100000) armregs[cpsr]&=~CFLAG;
                if (!shiftamount && !(opcode&0x10))
                {
                        if (opcode&0x100000 && temp&1) armregs[cpsr]|=CFLAG;
                        return (((cflag)?1:0)<<31)|(temp>>1);
                }
                if (!shiftamount)
                {
                        if (opcode&0x100000) armregs[cpsr]|=cflag;
                        return temp;
                }
                if (!(shiftamount&0x1F))
                {
                        if (opcode&0x100000 && temp&0x80000000) armregs[cpsr]|=CFLAG;
                        return temp;
                }
                if (opcode&0x100000)
                {
                        if (((temp>>shiftamount)|(temp<<(32-shiftamount)))&0x80000000) armregs[cpsr]|=CFLAG;
                }
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;
                default:
                sprintf(err2,"Shift mode %i amount %i\n",shiftmode,shiftamount);
                error("%s",err2);
                dumpregs();
                exit(-1);
        }
}

/*unsigned long shift3(unsigned long opcode)
{
        unsigned long c=armregs[cpsr],c1;
        unsigned long temp,temp2;
        temp=shift3s(opcode);
        c1=armregs[cpsr];
        armregs[cpsr]=c;
        temp2=shift3o(opcode);
        if (temp != temp2 || c1 != armregs[cpsr])
           rpclog("shift mismatch : new %08X %08X  old %08X %08X  data %08X mode %i amount %i %s %08X %08X\n",temp,c1,temp2,armregs[cpsr],armregs[RM],(opcode>>5)&3,(opcode>>7)&0x1F,(opcode&0x10)?"register":"immediate",opcode,armregs[(opcode>>8)&15]);
        return temp2;
}*/

#define shift(o)  ((o&0xFF0)?shift3(o):armregs[RM])
#define shift2(o) ((o&0xFF0)?shift4(o):armregs[RM])


#if 0
inline unsigned shift4(unsigned opcode)
{
        unsigned long rmreg=armregs[RM];
        int shiftamount;
        if (opcode&0x10) shiftamount=armregs[(opcode>>8)&15]&0xFF;
        else
        {
                shiftamount=(opcode>>7)&0x1F;
                if (!shiftamount) shiftamount=32;
        }
        switch ((opcode>>5)&3)
        {
                case 0: /*LSL*/
                if (shiftamount>31) return 0;
                return rmreg<<shiftamount;
                case 1: /*LSR*/
                if (shiftamount>31) return 0;
//                if (!shiftamount && !(opcode&0x10)) return 0;
                return rmreg>>shiftamount;
                case 2: /*ASR*/
                if (shiftamount>31) return (rmreg&0x80000000)?0xFFFFFFFF:0;
                return (int)rmreg>>shiftamount;
                case 3: /*ROR*/
                if ((shiftamount!=32) || (opcode&0x10)) return (rmreg>>shiftamount)|(rmreg<<(32-shiftamount));
                return (rmreg>>1)|((armregs[15]&CFLAG)?0x80000000:0);
        }
}
#endif

//#if 0
inline unsigned shift4(unsigned opcode)
{
        unsigned shiftmode=(opcode>>5)&3;
        unsigned shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
//                cycles--;
        }
        temp=armregs[RM];
//        if (RM==15) temp+=4;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount)    return temp;
                if (shiftamount>=32) return 0;
                return temp<<shiftamount;

                case 1: /*LSR*/
                if (!shiftamount && !(opcode&0x10))    return 0;
                if (shiftamount>=32) return 0;
                return temp>>shiftamount;

                case 2: /*ASR*/
                if (!shiftamount && !(opcode&0x10)) shiftamount=32;
                if (shiftamount>=32)
                {
                        if (temp&0x80000000)
                           return 0xFFFFFFFF;
                        return 0;
                }
                return (int)temp>>shiftamount;

                case 3: /*ROR*/
                if (!shiftamount && !(opcode&0x10)) return (((cflag)?1:0)<<31)|(temp>>1);
                if (!shiftamount)                   return temp;
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;

                default:
                sprintf(err2,"Shift2 mode %i amount %i\n",shiftmode,shiftamount);
                error("%s",err2);
                dumpregs();
                exit(-1);
        }
}
//#endif

#if 0
unsigned long shift4(unsigned long opcode)
{
        return shift4n(opcode);
//        unsigned long temp,temp2;
//        temp=shift4n(opcode);
/*        temp2=shift4o(opcode);
        if (temp != temp2)
           rpclog("shift mismatch : new %08X  old %08X  data %08X mode %i amount %i %s %08X %08X\n",temp,temp2,armregs[RM],(opcode>>5)&3,(opcode>>7)&0x1F,(opcode&0x10)?"register":"immediate",opcode,armregs[(opcode>>8)&15]);
        return temp2;*/
}
#endif

static inline unsigned rotate(unsigned data)
{
        uint32_t rotval;
        rotval=rotatelookup[data&4095];
        if (data&0x100000 && data&0xF00)
        {
                if (rotval&0x80000000) armregs[cpsr]|=CFLAG;
                else                   armregs[cpsr]&=~CFLAG;
        }
        return rotval;
}

#define rotate2(v) rotatelookup[v&4095]

static const int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

/*uint32_t ldrresult(uint32_t val, uint32_t addr)
{
//        if (addr&3) printf("Unaligned access\n");
//        return val;
        switch (addr&3)
        {
                case 0:
                return val;
                case 1:
                return (val>>8)|(val<<24);
                case 2:
                return (val>>16)|(val<<16);
                case 3:
                return (val>>24)|(val<<8);
        }
}*/

static void undefined(void)
{
        uint32_t templ;
//        rpclog("Undefined %i %i CPSR %08X R15 %08X\n",mode,prog32,armregs[16],armregs[15]);
//        printf("Undefined instruction\n");
//        printf("R14 = %08X\n",armregs[14]);
//        printf("%i : %07X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X  %08X %08X %08X - %08X %08X %08X %i R10=%08X\n",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[9],armregs[12],armregs[13],armregs[14],armregs[15],armregs[16],opcode,mode,armregs[10]);
//        printf("%i %i\n",mode,prog32);
        if (mode&16)
        {
                templ=armregs[15]-4;
                spsr[UNDEFINED]=armregs[16];
                updatemode(UNDEFINED|16);
                armregs[16]&=~0x1F;
                armregs[14]=templ;
                armregs[16]|=0x9B;
                armregs[15]=0x00000008;
                refillpipeline();
        }
        else if (prog32)
        {
                templ=armregs[15]-4;
                updatemode(UNDEFINED|16);
                armregs[14]=templ&0x3FFFFFC;
                spsr[UNDEFINED]=(armregs[16]&~0x1F)|(templ&3);
                armregs[15]=0x00000008;
                armregs[16]|=0x80;
                cycles-=4;
                refillpipeline();
        }
        else
        {
                templ=armregs[15]-4;
                armregs[15]|=3;
                updatemode(SUPERVISOR);
                armregs[14]=templ;
                armregs[15]&=0xFC000003;
                armregs[15]|=0x08000008;
                cycles-=4;
                refillpipeline();
        }
        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
}
/*
#define undefined()\
                                        templ=armregs[15]-4; \
                                        armregs[15]|=3;\
                                        updatemode(SUPERVISOR);\
                                        armregs[14]=templ;\
                                        armregs[15]&=0xFC000003;\
                                        armregs[15]|=0x08000008;\
                                        refillpipeline();\
                                        cycles--
*/
//static int accc=0;
//static FILE *slogfile;

static void refillpipeline(void)
{
//        if (!olog) olog=fopen("armlog.txt","wt");
//        opcode=readmeml(PC-8);
        opcode2=readmeml(PC-4);
//        rpclog("%08X from %07X %08X\n",opcode2,PC-4,rom[((PC-4)&0x3FFFFC)>>2]);
        opcode3=readmeml(PC);
//        rpclog("%08X from %07X %08X\n",opcode3,PC,rom[(PC&0x3FFFFC)>>2]);
//        cycles-=3;
//        sprintf(bigs,"Fetched - %08X %07X, %08X %07X\n",opcode2,PC-4,opcode3,PC);
//        fputs(bigs,olog);
}

static void refillpipeline2()
{
//        if (!olog) olog=fopen("armlog.txt","wt");
//        opcode=readmeml(PC-8);
        opcode2=readmeml(PC-8);
//        rpclog("%08X from %07X\n",opcode2,PC-8);
        opcode3=readmeml(PC-4);
//        rpclog("%08X from %07X\n",opcode3,PC-4);
//        cycles-=3;
//        sprintf(bigs,"Fetched - %08X %07X, %08X %07X\n",opcode2,PC-4,opcode3,PC);
//        fputs(bigs,olog);
}

static const uint32_t msrlookup[16]=
{
        0x00000000,0x000000FF,0x0000FF00,0x0000FFFF,
        0x00FF0000,0x00FF00FF,0x00FFFF00,0x00FFFFFF,
        0xFF000000,0xFF0000FF,0xFF00FF00,0xFF00FFFF,
        0xFFFF0000,0xFFFF00FF,0xFFFFFF00,0xFFFFFFFF
};

//static uint32_t oldr12;


static void bad_opcode(uint32_t opcode) 
{
     error("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
     dumpregs();
     exit(EXIT_FAILURE);
}


static void ldmstm(uint32_t ls_opcode, uint32_t opcode) 
{
  uint32_t templ, mask, addr, c;

  //  addr = mem_getphys(armregs[RN]);
  addr = armregs[RN];

  switch (ls_opcode) 
    {

    case 0x80: /*STMDA*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr-=4;
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;

    case 0x81: /*LDMDA*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    addr-=4;
                    cycles--;
            }
            mask>>=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;

    case 0x82: /*STMDA !*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr-=4;
                    armregs[RN]-=4;
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;
                                
    case 0x83: /*LDMDA !*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    addr-=4;
                    armregs[RN]-=4;
                    cycles--;
            }
            mask>>=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;

    case 0x85: /*LDMDA ^*/
    mask=0x8000;
    if (opcode&0x8000)
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr-=4;
                            cycles--;
                    }
                    mask>>=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr-=4;
                            cycles--;
                    }
                    mask>>=1;
            }
    }
    cycles-=3;
    break;

    case 0x88: /*STMIA*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr+=4;
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x89: /*LDMIA*/
    mask=1;

    for (c=0;c<15;c++)
    {
            if (opcode & mask)
            {
                    armregs[c]=readmeml(addr);
                    addr+=4;
                    cycles--;
            }
            mask<<=1;
    }

    if (opcode & mask) 
    {
	    armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
	    cycles--;
    }

    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;

    case 0x8A: /*STMIA !*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr+=4;
                    armregs[RN]+=4;
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x8B: /*LDMIA !*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    addr+=4;
                    armregs[RN]+=4;
                    cycles--;
            }
                    mask<<=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;
                                
    case 0x8C: /*STMIA ^*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
                    addr+=4;
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x8D: /*LDMIA ^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr+=4;
                            cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr+=4;
                            cycles--;
                    }
                    mask<<=1;
            }
    }
    cycles-=3;
    break;

    case 0x8F: /*LDMIA !^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr+=4;
                            armregs[RN]+=4;
                            cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr+=4;
                            armregs[RN]+=4;
                            cycles--;
                    }
                    mask<<=1;
            }
    }
    cycles-=3;
    break;

    case 0x90: /*STMDB*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;

    case 0x91: /*LDMDB*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    cycles--;
            }
            mask>>=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;

    case 0x92: /*STMDB !*/
    mask=0x8000;
    templ=0;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    armregs[RN]-=4;
//                    if (output) printf("Writing R%i %08X to %08X\n",c,armregs[c],addr);
                    if (c==15)                { writememl(addr,armregs[c]+r15diff); }
                    else if (c==RN && !templ) { writememl(addr,armregs[c]+4); }
                    else                      { writememl(addr,armregs[c]); templ=1;}
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;

    case 0x93: /*LDMDB !*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    armregs[RN]-=4;
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    cycles--;
            }
            mask>>=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;

    case 0x94: /*STMDB ^*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;

    case 0x95: /*LDMDB ^*/
    mask=0x8000;
    if (opcode&0x8000)
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            addr-=4;
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            cycles--;
                    }
                    mask>>=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            addr-=4;
                            *usrregs[c]=readmeml(addr);
                            cycles--;
                    }
                    mask>>=1;
            }
    }
    cycles-=3;
    break;

    case 0x96: /*STMDB !^*/
    mask=0x8000;
    templ=0;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    armregs[RN]-=4;
                    if (c==15)                { writememl(addr,*usrregs[c]+r15diff); }
                    else if (c==RN && !templ) { writememl(addr,*usrregs[c]+4); }
                    else                      { writememl(addr,*usrregs[c]); }
                    cycles--;
            }
            mask>>=1;
    }
    cycles-=2;
    break;
    case 0x98: /*STMIB*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x99: /*LDMIB*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    cycles--;
            }
            mask<<=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;
    
    case 0x9A: /*STMIB !*/
    mask=1;
    templ=0;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    armregs[RN]+=4;
                    if (c==15)                { writememl(addr,armregs[c]+r15diff); }
                    else if (c==RN && !templ) { writememl(addr,armregs[c]-4); }
                    else                      { writememl(addr,armregs[c]); }
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x9B: /*LDMIB !*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    armregs[RN]+=4;
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    cycles--;
            }
            mask<<=1;
    }
    if (opcode&0x8000) refillpipeline();
    cycles-=3;
    break;
    
    case 0x9C: /*STMIB ^*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
                    cycles--;
            }
            mask<<=1;
    }
    cycles-=2;
    break;

    case 0x9D: /*LDMIB ^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            addr+=4;
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            addr+=4;
                            *usrregs[c]=readmeml(addr);
                            cycles--;
                    }
                    mask<<=1;
            }
    }
    cycles-=3;
    break;


    }
}


void execarm(int cycs)
{
        uint32_t templ,templ2,addr,addr2;
        uint32_t a,b,c,d,e,f;
        int tempi;
        //int exec,c,cc,cyc,oldcyc,oldcyc2,d;
	int cyc, oldcyc, oldcyc2;
	//uint32_t c;
        unsigned char temp;
        //uint32_t oldr15[2];
	//        FILE *f;
        char s[80];
        //char bigs[1024];
        int linecyc;
        cycles+=cycs;
        while (cycles>0)
        {
                oldcyc=cycles;
                cyccount+=200;
                linecyc=200;
                while (linecyc>0)
                {
                        opcode=opcode2;
                        opcode2=opcode3;
                        oldcyc2=cycles;
/*                        if ((PC>>15)==pccache)
                           opcode3=pccache2[(PC&0x7FFF)>>2];
                        else
                        {
                                templ2=PC>>15;
                                templ=memstat[PC>>15];
                                if (modepritabler[memmode][memstat[PC>>15]])
                                {
                                        pccache=PC>>15;
                                        pccache2=mempoint[PC>>15];
                                        opcode3=mempoint[PC>>15][(PC&0x7FFF)>>2];
                                }
                                else
                                {
				        opcode3=readmemf(PC);
					pccache=0xFFFFFFFF;
                                }
                        }*/
                        if ((PC>>12)!=pccache)
                        {
                                pccache=PC>>12;
                                pccache2=getpccache(PC);
                        }
                        opcode3=pccache2[(PC&0xFFF)>>2];
                        if (flaglookup[opcode>>28][armregs[cpsr]>>28] && !prefabort)
                        {
/*                                if (!(opcode&0xC000000) && RD==15 && (opcode&0x100000) && (mode&16) & (((opcode>>20)&0xFF)!=0x25))
                                {
				        error("Bad instruction %08X\n",opcode);
					dumpregs();
					exit(-1);
                                }*/
#ifdef STRONGARM
                                if ((opcode&0xE0000F0)==0xB0) /*LDRH/STRH*/
                                {
                                        error("Bad opcode %08X\n",opcode);
                                        exit(-1);
                                }
                                else if ((opcode&0xE1000D0)==0x1000D0) /*LDRS*/
                                {
                                        error("Bad opcode %08X\n",opcode);
                                        exit(-1);
                                }
                                else
                                {
#endif
                                switch ((opcode>>20)&0xFF)
                                {
				        case 0x00: /*AND reg*/
				  /*    if (!opcode)
					{
					      printf("Opcode 0 at %08X\n",PC);
					      dumpregs();
					      exit(-1);
					}*/
					if (((opcode&0xE00090)==0x90)) /*MUL*/
					{
					      armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
					      cycles-=17;
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
					      cycles--;
					}
					break;

				        case 0x01: /*ANDS reg*/
					if (((opcode&0xE000090)==0x90)) /*MULS*/
					{
					       armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
					       setzn(armregs[MULRD]);
					       cycles-=17;
					}
					else
					{
					       if (RD==15)
					       {
						       templ=shift(opcode);
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
					       cycles--;
					}
					break;

				        case 0x02: /*EOR reg*/
					if (((opcode&0xE000090)==0x90)) /*MLA*/
					{
					       armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
					       cycles-=17;
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
					       cycles--;
                                        }
                                        break;

                                        case 0x03: /*EORS reg*/
                                        if (((opcode&0xE000090)==0x90)) /*MLA*/
                                        {
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
                                                setzn(armregs[MULRD]);
                                                cycles-=17;
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
                                                cycles--;
                                        }
                                        break;

                                        case 0x04: /*SUB reg*/
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
                                        cycles--;
                                        break;
                                        case 0x05: /*SUBS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(GETADDR(RN)-templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x06: /*RSB reg*/
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
                                        cycles--;
                                        break;
                                        case 0x07: /*RSBS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(templ-GETADDR(RN))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;

                                        case 0x08: /*ADD reg*/
                                #ifdef STRONGARM                                        
					if (((opcode&0xE000090)==0x000090)) /*MULL*/
					{
                                                a=(armregs[MULRS]&0xFFFF)*(armregs[MULRM]&0xFFFF);
                                                b=(armregs[MULRS]&0xFFFF)*(armregs[MULRM]>>16);
                                                c=(armregs[MULRS]>>16)*(armregs[MULRM]&0xFFFF);
                                                d=(armregs[MULRS]>>16)*(armregs[MULRM]>>16);
                                                e=b+c;
                                                if (e<b) e+=0x10000;
                                                f=e+(a>>16);
                                                if (f<e) f+=0x10000;
                                                armregs[MULRN]=(a&0xFFFF)|(f<<16);
                                                armregs[MULRD]=d|(e>>16);
                                        }
                                        else
                                        {
                                #endif
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
//                                                printf("R15=%08X+%08X+4=",GETADDR(RN),templ);
                                                armregs[15]=((GETADDR(RN)+templ+4)&r15mask)|(armregs[15]&~r15mask);
//                                                printf("%08X %i\n",armregs[15],mode&16);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)+templ;
                                        }
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;
                                        case 0x09: /*ADDS reg*/
                                #ifdef STRONGARM
					if (((opcode&0xE000090)==0x000090)) /*MULLS*/
					{
                                                a=(armregs[MULRS]&0xFFFF)*(armregs[MULRM]&0xFFFF);
                                                b=(armregs[MULRS]&0xFFFF)*(armregs[MULRM]>>16);
                                                c=(armregs[MULRS]>>16)*(armregs[MULRM]&0xFFFF);
                                                d=(armregs[MULRS]>>16)*(armregs[MULRM]>>16);
                                                e=b+c;
                                                if (e<b) e+=0x10000;
                                                f=e+(a>>16);
                                                if (f<e) f+=0x10000;
                                                armregs[MULRN]=(a&0xFFFF)|(f<<16);
                                                armregs[MULRD]=d|(e>>16);
                                                armregs[cpsr]&=~0xC0000000;
                                                if (!(armregs[MULRN]|armregs[MULRD])) armregs[cpsr]|ZFLAG;
                                                if (armregs[MULRD]&0x80000000) armregs[cpsr]|NFLAG;
                                        }
                                        else
                                        {
                                #endif
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
        //                                        printf("R15=%08X+%08X+4=",GETADDR(RN),templ);
                                                armregs[15]=GETADDR(RN)+templ+4;
                                                refillpipeline();
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
        //                                        printf("%08X\n",armregs[15]);
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
        //                                        printf("ADDS %08X+%08X = ",GETADDR(RN),templ);
                                                armregs[RD]=GETADDR(RN)+templ;
        //                                        printf("%08X\n",armregs[RD]);
        //                                        setzn(templ);
                                        }
                                        cycles--;
                                #ifdef STRONGARM                                                                                
                                        }
                                #endif
                                        break;
                                
                                        case 0x0A: /*ADC reg*/
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
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;
                                        case 0x0B: /*ADCS reg*/
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
                                                templ=shift(opcode);
                                                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                        case 0x0C: /*SBC reg*/
                                #ifdef STRONGARM                                        
					if (((opcode&0xE000090)==0x000090)) /*SMULL*/
					{
                                                templ=armregs[MULRS];
                                                templ2=armregs[MULRM];
                                                tempi=(templ^templ2)&0x80000000;
                                                if (templ&0x80000000) templ=~templ+1;
                                                if (templ2&0x80000000) templ2=~templ2+1;
                                                a=(templ&0xFFFF)*(templ2&0xFFFF);
                                                b=(templ&0xFFFF)*(templ2>>16);
                                                c=(templ>>16)*(templ2&0xFFFF);
                                                d=(templ>>16)*(templ2>>16);
                                                e=b+c;
                                                if (e<b) e+=0x10000;
                                                f=e+(a>>16);
                                                if (f<e) f+=0x10000;
                                                armregs[MULRN]=(a&0xFFFF)|(f<<16);
                                                armregs[MULRD]=d|(e>>16);
                                                if (tempi)
                                                {
                                                        armregs[MULRN]=~armregs[MULRN]+1;
                                                        armregs[MULRD]=~armregs[MULRD]+((armregs[MULRN])?0:1);
                                                }
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
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;
                                        case 0x0D: /*SBCS reg*/
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
                                                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;
                                        case 0x0E: /*RSC reg*/
                                #ifdef STRONGARM
					if (((opcode&0xE000090)==0x000090)) /*SMLAL*/
					{
                                                templ=armregs[MULRS];
                                                templ2=armregs[MULRM];
                                                addr=armregs[MULRN];
                                                addr2=armregs[MULRD];
                                                tempi=(templ^templ2)&0x80000000;
                                                if (templ&0x80000000) templ=~templ+1;
                                                if (templ2&0x80000000) templ2=~templ2+1;
                                                a=(templ&0xFFFF)*(templ2&0xFFFF);
                                                b=(templ&0xFFFF)*(templ2>>16);
                                                c=(templ>>16)*(templ2&0xFFFF);
                                                d=(templ>>16)*(templ2>>16);
                                                e=b+c;
                                                if (e<b) e+=0x10000;
                                                f=e+(a>>16);
                                                if (f<e) f+=0x10000;
                                                armregs[MULRN]=(a&0xFFFF)|(f<<16);
                                                armregs[MULRD]=d|(e>>16);
                                                if (tempi)
                                                {
                                                        armregs[MULRN]=~armregs[MULRN]+1;
                                                        armregs[MULRD]=~armregs[MULRD]+((armregs[MULRN])?0:1);
                                                }
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
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;
                                        case 0x0F: /*RSCS reg*/
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
                                                templ=shift(opcode);
                                                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                        case 0x10: /*SWP word*/
                                        if ((opcode&0xF0)==0x90)
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmeml(addr));
                                                writememl(addr,templ);
                                                cycles-=3;
                                        }
                                        else if (!(opcode&0xFFF)) /*MRS CPSR*/
                                        {
                                                if (!(mode&16))
                                                {
                                                        armregs[16]=(armregs[15]&0xF0000000)|(armregs[15]&3);
                                                        armregs[16]|=((armregs[15]&0xC000000)>>20);
//                                                        printf("CPSR %08X R15 %08X\n",armregs[16],armregs[15]);
                                                }
                                                armregs[RD]=armregs[16];
                                        }
                                        else
                                        {
					        bad_opcode(opcode);
                                        }
                                        break;
                                        
                                        case 0x11: /*TST reg*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                templ=armregs[15]&0x3FFFFFC;
                                                armregs[15]=((GETADDR(RN)&shift2(opcode))&0xFC000003)|templ;
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)&shift(opcode));
                                        }
                                        cycles--;
                                        break;

                                        case 0x12: /*SWP byte*/
                                        if ((opcode&0xF0)==0x90)
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmemb(addr));
                                                writememb(addr,templ);
                                                cycles-=3;
                                        }
                                        else if (!(opcode&0xFF0)) /*MSR CPSR*/
                                        {
                                                temp=armregs[16];
                                                armregs[16]&=~msrlookup[(opcode>>16)&0xF];
                                                armregs[16]|=(armregs[RM]&msrlookup[(opcode>>16)&0xF]);
//                                                printf("CPSR now %08X\n",armregs[16]);
                                                if (opcode&0x10000) updatemode(armregs[16]&0x1F);
                                        }
                                        else
                                        {
						bad_opcode(opcode);
                                        }
                                        break;
                                        
                                        case 0x13: /*TEQ reg*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                templ=armregs[15]&0x3FFFFFC;
                                                armregs[15]=((GETADDR(RN)^shift2(opcode))&0xFC000003)|templ;
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)^shift(opcode));
                                        }
                                        cycles--;
                                        break;

                                        case 0x14: /*MSR SPSR*/
                                        if (!(opcode&0xFFF)) /*MRS SPSR*/
                                        {
                                                armregs[RD]=spsr[mode&15];
                                        }
                                        else
                                        {
						bad_opcode(opcode);
                                        }
                                        break;
                                        
                                        case 0x15: /*CMP reg*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                armregs[15]&=0x3FFFFFC;
                                                armregs[15]|=((GETADDR(RN)-shift2(opcode))&0xFC000003);
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                           setsub(GETADDR(RN),shift(opcode),GETADDR(RN)-shift(opcode));
                                        cycles--;
                                        break;

                                        case 0x16:
                                        if (!(opcode&0xFF0)) /*MSR SPSR*/
                                        {
                                                temp=spsr[mode&15];
                                                spsr[mode&15]&=~msrlookup[(opcode>>16)&0xF];
                                                spsr[mode&15]|=(armregs[RM]&msrlookup[(opcode>>16)&0xF]);
                                        }
                                        else
                                        {
						bad_opcode(opcode);
                                        }
                                        break;

                                        case 0x17: /*CMN reg*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                armregs[15]&=0x3FFFFFC;
                                                armregs[15]|=((GETADDR(RN)+shift2(opcode))&0xFC000003);
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                           setadd(GETADDR(RN),shift2(opcode),GETADDR(RN)+shift2(opcode));
                                        cycles--;
                                        break;

                                        case 0x18: /*ORR reg*/
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
                                        cycles--;
                                        break;
                                        case 0x19: /*ORRS reg*/
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
                                        cycles--;
                                        break;

                                        case 0x1A: /*MOV reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&~r15mask)|((shift2(opcode)+4)&r15mask);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=shift2(opcode);
                                        cycles--;
                                        break;
                                        case 0x1B: /*MOVS reg*/
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
                                        cycles--;
                                        break;

                                        case 0x1C: /*BIC reg*/
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
                                        cycles--;
                                        break;
                                        case 0x1D: /*BICS reg*/
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
                                        cycles--;
                                        break;

                                        case 0x1E: /*MVN reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&~r15mask)|(((~shift2(opcode))+4)&r15mask);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~shift2(opcode);
                                        cycles--;
                                        break;
                                        case 0x1F: /*MVNS reg*/
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
                                        cycles--;
                                        break;

                                        case 0x20: /*AND imm*/
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
                                        cycles--;
                                        break;
                                        case 0x21: /*ANDS imm*/
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
                                        cycles--;
                                        break;

                                        case 0x22: /*EOR imm*/
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
                                        cycles--;
                                        break;
                                        case 0x23: /*EORS imm*/
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
                                        cycles--;
                                        break;

                                        case 0x24: /*SUB imm*/
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
                                        cycles--;
                                        break;
                                        case 0x25: /*SUBS imm*/
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
                                                templ=rotate(opcode);
                                                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x26: /*RSB imm*/
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
                                        cycles--;
                                        break;
                                        case 0x27: /*RSBS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(templ-GETADDR(RN))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;

                                        case 0x28: /*ADD imm*/
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
                                        cycles--;
                                        break;
                                        case 0x29: /*ADDS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=GETADDR(RN)+templ+4;
                                                refillpipeline();
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                                                armregs[RD]=GETADDR(RN)+templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x2A: /*ADC imm*/
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
                                        cycles--;
                                        break;
                                        case 0x2B: /*ADCS imm*/
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
                                                templ=rotate(opcode);
                                                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                        break;

                                        case 0x2C: /*SBC imm*/
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
                                        cycles--;
                                        break;
                                        case 0x2D: /*SBCS imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x2E: /*RSC imm*/
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
                                        cycles--;
                                        break;
                                        case 0x2F: /*RSCS imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                        break;

                                        case 0x31: /*TST imm*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                templ=armregs[15]&0x3FFFFFC;
                                                armregs[15]=((GETADDR(RN)&rotate2(opcode))&0xFC000003)|templ;
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)&rotate(opcode));
                                        }
                                        cycles--;
                                        break;
                                
                                        case 0x32: /*MSR rot->flags*/
                                        if ((opcode&0x3FF000)==0x28F000)
                                        {
                                                templ=rotate(opcode);
                                                armregs[cpsr]=(armregs[cpsr]&~0xF0000000)|(templ&0xF0000000);
                                        }
                                        cycles--;
                                        break;

                                        case 0x33: /*TEQ imm*/
                                        if (RD==15)
                                        {
/*                                                if (mode&16)
                                                {
                                                        error("TEQP in 32-bit mode %08X\n",rotate2(opcode));
                                                        dumpregs();
                                                        exit(-1);
                                                }*/
                                                opcode&=~0x100000;
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xFC000003)|templ;
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                        if (!olog) olog=fopen("armlog.txt","wt");
//                                                        sprintf(s,"TEQP %08X %i\n",armregs[15],getline());
//                                                        fputs(s,olog);
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xF0000000)|templ;
                                                }
//                                                refillpipeline();
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)^rotate(opcode));
                                        }
                                        cycles--;
                                        break;

                                        case 0x35: /*CMP imm*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                armregs[15]&=0x3FFFFFC;
                                                armregs[15]|=((GETADDR(RN)-rotate2(opcode))&0xFC000003);
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                           setsub(GETADDR(RN),rotate(opcode),GETADDR(RN)-rotate(opcode));
                                        cycles--;
                                        break;

                                        case 0x37: /*CMN imm*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                armregs[15]&=0x3FFFFFC;
                                                armregs[15]|=((GETADDR(RN)+rotate2(opcode))&0xFC000003);
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                                                refillpipeline();
                                        }
                                        else
                                           setadd(GETADDR(RN),rotate(opcode),GETADDR(RN)+rotate(opcode));
                                        cycles--;
                                        break;

                                        case 0x38: /*ORR imm*/
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
                                        cycles--;
                                        break;
                                        case 0x39: /*ORRS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                if (armregs[15]&3)
                                                   armregs[15]=(GETADDR(RN)|templ)+4;
                                                else
                                                   armregs[15]=(((GETADDR(RN)|templ)+4)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                                                refillpipeline();
                                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD]=GETADDR(RN)|templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x3A: /*MOV imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&~r15mask)|(rotate2(opcode)&r15mask);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=rotate2(opcode);
                                        cycles--;
                                        break;
                                        case 0x3B: /*MOVS imm*/
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
                                        cycles--;
                                        break;

                                        case 0x3C: /*BIC imm*/
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
                                        cycles--;
                                        break;
                                        case 0x3D: /*BICS imm*/
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
                                        cycles--;
                                        break;

                                        case 0x3E: /*MVN imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&~r15mask)|(((~rotate2(opcode))+4)&r15mask);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~rotate2(opcode);
                                        cycles--;
                                        break;
                                        case 0x3F: /*MVNS imm*/
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
                                        cycles--;
                                        break;
//#endif
/*case 0x40:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x41:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x42:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x43:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x44:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x45:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x46:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x47:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x48:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x49:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4A:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4B:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4C:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4D:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4E:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4F:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x50:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x51:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x52:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x53:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x54:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x55:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x56:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x57:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x58:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x59:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5A:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x5B:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5C:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x5D:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5E:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x5F:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x60:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x61:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x62:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x63:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x64:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x65:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x66:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x67:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x68:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x69:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6A:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6B:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6C:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6D:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6E:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6F:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x70:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x71:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x72:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x73:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x74:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x75:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x76:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x77:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x78:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x79:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7A:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x7B:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7C:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x7D:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7E:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x7F:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;
*/

//#if 0
                                        case 0x42: case 0x4A: /*STRT*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=memmode;
                                        memmode=0;
                                        writememl(addr,armregs[RD]);
                                        memmode=templ;
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=2;
                                        break;

                                        case 0x43: case 0x4B: /*LDRT*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=memmode;
                                        memmode=0;
                                        templ2=readmeml(addr);
                                        memmode=templ;
                                        if (databort) break;
                                        templ2=ldrresult(templ2,addr);
                                        LOADREG(RD,templ2);
//                                        if (RD==15) refillpipeline();
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=3;
                                        break;

                                        case 0x47: /*LDRBT*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=memmode;
                                        memmode=0;
                                        templ2=readmemb(addr);
                                        memmode=templ;
                                        if (databort) break;
                                        LOADREG(RD,templ2);
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=3;
/*                                        if (RD==7)
                                        {
                                                if (!olog) olog=fopen("armlog.txt","wt");
                                                sprintf(s,"LDRB R7 %02X,%07X\n",armregs[7],PC);
                                                fputs(s,olog);
                                        }*/
                                        break;

                                        case 0x60: case 0x68:
                                        case 0x70: case 0x72: case 0x78: case 0x7A:
                                        case 0x40: case 0x48: /*STR*/
                                        case 0x50: case 0x52: case 0x58: case 0x5A:
                                        if ((opcode&0x2000010)==0x2000010)
                                        {
                                                undefined();
                                                break;
                                        }
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        if (RD==15) { writememl(addr,armregs[RD]+r15diff); }
                                        else        { writememl(addr,armregs[RD]); }
                                        if (databort)
                                        {
//                                                rpclog("Data abort\n");
                                                break;
                                        }
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=2;
                                        break;

                                        case 0x41: case 0x49: /*LDR*/
                                        case 0x51: case 0x53: case 0x59: case 0x5B:
                                        case 0x61: case 0x69:
                                        case 0x71: case 0x73: case 0x79: case 0x7B:
                                        if ((opcode&0x2000010)==0x2000010)
                                        {
                                                undefined();
                                                break;
                                        }
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=readmeml(addr);
                                        templ=ldrresult(templ,addr);
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        LOADREG(RD,templ);
//                                        if (RD==15) refillpipeline();
                                        cycles-=3;
/*                                        if (RD==7)
                                        {
                                                if (!olog) olog=fopen("armlog.txt","wt");
                                                sprintf(s,"LDR R7 %08X,%07X\n",armregs[7],PC);
                                                fputs(s,olog);
                                        }*/
                                        break;

                                        case 0x65: case 0x6D:
                                        case 0x75: case 0x77: case 0x7D: case 0x7F:
                                        if (opcode&0x10)
                                        {
                                                undefined();
                                                break;
                                        }
                                        case 0x45: case 0x4D: /*LDRB*/
                                        case 0x55: case 0x57: case 0x5D: case 0x5F:
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=readmemb(addr);
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        armregs[RD]=templ;
                                        cycles-=3;
                                        break;

                                        case 0x64: case 0x6C:
                                        case 0x74: case 0x76: case 0x7C: case 0x7E:
                                        if (opcode&0x10)
                                        {
                                                undefined();
                                                break;
                                        }
                                        case 0x44: case 0x4C: /*STRB*/
                                        case 0x54: case 0x56: case 0x5C: case 0x5E:
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shift2(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        writememb(addr,armregs[RD]);
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=2;
                                        break;
                                        
                                        case 0x80: case 0x81: case 0x82: case 0x83:
                                        case 0x84: case 0x85: case 0x86: case 0x87:
                                        case 0x88: case 0x89: case 0x8A: case 0x8B:
                                        case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                                        case 0x90: case 0x91: case 0x92: case 0x93:
                                        case 0x94: case 0x95: case 0x96: case 0x97:
                                        case 0x98: case 0x99: case 0x9A: case 0x9B:
                                        case 0x9C: case 0x9D: case 0x9E: case 0x9F:
                                        ldmstm((opcode>>20)&0xFF, opcode);
                                        break;
//#endif

                                        case 0xB0: case 0xB1: case 0xB2: case 0xB3: /*BL*/
                                        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        if (templ&0x2000000) templ|=0xFC000000;
                                        armregs[14]=armregs[15]-4;
                                        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
                                        refillpipeline();
                                        cycles-=3;
                                        break;

                                        case 0xA0: case 0xA1: case 0xA2: case 0xA3: /*B*/
                                        case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        if (templ&0x2000000) templ|=0xFC000000;
                                        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
                                        refillpipeline();
                                        cycles-=3;
                                        break;

                                        case 0xE0: case 0xE2: case 0xE4: case 0xE6: /*MCR*/
                                        case 0xE8: case 0xEA: case 0xEC: case 0xEE:
                                        if (MULRS==15 && (opcode&0x10))
                                        {
                                                writecp15(RN,armregs[RD]);
                                        }
                                        else
                                        {
//                                                output=1;
//                                                timetolive=500;
                                                undefined();
/*                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();*/
                                        }
                                        break;

                                        case 0xE1: case 0xE3: case 0xE5: case 0xE7: /*MRC*/
                                        case 0xE9: case 0xEB: case 0xED: case 0xEF:
                                        if (MULRS==15 && (opcode&0x10))
                                        {
                                                if (RD==15) armregs[RD]=(armregs[RD]&r15mask)|(readcp15(RN)&~r15mask);
                                                else        armregs[RD]=readcp15(RN);
                                        }
                                        else
                                        {
                                                undefined();
/*                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();*/
                                        }
                                        break;

//#if 0
                                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*Co-pro*/
                                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                                        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                                        case 0xD0: case 0xD1: case 0xD2: case 0xD3:
                                        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                                        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
/*                                        if ((opcode>=0xEC500000) && (opcode<=0xEC500007))
                                           arculfs(opcode&7);
                                        else if (opcode==0xEC500008)
                                        {
                                                timetolive=500;
                                        }
                                        else
                                        {*/
                                        undefined();
//                                        timetolive=20;
//                                        printf("0 : %08X 4 : %08X\n",readmeml(0),readmeml(4));
//                                        }
                                        break;
//#endif
                                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*SWI*/
                                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                                        case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:

                                        templ=opcode&0xDFFFF;
//                                        if (templ>0x200 && templ!=0x41502) rpclog("SWI %05X %07X\n",templ,PC);
					if (templ == ARCEM_SWI_HOSTFS)
					  {
//                                                        dbug_hostfs("ARCEM_SWI %08X\n",templ);
					    ARMul_State state;
					    state.Reg = armregs;
					    hostfs(&state);
//					    dbug_hostfs("Results : %08X %08X %08X %08X\n",armregs[2],armregs[3],armregs[4],armregs[5]);
					  }
					  else
					  {

//                                        rpclog("SWI %05X\n",templ);
//                                        if ((opcode&0x4FFFF)==0x2B)
//                                           rpclog("OS_GenerateError %08X %08X %08X %08X %08X %08X\n",opcode,armregs[0],armregs[1],armregs[2],armregs[3],PC);
/*                                        if ((opcode&0x4FFFF)==0x0001E)
                                           rpclog("OS_Module %08X %08X %08X %08X\n",armregs[0],armregs[1],armregs[2],armregs[3]);
                                        if ((opcode&0x4FFFF)==0x00030)
                                           rpclog("OS_ServiceCall %08X %08X %08X %08X\n",armregs[0],armregs[1],armregs[2],armregs[3]);*/
/*                                        if (templ>=0x40540 && templ<=0x4054B)
                                        {
                                                printf("FileCore SWI %08X %08X %08X %08X %08X %08X %07X\n",opcode,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],PC);
                                        }*/
//                                        if ((opcode&0xFFFFF)==0x20030) printf("SWI %05X %08X %08X %08X %08X %i %i\n",opcode&0xFFFFF,armregs[0],armregs[1],armregs[2],armregs[15],irq,armirq);
//                                        if ((opcode&0xFFFFF)==0x20006 && armregs[0]==0x81 && armregs[1]==0xA4) out2=1;
                                        if (mode&16)
                                        {
                                                templ=armregs[15]-4;
                                                spsr[SUPERVISOR]=armregs[16];
                                                updatemode(SUPERVISOR|16);
                                                armregs[14]=templ;
                                                armregs[16]&=~0x1F;
                                                armregs[16]|=0x93;
                                                armregs[15]=0x0000000C;
                                                refillpipeline();
                                        }
                                        else if (prog32)
                                        {
                                                templ=armregs[15]-4;
                                                updatemode(SUPERVISOR|16);
                                                armregs[14]=templ&0x3FFFFFC;
                                                spsr[SUPERVISOR]=(armregs[16]&~0x1F)|(templ&3);
                                                armregs[15]=0x0000000C;
                                                armregs[16]|=0x80;
                                                cycles-=4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x0800000C;
                                                cycles-=4;
                                                refillpipeline();
                                        }
//                                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                                        }
                                        break;

				          default:
					    {
					      int ls_opcode = (opcode >> 20) & 0xff;

					      if (ls_opcode >= 0x80 && ls_opcode <= 0xa0)
					        ldmstm(ls_opcode, opcode);
                                              else
					        bad_opcode(opcode);
					    }
                                }
                        }
#ifdef STRONGARM
                        }
#endif
                        if (databort|armirq|prefabort)
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
                                if (output) rpclog("Exception process - %i %i %i %08X %08X %i\n",databort,armirq,prefabort,armregs[15],armregs[16],inscount);
//                                if (out2) printf("PC at the moment %07X %i %i %02X %08X\n",PC,ins,mode,armregs[16]&0xC0,armregs[15]);
                                if (prefabort)       /*Prefetch abort*/
                                {
                                error("Exception %i %i %i\n",databort,armirq,prefabort);
                                dumpregs();
                                exit(-1);
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000010;
                                        refillpipeline();
                                        prefabort=0;
                                }
                                else if (databort==1)     /*Data abort*/
                                {
/*                                error("Exception %i %i %i\n",databort,armirq,prefabort);
                                dumpregs();
                                exit(-1);*/
                                        if (mode&16)
                                        {
                                                templ=armregs[15];
                                                spsr[ABORT]=armregs[16];
                                                updatemode(ABORT|16);
                                                armregs[14]=templ;
                                                armregs[16]&=~0x1F;
                                                armregs[16]|=0xD7;
                                                armregs[15]=0x000000014;
                                                refillpipeline();
//                                                timetolive=500;
                                        }
                                        else if (prog32)
                                        {
                                                templ=armregs[15];
                                                updatemode(ABORT|16);
                                                armregs[14]=templ&0x3FFFFFC;
                                                spsr[ABORT]=(armregs[16]&~0x1F)|(templ&3);
                                                spsr[ABORT]&=~0x10;
                                                armregs[16]&=~0x1F;
                                                armregs[16]|=0xD7;
                                                armregs[15]=0x000000014;
                                                refillpipeline();
//                                                printf("R8 %08X R11 %08X R12 %08X R13 %08X\n",armregs[8],armregs[11],armregs[12],armregs[13]);
                                        }
                                        else
                                        {
                                                templ=armregs[15];
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x0C000014;
                                                refillpipeline();
                                        }
                                        databort=0;
                                }
                                else if (databort==2) /*Address Exception*/
                                {
                                error("Exception %i %i %i\n",databort,armirq,prefabort);
                                dumpregs();
                                exit(-1);
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000018;
                                        refillpipeline();
                                        databort=0;
                                }
                                else if ((armirq&2) && !(armregs[16]&0x40)) /*FIQ*/
                                {
/*                                        if (!fiqregs[11])
                                        {
                                                timetolive=500;
                                        }*/
//                                        if (output) printf("FIQ - FIQ R8=%08X\n",fiqregs[8]);
//                                        printf("%i : %07X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X  %08X %08X %08X - %08X %08X %08X %i R10=%08X\n",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[9],armregs[12],armregs[13],armregs[14],armregs[15],armregs[16],opcode,mode,armregs[10]);
/*                                printf("Exception %i %i %i\n",databort,armirq,prefabort);
                                dumpregs();
                                exit(-1);
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(FIQ);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000001;
                                        armregs[15]|=0x0C000020;
                                        refillpipeline();*/
                                        if (mode&16)
                                        {
                                                templ=armregs[15];
                                                spsr[FIQ]=armregs[16];
                                                updatemode(FIQ|16);
                                                armregs[14]=templ;
                                                armregs[16]&=~0x1F;
                                                armregs[16]|=0xD1;
                                                armregs[15]=0x000000020;
                                                refillpipeline();
//                                                timetolive=500;
                                        }
                                        else if (prog32)
                                        {
                                                templ=armregs[15];
                                                updatemode(FIQ|16);
                                                armregs[14]=templ&0x3FFFFFC;
                                                spsr[FIQ]=(armregs[16]&~0x1F)|(templ&3);
                                                spsr[FIQ]&=~0x10;
                                                armregs[16]|=0xC0;
                                                armregs[15]=0x000000020;
                                                refillpipeline();
//                                                printf("R8 %08X R11 %08X R12 %08X R13 %08X\n",armregs[8],armregs[11],armregs[12],armregs[13]);
                                        }
                                        else
                                        {
                                                templ=armregs[15];
                                                armregs[15]|=3;
                                                updatemode(FIQ);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000001;
                                                armregs[15]|=0x0C000020;
                                                refillpipeline();
                                        }
                                }
                                else if ((armirq&1) && !(armregs[16]&0x80)) /*IRQ*/
                                {
//                                        rpclog("IRQ %02X %02X\n",iomd.stata&iomd.maska,iomd.statb&iomd.maskb);
//                                        if (output) printf("IRQ %i %i\n",prog32,mode&16);
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
                                }
//                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                        }
                        armirq=irq;
                        armregs[15]+=4;
//                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
//                        if (output)
//                        {
//                                if (!olog) olog=fopen("olog.txt","wt");
//                                rpclog("%i : %07X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X  %08X %08X %08X - %08X %08X %08X %i R10=%08X R11=%08X R12=%08X %08X %08X %08X %08X\n",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[9],armregs[10],armregs[12],armregs[13],armregs[14],armregs[15],armregs[16],opcode,mode,armregs[10],armregs[11],armregs[12],spsr[mode&15],armregs[16],armregs[15],armregs[14]);
//                                fputs(err2,olog);
//                        }
/*                        if (timetolive)
                        {
                                timetolive--;
                                if (!timetolive)
                                   output=0;
                        }*/
//                        if (PC==0x38FD7D4) printf("R10=%08X\n",armregs[10]);
                        cyc=(oldcyc2-cycles);
                        linecyc-=cyc;
                        inscount++;
//                        ins++;
                }
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
                        fdccallback-=10;
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
                if (flyback) flyback--;
//                printf("T0 now %04X\n",iomd.t0c);
                cyc=(oldcyc-cycles);
        }
}
