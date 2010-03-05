/*RPCemu v0.6 by Tom Walker
  System coprocessor + MMU emulation*/
#include "rpcemu.h"
#include "mem.h"
#include "arm.h"

uint32_t oldpc = 0, oldpc2 = 0, oldpc3 = 0;
int dcache = 0; /* Data cache on StrongARM, unified cache pre-StrongARM */

#define TLBCACHESIZE 256

uint32_t tlbcache[0x100000] = {0};
static uint32_t tlbcache2[TLBCACHESIZE];
unsigned long *vraddrl = 0;
uint32_t vraddrls[1024] = {0}, vraddrphys[1024] = {0};
unsigned long *vwaddrl = 0;
uint32_t vwaddrls[1024] = {0}, vwaddrphys[1024] = {0};
static int tlbcachepos = 0;
int tlbs = 0, flushes = 0;
uint32_t pccache = 0;

static struct cp15
{
        uint32_t tlbbase,dacr;
        uint32_t far,fsr,ctrl;
} cp15;
static int icache = 0;

/* The bits of the processor's internal coprocessor (MMU) control register */
#define CP15_CTRL_MMU			(1 << 0)
#define CP15_CTRL_ALIGNMENT_FAULT	(1 << 1)
#define CP15_CTRL_CACHE			(1 << 2)  /* Data cache only on SA */
#define CP15_CTRL_WRITE_BUFFER		(1 << 3)
#define CP15_CTRL_PROG32		(1 << 4)  /* Always enabled in SA */
#define CP15_CTRL_DATA32		(1 << 5)  /* Always enabled in SA */
#define CP15_CTRL_LATE_ABORT_TIMING	(1 << 6)  /* Always enabled in 710 & SA */
#define CP15_CTRL_BIG_ENDIAN		(1 << 7)
#define CP15_CTRL_SYSTEM		(1 << 8)
#define CP15_CTRL_ROM			(1 << 9)  /* 710 & SA */
#define CP15_CTRL_ICACHE		(1 << 12) /* SA only */

/* Fault Status codes stored in FSR[0:3] when a fault occurs */
#define CP15_FAULT_TRANSLATION_SECTION	0x5
#define CP15_FAULT_TRANSLATION_PAGE	0x7
#define CP15_FAULT_DOMAIN_SECTION	0x9
#define CP15_FAULT_DOMAIN_PAGE		0xb
#define CP15_FAULT_PERMISSION_SECTION	0xd
#define CP15_FAULT_PERMISSION_PAGE	0xf

static void
cp15_tlb_flush(void)
{
	int c;

	for (c = 0; c < TLBCACHESIZE; c++) {
		if (tlbcache2[c] != 0xffffffff) {
			tlbcache[tlbcache2[c]] = 0xffffffff;
			tlbcache2[c] = 0xffffffff;
		}
	}
}

static void
cp15_vaddr_reset(void)
{
	int c;

	for (c = 0; c < 1024; c++) {
		if (vraddrls[c] != 0xFFFFFFFF) {
			vraddrl[vraddrls[c]] = 0xFFFFFFFF;
			vraddrls[c] = 0xFFFFFFFF;
			vraddrphys[c] = 0xFFFFFFFF;
		}
		if (vwaddrls[c] != 0xFFFFFFFF) {
			vwaddrl[vwaddrls[c]] = 0xFFFFFFFF;
			vwaddrls[c] = 0xFFFFFFFF;
			vwaddrphys[c] = 0xFFFFFFFF;
		}
	}
}

void getcp15fsr(void)
{
        rpclog("%08X %08X\n",cp15.far,cp15.fsr);
}

void resetcp15(void)
{
        prog32=1;
        mmu=0;
        memset(tlbcache, 0xff, 0x100000 * sizeof(uint32_t));
        memset(tlbcache2, 0xff, TLBCACHESIZE * sizeof(uint32_t));
        tlbcachepos=0;
        if (!vraddrl) vraddrl=malloc(0x100000*sizeof(uint32_t *));
        memset(vraddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vraddrls,0xFF,1024*sizeof(uint32_t));
        if (!vwaddrl) vwaddrl=malloc(0x100000*sizeof(uint32_t *));
        memset(vwaddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vwaddrls,0xFF,1024*sizeof(uint32_t));
}

static int translations;
static uint32_t *tlbram;
static uint32_t tlbrammask;

static void
cp15_tlb_flush_all(void)
{
	clearmemcache();
	pccache = 0xffffffff;
	blockend = 1;
	cp15_tlb_flush();
	cp15_vaddr_reset();
	flushes++;
}

