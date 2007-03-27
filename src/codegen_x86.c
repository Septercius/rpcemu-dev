#include "rpcemu.h"
#include <stdint.h>
#include "codegen_x86.h"

//#define HASH(l) ((l>>3)&0x3FFF)
int blockend;
int blocknum,blockcount;

int codeblockpos;

#define addbyte(a)         codeblock[blockcount][blocknum][codeblockpos++]=a
#define addlong(a)         *((unsigned long *)&codeblock[blockcount][blocknum][codeblockpos])=a; \
                           codeblockpos+=4

void resetcodeblocks()
{
        int c;
        int d;
//        rpclog("Reset code blocks\n");
        memset(codeblockpc,0xFF,2*4*0x2000);
        memset(codeblockcount,0,2*0x2000);
/*        for (c=0;c<0x4000;c++)
        {
                for (d=0;d<4;d++)
                    codeblockpc[d][c]=0xFFFFFFFF;
                codeblockcount[c]=0;
        }*/
}

/*int isblockvalid(unsigned long l)
{
        if ((l&0xFFC00000)==0x3800000) return 1;
        return 0;
}*/

void initcodeblock(unsigned long l)
{
//        rpclog("Initcodeblock %08X\n",l);
        blocknum=HASH(l);
        blockcount=codeblockcount[blocknum];
        codeblockcount[blocknum]=(codeblockcount[blocknum]+1)&1;
        codeblockpos=0;
        codeblockpc[blockcount][blocknum]=l;
        addbyte(0xC3); /*RET*/
}
void generatecall(unsigned long addr)
{
        addbyte(0xE8); /*CALL*/
        addlong(addr-(uint32_t)(&codeblock[blockcount][blocknum][codeblockpos+4]));
}
void generatepcinc()
{
        addbyte(0x83); /*ADD $4,armregs[15]*/
        addbyte(0x05);
        addlong(&armregs[15]);
        addbyte(4);
        if (codeblockpos>=4000) blockend=1;
}
void endblock(int c)
{
        addbyte(0xC3); /*RET*/
        codeinscount[blockcount][blocknum]=c;
}
int codecallblock(unsigned long l)
{
        int hash=HASH(l);
        void (*gen_func)(void);
        if (codeblockpc[0][hash]==l)
        {
                gen_func=(void *)(&codeblock[0][HASH(l)][1]);
                gen_func();
                return 1;
        }
        if (codeblockpc[1][hash]==l)
        {
                gen_func=(void *)(&codeblock[1][HASH(l)][1]);
                gen_func();
                return 1;
        }
/*        if (codeblockpc[2][hash]==l)
        {
                gen_func=(void *)(&codeblock[2][HASH(l)][1]);
                gen_func();
                return 1;
        }
        if (codeblockpc[3][hash]==l)
        {
                gen_func=(void *)(&codeblock[3][HASH(l)][1]);
                gen_func();
                return 1;
        }*/
        return 0;
}
void generatemove(unsigned long addr, unsigned long dat)
{
        addbyte(0xC7); /*MOVL $dat,(addr)*/
        addbyte(0x05);
        addlong(addr);
        addlong(dat);
//        asm("movl $0x11223344,0x55667788");
}
void generateflagtestandbranch(unsigned long opcode, unsigned long *pcpsr)
{
        /*movl (pcpsr),%eax
          movl (opcode>>28)<<4,%edx
          shrl %eax,28
          or   %edx,%eax
          cmpb $0,(%eax,flaglookup)
          je skipins*/
/*        asm("movl 0x12345678,%eax;");
        asm("movl $0x10,%edx;");
        asm("shrl $28,%eax;");
        asm("or   %edx,%eax;");
        asm("cmpb $0x11,flaglookup(%eax);");
        asm("je   5;");*/
        if ((opcode>>28)==0xE) return; /*No need if 'always' condition code*/
        addbyte(0xA1);                 /*MOVL (pcpsr),%eax*/
        addlong((unsigned long)pcpsr);
//        addbyte(0xBA);                 /*MOVL ((opcode>>28)<<4),%edx*/
//        addlong((opcode>>28)<<4);
        addbyte(0xC1);                 /*SHRL $28,%eax*/
        addbyte(0xE8);
        addbyte(0x1C);
//        addbyte(0x09);                 /*ORL %edx,%eax*/
//        addbyte(0xD0);
        addbyte(0x80);                 /*CMPB $0,flaglookup(%eax)*/
        addbyte(0xB8);
        addlong((unsigned long)(&flaglookup[opcode>>28][0]));
        addbyte(0);
        addbyte(0x74);                 /*JE +5*/
        addbyte(5+10);
}
void generateirqtest()
{
//        asm("testb $0xC0,0x12345678");
//        addbyte(0xF6); /*TESTB $0xC0,armirq*/
//        addbyte(0x05);
//        addlong(&armirq);
//        addbyte(0xC0);
        addbyte(0x80); /*CMPB $0,armirq*/
        addbyte(0x3D);
        addlong(&armirq);
        addbyte(0);
        addbyte(0x0F); /*JNE 0*/
        addbyte(0x85);
        addlong(&codeblock[blockcount][blocknum][0]-(uint32_t)(&codeblock[blockcount][blocknum][codeblockpos+4]));
//        asm("cmpb $0,0x12345678;");
//        asm("jne 0;");
}
