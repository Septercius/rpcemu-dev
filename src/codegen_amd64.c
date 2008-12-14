#include "rpcemu.h"
#ifdef DYNAREC
#ifdef __amd64__

#include <stdint.h>
#include "codegen_amd64.h"
#include "mem.h"
#include "arm.h"

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

void generateupdatepc();
int lastflagchange;
unsigned char rcodeblock[BLOCKS][1792];
unsigned long codeblockaddr[BLOCKS];
uint32_t codeblockpc[0x8000];
unsigned char codeblockisrom[0x8000];
int codeblocknum[0x8000];
int codeinscount[0x8000];
unsigned char codeblockpresent[0x10000];

//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3FFF)
int blockend;
int blocknum;//,blockcount;
int tempinscount;

int codeblockpos;

#define addbyte(a)         rcodeblock[blockpoint2][codeblockpos++]=a
#define addlong(a)         *((unsigned long *)&rcodeblock[blockpoint2][codeblockpos])=a; \
                           codeblockpos+=4

int blockpoint=0,blockpoint2;
uint32_t blocks[BLOCKS];
int pcinc=0;
void initcodeblocks()
{
        int c;
#ifdef __linux__
	void *start;
	size_t len;
	long pagesize = sysconf(_SC_PAGESIZE);
	long pagemask = ~(pagesize - 1);
#endif
        /*Clear all blocks*/
        memset(codeblockpc,0xFF,4*0x1000);
//        memset(codeblockcount,0,0x1000);
        blockpoint=0;
        for (c=0;c<BLOCKS;c++) blocks[c]=0xFFFFFFFF;
        for (c=0;c<BLOCKS;c++) codeblockaddr[c]=&rcodeblock[c][4];

#ifdef __linux__
	/* Set memory pages containing rcodeblock[]s executable -
	   necessary when NX/XD feature is active on CPU(s) */
	start = (void *)((long)rcodeblock & pagemask);
	len = (sizeof rcodeblock + pagesize) & pagemask;
	if (mprotect(start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		perror("mprotect");
		exit(1);
	}
#endif
}

void resetcodeblocks()
{
        int c;
        /*Clear all blocks _except_ those pointing between 0x3800000 and 0x3FFFFFF (ROM)*/
/*        for (c=0;c<0x1000;c++)
        {
                if ((codeblockpc[c][2]&0xFF800000)!=0x3800000)
                   codeblockpc[c][2]=0xFFFFFFFF;
                if ((codeblockpc[c][1]&0xFF800000)!=0x3800000)
                {
                        codeblockpc[c][1]=0xFFFFFFFF;
                        codeblockcount[c]=1;
                }
                if ((codeblockpc[c][0]&0xFF800000)!=0x3800000)
                {
                        codeblockpc[c][0]=0xFFFFFFFF;
                        codeblockcount[c]=0;
                }
        }*/
        blockpoint=0;
        for (c=0;c<BLOCKS;c++)
        {
                if (blocks[c]!=0xFFFFFFFF)
                {
//                        if (codeblockisrom[blocks[c]&0x7FFF])
                        if ((codeblockpc[blocks[c]&0x7FFF]&0xFF800000)!=0x3800000)
                        {
                                codeblockpc[blocks[c]&0x7FFF]=0xFFFFFFFF;
                                codeblocknum[blocks[c]&0x7FFF]=0xFFFFFFFF;
                                blocks[c]=0xFFFFFFFF;
                        }
                }
        }
}

//#if 0
void cacheclearpage(uint32_t a)
{
        int c,d;
        if (!codeblockpresent[a&0xFFFF]) return;
        codeblockpresent[a&0xFFFF]=0;
        ins++;
//        a>>=10;
d=HASH(a<<12);
        for (c=0;c<0x400;c++)
        {
                if ((codeblockpc[c+d]>>12)==a) codeblockpc[c+d]=0xFFFFFFFF;
        }
/*        codeblockpc[hash][0]=0xFFFFFFFF;
        codeblockpc[hash][1]=0xFFFFFFFF;
        codeblockpc[hash][2]=0xFFFFFFFF;*/
//        waddrl=0xFFFFFFFF;
}
//#endif

/*int isblockvalid(unsigned long l)
{
        if ((l&0xFFC00000)==0x3800000) return 1;
        return 0;
}*/

void initcodeblock(uint32_t l)
{
	codeblockpresent[(l>>12)&0xFFFF]=1;
        tempinscount=0;
//        rpclog("Initcodeblock %08X\n",l);
        blockpoint++;
        blockpoint&=(BLOCKS-1);
        if (blocks[blockpoint]!=0xFFFFFFFF)
        {
//                rpclog("Chucking out block %08X %i %03X\n",blocks[blockpoint],blocks[blockpoint]>>24,blocks[blockpoint]&0xFFF);
                codeblockpc[blocks[blockpoint]&0x7FFF]=0xFFFFFFFF;
                codeblocknum[blocks[blockpoint]&0x7FFF]=0xFFFFFFFF;
        }
        blocknum=HASH(l);
//        blockcount=0;//codeblockcount[blocknum];
//        codeblockcount[blocknum]++;
//        if (codeblockcount[blocknum]==3) codeblockcount[blocknum]=0;
        codeblockpos=0;
        codeblockpc[blocknum]=l;
	//printf("New block %08X %08X %08X\n",blocknum,l,codeblockpc[blocknum]);
        codeblockisrom[blocknum]=pcisrom;
        codeblocknum[blocknum]=blockpoint;
        blocks[blockpoint]=blocknum;
        blockpoint2=blockpoint;
	addbyte(0x48); /*ADDL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x08);
        addbyte(0xC3); /*RET*/
addbyte(0); addbyte(0); addbyte(0);
	addbyte(0x48); /*SUBL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xEC);
        addbyte(0x08);
	addbyte(0x49); /*MOVQ armregs,%r15*/
	addbyte(0xBF);
	addlong(&armregs[0]);
	addlong(((uint64_t)(&armregs[0]))>>32);
//	printf("New block %08X %08X %08X\n",blocknum,l,codeblockpc[blocknum]);
}
uint32_t opcode;
void generatecall(OpFn addr, uint32_t opcode,uint32_t *pcpsr)
{
//asm("addq $4,0x12345678;");
        tempinscount++;
        //addbyte(0xC7); /*MOVL $opcode,(%esp)*/
        //addbyte(0x04);
        //addbyte(0x24);
	addbyte(0xBF); /*MOVL $opcode,%edi*/
        addlong(opcode);
        addbyte(0xE8); /*CALL*/
        addlong(((char *)addr-(char *)(&rcodeblock[blockpoint2][codeblockpos+4])));
//#if 0
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000)
        {
//                rpclog("Carrying on - %i\n",pcinc);
//                generateupdatepc();
        if (pcinc)
        {
//addbyte(0x67);
		addbyte(0x41); /*ADD $4,armregs[15](%r15)*/
		addbyte(0x83);
		addbyte(0x47);
		addbyte(15<<2); /*armregs[15]*/
		addbyte(pcinc);

//                addbyte(0x83); /*ADD $4,armregs[15]*/
                //addbyte(0x04);
		//addbyte(0x25);
                //addlong(&armregs[15]);
                //addbyte(pcinc);
//                pcinc=0;
        }
                addbyte(0xE9); /*JMP 0*/
                addlong(&rcodeblock[blockpoint2][0]-(uint32_t)(&rcodeblock[blockpoint2][codeblockpos+4]));
        }
//        #endif
}
void generateupdatepc()
{
//asm("addl $4,0x12345678;");
        if (pcinc)
      {
		addbyte(0x41); /*ADD $4,armregs[15](%r15)*/
		addbyte(0x83);
		addbyte(0x47);
		addbyte(15<<2); /*armregs[15]*/
		addbyte(pcinc);
//                addbyte(0x83); /*ADD $4,armregs[15]*/
                //addbyte(0x04);
		//addbyte(0x25);
                //addlong(&armregs[15]);
                //addbyte(pcinc);
                pcinc=0;
        }
}
void generateupdateinscount()
{
        if (tempinscount)
        {
                addbyte(0x83); /*ADD tempinscount,inscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&inscount);
                addbyte(tempinscount);
                tempinscount=0;
        }
}

void generatepcinc()
{
        pcinc+=4;
        if (pcinc==252) generateupdatepc();
        if (codeblockpos>=1400) blockend=1;
}

void removeblock()
{
        codeblockpc[blocknum]=0xFFFFFFFF;
        codeblocknum[blocknum]=0xFFFFFFFF;
}

int linecyc;
void endblock(int c, uint32_t *pcpsr)
{
        /*asm("decl 0x12345678;");
	asm("testb $0xFF,0x12345678;");
asm("mov %eax,%edx;");
asm("and $0x12345678,%eax;");
asm("and $0x12345678,%edx;");
asm("cmp 0x12345678(%edx),%eax;");
asm("mov 0x12345678(%edx),%eax;");
asm("mov 0x12345678(,%eax,8),%rax;");
asm("jmp *%rax;");*/
/*asm("mov %rax,%rdx;");
asm("cmp 0x12345678(%rdx),%eax;");
asm("movl 0x12345678(%rdx),%eax;");
asm("movq 0x12345678(,%rax,8),%rax;");*/
        generateupdatepc();
        generateupdateinscount();
	addbyte(0x48); /*ADDL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x08);
        
        if (c<128)
        {
                addbyte(0x83); /*ADDL $c,rinscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&rinscount);
                addbyte(c);
        }
        else
        {
                addbyte(0x81); /*ADDL $c,rinscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&rinscount);
                addlong(c);
        }

        addbyte(0xFF); /*DECL linecyc*/
        addbyte(0x0C);
	addbyte(0x25);
        addlong(&linecyc);
addbyte(0xC3); /*RET*/
        
        addbyte(0x79); /*JNS +1*/
        addbyte(1);
        addbyte(0xC3); /*RET*/
        addbyte(0xF6); /*TESTB $0xFF,armirq*/
        addbyte(0x04);
	addbyte(0x25);
        addlong(&armirq);
        addbyte(0xFF);
        addbyte(0x75); /*JNZ*/
        addbyte(-11);

        addbyte(0x8B); /*MOVL armregs[15],%eax*/
	addbyte(0x04);
	addbyte(0x25);
        addlong(&armregs[15]);
        addbyte(0x83); /*SUBL $8,%eax*/
        addbyte(0xE8);
        addbyte(0x08);
	addbyte(0x48); /*MOVQ %rax,%rdx*/
        addbyte(0x89);
        addbyte(0xC2);
        if (r15mask!=0xFFFFFFFF)
        {
                addbyte(0x25); /*ANDL $r15mask,%eax*/
                addlong(r15mask);
        }
        addbyte(0x81); /*ANDL $0x1FFFC,%edx*/
        addbyte(0xE2);
        addlong(0x1FFFC);
	addbyte(0x67); /*CMPL codeblockpc[%edx],%eax*/
        addbyte(0x3B);
        addbyte(0x82);
        addlong(codeblockpc);
        addbyte(0x74); /*JZ +1*/
        addbyte(1);
        addbyte(0xC3); /*RET*/
        addbyte(0x8B); /*MOVL codeblocknum[%rdx],%eax*/
        addbyte(0x82);
        addlong(codeblocknum);
	addbyte(0x48); /*MOVL codeblockaddr[%eax*4],%rax*/
        addbyte(0x8B);
        addbyte(0x04);
        addbyte(0xC5);
        addlong(codeblockaddr);
        addbyte(0xFF); /*JMP *%rax*/
        addbyte(0xE0);
        codeinscount[blocknum]=c;
}

void dumplastblock()
{
/*        FILE *f=fopen("block.dmp","wb");
        fwrite(codeblock[blockcount][blocknum],1600,1,f);
        fclose(f);*/
}

/*int codecallblock(unsigned long l)
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
        return 0;
}*/
void generatemove(unsigned long addr, unsigned long dat)
{
//        asm("movl $0x12345678,0x9ABCDEF0");
        addbyte(0xC7); /*MOVL $dat,(addr)*/
        addbyte(0x04);
	addbyte(0x25);
        addlong(addr);
        addlong(dat);
//        asm("movl $0x11223344,0x55667788");
}
void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr)
{
//asm("movl 0x12345678,%eax;");
//asm("shrl $28,%eax;");
//asm("cmpb $0,0x12345678(%rax);");
        /*movl (pcpsr),%eax
          movl (opcode>>28)<<4,%edx
          shrl %eax,28
          or   %edx,%eax
          cmpb $0,(%eax,flaglookup)
          je skipins*/
	        if ((opcode>>28)==0xE) return; /*No need if 'always' condition code*/
        switch (opcode>>28)
        {
                case 0: /*EQ*/
                case 1: /*NE*/
                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x40*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(((uint32_t)pcpsr)+3);
                addbyte(0x40);
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                case 2: /*CS*/
                case 3: /*CC*/
                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x20*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(((uint32_t)pcpsr)+3);
                addbyte(0x20);
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                case 4: /*MI*/
                case 5: /*PL*/
                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x80*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(((uint32_t)pcpsr)+3);
                addbyte(0x80);
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                case 6: /*VS*/
                case 7: /*VC*/
                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x10*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(((uint32_t)pcpsr)+3);
                addbyte(0x10);
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                default:
                addbyte(0x8B);                 /*MOVL (pcpsr),%eax*/
		addbyte(0x04);
		addbyte(0x25);
                addlong((uint32_t)pcpsr);
                addbyte(0xC1);                 /*SHRL $28,%eax*/
                addbyte(0xE8);
                addbyte(0x1C);
                addbyte(0x80);                 /*CMPB $0,flaglookup(%eax)*/
                addbyte(0xB8);
                addlong((uint32_t)(&flaglookup[opcode>>28][0]));
                addbyte(0);
                addbyte(0x74);                 /*JE +5*/
                break;
        }
//        if (output) rpclog("PC %07X - %08X  %i\n",PC,opcode,(((opcode+0x6000000)&0xF000000)>0xA000000));
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000) addbyte(5+5+5+((pcinc)?5:0));
#ifdef ABORTCHECKING
        else if (((opcode+0x6000000)&0xF000000)>=0xA000000)
        {
                if (((uint32_t)(&rcodeblock[blockpoint2][codeblockpos+17])-(uint32_t)(&rcodeblock[blockpoint2][0]))<120) addbyte(5+5+2+2);
                else                                                                                                                    addbyte(5+5+2+6);
        }
#endif
        else                                                                        addbyte(5+5);
}
void generateirqtest()
{
        //asm("testl %eax,%eax");
//asm("jne 0");
//        asm("testb $0xC0,0x12345678");
//        addbyte(0xF6); /*TESTB $0x40,armirq*/
//        addbyte(0x05);
//        addlong(&armirq);
//        addbyte(0x40);
        addbyte(0x85); /*TESTL %eax,%eax*/
        addbyte(0xC0);
//        #if 0
        if (((uint32_t)(&rcodeblock[blockpoint2][codeblockpos+4])-(uint32_t)(&rcodeblock[blockpoint2][0]))<120)
        {
                addbyte(0x75); /*JNE 0*/
                addbyte(&rcodeblock[blockpoint2][0]-(uint32_t)(&rcodeblock[blockpoint2][codeblockpos+1]));
        }
        else
        {
//                #endif
                addbyte(0x0F); /*JNE 0*/
                addbyte(0x85);
                addlong(&rcodeblock[blockpoint2][0]-(uint32_t)(&rcodeblock[blockpoint2][codeblockpos+4]));
        }
}
#endif
#endif
