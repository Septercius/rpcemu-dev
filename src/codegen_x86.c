//ESI is pointer to armregs[]

#include "rpcemu.h"

#ifdef DYNAREC
#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include <stdint.h>
#include "codegen_x86.h"
#include "mem.h"
#include "arm.h"
#include "cp15.h"

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

void generateupdatepc(void);
int linecyc;
int hasldrb[BLOCKS];
#define mwritemem rcodeblock[BLOCKS]
#define mreadmem rcodeblock[BLOCKS+1]
#define mreadmemfast rcodeblock[BLOCKS+2]
#define mreadmemslow &rcodeblock[BLOCKS+2][256]
#define mwritememfast rcodeblock[BLOCKS+3]
#define mwritememslow &rcodeblock[BLOCKS+3][256]
static void codereadmemlnt(void);
static void codewritememflnt(void);
//#define mreadmem rcodeblock[BLOCKS+1]
unsigned char rcodeblock[BLOCKS+4][1792+512+64] = {{0}};
static unsigned long codeblockaddr[BLOCKS];
unsigned long codeblockpc[0x8000] = {0};
static unsigned char codeblockpresent[0x10000];
int codeblocknum[0x8000] = {0};
int codeinscount[0x8000] = {0};

static int flagsdirty = 0;
//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3FFF)
int blockend = 0;
static int blocknum;//,blockcount;
static int tempinscount;

static int bigflagtest = 0;
static int codeblockpos = 0;

#define addbyte(a)         rcodeblock[blockpoint2][codeblockpos]=(uint8_t)(a),codeblockpos++
#define addlong(a)         *((unsigned long *)&rcodeblock[blockpoint2][codeblockpos])=(unsigned long)a; \
                           codeblockpos+=4

static unsigned char lahftable[256], lahftablesub[256];

static void generateloadgen(int reg, int x86reg);
static void generatesavegen(int reg, int x86reg);

static int blockpoint = 0, blockpoint2 = 0;
static uint32_t blocks[BLOCKS];
static int pcinc = 0;

#include "codegen_x86_common.h"

void initcodeblocks(void)
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
        for (c=0;c<BLOCKS;c++) codeblockaddr[c]=(unsigned long)&rcodeblock[c][12];
        for (c=0;c<256;c++)
        {
                lahftable[c]=0;
                if (c&1) lahftable[c]|=0x20; /*C flag*/
                lahftable[c]|=(c&0xC0);      /*Z and N flags*/
                lahftablesub[c]=lahftable[c]^0x20;
        }
        /*Generate mwritemem*/
        /*EDI=address,EBX=data*/
        blockpoint2=BLOCKS;
        codeblockpos=0;
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHR $12,%edx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x95); /*MOV vwaddrl(,%edx,4),%ecx*/
        addlong(vwaddrl);
        addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
        addbyte(0x75); addbyte(4); /*JNZ inbuffer*/
        addbyte(0x89); addbyte(0x1C); addbyte(0x39); /*MOVL %ebx,(%ecx,%edi)*/
        addbyte(0xC3); /*RET*/
        
        addbyte(0x53); /*PUSH %ebx*/
        addbyte(0x57); /*PUSH %edi*/
        gen_x86_call(writememfl);

        //addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        //gen_x86_call(codewritememflnt);
        
        addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
        addbyte(0xC1); addbyte(0xE9); addbyte(12); /*SHR $12,%ecx*/
        addbyte(0x83); addbyte(0xC4); addbyte(0x08); /*ADDL $8,%esp*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x8D); addlong(vwaddrl); /*MOV vwaddrl(,%ecx,4),%ecx*/
        addbyte(0xC3); /*RET*/

        /*Generate mreadmem*/
        blockpoint2=BLOCKS+1;
        codeblockpos=0;
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHR $12,%edx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x95); /*MOV vraddrl(,%edx,4),%ecx*/
        addlong(vraddrl);
        addbyte(0xF6); addbyte(0xC1); addbyte(1); /*TST %cl,1*/
        addbyte(0x75); addbyte(4); /*JNZ notinbuffer*/
        addbyte(0x8B); addbyte(0x14); addbyte(0x39); /*MOVL (%ecx,%edi),%edx*/
        addbyte(0xC3); /*RET*/
        addbyte(0x57); /*PUSH %edi*/
        gen_x86_call(readmemfl);
        addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
        addbyte(0xC1); addbyte(0xE9); addbyte(12); /*SHR $12,%ecx*/
        addbyte(0x83); addbyte(0xC4); addbyte(0x04); /*ADDL $4,%esp*/
        addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x8D); addlong(vraddrl); /*MOV vraddrl(,%ecx,4),%ecx*/
        addbyte(0xC3); /*RET*/

        /*Generatemreadmemfast*/
        blockpoint2=BLOCKS+2;
        codeblockpos=0;
        addbyte(0xF7); addbyte(0xC7); addlong(0xFFF); /*TST $0xFFF,%edi*/
        addbyte(0x75); addbyte(41); /*JNZ samepage*/
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHR $12,%edx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x95); /*MOV vraddrl(,%edx,4),%ecx*/
        addlong(vraddrl);
        addbyte(0xF6); addbyte(0xC1); addbyte(1); /*TST %cl,1*/
        addbyte(0x75); addbyte(4); /*JNZ notinbuffer*/
        addbyte(0x8B); addbyte(0x14); addbyte(0x39); /*MOVL (%ecx,%edi),%edx*/
        addbyte(0xC3); /*RET*/
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        gen_x86_call(codereadmemlnt);
        addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
        addbyte(0xC1); addbyte(0xE9); addbyte(12); /*SHR $12,%ecx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x8D); addlong(vraddrl); /*MOV vraddrl(,%ecx,4),%ecx*/
        addbyte(0xC3); /*RET*/
        /*.samepage*/
        addbyte(0xF6); addbyte(0xC1); addbyte(1); /*TST %cl,1*/
        addbyte(0x75); addbyte(/*8-(codeblockpos+1)*/-46); /*JNZ backup*/
        addbyte(0x8B); addbyte(0x14); addbyte(0x39); /*MOVL (%ecx,%edi),%edx*/
        addbyte(0xC3); /*RET*/

        /*Generatemreadmemslow*/
        blockpoint2=BLOCKS+2;
        codeblockpos=256;
        /*EDI=address, EBP=mask*/
        for (c=1;c<16;c++)
        {
                addbyte(0xD1); addbyte(0xED); /*SHR $1,%ebp*/
                addbyte(0x73); addbyte(8+3); /*JNC next*/
                gen_x86_call(mreadmem);
                generatesavegen(c,EDX);
                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
        }
        addbyte(0xC3); /*RET*/
        
        /*Generatemwritememfast*/
        blockpoint2=BLOCKS+3;
        codeblockpos=0;
        addbyte(0xF7); addbyte(0xC7); addlong(0xFFF); /*TST $0xFFF,%edi*/
        addbyte(0x75); addbyte(41+3+3); /*JNZ samepage*/
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHR $12,%edx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x95); /*MOV vwaddrl(,%edx,4),%ecx*/
        addlong(vwaddrl);
        addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
        addbyte(0x75); addbyte(4+3); /*JNZ notinbuffer*/
        addbyte(0x89); addbyte(0x1C); addbyte(0x39); /*MOVL %ebx,(%ecx,%edi)*/
addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
        addbyte(0xC3); /*RET*/
        addbyte(0x89); addbyte(0xFA); /*MOVL %edi,%edx*/
        gen_x86_call(codewritememflnt);
        addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
        addbyte(0xC1); addbyte(0xE9); addbyte(12); /*SHR $12,%ecx*/
        addbyte(0x8B); addbyte(0x0C); addbyte(0x8D); addlong(vwaddrl); /*MOV vwaddrl(,%ecx,4),%ecx*/
addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
        addbyte(0xC3); /*RET*/
        /*.samepage*/
//        addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,1*/
//        addbyte(0x75); addbyte(8-(codeblockpos+1)); /*JNZ backup*/
        addbyte(0x89); addbyte(0x1C); addbyte(0x39); /*MOVL %ebx,(%ecx,%edi)*/
addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
        addbyte(0xC3); /*RET*/

        /*Generatemwritememslow*/
        blockpoint2=BLOCKS+3;
        codeblockpos=256;
        /*EDI=address, EBP=mask*/
        for (c=1;c<16;c++)
        {
                addbyte(0xD1); addbyte(0xED); /*SHR $1,%ebp*/
                generateloadgen(c,EBX); /*MOVL armregs[c],%ebx*/
                addbyte(0x73); addbyte(5+3); /*JNC next*/
                gen_x86_call(mwritemem);
                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
        }
        addbyte(0xC3); /*RET*/

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

void resetcodeblocks(void)
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
}
//#endif

/*int isblockvalid(unsigned long l)
{
        if ((l&0xFFC00000)==0x3800000) return 1;
        return 0;
}*/

static uint32_t currentblockpc, currentblockpc2;

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
        codeblocknum[blocknum]=blockpoint;
        blocks[blockpoint]=blocknum;
        blockpoint2=blockpoint;
        
        hasldrb[blockpoint2]=0;
        addbyte(0x83); /*ADDL $8,%esp*/
        addbyte(0xC4);
        addbyte(0x08);
        addbyte(0xC3); /*RET*/
        addbyte(0xE9); /*JMP end*/
        addlong(0); /*Don't know where end is yet - see endblock()*/
        addbyte(0); addbyte(0); addbyte(0); /*Padding*/
        addbyte(0x83); /*SUBL $8,%esp*/
        addbyte(0xEC);
        addbyte(0x08);
#ifndef _MSC_VER
        addbyte(0xBE); addlong(armregs); /*MOVL armregs,%esi*/
#endif
        currentblockpc=armregs[15]&r15mask;
        currentblockpc2=PC;
        flagsdirty=0;
}

void removeblock(void)
{
        codeblockpc[blocknum]=0xFFFFFFFF;
        codeblocknum[blocknum]=0xFFFFFFFF;
}

int lastflagchange=0;

static const int recompileinstructions[256]=
{
        1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0, //00
        0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, //10
        1&0,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0, //20
        0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, //30
	#ifdef _MSC_VER
        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //40
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //50
        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //60
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //70

        1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, //80
        1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, //90
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //A0
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //B0
	#else
        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //40
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //50
        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //60
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //70

        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //80
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //90
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //A0
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //B0
	#endif
//        1,1,1,1,1,0,1,0,1,1,1,1,1,0,1,0, //80
//        1,1,1,1,1,0,1,0,1,1,1,1,1,0,1,0, //90
        
//        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //A0
//        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //B0
        
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //C0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //D0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //E0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  //F0
};

static void generateloadgen(int reg, int x86reg)
{
	#ifdef _MSC_VER
        addbyte(0x8B); addbyte(5|x86reg); addlong(&armregs[reg]);
	#else
        if (reg)
        {
                addbyte(0x8B); addbyte(0x46|x86reg); addbyte(reg*4);
        }
        else
        {
                addbyte(0x8B); addbyte(0x06|x86reg);
        }
	#endif
}

static inline void generateload(int reg)
{
#ifdef _MSC_VER
	addbyte(0xa1); addlong(&armregs[reg]);
#else
	generateloadgen(reg, EAX);
#endif
}

static void generatesavegen(int reg, int x86reg)
{
	#ifdef _MSC_VER
        addbyte(0x89); addbyte(5|x86reg); addlong(&armregs[reg]);
	#else
        if (reg)
        {
                addbyte(0x89); addbyte(0x46|x86reg); addbyte(reg*4);
        }
        else
        {
                addbyte(0x89); addbyte(0x06|x86reg);
        }
	#endif
}

static inline void generatesave(int reg)
{
#ifdef _MSC_VER
	addbyte(0xa3); addlong(&armregs[reg]);
#else
	generatesavegen(reg, EAX);
#endif
}

static int generatedataproc(uint32_t opcode, unsigned char dataop, uint32_t templ)
{
        int temp=0;
//        #if 0
        if (RN==RD)
        {
                addbyte(0x81); /*ORRL $dat,(addr)*/
                addbyte(0x05|dataop);
                addlong(&armregs[RD]);
                addlong(templ);
                temp+=10;
        }
        else
        {
//                #endif
                generateload(RN);
                if (RN==15 && r15mask!=0xFFFFFFFC)
                {
                        addbyte(0x25); addlong(r15mask); /*ANDL $r15mask,%eax*/
                }
                if (!(templ&~0x7F))
                {
                        addbyte(0x83); /*ORRL $8,%eax*/
                        addbyte(0xC0|dataop);
                        addbyte(templ);
                }
                else
                {
                        addbyte(0x81); /*ORRL $8,%eax*/
                        addbyte(0xC0|dataop);
                        addlong(templ);
                        temp+=3;
                }
                generatesave(RD);
                temp+=5+3+5;
        }
        return temp;
}

