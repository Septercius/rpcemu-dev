/*RPCemu v0.6 by Tom Walker
  System coprocessor + MMU emulation*/
#include "rpcemu.h"
#include "mem.h"
#include "arm.h"

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

static struct cp15 {
	uint32_t ctrl;				/**< Control register */
	uint32_t translation_table;		/**< Translation Table Base register */
	uint32_t domain_access_control;		/**< Domain Access Control register */
	uint32_t fault_status;			/**< Fault Status register */
	uint32_t fault_address;			/**< Fault Address register */

	CPUModel cpu_model;			/**< CPU model emulated */
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

/**
 * Called on program startup and emulated machine reset to
 * prepare the cp15 module
 *
 * @param cpu_model Model of CPU (and associated mmu/cp15) being emulated
 */
void
cp15_reset(CPUModel cpu_model)
{
        cp15.cpu_model = cpu_model;
        prog32=1;
        mmu=0;
        memset(tlbcache, 0xff, 0x100000 * sizeof(uint32_t));
        memset(tlbcache2, 0xff, TLBCACHESIZE * sizeof(uint32_t));
        tlbcachepos=0;
        memset(vraddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vraddrls,0xFF,1024*sizeof(uint32_t));
        memset(vwaddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vwaddrls,0xFF,1024*sizeof(uint32_t));
}

/**
 * Called on program startup to prepare the cp15 module
 */
void
cp15_init(void)
{
	vraddrl = malloc(0x100000 * sizeof(uint32_t *));
	vwaddrl = malloc(0x100000 * sizeof(uint32_t *));
}

static uint32_t *tlbram;
static uint32_t tlbrammask;

static void
cp15_tlb_flush_all(void)
{
	clearmemcache();
	pccache = 0xffffffff;
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

void
cp15_write(uint32_t addr, uint32_t val, uint32_t opcode)
{
	uint32_t CRm = opcode & 0xf;
	uint32_t OPC2 = (opcode >> 5) & 7;

	switch (addr & 0xf) {
	case 1: /* Control */
		cp15.ctrl = val;
		if (!icache && (val & CP15_CTRL_ICACHE)) {
			resetcodeblocks();
		}
		icache = val & CP15_CTRL_ICACHE;
		dcache = val & CP15_CTRL_CACHE;

		if ((val & CP15_CTRL_MMU) != mmu) {
			resetcodeblocks();
			cp15_vaddr_reset();
		}
		mmu = val & CP15_CTRL_MMU;
		prog32 = val & CP15_CTRL_PROG32;
		if (!prog32 && (arm.mode & 0x10)) {
			updatemode(arm.mode & 0xf);
		}
		return;

	case 2: /* Translation Table Base */
		cp15.translation_table = val & ~0x3fffu;
		cp15_vaddr_reset();
		// resetcodeblocks();
		switch (cp15.translation_table & 0x1f000000) {
		case 0x02000000: /* VRAM */
			tlbram = vram;
			tlbrammask = config.vrammask >> 2;
			break;
		case 0x10000000: /* SIMM 0 bank 0 */
		case 0x11000000:
		case 0x12000000:
		case 0x13000000:
			tlbram = ram00;
			tlbrammask = mem_rammask >> 2;
			break;
		case 0x14000000: /* SIMM 0 bank 1 */
		case 0x15000000:
		case 0x16000000:
		case 0x17000000:
			tlbram = ram01;
			tlbrammask = mem_rammask >> 2;
			break;
		case 0x18000000: /* SIMM 1 bank 0 */
		case 0x19000000:
		case 0x1a000000:
		case 0x1b000000:
		case 0x1c000000: /* SIMM 1 bank 1 */
		case 0x1d000000:
		case 0x1e000000:
		case 0x1f000000:
			tlbram = ram1;
			tlbrammask = 0x7ffffff >> 2;
			break;
		}
		return;

	case 3: /* Domain Access Control */
		cp15.domain_access_control = val;
		return;

	case 5:
	case 6:
	case 8:
		switch (cp15.cpu_model) {
		/* ARMv3 Architecture */
		case CPUModel_ARM610:
		case CPUModel_ARM710:
		case CPUModel_ARM7500:
		case CPUModel_ARM7500FE:
			switch (addr & 0xf) {
			case 5: /* TLB Flush */
			case 6: /* TLB Purge */
				cp15_tlb_flush_all();
				resetcodeblocks();
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
				cp15.fault_status = val;
				break;

			case 6: /* Fault Address Register */
				cp15.fault_address = val;
				break;

			case 8: /* TLB Operations */
				if ((CRm & 1) && (OPC2 == 0)) {
					resetcodeblocks();
				}
				cp15_tlb_flush_all();
				break;
			}
			return;

		default:
			fprintf(stderr, "cp15_write(): unknown CPU model %d\n",
				cp15.cpu_model);
			exit(EXIT_FAILURE);
		}
		break;

	case 7: /* Flush Cache */
		if ((CRm & 1) && (OPC2 == 0)) {
			resetcodeblocks();
		}
		pccache = 0xffffffff;
		return;

	case 15:
		switch (cp15.cpu_model) {
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
			  cp15.cpu_model);
		}
		break;

	default:
		UNIMPLEMENTED("CP15 Write", "Unknown register %u", addr & 0xf);
		break;
	}
}

uint32_t
cp15_read(uint32_t addr)
{
	switch (addr & 0xf) {
	case 0: /* ID */
		switch (cp15.cpu_model) {
		case CPUModel_ARM7500:   return 0x41027100;
		case CPUModel_ARM7500FE: return 0x41077100;
		case CPUModel_ARM610:    return 0x41560610;
		case CPUModel_ARM710:    return 0x41007100;
		case CPUModel_ARM810:    return 0x41018100;
		case CPUModel_SA110:     return 0x4401a102;
		}
		break;
	case 1: /* Control */
		return cp15.ctrl;
	case 2: /* Translation Table Base */
		return cp15.translation_table;
	case 3: /* Domain Access Control */
		return cp15.domain_access_control;
	case 5: /* Fault Status */
		return cp15.fault_status;
	case 6: /* Fault Address */
		return cp15.fault_address;
	default:
		UNIMPLEMENTED("CP15 Read", "Unknown register %u", addr & 0xf);
	}
	fatal("Bad read CP15 %x %08x\n", addr, PC);
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
        int temp = cp15.domain_access_control >> (domain << 1);

        return temp&3;
}

uint32_t
translateaddress2(uint32_t addr, int rw, int prefetch)
{
	uint32_t fld_addr, fld;
	uint32_t sld_addr, sld;
	uint32_t oa = addr;
	uint32_t domain, fault_code;
	uint32_t domain_access;
	uint32_t temp, temp2 = 0;

	armirq &= ~0x40u;

	tlbs++;

	/* Fetch first-level descriptor */
	fld_addr = cp15.translation_table | ((addr >> 18) & ~3u);
	fld = tlbram[(fld_addr >> 2) & tlbrammask];
	domain = (fld >> 5) & 0xf;

	switch (fld & 3) {
	case 0: /* Fault (Section Translation) */
		fault_code = CP15_FAULT_TRANSLATION_SECTION;
		goto do_fault;

	case 1: /* Page */
		/* Fetch second-level descriptor */
		sld_addr = (fld & 0xfffffc00) | ((addr >> 10) & 0x3fc);
		if ((sld_addr & 0x1f000000) == 0x02000000) {
			sld = vram[(sld_addr & config.vrammask) >> 2];
		} else if (sld_addr & 0x8000000) {
			sld = ram1[(sld_addr & 0x7ffffff) >> 2];
		} else if (sld_addr & 0x4000000) {
			sld = ram01[(sld_addr & mem_rammask) >> 2];
		} else {
			sld = ram00[(sld_addr & mem_rammask) >> 2];
		}

		/* Check for invalid Page Table Entry */
		if ((sld & 3) == 0 || (sld & 3) == 3) {
			/* Fault or Reserved */
			fault_code = CP15_FAULT_TRANSLATION_PAGE;
			goto do_fault;
		}
		domain_access = checkdomain(domain);
		if (domain_access == 0 || domain_access == 2) {
			fault_code = CP15_FAULT_DOMAIN_PAGE;
			goto do_fault;
		}
		switch (sld & 3) {
		case 1: /* Large page (64 KB) */
			temp = (addr & 0xc000) >> 13;
			temp2 = sld & (0x30 << temp);
			temp2 >>= (4 + temp);
			break;
		case 2: /* Small page (4 KB) */
			temp = (addr & 0xc00) >> 9;
			temp2 = sld & (0x30 << temp);
			temp2 >>= (4 + temp);
			break;
		}
		if (domain_access == 1) {
			/* Client Domain - check permissions */
			if (checkpermissions(temp2, rw)) {
				fault_code = CP15_FAULT_PERMISSION_PAGE;
				goto do_fault;
			}
		}
		if ((sld & 3) == 1) {
			sld = ((sld & 0xffff0fff) | (addr & 0xf000));
		}
		addr = (sld & 0xfffff000) | (addr & 0xfff);
		cp15_tlb_add_entry(oa, addr);
		return addr;

	case 2: /* Section */
		domain_access = checkdomain(domain);
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
		addr = (fld & 0xfff00000) | (addr & 0xfffff);
		cp15_tlb_add_entry(oa, addr);
		return addr;

	default:
		fatal("Bad descriptor type %u %08x Address %08x\n", fld & 3, fld, addr);
	}
	exit(-1);

do_fault:
	armirq |= 0x40;
	if (!prefetch) {
		cp15.fault_address = addr;
		cp15.fault_status = (domain << 4) | fault_code;
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
                addr2=translateaddress(addr,0,1);
                if (armirq&0x40)
                {
                        armirq&=~0x40;
                        armirq|=0x80;
//                        databort=0;
//                        prefabort=1;
                        return NULL;
                }
        }
        else     addr2=addr;
        /*Invalidate write pointer for this page - so we can handle code modification*/
        vwaddrl[addr>>12]=0xFFFFFFFF;

        switch (addr2&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                return &rom[((long)(addr2&0x7FF000)-(long)addr)>>2];
                case 0x02000000: /*VRAM*/
                return &vram[((long) (addr2 & config.vrammask) - (long) addr) >> 2];
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                return &ram00[((long) (addr2 & mem_rammask) - (long) addr) >> 2];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                return &ram01[((long) (addr2 & mem_rammask) - (long) addr) >> 2];
                case 0x18000000: /*SIMM 1 bank 0*/
                case 0x19000000:
                case 0x1a000000:
                case 0x1b000000:
                case 0x1c000000: /*SIMM 1 bank 1*/
                case 0x1d000000:
                case 0x1e000000:
                case 0x1f000000:
                if (ram1 != NULL) {
                	return &ram1[((long) (addr2 & 0x7ffffff) - (long) addr) >> 2];
                }
        }
        fatal("Bad PC %08X %08X\n", addr, addr2);
}