static void
cp15_tlb_add_entry(uint32_t vaddr, uint32_t paddr)
{
	if (tlbcache2[tlbcachepos] != 0xffffffff) {
		tlbcache[tlbcache2[tlbcachepos]] = 0xffffffff;
	}
	tlbcache2[tlbcachepos] = vaddr >> 12;
	tlbcache[vaddr >> 12] = paddr & 0xfffff000;

	tlbcachepos = (tlbcachepos + 1) & (TLBCACHESIZE - 1);
}

#define CRm (opcode&0xF)
#define OPC2 ((opcode>>5)&7)
void writecp15(uint32_t addr, uint32_t val, uint32_t opcode)
{
        switch (addr&15)
        {
        case 1: /* Control */
                cp15.ctrl=val;
                if (!icache && (val & CP15_CTRL_ICACHE))
                       resetcodeblocks();
                icache = val & CP15_CTRL_ICACHE;
                dcache = val & CP15_CTRL_CACHE;
                if (!(val & CP15_CTRL_MMU)) {
                       rpclog("MMU disable at %08X\n",PC);
                       ins = 0;
                }

                if (mmu != (val & CP15_CTRL_MMU))
                {
                        resetcodeblocks();
                        cp15_vaddr_reset();
                }
                mmu    = val & CP15_CTRL_MMU;
                prog32 = val & CP15_CTRL_PROG32;
                if (!prog32 && (mode&16))
                {
                        updatemode(mode&15);
                }
                // rpclog("CP15 control write %08X %08X %i\n",val,PC,blockend);
                return; /*We can probably ignore all other bits*/

        case 2: /* TLB base */
                cp15.tlbbase=val&~0x3FFF;
                cp15_vaddr_reset();
                // resetcodeblocks();
                switch (cp15.tlbbase&0x1F000000)
                {
                case 0x02000000: /*VRAM - yes RISC OS 3.7 does put the TLB in VRAM at one point*/
                        tlbram = vram;
                        tlbrammask = config.vrammask >> 2;
                        break;
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                        tlbram = ram;
                        tlbrammask = config.rammask >> 2;
                        break;
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                        tlbram = ram2;
                        tlbrammask = config.rammask >> 2;
                        break;
                }
                // printf("CP15 tlb base now %08X\n",cp15.tlbbase);
                return;

        case 3: /* Domain Access Control */
                cp15.dacr=val;
                // printf("CP15 DACR now %08X\n",cp15.dacr);
                return;

        case 5:
        case 6:
        case 8:
                switch (config.model) {
                /* ARMv3 Architecture */
                case CPUModel_ARM610:
                case CPUModel_ARM710:
                case CPUModel_ARM7500:
                case CPUModel_ARM7500FE:
                        switch (addr & 0xf) {
                        case 5: /* TLB Flush */
                        case 6: /* TLB Purge */
                                cp15_tlb_flush_all();
                                break;

                        default:
                                UNIMPLEMENTED("CP15 ARMv3 Write",
                                   "Unsupported write to reg 8 in ARMv3 mode");
                        }
                        return;

                /* ARMv4 Architecture */
                case CPUModel_SA110:
                case CPUModel_ARM810:
                        switch (addr & 0xf) {
                        case 5: /* Fault Status Register */
                                cp15.fsr = val;
                                break;

                        case 6: /* Fault Address Register */
                                cp15.far = val;
                                break;

                        case 8: /* TLB Operations */
                                if ((CRm & 1) && !(OPC2)) {
                                        resetcodeblocks();
                                }
                                cp15_tlb_flush_all();
                                break;
                        }
                        return;

                default:
                        fprintf(stderr, "writecp15(): unknown CPU model %d\n",
                                config.model);
                        exit(EXIT_FAILURE);
                }
                break;

        case 7: /* Flush Cache */
                /* for (c=0;c<1024;c++)
                {
                        if (vraddrls[c]!=0xFFFFFFFF)
                        {
                                vraddrl[vraddrls[c]]=0xFFFFFFFF;
                                vraddrls[c]=0xFFFFFFFF;
                                vraddrphys[c]=0xFFFFFFFF;
                        }
                }*/
                if ((CRm&1) && !(OPC2)) resetcodeblocks();
                // rpclog("Cache invalidate %08X\n",PC);
                pccache = 0xFFFFFFFF;
                // blockend=1;
                return;

	case 15:
		switch (config.model) {
		case CPUModel_SA110: /* Test, Clock and Idle control */
			if (OPC2 == 2 && CRm == 1) {
				/* Enable clock switching - no need to implement */
			} else {
				UNIMPLEMENTED("CP15 Write",
				  "Write to SA110 Reg 15, OPC2=0x%02x CRm=0x%02x",
				  OPC2, CRm);
			}
			break;

		default:
			UNIMPLEMENTED("CP15 Write",
			  "Unknown processor '%d' writing to reg 15",
			  config.model);
		}
		break;

        default:
                UNIMPLEMENTED("CP15 Write", "Unknown register %u", addr & 15);
                break;
        }
        // error("Bad write CP15 %08X %08X %07X\n",addr,val,PC);
        // dumpregs();
        // exit(-1);
}