static int generatedataprocS(uint32_t opcode, unsigned char dataop, uint32_t templ)
{
        int temp=0;
        if (RN==RD)
        {
                addbyte(0x81); /*ORRL $dat,(addr)*/
                addbyte(0x05|dataop);
                addlong(&armregs[RD]);
                addlong(templ);
                addbyte(0x9F); /*LAHF*/
                temp+=10;
        }
        else
        {
                generateloadgen(RN,EDX);
                if (RN==15 && r15mask!=0xFFFFFFFC)
                {
                        addbyte(0x81); addbyte(0xE2); addlong(r15mask); /*ANDL $r15mask,%edx*/
                }
                if (!(templ&~0x7F))
                {
                        addbyte(0x83); /*ORRL $8,%edx*/
                        addbyte(0xC2|dataop);
                        addbyte(templ);
                }
                else
                {
                        addbyte(0x81); /*ORRL $8,%edx*/
                        addbyte(0xC2|dataop);
                        addlong(templ);
                        temp+=3;
                }
                addbyte(0x9F); /*LAHF*/
                generatesavegen(RD,EDX);
                temp+=5+3+5;
        }
//                      addbyte(0x9F); /*LAHF*/
        return temp;
}

/*static int codewritememfb(void)
{
        uint32_t a;
        uint8_t v;
        asm("movl %%edx,%0;"
            "movb %%bl,%1;"
            : "=&q" (a),
              "=&q" (v)
        );
//        rpclog("Writememfb %08X %02X %07X %08X %08X\n",a,v,PC,armregs[6],armregs[7]);
        writememfb(a,v);
        return armirq&0x40;
}*/

/*Nasty hack! I hope to get rid of this soon.
  What I _should_ be doing is pushing edx and bl on the stack, and calling writememfb
  directly. Instead, I just call this. The register variables are to preserve the registers
  across the call, and stop GCC's optimiser breaking the code*/
#ifdef _MSC_VER
static int codewritememfb(void)
{
        uint32_t a;
        uint8_t v;
		_asm
		{
			mov a,edx
			mov v,bl
		}
		writememfb(a,v);
        return (armirq&0x40)?1:0;
}

static int codewritememfl(void)
{
        uint32_t a;
        uint32_t v;
		_asm
		{
			mov a,edx
			mov v,ebx
		}
        writememfl(a,v);
        return (armirq&0x40)?1:0;
}

static int codereadmemb(void)
{
        uint32_t a;
        uint32_t v;
		_asm mov a,edx
		v=readmemfb(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
		_asm mov ecx,v
		return (armirq&0x40)?1:0;
}

static int codereadmeml(void)
{
        uint32_t a;
        uint32_t v;
		_asm mov a,edx
        v=readmemfl(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
		_asm mov edx,v
        return (armirq&0x40)?1:0;
}

static int mwritemem(void)
{
        uint32_t a;
        uint32_t v;
		_asm mov a,edi
		_asm mov v,eax
        writememl(a,v);
        return (armirq&0x40)?1:0;
}

static int mreadmem(void)
{
        uint32_t a;
        uint32_t v;
		_asm mov a,edi
        v=readmeml(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
		_asm mov edx,v
        return (armirq&0x40)?1:0;
}
#else
static int codewritememfb(void)
{
        register uint32_t a asm("edx");
        register uint8_t v asm("bl");
        writememfb(a,v);
        return (armirq&0x40)?1:0;
}

static int codewritememfl(void)
{
        register uint32_t a asm("edx");
        register uint32_t v asm("ebx");
        writememfl(a,v);
        return (armirq&0x40)?1:0;
}

static void codewritememflnt(void)
{
        register uint32_t a asm("edx");
        register uint32_t v asm("ebx");
        writememfl(a,v);
}

static int codereadmemb(void)
{
        register uint32_t a asm("edx");
        register uint32_t v asm("ecx");
        v=readmemfb(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
        asm("movl %0,%%edx;"
            :
            : "r" (v)
        );
        return (armirq&0x40)?1:0;
}

static int codereadmeml(void)
{
        register uint32_t a asm("edx");
        register uint32_t v asm("ecx");
        v=readmemfl(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
        asm("movl %0,%%edx;"
            :
            : "r" (v)
        );
        return (armirq&0x40)?1:0;
}

static void codereadmemlnt(void)
{
        register uint32_t a asm("edx");
//        register uint32_t v asm("ecx");
        a=readmemfl(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
        asm("movl %0,%%edx;"
            :
            : "r" (a)
        );
}

/*int mwritemem(void)
{
        register uint32_t a asm("edi");
        register uint32_t v asm("eax");
        writememl(a,v);
        return (armirq&0x40)?1:0;
}*/

#if 0
int mreadmem(void)
{
        register uint32_t a asm("edi");
        register uint32_t v asm("edx");
        v=readmeml(a);
        /*This is to make sure that GCC doesn't optimise out the load*/
        asm("movl %0,%%edx;"
            :
            : "r" (v)
        );
        return (armirq&0x40)?1:0;
}
#endif
#endif

void test(int a, int v)
{
        writememb(a,v);
}

static int generateshiftnoflags(uint32_t opcode)
{
        unsigned int temp;
        if (opcode&0x10) return 0; /*Can't do shift by register ATM*/
        if (!(opcode&0xFF0)) /*No shift*/
        {
                generateload(opcode&0xF);
                return 1;
        }
        temp=(opcode>>7)&31;
//        if ((temp-1)>=31) return 0;
        switch (opcode&0x60)
        {
                case 0x00: /*LSL*/
                generateload(opcode&0xF);
                if (temp) addbyte(0xC1); addbyte(0xE0); addbyte(temp); /*SHL $temp,%eax*/
                return 1;
                case 0x20: /*LSR*/
                if (temp)
                {
                        generateload(opcode&0xF);
                        addbyte(0xC1); addbyte(0xE8); addbyte(temp); /*SHR $temp,%eax*/
                }
                else
                {
                        addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                }
                return 1;
                case 0x40: /*ASR*/
                if (!temp) temp=31;
                generateload(opcode&0xF);
                addbyte(0xC1); addbyte(0xF8); addbyte(temp); /*SAR $temp,%eax*/
                return 1;
                case 0x60: /*ROR*/
                if (!temp) break;
                generateload(opcode&0xF);
                addbyte(0xC1); addbyte(0xC8); addbyte(temp); /*ROR $temp,%eax*/
                return 1;
        }
        return 0;
}

static int generateshiftflags(uint32_t opcode, uint32_t *pcpsr)
{
        unsigned int temp;
        if (opcode&0x10) return 0; /*Can't do shift by register ATM*/
        if (!(opcode&0xFF0)) /*No shift*/
        {
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                generateload(opcode&0xF);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG,%cl*/
                return 1;
        }
        temp=(opcode>>7)&31;
        switch (opcode&0x60)
        {
                case 0x00: /*LSL*/
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                generateload(opcode&0xF);
                if (temp)  
                {
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        addbyte(0xC1); addbyte(0xE0); addbyte(temp); /*SHL $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                else
                {
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG,%cl*/
                }
                return 1;
                case 0x20: /*LSR*/
//                return 0;
                if (temp)
                {
                        addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        generateload(opcode&0xF);
                        addbyte(0xC1); addbyte(0xE8); addbyte(temp); /*SHR $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                else
                {
                        return 0;
                        addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        addbyte(0xA9); addlong(0x80000000); /*TEST $0x80000000,%eax*/
                        addbyte(0x74); addbyte(3); /*JZ nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                        addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                }
                return 1;
                case 0x40: /*ASR*/
                return 0;
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                if (!temp)
                {
                        generateload(opcode&0xF);
                        addbyte(0xA9); addlong(0x80000000); /*TEST $0x80000000,%eax*/
                        addbyte(0x74); addbyte(3); /*JZ nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                        addbyte(0xC1); addbyte(0xF8); addbyte(31); /*SAR $31,%eax*/
                }
                else
                {
                        generateload(opcode&0xF);
                        addbyte(0xC1); addbyte(0xF8); addbyte(temp); /*SAR $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                return 1;
                case 0x60: /*ROR*/
                return 0;
                if (!temp) break;
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                generateload(opcode&0xF);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                addbyte(0xC1); addbyte(0xC8); addbyte(temp); /*ROR $temp,%eax*/
                addbyte(0x73); addbyte(3); /*JNC nocarry*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                return 1;
        }
        return 0;
}

static uint32_t generaterotate(uint32_t opcode, uint32_t *pcpsr, uint8_t mask)
{
        uint32_t temp;
        if (!flagsdirty)
           { addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); } /*MOVB *pcpsr,%cl*/
        temp=rotate2(opcode);
        if (mask!=0xF0)
        {
                if (opcode&0xF00)
                {
                        if (temp&0x80000000) { addbyte(0x80); addbyte(0xC9); addbyte( 0x20); } /*OR $CFLAG,%cl*/
                        else                 { addbyte(0x80); addbyte(0xE1); addbyte(~(0x20|mask)); } /*AND $~CFLAG,%cl*/
                }
        }
        if (!(opcode&0xF00) || (temp&0x80000000) || mask==0xF0) { addbyte(0x80); addbyte(0xE1); addbyte(~mask); } /*AND $~mask,%cl*/
        return temp;
}

static void generatesetzn(uint32_t opcode, uint32_t *pcpsr)
{
//        addbyte(0x75); addbyte(3); /*JNZ testn*/
//        addbyte(0x80); addbyte(0xC9); addbyte(0x40); /*OR $ZFLAG,%cl*/
        /*.testn*/
//        addbyte(0x79); addbyte(3); /*JNS over*/
//        addbyte(0x80); addbyte(0xC9); addbyte(0x80); /*OR $NFLAG,%cl*/
        /*over*/

        addbyte(0x9F); /*LAHF*/
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/

        if ((opcode>>28)==0xE) flagsdirty=1;
//        addbyte(0x88); addbyte(0x4E); addbyte((uint32_t)(pcpsr+3)-armregs);
//        rpclog("generatesetzn %08X %08X %i\n",(pcpsr+3),armregs,(uint32_t)(pcpsr+3)-(uint32_t)armregs);
        addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
//                        hasldrb[blockpoint2]=1;
}

static void generatesetzn2(uint32_t opcode, uint32_t *pcpsr)
{
        addbyte(0x9F); /*LAHF*/
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/
        if ((opcode>>28)==0xE) flagsdirty=1;
/*        if ((opcode>>28)==0xE)
           flagsdirty=1;
        else
        {*/
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
//        }
}

static void generatesetznS(uint32_t opcode, uint32_t *pcpsr)
{
//        addbyte(0x9F); /*LAHF*/
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/
        addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
        if ((opcode>>28)==0xE) flagsdirty=1;
//        flagsdirty=1;
}

static int lastrecompiled;

static int recompile(uint32_t opcode, uint32_t *pcpsr)
{
        int temp=0;
        int old=codeblockpos;
        int c,d;
        int first=0;
        uint32_t templ;

        switch ((opcode>>20)&0xFF)
        {
        case 0x00: /* AND reg */
                if ((opcode & 0xf0) == 0x90) /* MUL */
                {
                        if (MULRD==MULRM)
                        {
                                addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                        }
                        else
                        {
                                generateload(MULRM);
                                addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                                generatesave(MULRD);
                        }
                        break;
                }
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x23); addbyte(0x05); /*ANDL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x01: /* ANDS reg */
                if ((opcode & 0xf0) == 0x90) /* MULS */
                {
                        if (!flagsdirty) { addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); } /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        if (MULRD==MULRM)
                        {
                                addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                        }
                        else
                        {
                                generateload(MULRM);
                                addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                                generatesave(MULRD);
                        }
                        addbyte(0x85); addbyte(0xC0); /*TEST %eax,%eax*/
                        generatesetzn(opcode, pcpsr);
                        break;
                }
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x23); addbyte(0x05); /*ANDL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x02: /* EOR reg */
                if ((opcode & 0xf0) == 0x90) /* MLA */
                {
                        if (MULRD==MULRM)
                        {
                                addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                        }
                        else
                        {
                                generateload(MULRM);
                                addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                                addbyte(0x03); addbyte(0x05); addlong(&armregs[MULRN]); /*ADDL armregs[MULRN],%eax*/
                                generatesave(MULRD);
                        }
                        break;
                }
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x33); addbyte(0x05); /*XORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x03: /* EORS reg */
                if ((opcode & 0xf0) == 0x90) /* MLAS */
                {
                        if (!flagsdirty) { addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); } /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        if (MULRD==MULRM)
                        {
                                addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                        }
                        else
                        {
                                generateload(MULRM);
                                addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                                addbyte(0x03); addbyte(0x05); addlong(&armregs[MULRN]); /*ADDL armregs[MULRN],%eax*/
                                generatesave(MULRD);
                        }
                        addbyte(0x85); addbyte(0xC0); /*TEST %eax,%eax*/
                        generatesetzn(opcode, pcpsr);
                        break;
                }
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                flagsdirty=0;
                /*Shifted val now in %eax*/
                addbyte(0x33); addbyte(0x05); /*XORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x04: /* SUB reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
//                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                generateloadgen(RN,EDX);
//                addbyte(0x29); addbyte(0xD0); /*SUBL %edx,%eax*/
                addbyte(0x29); addbyte(0xC2); /*SUBL %eax,%edx*/
                generatesavegen(RD,EDX);
                break;

        case 0x05: /* SUBS reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xF0); /*AND $ZFLAG+NFLAG+VFLAG+CFLAG,%cl*/
                generateloadgen(RN,EDX);
                addbyte(0x29); addbyte(0xC2); /*SUBL %eax,%edx*/
                addbyte(0x9F); /*LAHF*/
                generatesavegen(RD,EDX);
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0x71); addbyte(3); /*JNO notoverflow*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x10); /*OR $VFLAG,%cl*/
                /*.notoverflow*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftablesub); /*OR lahftable(%edx),%cl*/
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                break;

        case 0x06: /* RSB reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x2B); addbyte(0x05); /*SUBL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x08: /* ADD reg */
                if ((opcode & 0xf0) == 0x90) /* UMULL */
                {
                        generateload(MULRM);
                        addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                        generatesave(MULRN);
                        generatesavegen(MULRD,EDX);
                        break;
                }
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x03); addbyte(0x05); /*ADDL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x09: /* ADDS reg */
                if ((opcode & 0xf0) == 0x90) /* UMULLS */
                {
                        if (!flagsdirty) { addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); } /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        generateload(MULRM);
                        addbyte(0xF7); addbyte(0x25); addlong(&armregs[MULRS]); /*MULL armregs[MULRS],%eax*/
                        generatesave(MULRN);
                        generatesavegen(MULRD,EDX);
                        addbyte(0x85); addbyte(0xD2); /*TEST %edx,%edx*/
                        addbyte(0x79); addbyte(3); /*JNS notn*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x80); /*OR $NFLAG,%cl*/
                        addbyte(0x09); addbyte(0xD0); /*OR %edx,%eax*/
                        addbyte(0x75); addbyte(3); /*JNZ testn*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x40); /*OR $ZFLAG,%cl*/
                        addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                        flagsdirty=1;
                        break;
                }
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                flagsdirty=0;
                /*Shifted val now in %eax*/
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xF0); /*AND $ZFLAG+NFLAG+VFLAG+CFLAG,%cl*/
                generateloadgen(RN,EDI);
                addbyte(0x01); addbyte(0xC7); /*ADDL %eax,%edi*/
                addbyte(0x9F); /*LAHF*/
                generatesavegen(RD,EDI);
                addbyte(0x71); addbyte(3); /*JNO notoverflow*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x10); /*OR $VFLAG,%cl*/
                /*.notoverflow*/
                addbyte(0xF6); addbyte(0xC4); addbyte(1); /*TEST cflag,%ah*/
                addbyte(0x74); addbyte(3); /*JZ notc*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                /*.notc*/
                /*Convenient trick here - Z & V flags are in the same place on x86 and ARM*/
                addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
                addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                break;

        case 0x0a: /* ADC reg */
                flagsdirty=0;
                if ((opcode & 0xf0) == 0x90) return 0; /* UMLAL */
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0xF6); addbyte(0x05); addlong(((unsigned long)pcpsr)+3); addbyte(0x20); /*TESTB (pcpsr>>24),$0x20*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                generateload(RN);
                addbyte(0x74); addbyte(1); /*JZ +1*/
                addbyte(0x42); /*INC %edx*/
                addbyte(0x01); addbyte(0xD0); /*ADDL %edx,%eax*/
                generatesave(RD);
                break;

        case 0x0b: /* ADCS reg */
                flagsdirty=0;
                if ((opcode & 0xf0) == 0x90) return 0; /* UMLALS */
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                addbyte(0x88); addbyte(0xCD);                   /*MOVB %cl,%ch*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xF0);   /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                generateloadgen(RN,EDX);
                addbyte(0xC0); addbyte(0xE5); addbyte(3);       /*SHL $3,%ch - put ARM carry into x86 carry*/
                addbyte(0x11); addbyte(0xC2); /*ADCL %eax,%edx*/
                addbyte(0x9F); /*LAHF*/
                generatesavegen(RD,EDX);
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0x71); addbyte(3); /*JNO notoverflow*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x10); /*OR $VFLAG,%cl*/
                /*.notoverflow*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftable); /*OR lahftable(%edx),%cl*/
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                break;

        case 0x0c: /* SBC reg */
                if ((opcode & 0xf0) == 0x90) /* SMULL */
                {
                        generateload(MULRM);
                        addbyte(0xF7); addbyte(0x2D); addlong(&armregs[MULRS]); /*IMULL armregs[MULRS],%eax*/
                        generatesave(MULRN);
                        generatesavegen(MULRD,EDX);
                        break;
                }
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                flagsdirty=0;
                /*Shifted val now in %eax*/
                addbyte(0xF6); addbyte(0x05); addlong(((unsigned long)pcpsr)+3); addbyte(0x20); /*TESTB (pcpsr>>24),$0x20*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                generateload(RN);
                addbyte(0x75); addbyte(1); /*JNZ +1*/
                addbyte(0x42); /*INC %edx*/
                addbyte(0x29); addbyte(0xD0); /*SUBL %edx,%eax*/
                generatesave(RD);
                break;

        case 0x0e: /* RSC reg */
                flagsdirty=0;
                if ((opcode & 0xf0) == 0x90) /* SMLAL */
                {
                        generateload(MULRM);
                        generateloadgen(MULRN,EBX);
                        generateloadgen(MULRD,ECX);
                        addbyte(0xF7); addbyte(0x2D); addlong(&armregs[MULRS]); /*IMULL armregs[MULRS],%eax*/
                        addbyte(0x01); addbyte(0xD8); /*ADDL %ebx,%eax*/
                        addbyte(0x01); addbyte(0xCA); /*ADDL %ecx,%edx*/
                        generatesave(MULRN);
                        generatesavegen(MULRD,EDX);
                        break;
                }
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x31); addbyte(0xC9); /*XOR %ecx,%ecx*/
                addbyte(0xF6); addbyte(0x05); addlong(((unsigned long)pcpsr)+3); addbyte(0x20); /*TESTB (pcpsr>>24),$0x20*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                addbyte(0x0F); addbyte(0x94); addbyte(0xC1); /*SETZ %cl*/
                generateload(RN);
                addbyte(0x29); addbyte(0xCA); /*SUBL %ecx,%edx*/
                addbyte(0x29); addbyte(0xC2); /*SUBL %eax,%edx*/
                generatesavegen(RD,EDX);
                break;

        case 0x18: /* ORR reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x0B); addbyte(0x05); /*ORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x19: /* ORRS reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x0B); addbyte(0x05); /*ORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x1a: /* MOV reg */
                flagsdirty=0;
//                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                if (RD==15)
                {
                        if (r15mask!=0xFFFFFFFC) generateloadgen(15,EDX);
                        addbyte(0x83); addbyte(0xC0); addbyte(4); /*ADDL $4,%eax*/
                        if (r15mask!=0xFFFFFFFC)
                        {
                                addbyte(0x81); addbyte(0xE2); addlong(~r15mask); /*ANDL $~r15mask,%edx*/
                                addbyte(0x25); addlong(r15mask); /*ANDL $r15mask,%eax*/
                                addbyte(0x09); addbyte(0xD0); /*ORL %edx,%eax*/
                        }
                }
                generatesave(RD);
                break;

        case 0x1b: /* MOVS reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x85); addbyte(0xC0); /*TEST %eax,%eax*/
                generatesave(RD);
                generatesetzn2(opcode, pcpsr);
//                hasldrb[blockpoint2]=1;
                break;

        case 0x1c: /* BIC reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0xF7); addbyte(0xD0); /*NOT %eax*/
                addbyte(0x23); addbyte(0x05); /*ANDL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                break;

        case 0x1d: /* BICS reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0xF7); addbyte(0xD0); /*NOT %eax*/
                addbyte(0x23); addbyte(0x05); /*ORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesave(RD);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x1e: /* MVN reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                addbyte(0xF7); addbyte(0xD0); /*NOT %eax*/
                /*Shifted val now in %eax*/
                generatesave(RD);
                break;

        case 0x1f: /* MVNS reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0xF7); addbyte(0xD0); /*NOT %eax*/
                addbyte(0x85); addbyte(0xC0); /*TEST %eax,%eax*/
                generatesave(RD);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x11: /* TST reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x85); addbyte(0x05); /*TEST armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x13: /* TEQ reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftflags(opcode,pcpsr)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x33); addbyte(0x05); /*XORL armregs[RN],%eax*/
                addlong(&armregs[RN]);
                generatesetzn(opcode, pcpsr);
                break;

        case 0x15: /* CMP reg */
                flagsdirty=0;
                if (RD==15 || RN==15) return 0;
                if (!generateshiftnoflags(opcode)) return 0;
                /*Shifted val now in %eax*/
                addbyte(0x8A); addbyte(0x0D); addlong(pcpsr+3); /*MOVB *pcpsr,%cl*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xF0); /*AND $ZFLAG+NFLAG+VFLAG+CFLAG,%cl*/
                generateloadgen(RN,EDX);
                addbyte(0x29); addbyte(0xC2); /*SUBL %eax,%edx*/
                addbyte(0x9F); /*LAHF*/
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0x71); addbyte(3); /*JNO notoverflow*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x10); /*OR $VFLAG,%cl*/
                /*.notoverflow*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftablesub); /*OR lahftable(%edx),%cl*/
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                flagsdirty=1;
                break;

        case 0x20: /* AND imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_AND, templ);
                break;

        case 0x21: /* ANDS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
//                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                generatedataprocS(opcode, X86_OP_AND, templ);
                generatesetznS(opcode, pcpsr);
                break;

        case 0x22: /* EOR imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_XOR, templ);
                break;

        case 0x23: /* EORS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
//                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                generatedataprocS(opcode, X86_OP_XOR, templ);
                generatesetznS(opcode, pcpsr);
                break;

        case 0x24: /* SUB imm */
  //              flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_SUB, templ);
                break;

        case 0x25: /* SUBS imm */
                flagsdirty=0;
                if (RD==15) return 0;
                addbyte(0x80); addbyte(0x66); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); addbyte(0xF); /*ANDB 0xF,pcpsr*/
//                addbyte(0x80); addbyte(0x25); addlong(pcpsr+3); addbyte(0xF); /*ANDB 0xF,pcpsr*/
                templ=rotate2(opcode);//,pcpsr,0xF0);
                generatedataprocS(opcode, X86_OP_SUB, templ);
//                addbyte(0x9F); /*LAHF*/
                addbyte(0x0F); addbyte(0x90); addbyte(0xC1); /*SETO %cl*/
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0xC0); addbyte(0xE1); addbyte(4); /*SHL $4,%cl*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftablesub); /*OR lahftable(%edx),%cl*/
                addbyte(0x08); addbyte(0x4E); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); /*OR %cl,pcpsr*/
//                flagsdirty=1;
                break;

        case 0x28: /* ADD imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_ADD, templ);
                break;

        case 0x29: /* ADDS imm */
                flagsdirty=0;
                if (RD==15) return 0;
                addbyte(0x80); addbyte(0x66); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); addbyte(0xF); /*ANDB 0xF,pcpsr*/
                templ=rotate2(opcode);
                generatedataprocS(opcode, X86_OP_ADD, templ);
                
                addbyte(0x0F); addbyte(0x90); addbyte(0xC1); /*SETO %cl*/
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0xC0); addbyte(0xE1); addbyte(4); /*SHL $4,%cl*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftable); /*OR lahftable(%edx),%cl*/
                addbyte(0x08); addbyte(0x4E); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); /*OR %cl,pcpsr*/