uint32_t readcp15(uint32_t addr)
{
        switch (addr&15)
        {
                case 0: /*ARM ID*/
                switch (config.model)
                {
                        case CPUModel_ARM7500:   return 0x41027100;
                        case CPUModel_ARM7500FE: return 0x41077100;
                        case CPUModel_ARM610:    return 0x41560610;
                        case CPUModel_ARM710:    return 0x41007100;
                        case CPUModel_ARM810:    return 0x41018100;
                        case CPUModel_SA110:     return 0x4401a102;
                }
                break;
                case 1: /*Control*/
                return cp15.ctrl;
                case 2: /*???*/
                return cp15.tlbbase;
                case 3: /*DACR*/
                return cp15.dacr;
                case 5: /*Fault status*/
//                printf("Fault status read %08X\n",cp15.fsr);
                return cp15.fsr;
                case 6: /*Fault address*/
                //printf("Fault address read %08X\n",cp15.far);
                return cp15.far;
                default:
                UNIMPLEMENTED("CP15 Read", "Unknown register %u", addr & 15);
        }
        error("Bad read CP15 %08X %07X\n",addr,PC);
        dumpregs();
        exit(-1);
}

/*DOMAIN -
  No access - fault
  Client - checkpermissions
  Manager - always allow*/
//54F13001
//1010042e
//databort=1;

static int checkpermissions(int p, int rw)
{
        switch (p)
        {
        case 0:
                switch (cp15.ctrl&0x300)
                {
                case 0x000: /* No access */
                case 0x300: /* Unpredictable */
                        return 1;

                case 0x100: /* Supervisor read-only */
//                        break; /*delibrately broken for Linux*/
                        /*Linux will crash very early on if this is implemented properly*/
                        if (!memmode || rw) { return 1; }
                        break;

                case 0x200: /* Read-only */
                        if (rw) { return 1; }
                        break;
                }
                break;

        case 1: /* Supervisor read/write */
//                break;
                if (!memmode) { return 1; }
                break;

        case 2: /* Supervisor read/write, User read-only*/
                if (!memmode && rw) { return 1; }
                break;
        }
        return 0;
}

static int checkdomain(uint32_t domain)
{
        int temp=cp15.dacr>>(domain<<1);

        return temp&3;
}

uint32_t translateaddress2(uint32_t addr, int rw, int prefetch)
{
        uint32_t vaddr=((addr>>18)&~3)|cp15.tlbbase;
        uint32_t fld;
        uint32_t sldaddr,sld; //,taddr;
        uint32_t oa=addr;
        uint32_t domain, fault_code;
        uint32_t domain_access;
        int temp,temp2 = 0;

        armirq&=~0x40;

        translations++;
        tlbs++;

        fld=tlbram[(vaddr>>2)&tlbrammask];
        domain = (fld >> 5) & 0xf;
        if (fld & 3) domain_access = checkdomain(domain);
        switch (fld&3)
        {
        case 0: /* Fault (Section Translation) */
                fault_code = CP15_FAULT_TRANSLATION_SECTION;
                goto do_fault;

        case 1: /* Page */
                sldaddr=((addr&0xFF000)>>10)|(fld&0xFFFFFC00);
                if ((sldaddr&0x1F000000)==0x02000000)
                   sld = vram[(sldaddr & config.vrammask) >> 2];
                else if (sldaddr&0x4000000)
                   sld = ram2[(sldaddr & config.rammask) >> 2];
                else
                   sld = ram[(sldaddr & config.rammask) >> 2];

                /* Check for invalid Page Table Entry */
                if ((sld & 3) == 0 || (sld & 3) == 3) {
                        /* Fault or Reserved */
                        fault_code = CP15_FAULT_TRANSLATION_PAGE;
                        goto do_fault;
                }
                if (domain_access == 0 || domain_access == 2) {
                        fault_code = CP15_FAULT_DOMAIN_PAGE;
                        goto do_fault;
                }
                switch (sld&3)
                {
                        case 1: /*64kb - NetBSD*/
                        temp=(addr&0xC000)>>13;
                        temp2=sld&(0x30<<temp);
                        temp2>>=(4+temp);
                        break;
                        case 2: /*4kb - RISC OS, Linux*/
                        temp=(addr&0xC00)>>9;
                        temp2=sld&(0x30<<temp);
                        temp2>>=(4+temp);
                        break;
                }
                if (domain_access == 1) {
                        /* Client Domain - check permissions */
                        if (checkpermissions(temp2, rw)) {
                                fault_code = CP15_FAULT_PERMISSION_PAGE;
                                goto do_fault;
                        }
                }
                if ((sld&3)==1) sld=((sld&0xFFFF0FFF)|(addr&0xF000));
                addr=(sld&0xFFFFF000)|(addr&0xFFF);
                cp15_tlb_add_entry(oa, addr);
                return addr;

        case 2: /* Section */
                if (domain_access == 0 || domain_access == 2) {
                        fault_code = CP15_FAULT_DOMAIN_SECTION;
                        goto do_fault;
                }
                if (domain_access == 1) {
                        /* Client Domain - check permissions */
                        if (checkpermissions((fld & 0xc00) >> 10, rw)) {
                                fault_code = CP15_FAULT_PERMISSION_SECTION;
                                goto do_fault;
                        }
                }
                addr=(addr&0xFFFFF)|(fld&0xFFF00000);
                cp15_tlb_add_entry(oa, addr);
                return addr;

        default:
                error("Bad descriptor type %i %08X\n",fld&3,fld);
                error("Address %08X\n",addr);
                dumpregs();
                exit(-1);
        }
        exit(-1);

do_fault:
	armirq |= 0x40;
	if (!prefetch) {
		cp15.far = addr;
		cp15.fsr = (domain << 4) | fault_code;
	}
	return 0;
}