//                flagsdirty=1;
                break;

        case 0x38: /* ORR imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_OR, templ);
                break;

        case 0x39: /* ORRS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
                generatedataprocS(opcode, X86_OP_OR, templ);
                generatesetznS(opcode, pcpsr);
                break;

        case 0x3a: /* MOV imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                addbyte(0xC7); /*MOVL $dat,(addr)*/
                addbyte(0x05);
                addlong(&armregs[RD]);
                addlong(templ);
                break;

        case 0x3b: /* MOVS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
//                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                if (!templ)                { addbyte(0x80); addbyte(0xC9); addbyte(0x40); } /*OR $ZFLAG,%cl*/
                else if (templ&0x80000000) { addbyte(0x80); addbyte(0xC9); addbyte(0x80); } /*OR $NFLAG,%cl*/
                addbyte(0xC7); addbyte(0x05); /*MOVL $templ,(armregs[RD])*/
                addlong(&armregs[RD]); addlong(templ);
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                if ((opcode>>28)==0xE) flagsdirty=1;
                break;

        case 0x3c: /* BIC imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=~rotate2(opcode);
                temp += generatedataproc(opcode, X86_OP_AND, templ);
                break;

        case 0x3d: /* BICS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=~generaterotate(opcode,pcpsr,0xC0);
//                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                generatedataprocS(opcode, X86_OP_AND, templ);
                generatesetznS(opcode, pcpsr);
                break;

        case 0x3e: /* MVN imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=rotate2(opcode);
                addbyte(0xC7); /*MOVL $dat,(addr)*/
                addbyte(0x05);
                addlong(&armregs[RD]);
                addlong(~templ);
                temp+=10;
                break;

        case 0x3f: /* MVNS imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=~generaterotate(opcode,pcpsr,0xC0);
//                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                if (!templ)                { addbyte(0x80); addbyte(0xC9); addbyte(0x40); } /*OR $ZFLAG,%cl*/
                else if (templ&0x80000000) { addbyte(0x80); addbyte(0xC9); addbyte(0x80); } /*OR $NFLAG,%cl*/
                addbyte(0xC7); addbyte(0x05); /*MOVL $templ,(armregs[RD])*/
                addlong(&armregs[RD]); addlong(templ);
                addbyte(0x88); addbyte(0x0D); addlong(pcpsr+3); /*MOV %cl,pcpsr*/
                if ((opcode>>28)==0xE) flagsdirty=1;
                break;

        case 0x31: /* TST imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
                generateload(RN);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                if (RN==15 && r15mask!=0xFFFFFFFC) { addbyte(0x25); addlong(r15mask); /*ANDL $r15mask,%eax*/ }
                addbyte(0xA9); addlong(templ); /*TEST $templ,%eax*/
                generatesetzn(opcode, pcpsr);
                break;

        case 0x33: /* TEQ imm */
//                flagsdirty=0;
                if (RD==15) return 0;
                templ=generaterotate(opcode,pcpsr,0xC0);
                generateload(RN);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $~(ZFLAG|NFLAG),%cl*/
                if (RN==15 && r15mask!=0xFFFFFFFC) { addbyte(0x25); addlong(r15mask); /*ANDL $r15mask,%eax*/ }
                addbyte(0x35); addlong(templ); /*XOR $templ,%eax*/
                generatesetzn(opcode, pcpsr);
                break;

        case 0x35: /* CMP imm */
                flagsdirty=0;
                if (RD==15) return 0;
                addbyte(0x80); addbyte(0x66); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); addbyte(0xF); /*ANDB 0xF,pcpsr*/
                templ=rotate2(opcode);
                generateload(RN);
                addbyte(0x3D); addlong(templ); /*CMP $templ,%eax*/
                addbyte(0x9F); /*LAHF*/
                addbyte(0x0F); addbyte(0x90); addbyte(0xC1); /*SETO %cl*/
                addbyte(0x0F); addbyte(0xB6); addbyte(0xD4); /*MOVZBL %ah,%edx*/
                addbyte(0xC0); addbyte(0xE1); addbyte(4); /*SHL $4,%cl*/
                addbyte(0x0A); addbyte(0x8A); addlong(lahftablesub); /*OR lahftable(%edx),%cl*/
                addbyte(0x08); addbyte(0x4E); addbyte((uint32_t)(pcpsr)-(uint32_t)(&armregs[0])+3); /*OR %cl,pcpsr*/
                break;

                
//                #if 0
        case 0x40: /* STR  Rd, [Rn], #-imm   */
        case 0x48: /* STR  Rd, [Rn], #+imm   */
        case 0x44: /* STRB Rd, [Rn], #-imm   */
        case 0x4c: /* STRB Rd, [Rn], #+imm   */
        case 0x60: /* STR  Rd, [Rn], -reg... */
        case 0x68: /* STR  Rd, [Rn], +reg... */
        case 0x64: /* STRB Rd, [Rn], -reg... */
        case 0x6c: /* STRB Rd, [Rn], +reg... */
                flagsdirty=0;
                if (!generateshiftnoflags(opcode) && opcode&0x2000000) return 0;
                codeblockpos=old;
                if (RD==15 || RN==15) return 0;
                generateload(RN);
                generateloadgen(RD,EBX);
                addbyte(0x89); /*MOVL %eax,%edx*/
                addbyte(0xC2);
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vwaddrl(,%eax,4),%ecx*/
                addlong(vwaddrl);
                if (!(opcode&0x400000))
                {
                        addbyte(0x83); addbyte(0xE2); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edx*/
                }
                addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
                addbyte(0x75); addbyte(5); /*JNZ notinbuffer*/
                if (opcode&0x400000)
                {
                        addbyte(0x88); addbyte(0x1C); addbyte(0x11); /*MOVB %bl,(%ecx,%edx)*/
                }
                else
                {
                        addbyte(0x89); addbyte(0x1C); addbyte(0x11); /*MOVL %ebx,(%ecx,%edx)*/
                }
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }
                /*.notinbuffer*/
                if (opcode&0x400000)
                {
                        gen_x86_call(codewritememfb);
                }
                else
                {
                        gen_x86_call(codewritememfl);
                }
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }

                /*.nextbit*/
                if (opcode&0x2000000)
                {
                        generateshiftnoflags(opcode);
                        if (opcode&0x800000) { addbyte(0x01); addbyte(0x05); addlong(&armregs[RN]); } /*ADD %eax,armregs[RN]*/
                        else                 { addbyte(0x29); addbyte(0x05); addlong(&armregs[RN]); } /*SUB %eax,armregs[RN]*/
                }
                else
                {
                        if (!(opcode&0xF80) && (opcode&0x7F))
                        {
                                addbyte(0x83); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addbyte(opcode&0x7F);
                        }
                        else if (opcode&0xFFF)
                        {
                                addbyte(0x81); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addlong(opcode&0xFFF);
                        }
                }
                break;

                #if 0
        case 0x40: /* STR Rd, [Rn], #-imm   */
        case 0x48: /* STR Rd, [Rn], #+imm   */
        case 0x60: /* STR Rd, [Rn], -reg... */
        case 0x68: /* STR Rd, [Rn], +reg... */
                flagsdirty=0;
                if (!generateshiftnoflags(opcode) && opcode&0x2000000) return 0;
                codeblockpos=old;
                if (RD==15 || RN==15) return 0;
                generateloadgen(RN,EDI);
                generateloadgen(RD,EBX);
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                gen_x86_call(mwritemem);
                if (opcode&0x2000000) generateshiftnoflags(opcode);
addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
//                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }

                /*.nextbit*/
                if (opcode&0x2000000)
                {
//                        generateshiftnoflags(opcode);
                        if (opcode&0x800000) { addbyte(0x01); addbyte(0x05); addlong(&armregs[RN]); } /*ADD %eax,armregs[RN]*/
                        else                 { addbyte(0x29); addbyte(0x05); addlong(&armregs[RN]); } /*SUB %eax,armregs[RN]*/
                }
                else
                {
                        if (!(opcode&0xF80) && (opcode&0x7F))
                        {
                                addbyte(0x83); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addbyte(opcode&0x7F);
                        }
                        else if (opcode&0xFFF)
                        {
                                addbyte(0x81); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addlong(opcode&0xFFF);
                        }
                }
                break;
                #endif

        case 0x41: /* LDR  Rd, [Rn], #-imm   */
        case 0x49: /* LDR  Rd, [Rn], #+imm   */
        case 0x45: /* LDRB Rd, [Rn], #-imm   */
        case 0x4d: /* LDRB Rd, [Rn], #+imm   */
        case 0x61: /* LDR  Rd, [Rn], -reg... */
        case 0x69: /* LDR  Rd, [Rn], +reg... */
        case 0x65: /* LDRB Rd, [Rn], -reg... */
        case 0x6d: /* LDRB Rd, [Rn], +reg... */
                flagsdirty=0;
                if (!generateshiftnoflags(opcode) && opcode&0x2000000) return 0;
                codeblockpos=old;
                if (RD==15 || RN==15) return 0;
                generateload(RN);
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                if (!(opcode&0x400000))
                {
                        addbyte(0x89); addbyte(0xC7); /*MOVL %eax,%edi*/
                }
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vraddrl(,%eax,4),%ecx*/
                addlong(vraddrl);
                if (!(opcode&0x400000))
                {
                        addbyte(0x83); addbyte(0xE2); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edx*/
                }
                addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
                if (opcode&0x400000)
                {
                        addbyte(0x75); addbyte(6); /*JNZ notinbuffer*/
                        addbyte(0x0F); addbyte(0xB6); addbyte(0x0C); addbyte(0x11); /*MOVZB (%ecx,%edx),%ecx*/
                }
                else
                {
                        addbyte(0x75); addbyte(5); /*JNZ notinbuffer*/
                        addbyte(0x8B); addbyte(0x14); addbyte(0x11); /*MOVL (%ecx,%edx),%edx*/
                }
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }
                /*.notinbuffer*/
                if (opcode&0x400000)
                {
                        gen_x86_call(codereadmemb);
                }
                else
                {
                        gen_x86_call(codereadmeml);
                }
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }

                /*.nextbit*/
                if (opcode&0x400000)
                {
                        generatesavegen(RD,ECX);
                }
                else
                {
                        addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
                        addbyte(0xC1); addbyte(0xE1); addbyte(3); /*SHL $3,%ecx*/
                        addbyte(0xD3); addbyte(0xCA); /*ROR %cl,%edx*/
                        generatesavegen(RD,EDX);
                }
                if (opcode&0x2000000)
                {
                        generateshiftnoflags(opcode);
                        if (opcode&0x800000) { addbyte(0x01); addbyte(0x05); addlong(&armregs[RN]); } /*ADD %eax,armregs[RN]*/
                        else                 { addbyte(0x29); addbyte(0x05); addlong(&armregs[RN]); } /*SUB %eax,armregs[RN]*/
                }
                else
                {
                        if (!(opcode&0xF80) && (opcode&0x7F))
                        {
                                addbyte(0x83); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addbyte(opcode&0x7F);
                        }
                        else if (opcode&0xFFF)
                        {
                                addbyte(0x81); /*ADDL $8,%eax*/
                                if (opcode&0x800000) addbyte(0x05); /*ADD*/
                                else                 addbyte(0x2D); /*SUB*/
                                addlong(&armregs[RN]);
                                addlong(opcode&0xFFF);
                        }
                }
                break;

//#if 0
        case 0x50: /* STR Rd, [Rn, #-imm]    */
        case 0x52: /* STR Rd, [Rn, #-imm]!   */
        case 0x58: /* STR Rd, [Rn, #+imm]    */
        case 0x5a: /* STR Rd, [Rn, #+imm]!   */
        case 0x70: /* STR Rd, [Rn, -reg...]  */
        case 0x72: /* STR Rd, [Rn, -reg...]! */
        case 0x78: /* STR Rd, [Rn, +reg...]  */
        case 0x7a: /* STR Rd, [Rn, +reg...]! */
                flagsdirty=0;
                if (RD==15) return 0;
                if (opcode&0x2000000) { if (!generateshiftnoflags(opcode)) return 0; }
                else                  { addbyte(0xB8); addlong(opcode&0xFFF); }
                if (!(opcode&0x800000)) { addbyte(0xF7); addbyte(0xD8); } /*NEG %eax*/
                generateloadgen(RD,EBX);
                /*Shifted value now in %eax*/
                if (RN==15)
                {
                        generateloadgen(RN,EDX);
                        addbyte(0x81); addbyte(0xE2); addlong(r15mask);      /*ANDL $r15mask,%edx*/
                        addbyte(0x01); addbyte(0xD0); /*ADDL %edx,%eax*/
                }
                else
                {
                        addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                }
//                addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                addbyte(0x83); addbyte(0xE0); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%eax*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                if (opcode&0x200000) generatesavegen(17,EAX);
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vwaddrl(,%eax,4),%ecx*/
                addlong(vwaddrl);
                addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
                addbyte(0x75); addbyte(5); /*JNZ notinbuffer*/
                
                addbyte(0x89); addbyte(0x1C); addbyte(0x11); /*MOV %ebx,(%ecx,%edx)*/
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }
                /*.notinbuffer*/
                gen_x86_call(codewritememfl);
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }

                /*.nextbit*/
                if (opcode&0x200000) { generateloadgen(17,EDX);
                        addbyte(0x89); addbyte(0x15); addlong(&armregs[RN]); /*MOV %edx,armregs[RN]*/ }
                break;

        case 0x54: /* STRB Rd, [Rn, #-imm]    */
        case 0x56: /* STRB Rd, [Rn, #-imm]!   */
        case 0x5c: /* STRB Rd, [Rn, #+imm]    */
        case 0x5e: /* STRB Rd, [Rn, #+imm]!   */
        case 0x74: /* STRB Rd, [Rn, -reg...]  */
        case 0x76: /* STRB Rd, [Rn, -reg...]! */
        case 0x7c: /* STRB Rd, [Rn, +reg...]  */
        case 0x7e: /* STRB Rd, [Rn, +reg...]! */
                flagsdirty=0;
                if (RD==15) return 0;
                if (opcode&0x2000000) { if (!generateshiftnoflags(opcode)) return 0; }
                else                  { addbyte(0xB8); addlong(opcode&0xFFF); }
                if (!(opcode&0x800000)) { addbyte(0xF7); addbyte(0xD8); } /*NEG %eax*/
                addbyte(0x8A); addbyte(0x1D); /*MOVB armregs[RD],%bl*/
                addlong(&armregs[RD]);
                /*Shifted value now in %eax*/
                if (RN==15)
                {
                        generateloadgen(RN,EDX);
                        addbyte(0x81); addbyte(0xE2); addlong(r15mask);      /*ANDL $r15mask,%edx*/
                        addbyte(0x01); addbyte(0xD0); /*ADDL %edx,%eax*/
                }
                else
                {
                        addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                }
//                addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                if (opcode&0x200000) generatesavegen(17,EAX);
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vwaddrl(,%eax,4),%ecx*/
                addlong(vwaddrl);
                addbyte(0xF6); addbyte(0xC1); addbyte(3); /*TST %cl,3*/
                addbyte(0x75); addbyte(5); /*JNZ notinbuffer*/
                /*.inbuffer*/
                addbyte(0x88); addbyte(0x1C); addbyte(0x11); /*MOVB %bl,(%ecx,%edx)*/
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }
                /*.notinbuffer*/
                gen_x86_call(codewritememfb);
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }

                /*.nextbit*/
                if (opcode&0x200000) { generateloadgen(17,EDX);
                        addbyte(0x89); addbyte(0x15); addlong(&armregs[RN]); /*MOV %edx,armregs[RN]*/ }
                break;

//#if 0
        case 0x51: /* LDR Rd, [Rn, #-imm]    */
        case 0x53: /* LDR Rd, [Rn, #-imm]!   */
        case 0x59: /* LDR Rd, [Rn, #+imm]    */
        case 0x5b: /* LDR Rd, [Rn, #+imm]!   */
        case 0x71: /* LDR Rd, [Rn, -reg...]  */
        case 0x73: /* LDR Rd, [Rn, -reg...]! */
        case 0x79: /* LDR Rd, [Rn, +reg...]  */
        case 0x7b: /* LDR Rd, [Rn, +reg...]! */
                flagsdirty=0;
                if (RD==15) return 0;
                if (opcode&0x2000000) { if (!generateshiftnoflags(opcode)) return 0; }
                else                  { addbyte(0xB8); addlong(opcode&0xFFF); }
                if (!(opcode&0x800000)) { addbyte(0xF7); addbyte(0xD8); } /*NEG %eax*/
                /*Shifted value now in %eax*/
                if (RN==15)
                {
                        generateloadgen(RN,EDX);
                        addbyte(0x81); addbyte(0xE2); addlong(r15mask);      /*ANDL $r15mask,%edx*/
                        addbyte(0x01); addbyte(0xD0); /*ADDL %edx,%eax*/
                }
                else
                {
                        addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                }
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                addbyte(0x89); addbyte(0xC7); /*MOVL %eax,%edi*/
                /*if (opcode&0x200000)*/// addbyte(0x52); /*PUSH %edx*/
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vraddrl(,%eax,4),%ecx*/
                addlong(vraddrl);
                addbyte(0x83); addbyte(0xE2); addbyte(0xFC); /*AND $FFFFFFFC,%edx*/
                addbyte(0xF6); addbyte(0xC1); addbyte(1); /*TST %cl,1*/
                addbyte(0x75); addbyte(5); /*JNZ notinbuffer*/
/*.inbuffer*/   addbyte(0x8B); addbyte(0x14); addbyte(0x11); /*MOVL (%ecx,%edx),%edx*/
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }
                
                gen_x86_call(codereadmeml);
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
/*.nextbit*/    addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
                if (opcode&0x200000) {
                        addbyte(0x89); addbyte(0x0D); addlong(&armregs[RN]); /*MOV %ecx,armregs[RN]*/ }
//                addbyte(0x83); addbyte(0xE1); addbyte(3); /*AND $3,%ecx*/ /*x86-32 masks shifts to 32 bits, so this isn't necessary*/
                addbyte(0xC1); addbyte(0xE1); addbyte(3); /*SHL $3,%ecx*/
                addbyte(0xD3); addbyte(0xCA); /*ROR %cl,%edx*/
                generatesavegen(RD,EDX);
                break;
//#endif

        case 0x55: /* LDRB Rd, [Rn, #-imm]    */
        case 0x57: /* LDRB Rd, [Rn, #-imm]!   */
        case 0x5d: /* LDRB Rd, [Rn, #+imm]    */
        case 0x5f: /* LDRB Rd, [Rn, #+imm]!   */
        case 0x75: /* LDRB Rd, [Rn, -reg...]  */
        case 0x77: /* LDRB Rd, [Rn, -reg...]! */
        case 0x7d: /* LDRB Rd, [Rn, +reg...]  */
        case 0x7f: /* LDRB Rd, [Rn, +reg...]! */
                flagsdirty=0;
                if (RD==15) return 0;
                if (opcode&0x2000000) { if (!generateshiftnoflags(opcode)) return 0; }
                else                  { addbyte(0xB8); addlong(opcode&0xFFF); }
                if (!(opcode&0x800000)) { addbyte(0xF7); addbyte(0xD8); } /*NEG %eax*/
                /*Shifted value now in %eax*/
                if (RN==15)
                {
                        generateloadgen(RN,EDX);
                        addbyte(0x81); addbyte(0xE2); addlong(r15mask);      /*ANDL $r15mask,%edx*/
                        addbyte(0x01); addbyte(0xD0); /*ADDL %edx,%eax*/
                }
                else
                {
                        addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                }
//                addbyte(0x03); addbyte(0x05); addlong(&armregs[RN]); /*ADDL armregs[RN],%eax*/
                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                if (opcode&0x200000) generatesavegen(17,EAX);
                addbyte(0xC1); addbyte(0xE8); addbyte(12); /*SHR $12,%eax*/
                addbyte(0x8B); addbyte(0x0C); addbyte(0x85); /*MOV vraddrl(,%eax,4),%ecx*/
                addlong(vraddrl);
                addbyte(0xF6); addbyte(0xC1); addbyte(1); /*TST %cl,1*/
                addbyte(0x75); addbyte(6); /*JNZ notinbuffer*/
/*.inbuffer*/   addbyte(0x0F); addbyte(0xB6); addbyte(0x0C); addbyte(0x11); /*MOVZB (%ecx,%edx),%ecx*/
                if (codeblockpos<115) { addbyte(0xEB); addbyte(9); /*JMP nextbit*/ }
                else                  { addbyte(0xEB); addbyte(13); /*JMP nextbit*/ }

                gen_x86_call(codereadmemb);
                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
                if (codeblockpos<124)
                {
                        addbyte(0x75); addbyte(-(codeblockpos+1)); /*JNZ 0*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
/*.nextbit*/    if (opcode&0x200000) { generateloadgen(17,EDX);
                        addbyte(0x89); addbyte(0x15); addlong(&armregs[RN]); /*MOV %edx,armregs[RN]*/ }
                generatesavegen(RD,ECX);
                break;

//#if 0
        case 0x80: /* STMDA */
        case 0x82: /* STMDA ! */
        case 0x90: /* STMDB */
        case 0x92: /* STMDB ! */
                flagsdirty=0;
                first=1;
                templ=opcode&0xFFFF;
                temp=isvalidforfastwrite(armregs[RN]);
                if (!temp) goto stmdbslow;
                generateloadgen(RN,EDI);
                if (opcode&0x200000) generatesavegen(17,EDI); /*PUSH EDI*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF));   /*SUBL $4,%edi*/ }
                else                  { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)-4); /*SUBL $4,%edi*/ }
                addbyte(0x89); addbyte(0xF8); /*MOVL %edi,%eax*/
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                addbyte(0x25); addlong(0xFC0); /*ANDL $0xFC0,%eax*/
                addbyte(0x3D); addlong(0xFC0); /*CMP $0xFC0,%eax*/
                addbyte(0x0F); addbyte(0x95); addbyte(0xC0); /*SETLE %al*/
                c=0;
                while (!(templ&1))
                {
                        templ>>=1;
                        c++;
                }
                generateloadgen(c,EBX);
                addbyte(0x50); /*PUSH %eax*/
                if (c==15) { addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%ebx*/ }
                gen_x86_call(mwritemem);
                addbyte(0x58); /*POP %eax*/
                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                addrel32(&rcodeblock[blockpoint2][0]);
                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                c++;
                templ>>=1;
                if (templ)
                {
                        addbyte(0x08); addbyte(0xC0); /*OR %al,%al*/
//                        addbyte(0x3D); addlong(0xFC0); /*CMP $0xFC0,%eax*/
//                        addbyte(0x7F); addbyte(14); /*JLE fast*/
                        addbyte(0x75); /*JNZ fast*/
                        if (opcode&0x200000) { addbyte(17+2+/*1+*/3+2+((RN)?1:0)+5+7); }
                        else                 { addbyte(17+2+5+7); }
                        addbyte(0x55); /*PUSH %ebp*/
                        addbyte(0xBD); addlong((templ<<c)>>1); /*MOVL (templ<<c)>>1,%ebp*/
                        gen_x86_call(mwritememslow);
                        addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                        addbyte(0x5D); /*POP %ebp*/
                        addbyte(0x75); addbyte(5);
                        addbyte(0xE9); temp=codeblockpos; addlong(0); /*JMP end*/
                        if (opcode&0x200000)
                        {
                                generateloadgen(17,EDI); /*POP %edi*/
                                generatesavegen(RN,EDI);
                        }
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        
                        
                        d=0;
                        addbyte(0x01); addbyte(0xCF); /*ADDL %ecx,%edi*/
                        for (;c<16;c++)
                        {
                                if (templ&1)
                                {
                                        generateloadgen(c,EBX);
                                        if (c==15) { addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%ebx*/ }
                                        if (!d) { addbyte(0x89); addbyte(0x1F); /*MOVL %ebx,(%edi)*/ }
                                        else    { addbyte(0x89); addbyte(0x5F); addbyte(d); /*MOVL %ebx,d(%edi)*/}
                                        d+=4;
                                }
                                templ>>=1;
                        }
                        c=codeblockpos;
                        codeblockpos=temp;
                        addlong(c-(codeblockpos+4));
                        codeblockpos=c;
                }
                if (opcode&0x200000)
                {
                        addbyte(0x83); addbyte(0x2D); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*SUBL $countbits(opcode&0xFFFF),armregs[RN]*/
//                        addbyte(0x5F); /*POP %edi*/
                }
                break;