/*uint32_t translateaddress(uint32_t addr, int rw)
{
        if (!(addr&0xFC000000) && !(tlbcache[(addr>>12)&0x3FFF]&0xFFF))
        {
//                rpclog("Cached %08X\n",tlbcache[addr>>12]);
                return tlbcache[addr>>12]|(addr&0xFFF);
        }
        return translateaddress2(addr,rw);
}*/


uint32_t *getpccache(uint32_t addr)
{
        uint32_t addr2;

        addr&=~0xFFF;
        if (mmu)
        {
                armirq&=~0x40;
//                if (indumpregs) rpclog("Translate prefetch %08X %02X ",addr,armirq);
                addr2=translateaddress(addr,0,1);
                if (armirq&0x40)
                {
//                        rpclog("Translate prefetch abort!!! %07X %07X %07X %07X\n",PC,oldpc,oldpc2,oldpc3);
//                        if (indumpregs) rpclog("Abort!\n");
                        armirq&=~0x40;
                        armirq|=0x80;
//                        databort=0;
//                        prefabort=1;
                        return NULL;
                }
//                if (indumpregs) rpclog("\n");
        }
        else     addr2=addr;
        /*Invalidate write pointer for this page - so we can handle code modification*/
        vwaddrl[addr>>12]=0xFFFFFFFF;

        switch (addr2&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                return &rom[((long)(addr2&0x7FF000)-(long)addr)>>2];
                case 0x02000000: /*VRAM*/
                return &vram[((long)(addr2&0x1FF000)-(long)addr)>>2];
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram[((long)(addr2 & config.rammask) - (long)addr) >> 2];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram2[((long)(addr2 & config.rammask) - (long)addr) >> 2];
        }
        error("Bad PC %08X %08X\n",addr,addr2);
        dumpregs();
        exit(-1);
}

int isvalidforfastread(uint32_t addr)
{
//        rpclog("Valid for fast read? %08X\n",addr);
        if (mmu)
        {
                if ((tlbcache[(addr)>>12]&0xFFF)) return 0;
                addr=tlbcache[(addr)>>12]|((addr)&0xFFF);
        }
        else
        return 0;
//        rpclog("%08X\n",addr);
        switch (addr&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                case 0x02000000: /*VRAM*/
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                return 1;
        }
        return 0;
}
int isvalidforfastwrite(uint32_t addr)
{
//        rpclog("Valid for fast write? %08X\n",addr);
        if (mmu)
        {
                if ((tlbcache[(addr)>>12]&0xFFF)) return 0;
                addr=tlbcache[(addr)>>12]|((addr)&0xFFF);
        }
        else
           return 0;
//        rpclog("%08X\n",addr);
        switch (addr&0x1F000000)
        {
//                case 0x00000000: /*ROM*/
                case 0x02000000: /*VRAM*/
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                return 1;
        }
        return 0;
}