//#endif

        //case 0x80: /* STMDA */
        //case 0x82: /* STMDA ! */
        case 0x84: /* STMDA ^ */
        case 0x86: /* STMDA ^! */
        //case 0x90: /* STMDB */
        //case 0x92: /* STMDB ! */
        case 0x94: /* STMDB ^ */
        case 0x96: /* STMDB ^! */
                flagsdirty=0;
                templ=opcode&0xFFFF;
                temp=isvalidforfastwrite(armregs[RN]);
                stmdbslow:
                generateloadgen(RN,EDI);
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF));   /*SUBL $4,%edi*/ }
                else                  { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)-4); /*SUBL $4,%edi*/ }
                c=0;
                if (opcode&0x200000 && templ)
                {
                        while (!(templ&1))
                        {
                                templ>>=1;
                                c++;
                        }
                        if (c==15) { generateloadgen(c,EBX); addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%eax*/ }
                        else if (!(opcode&0x400000)) generateloadgen(c,EBX);
                        else
                        {
                                addbyte(0x8B); addbyte(0x0D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
//                                addbyte(0xB9); addlong(usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                addbyte(0x8B); addbyte(0x19); /*MOVL (%ecx),%ebx*/
                        }
                        gen_x86_call(mwritemem);
                        addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                        c++;
                        templ>>=1;
                        temp|=2;
                }
                for (;c<16;c++)
                {
                        if (templ&1)
                        {
                                if (c==15) { generateloadgen(c,EBX); addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%eax*/ }
                                else if (!(opcode&0x400000)) generateloadgen(c,EBX);
                                else
                                {
                                        addbyte(0x8B); addbyte(0x1D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ebx*/
//                                        addbyte(0xB9); addlong(usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                        addbyte(0x8B); addbyte(0x1B); /*MOVL (%ebx),%ebx*/
                                }
                                if (temp==3)
                                {
                                        gen_x86_call(mwritememfast);
//                                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                                }
                                else
                                {
                                        gen_x86_call(mwritemem);
                                        temp|=2;
                                        if (templ&~1 && temp==3)
                                        {
                                                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                                                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                                                addrel32(&rcodeblock[blockpoint2][0]);
                                        }
                                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                                }
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                if (opcode&0x200000)
                {
                        addbyte(0x74); addbyte(/*14*/5); /*JZ +14*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        addbyte(0x83); addbyte(0x2D); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*SUBL $countbits(opcode&0xFFFF),armregs[RN]*/
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;

        case 0x88: /* STMIA */
        case 0x8a: /* STMIA ! */
        case 0x98: /* STMIB */
        case 0x9a: /* STMIB ! */
                flagsdirty=0;
                first=1;
                templ=opcode&0xFFFF;
                temp=isvalidforfastwrite(armregs[RN]);
                if (!temp) goto stmiaslow;
                generateloadgen(RN,EDI);
                if (opcode&0x200000) generatesavegen(17,EDI); /*PUSH EDI*/
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                addbyte(0x89); addbyte(0xF8); /*MOVL %edi,%eax*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
                addbyte(0x25); addlong(0xFC0); /*ANDL $0xFC0,%eax*/
                addbyte(0x3D); addlong(0xFC0); /*CMP $0xFC0,%eax*/
                addbyte(0x0F); addbyte(0x95); addbyte(0xC0); /*SETLE %al*/
                c=0;
                while (!(templ&1))
                {
                        templ>>=1;
                        c++;
                }
                generateloadgen(c,EBX);
                addbyte(0x50); /*PUSH %eax*/
                if (c==15) { addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%ebx*/ }
                gen_x86_call(mwritemem);
                addbyte(0x58); /*POP %eax*/
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                addrel32(&rcodeblock[blockpoint2][0]);
                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                c++;
                templ>>=1;
                if (templ)
                {
                        addbyte(0x08); addbyte(0xC0); /*OR %al,%al*/
//                        addbyte(0x3D); addlong(0xFC0); /*CMP $0xFC0,%eax*/
//                        addbyte(0x7F); addbyte(14); /*JLE fast*/
                        addbyte(0x75); /*JNZ fast*/
                        if (opcode&0x200000) { addbyte(17+2+/*1+*/3+2+((RN)?1:0)+5+7); }
                        else                 { addbyte(17+2+5+7); }
                        addbyte(0x55); /*PUSH %ebp*/
                        addbyte(0xBD); addlong((templ<<c)>>1); /*MOVL (templ<<c)>>1,%ebp*/
                        gen_x86_call(mwritememslow);
                        addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                        addbyte(0x5D); /*POP %ebp*/
                        addbyte(0x75); addbyte(5);
                        addbyte(0xE9); temp=codeblockpos; addlong(0); /*JMP end*/
                        if (opcode&0x200000)
                        {
//                                addbyte(0x5F); /*POP %edi*/
                                generateloadgen(17,EDI);
                                generatesavegen(RN,EDI);
                        }
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        
                        d=0;
                        addbyte(0x01); addbyte(0xCF); /*ADDL %ecx,%edi*/
                        for (;c<16;c++)
                        {
                                if (templ&1)
                                {
                                        generateloadgen(c,EBX);
                                        if (c==15) { addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%ebx*/ }
                                        if (!d) { addbyte(0x89); addbyte(0x1F); /*MOVL %ebx,(%edi)*/ }
                                        else    { addbyte(0x89); addbyte(0x5F); addbyte(d); /*MOVL %ebx,d(%edi)*/}
                                        d+=4;
                                }
                                templ>>=1;
                        }
                        c=codeblockpos;
                        codeblockpos=temp;
                        addlong(c-(codeblockpos+4));
                        codeblockpos=c;
                }
                if (opcode&0x200000)
                {
//                        addbyte(0x5F); /*POP %edi*/
                        generateloadgen(17,EDI);
                        addbyte(0x83); addbyte(0xC7); addbyte(countbits(opcode&0xFFFF));   /*ADDL $4,%edi*/
                        generatesavegen(RN,EDI);
//                        addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                }
                break;

                
        //case 0x88: /* STMIA */
        //case 0x8a: /* STMIA ! */
        case 0x8c: /* STMIA ^ */
        case 0x8e: /* STMIA ^! */
        //case 0x98: /* STMIB */
        //case 0x9a: /* STMIB ! */
        case 0x9c: /* STMIB ^ */
        case 0x9e: /* STMIB ^! */
                flagsdirty=0;
                first=1;
                templ=opcode&0xFFFF;
                temp=isvalidforfastwrite(armregs[RN]);
        stmiaslow:
                generateloadgen(RN,EDI);
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
                c=0;
                if (opcode&0x200000 && templ)
                {
                        while (!(templ&1))
                        {
                                templ>>=1;
                                c++;
                        }
                        if (c==15) { generateloadgen(c,EBX); addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%eax*/ }
                        else if (!(opcode&0x400000)) generateloadgen(c,EBX);
                        else
                        {
                                addbyte(0x8B); addbyte(0x1D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ebx*/
                                //addbyte(0xB9); addlong(usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                addbyte(0x8B); addbyte(0x1B); /*MOVL (%ebx),%ebx*/
                        }
                        gen_x86_call(mwritemem);
                        addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);

//                        addbyte(0x89); addbyte(0xF8); /*MOVL %edi,%eax*/
                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
//                        addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
//                        if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC0); addbyte(countbits(opcode&0xFFFF)-4); } /*ADD countbits(opcode&0xFFFF)-4,%eax*/
//                        else                  { addbyte(0x83); addbyte(0xC0); addbyte(countbits(opcode&0xFFFF)); } /*ADD countbits(opcode&0xFFFF),%eax*/
//                        generatesave(RN);
                        c++;
                        templ>>=1;
                        temp|=2;
                        first=0;
                }
                for (;c<16;c++)
                {
                        if (templ&1)
                        {
                                if (c==15) { generateloadgen(c,EBX); addbyte(0x83); addbyte(0xC0|EBX); addbyte(4); /*ADDL $4,%eax*/ }
                                else if (!(opcode&0x400000)) generateloadgen(c,EBX);
                                else
                                {
                                        addbyte(0x8B); addbyte(0x1D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ebx*/
//                                        addbyte(0xB9); addlong(usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                        addbyte(0x8B); addbyte(0x1B); /*MOVL (%ebx),%ebx*/
                                }
                                if (temp==3)
                                {
                                        gen_x86_call(mwritememfast);
//                                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                                }
                                else
                                {
                                        gen_x86_call(mwritemem);
                                        temp|=2;
                                        if (templ&~1 && (temp==3 || first))
                                        {
                                                first=0;
                                                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                                                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                                                addrel32(&rcodeblock[blockpoint2][0]);
                                        }
                                        addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                                }
//                                addbyte(0x85); addbyte(0xC0); /*TESTL %eax,%eax*/
//                                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
//                                addrel32(&rcodeblock[blockpoint2][0]);
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
//                #if 0
                if (opcode&0x200000)
                {
                        addbyte(0x74); addbyte(5/*14*/); /*JZ +14*/
//                        addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)+((opcode&0x1000000)?4:0)); /*SUBL countbits(opcode&0xFFFF),%edi*/
//                        addbyte(0x89); addbyte(0x3D); addlong(&armregs[RN]); /*MOVL %edi,armregs[RN]*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                }
                else
                {
//                        #endif
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;
                
        case 0x81: /* LDMDA */
        case 0x83: /* LDMDA ! */
        case 0x91: /* LDMDB */
        case 0x93: /* LDMDB ! */
                flagsdirty=0;
//                if (opcode&0x8000) return 0;
//hasldrb[blockpoint2]=1;
                templ=opcode&0xFFFF;
                temp=isvalidforfastread(armregs[RN]);
                generateloadgen(RN,EDI);
                if (opcode&0x200000) generatesavegen(17,EDI); /*PUSH EDI*/
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
//                if (opcode&0x200000) { addbyte(0x83); addbyte(0x2D); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); } /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF));   /*SUBL $4,%edi*/ }
                else                  { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)-4); /*SUBL $4,%edi*/ }
                for (c=0;c<16;c++)
                {
                        if (templ&1)
                        {
                                if (temp==3)
                                {
                                        gen_x86_call(mreadmemfast);
                                }
                                else
                                {
                                        gen_x86_call(mreadmem);
                                        temp|=2;
                                        #if 0
                                        if (templ&~1)
                                        {
                                                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                                                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                                                addrel32(&rcodeblock[blockpoint2][0]);
                                        }
                                        #endif
                                }
                                if (c==15)
                                {
                                        if (r15mask!=0xFFFFFFFC) generateload(c);
                                        addbyte(0x83); addbyte(0xC2); addbyte(4); /*ADDL $4,%edx*/
                                        if (r15mask!=0xFFFFFFFC)
                                        {
                                                addbyte(0x25); addlong(~r15mask); /*ANDL $~r15mask,%eax*/
                                                addbyte(0x81); addbyte(0xE2); addlong(r15mask); /*ANDL $r15mask,%edx*/
                                                addbyte(0x09); addbyte(0xC2); /*ORL %eax,%edx*/
                                        }
                                }
                                generatesavegen(c,EDX);
                                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/

                if (opcode&0x200000)
                {
                        generateloadgen(17,EDI); /*POP EDI*/
                        addbyte(0x74); addbyte(/*14*/5); /*JZ +14*/
//                        addbyte(0x83); addbyte(0xC7); addbyte(countbits(opcode&0xFFFF)+((opcode&0x1000000)?0:4)); /*ADDL countbits(opcode&0xFFFF),%edi*/
//                        addbyte(0x89); addbyte(0x3D); addlong(&armregs[RN]); /*MOVL %edi,armregs[RN]*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF));   /*SUBL $4,%edi*/
                        generatesavegen(RN,EDI);
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;

        case 0x89: /* LDMIA */
        case 0x8b: /* LDMIA ! */
        case 0x99: /* LDMIB */
        case 0x9b: /* LDMIB ! */
                flagsdirty=0;
                first=1;
                templ=opcode&0xFFFF;
                temp=isvalidforfastwrite(armregs[RN]);
                if (!temp) goto ldmiaslow;
                if (opcode&0x8000) goto ldmiaslow;
                generateloadgen(RN,EDI);
                if (opcode&0x200000) generatesavegen(17,EDI);/*PUSH %edi*/
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                addbyte(0x89); addbyte(0xF8); /*MOVL %edi,%eax*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
                addbyte(0x25); addlong(0xFC0); /*ANDL $0xFC0,%eax*/
                addbyte(0x3D); addlong(0xFC0); /*CMP $0xFC0,%eax*/
                addbyte(0x0F); addbyte(0x95); addbyte(0xC0); /*SETLE %al*/
                c=0;
                while (!(templ&1))
                {
                        templ>>=1;
                        c++;
                }
                addbyte(0x50); /*PUSH %eax*/
                gen_x86_call(mreadmem);
                addbyte(0x58); /*POP %eax*/
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                if (opcode&0x200000)
                {
                        addbyte(0x74); addbyte(10+((RN)?1:0)); /*JZ +*/
                        generateloadgen(17,EDI); /*POP %edi*/
                        generatesavegen(RN,EDI);
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                generatesavegen(c,EDX);
                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                c++;
                templ>>=1;
                if (templ)
                {
                        hasldrb[blockpoint2]=1;
                        addbyte(0x08); addbyte(0xC0); /*OR %al,%al*/
                        addbyte(0x75); /*JNZ fast*/
                        if (opcode&0x200000) { addbyte(17+2+/*1+*/3+2+((RN)?1:0)+5+7); }
                        else                 { addbyte(17+2+5+7); }
                        addbyte(0x55); /*PUSH %ebp*/
                        addbyte(0xBD); addlong((templ<<c)>>1); /*MOVL (templ<<c)>>1,%ebp*/
                        gen_x86_call(mreadmemslow);
                        addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                        addbyte(0x5D); /*POP %ebp*/
                        addbyte(0x75); addbyte(5);
                        addbyte(0xE9); temp=codeblockpos; addlong(0); /*JMP end*/
                        if (opcode&0x200000)
                        {
                                generateloadgen(17,EDI);
//                                addbyte(0x5F); /*POP %edi*/
                                generatesavegen(RN,EDI);
                        }
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        
                        d=0;
                        addbyte(0x01); addbyte(0xCF); /*ADDL %ecx,%edi*/
                        for (;c<16;c++)
                        {
                                if (templ&1)
                                {
                                        if (!d) { addbyte(0x8B); addbyte(0x17); /*MOVL (%edi),%edx*/ }
                                        else    { addbyte(0x8B); addbyte(0x57); addbyte(d); /*MOVL d(%edi),%edx*/}
//                                        if (c==15) { addbyte(0x83); addbyte(0xC2); addbyte(4); /*ADDL $4,%edx*/ }
                                        generatesavegen(c,EDX);
                                        d+=4;
                                }
                                templ>>=1;
                        }
                        c=codeblockpos;
                        codeblockpos=temp;
                        addlong(c-(codeblockpos+4));
                        codeblockpos=c;
                }
                if (opcode&0x200000)
                {
//                        addbyte(0x5F); /*POP %edi*/
                        generateloadgen(17,EDI);
                        if (!(opcode&(1<<RN)))
                        {
                                addbyte(0x83); addbyte(0xC7); addbyte(countbits(opcode&0xFFFF));   /*ADDL $4,%edi*/
                                generatesavegen(RN,EDI);
                        }
//                        addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                }
                break;


        //case 0x89: /* LDMIA */
        //case 0x8b: /* LDMIA ! */
        //case 0x99: /* LDMIB */
        //case 0x9b: /* LDMIB ! */
                flagsdirty=0;
                templ=opcode&0xFFFF;
                temp=isvalidforfastread(armregs[RN]);
        ldmiaslow:
                generateloadgen(RN,EDI);
                if (opcode&0x200000) generatesavegen(17,EDI); /*PUSH %edi*/
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
//                if (opcode&0x200000) { addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); } /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
                for (c=0;c<16;c++)
                {
                        if (templ&1)
                        {
                                if (temp==3)
                                {
                                        gen_x86_call(mreadmemfast);
                                }
                                else
                                {
                                        gen_x86_call(mreadmem);
                                        temp|=2;
                                        #if 0
                                        if (templ&~1)
                                        {
                                                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                                                addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                                                addrel32(&rcodeblock[blockpoint2][0]);
                                        }
                                        #endif
                                }
                                if (c==15)
                                {
                                        if (r15mask!=0xFFFFFFFC) generateload(c);
                                        addbyte(0x83); addbyte(0xC2); addbyte(4); /*ADDL $4,%edx*/
                                        if (r15mask!=0xFFFFFFFC)
                                        {
                                                addbyte(0x25); addlong(~r15mask); /*ANDL $~r15mask,%eax*/
                                                addbyte(0x81); addbyte(0xE2); addlong(r15mask); /*ANDL $r15mask,%edx*/
                                                addbyte(0x09); addbyte(0xC2); /*ORL %eax,%edx*/
                                        }
                                }
                                generatesavegen(c,EDX);
                                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                if (opcode&0x200000)
                {
//                        addbyte(0x5F); /*POP %edi*/
                        generateloadgen(17,EDI);
                        addbyte(0x74); addbyte(/*14*/5); /*JZ +14*/
//                        addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)+((opcode&0x1000000)?4:0)); /*SUBL countbits(opcode&0xFFFF),%edi*/
//                        addbyte(0x89); addbyte(0x3D); addlong(&armregs[RN]); /*MOVL %edi,armregs[RN]*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                        if (!(opcode&(1<<RN)))
                        {
                                addbyte(0x83); addbyte(0xC7); addbyte(countbits(opcode&0xFFFF)); /*ADDL $4,%edi*/
                                generatesavegen(RN,EDI);
                        }
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;

        case 0x85: /* LDMDA ^ */
        case 0x87: /* LDMDA ^! */
        case 0x95: /* LDMDB ^ */
        case 0x97: /* LDMDB ^! */
                flagsdirty=0;
                if (opcode&0x8000) return 0;
                templ=opcode&0xFFFF;
                temp=isvalidforfastread(armregs[RN]);
                generateloadgen(RN,EDI);
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                if (opcode&0x200000) { addbyte(0x83); addbyte(0x2D); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); } /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF));   /*SUBL $4,%edi*/ }
                else                  { addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)-4); /*SUBL $4,%edi*/ }
                for (c=0;c<16;c++)
                {
                        if (templ&1)
                        {
                                gen_x86_call(mreadmem);
                                addbyte(0x8B); addbyte(0x0D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                                addbyte(0x89); addbyte(0x11); /*MOVL %edx,(%ecx)*/
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/

                if (opcode&0x200000)
                {
                        addbyte(0x74); addbyte(14); /*JZ +14*/
                        addbyte(0x83); addbyte(0xC7); addbyte(countbits(opcode&0xFFFF)+((opcode&0x1000000)?0:4)); /*ADDL countbits(opcode&0xFFFF),%edi*/
                        addbyte(0x89); addbyte(0x3D); addlong(&armregs[RN]); /*MOVL %edi,armregs[RN]*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;

        case 0x8d: /* LDMIA ^ */
        case 0x8f: /* LDMIA ^! */
        case 0x9d: /* LDMIB ^ */
        case 0x9f: /* LDMIB ^! */
                flagsdirty=0;
                if (opcode&0x8000) return 0;
                templ=opcode&0xFFFF;
                temp=isvalidforfastread(armregs[RN]);
                generateloadgen(RN,EDI);
                addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL ~3,%edi*/
                if (opcode&0x200000) { addbyte(0x83); addbyte(0x05); addlong(&armregs[RN]); addbyte(countbits(opcode&0xFFFF)); } /*ADDL $countbits(opcode&0xFFFF),armregs[RN]*/
                if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
                for (c=0;c<15;c++)
                {
                        if (templ&1)
                        {
                                gen_x86_call(mreadmem);
                                addbyte(0x8B); addbyte(0x0D); addlong(&usrregs[c]); /*MOVL usrregs+(c*4),%ecx*/
                                addbyte(0x89); addbyte(0x11); /*MOVL %edx,(%ecx)*/
                                addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/
                        }
                        templ>>=1;
                }
                addbyte(0xF6); addbyte(0x05); addlong(&armirq); addbyte(0x40); /*TESTB $0x40,armirq*/
                if (opcode&0x200000)
                {
                        addbyte(0x74); addbyte(14); /*JZ +14*/
                        addbyte(0x83); addbyte(0xEF); addbyte(countbits(opcode&0xFFFF)+((opcode&0x1000000)?4:0)); /*SUBL countbits(opcode&0xFFFF),%edi*/
                        addbyte(0x89); addbyte(0x3D); addlong(&armregs[RN]); /*MOVL %edi,armregs[RN]*/
                        addbyte(0xE9); /*JMP 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                else
                {
                        addbyte(0x0F); addbyte(0x85); /*JNZ 0*/
                        addrel32(&rcodeblock[blockpoint2][0]);
                }
                break;


        case 0xa0: case 0xa1: case 0xa2: case 0xa3: /* B */
        case 0xa4: case 0xa5: case 0xa6: case 0xa7:
        case 0xa8: case 0xa9: case 0xaa: case 0xab:
        case 0xac: case 0xad: case 0xae: case 0xaf:
                flagsdirty=0;
                templ=(opcode&0xFFFFFF)<<2;
                if (templ&0x2000000) templ|=0xFC000000;
                templ+=4;
                if (!flaglookup[opcode>>28][(*pcpsr)>>28] && pcinc)
                   templ+=pcinc;
                if (!((PC+templ)&0xFC000000))
                {
                        if (templ<0x80)
                        {
                                addbyte(0x83); /*ADD templ,armregs[15]*/
                                addbyte(0x05);
                                addlong(&armregs[15]);
                                addbyte(templ);
                        }
                        else
                        {
                                addbyte(0x81); /*ADD templ,armregs[15]*/
                                addbyte(0x05);
                                addlong(&armregs[15]);
                                addlong(templ);
                        }
                }
                else
                {
                        generateload(15);
                        if ((unsigned int)r15mask!=0xFFFFFFFC)
                        {
                                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                        }
                        addbyte(0x81); addbyte(0xC0); addlong(templ); /*ADDL $templ,%eax*/
                        if ((unsigned int)r15mask!=0xFFFFFFFC)
                        {
                                addbyte(0x81); addbyte(0xE2); addlong(0xFC000003); /*ANDL $templ,%edx*/
                                addbyte(0x81); addbyte(0xE0); addlong(0x03FFFFFC); /*ANDL $templ,%eax*/
                                addbyte(0x09); addbyte(0xD0); /*ORL %edx,%eax*/
                        }
                        generatesave(15);
                }
                #if 0
                if ((PC+templ+4)==currentblockpc2 && flaglookup[opcode>>28][(*pcpsr)>>28])
                {
//                        rpclog("Possible %07X %07X %08X\n",PC,currentblockpc,&rcodeblock[blockpoint2][codeblockpos]);
                        addbyte(0xFF); /*DECL linecyc*/
                        addbyte(0x0D);
                        addlong(&linecyc);
                        addbyte(0x78); addbyte(12+10); /*JS endit*/

                addbyte(0x81); /*ADDL $c,rinscount*/
                addbyte(0x05);
                addlong(&rinscount);
                addlong(rins);

                        addbyte(0x83); /*ADD $4,armregs[15]*/
                        addbyte(0x05);
                        addlong(&armregs[15]);
                        addbyte(4);
                        addbyte(0xE9); addlong((-(codeblockpos+4))+BLOCKSTART+8); /*JMP start*/
                        /*.endit*/
//                        hasldrb[blockpoint2]=1;
                }
                #endif
                if (!flaglookup[opcode>>28][(*pcpsr)>>28])
                {
                        addbyte(0xE9); /*JMP 4*/
                        addrel32(&rcodeblock[blockpoint2][4]);
                }
   //     addbyte(0xA3); /*MOVL %eax,armregs[RN]*/
//        addlong(0);
                break;

        case 0xb0: case 0xb1: case 0xb2: case 0xb3: /* BL */
        case 0xb4: case 0xb5: case 0xb6: case 0xb7:
        case 0xb8: case 0xb9: case 0xba: case 0xbb:
        case 0xbc: case 0xbd: case 0xbe: case 0xbf:
                flagsdirty=0;
                templ=(opcode&0xFFFFFF)<<2;
                if (templ&0x2000000) templ|=0xFC000000;
                templ+=4;
                if (!flaglookup[opcode>>28][(*pcpsr)>>28] && pcinc)
                   templ+=pcinc;
                generateload(15);
                addbyte(0x83); addbyte(0xE8); addbyte(0x04); /*SUBL $4,%eax*/
                if (!((PC+templ)&0xFC000000))
                {
                        if (templ<0x80)
                        {
                                addbyte(0x83); /*ADD templ,armregs[15]*/
                                addbyte(0x05);
                                addlong(&armregs[15]);
                                addbyte(templ);
                        }
                        else
                        {
                                addbyte(0x81); /*ADD templ,armregs[15]*/
                                addbyte(0x05);
                                addlong(&armregs[15]);
                                addlong(templ);
                        }
                        generatesave(14);
                }
                else
                {
                        generatesave(14);
                        generateload(15);
                        if ((unsigned int)r15mask!=0xFFFFFFFC)
                        {
                                addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
                        }
                        addbyte(0x81); addbyte(0xC0); addlong(templ); /*ADDL $templ,%eax*/
                        if ((unsigned int)r15mask!=0xFFFFFFFC)
                        {
                                addbyte(0x81); addbyte(0xE2); addlong(0xFC000003); /*ANDL $templ,%edx*/
                                addbyte(0x81); addbyte(0xE0); addlong(0x03FFFFFC); /*ANDL $templ,%eax*/
                                addbyte(0x09); addbyte(0xD0); /*ORL %edx,%eax*/
                        }
                        generatesave(15);
                }
                if (!flaglookup[opcode>>28][(*pcpsr)>>28])
                {
                        addbyte(0xE9); /*JMP 4*/
                        addrel32(&rcodeblock[blockpoint2][4]);
                }
                break;

        default:
                return 0;
        }
        lastrecompiled=1;
        if (lastflagchange)
        {
                if (bigflagtest) *((uint32_t *)&rcodeblock[blockpoint2][lastflagchange])=codeblockpos-old;
                else             rcodeblock[blockpoint2][lastflagchange]=codeblockpos-old;
//                rpclog("Flag change %i %08X\n",temp,&rcodeblock[blockpoint2][lastflagchange]);
        }
        if ((opcode>>28)!=0xF) flagsdirty=0;
        return 1;
}

void generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr)
{
        int temp=7+5;
        int old=codeblockpos;
//        if ((PC>=0x40050FF0 && PC<0x40051010) || output==1) { rpclog("Instruction %08X %07X %08X %i\n",opcode,PC,&rcodeblock[blockpoint2][codeblockpos],codeblockpos); output=1; }
//        rpclog("%08X %02X %02X %02X %02X %08X %i %02X\n",&rcodeblock[8][0x5F],rcodeblock[8][0x5E],rcodeblock[8][0x5F],rcodeblock[8][0x60],rcodeblock[8][0x61],opcode,blockpoint2,codeblockpos);
        lastrecompiled=0;
        tempinscount++;
        if (recompileinstructions[(opcode>>20)&0xFF])
        {
                if (recompile(opcode,pcpsr)) return;
        }
        flagsdirty=0;
        codeblockpos=old;
        addbyte(0xC7); /*MOVL $opcode,(%esp)*/
        addbyte(0x04);
        addbyte(0x24);
        addlong(opcode);
        gen_x86_call(addr);
//#if 0
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000)
        {
//                rpclog("Carrying on - %i\n",pcinc);
//                generateupdatepc();
                if (pcinc)
                {
                        addbyte(0x83); /*ADD $4,armregs[15]*/
                        addbyte(0x05);
                        addlong(&armregs[15]);
                        addbyte(pcinc);
                        temp+=7;
//                pcinc=0;
                }
                addbyte(0xE9); /*JMP 4*/
                addrel32(&rcodeblock[blockpoint2][4]);
                temp+=5;
        }
        if (lastflagchange)
        {
                rcodeblock[blockpoint2][lastflagchange]=temp;
//                if (&rcodeblock[blockpoint2][lastflagchange]==0x87CD5A)
//                        rpclog("2Flag change %i %08X\n",temp,&rcodeblock[blockpoint2][lastflagchange]);
        }
//        rpclog("-%08X %02X %02X %08X %i %02X\n",&rcodeblock[8][0x5F],rcodeblock[8][0x5F],rcodeblock[8][0x60],opcode,blockpoint2,codeblockpos);
//        #endif
}
void generateupdatepc(void)
{
        if (pcinc)
      {
                addbyte(0x83); /*ADD $4,armregs[15]*/
                addbyte(0x05);
                addlong(&armregs[15]);
                addbyte(pcinc);
                pcinc=0;
        }
}
void generateupdateinscount(void)
{
        if (tempinscount)
        {
                addbyte(0x83); /*ADD tempinscount,inscount*/
                addbyte(0x05);
                addlong(&inscount);
                addbyte(tempinscount);
                tempinscount=0;
        }
}

void generatepcinc(void)
{
        pcinc+=4;
        if (pcinc==252) generateupdatepc();
        if (codeblockpos>=1800) blockend=1;
}

void endblock(uint32_t opcode, int c, uint32_t *pcpsr)
{
        int temp;
//        if (output) rpclog("endblock! %i\n",codeblockpos);
//        output=0;
        flagsdirty=0;
//        asm("decl 0x12345678;");
        generateupdatepc();
        generateupdateinscount();
        if (c<128)
        {
                addbyte(0x83); /*ADDL $c,rinscount*/
                addbyte(0x05);
                addlong(&rinscount);
                addbyte(c);
        }
        else
        {
                addbyte(0x81); /*ADDL $c,rinscount*/
                addbyte(0x05);
                addlong(&rinscount);
                addlong(c);
        }

        temp=codeblockpos;
        codeblockpos=5;
        addlong(&rcodeblock[blockpoint2][temp]-((uint32_t)&rcodeblock[blockpoint2][codeblockpos+4]));
        codeblockpos=temp;

        addbyte(0x83); /*ADDL $8,%esp*/
        addbyte(0xC4);
        addbyte(0x08);

        addbyte(0xFF); /*DECL linecyc*/
        addbyte(0x0D);
        addlong(&linecyc);

//        addbyte(0xC3); /*RET*/

        addbyte(0x79); /*JNS +1*/
        addbyte(1);
        temp=codeblockpos;
        addbyte(0xC3); /*RET*/

        generateloadgen(15,EAX); /*MOVL armregs[15],%eax*/
        if ((unsigned int)r15mask!=0xFFFFFFFC)
        {
                addbyte(0x25); /*ANDL $r15mask,%eax*/
                addlong(r15mask);
        }

        addbyte(0xF6); /*TESTB $0xFF,armirq*/
        addbyte(0x05);
        addlong(&armirq);
        addbyte(0xFF);
        addbyte(0x75); /*JNZ*/
        addbyte(temp-(codeblockpos+1));

//        addbyte(0xA1); /*MOVL armregs[15],%eax*/
//        addlong(&armregs[15]);
        if (((opcode>>20)&0xFF)==0xAF)
        {
                addbyte(0x3D); addlong(currentblockpc); /*CMP $thisblock,%eax*/
                addbyte(0x0F); addbyte(0x84); addlong(BLOCKSTART-(codeblockpos+4)); /*JZ back*/
        }
        addbyte(0x83); /*SUBL $8,%eax*/
        addbyte(0xE8);
        addbyte(0x08);
        addbyte(0x89); /*MOVL %eax,%edx*/
        addbyte(0xC2);
        addbyte(0x81); /*ANDL $0x1FFFC,%edx*/
        addbyte(0xE2);
        addlong(0x1FFFC);
        addbyte(0x3B); /*CMPL codeblockpc[%edx],%eax*/
        addbyte(0x82);
        addlong(codeblockpc);
        addbyte(0x74); /*JZ +1*/
        addbyte(1);
        addbyte(0xC3); /*RET*/
        addbyte(0x8B); /*MOVL codeblocknum[%edx],%eax*/
        addbyte(0x82);
        addlong(codeblocknum);
        addbyte(0x8B); /*MOVL codeblockaddr[%eax*4],%eax*/
        addbyte(0x04);
        addbyte(0x85);
        addlong(codeblockaddr);
        addbyte(0xFF); /*JMP *%eax*/
        addbyte(0xE0);
        codeinscount[blocknum]=c;
}

void dumplastblock(void)
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
//        asm("movl $0x12345678,(%esp)");
        addbyte(0xC7); /*MOVL $dat,(addr)*/
        addbyte(0x05);
        addlong(addr);
        addlong(dat);
//        asm("movl $0x11223344,0x55667788");
}

void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr)
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
//        rpclog("%08X %02X %02X %02X %02X %08X %i %02X\n",&rcodeblock[8][0x5F],rcodeblock[8][0x5E],rcodeblock[8][0x5F],rcodeblock[8][0x60],rcodeblock[8][0x61],opcode,blockpoint2,codeblockpos);
        if ((opcode>>28)==0xE) { return; } /*No need if 'always' condition code*/
        if (((opcode>>20)&0xE0)==0x80 && recompileinstructions[(opcode>>20)&0xFF]) bigflagtest=0x10;
        else                                                                       bigflagtest=0;
        switch (opcode>>28)
        {
                case 0: /*EQ*/
                case 1: /*NE*/
                if (flagsdirty)
                {
                        addbyte(0xF6); addbyte(0xC1); addbyte(0x40); /*TESTB $0x40,%cl*/
                }
                else
                {
                        addbyte(0xF6); /*TESTB (pcpsr>>24),$0x40*/
                        addbyte(0x05);
                        addlong(((unsigned long)pcpsr)+3);
                        addbyte(0x40);
                }
                if (bigflagtest) addbyte(0x0F);
                if ((opcode>>28)&1) addbyte(0x75+bigflagtest);                 /*JNE +5*/
                else                addbyte(0x74+bigflagtest);                 /*JE +5*/
                break;
                case 2: /*CS*/
                case 3: /*CC*/
                if (flagsdirty)
                {
                        addbyte(0xF6); addbyte(0xC1); addbyte(0x20); /*TESTB $0x20,%cl*/
                }
                else
                {
                        addbyte(0xF6); /*TESTB (pcpsr>>24),$0x20*/
                        addbyte(0x05);
                        addlong(((unsigned long)pcpsr)+3);
                        addbyte(0x20);
                }
                if (bigflagtest) addbyte(0x0F);
                if ((opcode>>28)&1) addbyte(0x75+bigflagtest);                 /*JNE +5*/
                else                addbyte(0x74+bigflagtest);                 /*JE +5*/
                break;
                case 4: /*MI*/
                case 5: /*PL*/
                if (flagsdirty)
                {
                        addbyte(0xF6); addbyte(0xC1); addbyte(0x80); /*TESTB $0x80,%cl*/
                }
                else
                {
                        addbyte(0xF6); /*TESTB (pcpsr>>24),$0x80*/
                        addbyte(0x05);
                        addlong(((unsigned long)pcpsr)+3);
                        addbyte(0x80);
                }
                if (bigflagtest) addbyte(0x0F);
                if ((opcode>>28)&1) addbyte(0x75+bigflagtest);                 /*JNE +5*/
                else                addbyte(0x74+bigflagtest);                 /*JE +5*/
                break;
                case 6: /*VS*/
                case 7: /*VC*/
                if (flagsdirty)
                {
                        addbyte(0xF6); addbyte(0xC1); addbyte(0x10); /*TESTB $0x10,%cl*/
                }
                else
                {
                        addbyte(0xF6); /*TESTB (pcpsr>>24),$0x10*/
                        addbyte(0x05);
                        addlong(((unsigned long)pcpsr)+3);
                        addbyte(0x10);
                }
                if (bigflagtest) addbyte(0x0F);
                if ((opcode>>28)&1) addbyte(0x75+bigflagtest);                 /*JNE +5*/
                else                addbyte(0x74+bigflagtest);                 /*JE +5*/
                break;
                default:
                if (flagsdirty)
                {
                        addbyte(0x0F); addbyte(0xB6); addbyte(0xC1); /*MOVZBL %cl,%eax*/
                        addbyte(0xC1);                 /*SHRL $4,%eax*/
                        addbyte(0xE8);
                        addbyte(0x4);
                }
                else
                {
                        addbyte(0xA1);                 /*MOVL (pcpsr),%eax*/
                        addlong((unsigned long)pcpsr);
                        addbyte(0xC1);                 /*SHRL $28,%eax*/
                        addbyte(0xE8);
                        addbyte(0x1C);
                }
                addbyte(0x80);                 /*CMPB $0,flaglookup(%eax)*/
                addbyte(0xB8);
                addlong((unsigned long)(&flaglookup[opcode>>28][0]));
                addbyte(0);
                if (bigflagtest) addbyte(0x0F);
                addbyte(0x74+bigflagtest);                 /*JE +5*/
                break;
        }
//        flagsdirty=0;
        lastflagchange=codeblockpos;
        if (bigflagtest) { addlong(0); }
        else             addbyte(0);
//        if (output) rpclog("PC %07X - %08X  %i\n",PC,opcode,(((opcode+0x6000000)&0xF000000)>0xA000000));
/*        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000) addbyte(5+7+5+((pcinc)?7:0));
#ifdef ABORTCHECKING
        else if (((opcode+0x6000000)&0xF000000)>=0xA000000)
        {
                if (((uint32_t)(&rcodeblock[blockpoint2][codeblockpos+19])-(uint32_t)(&rcodeblock[blockpoint2][0]))<120) addbyte(5+7+2+2);
                else                                                                                                                    addbyte(5+7+2+6);
        }
#endif
        else                                                                        addbyte(5+7);*/
}
void generateirqtest(void)
{
        int temp=5+7;
        if (lastrecompiled) return;
//        asm("testl %eax,%eax");
//        asm("testb $0xC0,0x12345678");
//        addbyte(0xF6); /*TESTB $0x40,armirq*/
//        addbyte(0x05);
//        addlong(&armirq);
//        addbyte(0x40);
//                rpclog("genirq %02X %02X\n",rcodeblock[8][0x5F],rcodeblock[8][0x60]);
        addbyte(0x85); /*TESTL %eax,%eax*/
        addbyte(0xC0);
//        #if 0
        temp+=2;
        if (((uint32_t)(&rcodeblock[blockpoint2][codeblockpos+4])-(uint32_t)(&rcodeblock[blockpoint2][0]))<120)
        {
                addbyte(0x75); /*JNE 0*/
                addbyte((uint32_t)&rcodeblock[blockpoint2][0]-(uint32_t)(&rcodeblock[blockpoint2][codeblockpos+1]));
//                rpclog("JNE %08X %08X %i\n",&rcodeblock[blockpoint2][0],&rcodeblock[blockpoint2][codeblockpos],codeblockpos-1);
//                rpclog("%02X %02X\n",rcodeblock[8][0x5F],rcodeblock[8][0x60]);
                temp+=2;
        }
        else
        {
//                #endif
                addbyte(0x0F); /*JNE 0*/
                addbyte(0x85);
                addrel32(&rcodeblock[blockpoint2][0]);
//                rpclog("%02X %02X\n",rcodeblock[8][0x5F],rcodeblock[8][0x60]);
                temp+=6;
        }
        if (lastflagchange && !lastrecompiled)
        {
                rcodeblock[blockpoint2][lastflagchange]=temp;
//                if (&rcodeblock[blockpoint2][lastflagchange]==0x87CD5A)
//                        rpclog("3Flag change %i %08X\n",temp,&rcodeblock[blockpoint2][lastflagchange]);
        }

}

#endif
#endif
