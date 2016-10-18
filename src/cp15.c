/*RPCemu v0.6 by Tom Walker
  System coprocessor + MMU emulation*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpcemu.h"
#include "arm.h"
#include "cp15.h"
#include "mem.h"

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
#define CP15_CTRL_ABORT_TIMING		(1 << 6)  /* Always enabled in 710 & SA */
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
 * Invalidate Write-TLB entries corresponding to the given region of physical
 * addresses.
 *
 * @param addr Physical address
 */
void
cp15_tlb_invalidate_physical(uint32_t addr)
{
	int c;

	for (c = 0; c < 1024; c++) {
		if ((vwaddrphys[c] & 0x1f000000) == addr) {
			vwaddrl[vwaddrls[c]] = 0xffffffff;
			vwaddrls[c] = 0xffffffff;
			vwaddrphys[c] = 0xffffffff;
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
	switch (cpu_model) {
	case CPUModel_ARM610:
		cp15.ctrl = 0;
		break;
	case CPUModel_ARM710:
	case CPUModel_ARM7500:
	case CPUModel_ARM7500FE:
		cp15.ctrl = CP15_CTRL_ABORT_TIMING;
		break;
	case CPUModel_SA110:
	case CPUModel_ARM810:
		cp15.ctrl = CP15_CTRL_ABORT_TIMING | CP15_CTRL_DATA32 | CP15_CTRL_PROG32;
		break;
	}
	dcache = 0;
	icache = 0;
	mmu = 0;
	prog32 = (cp15.ctrl & CP15_CTRL_PROG32) != 0;

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
		if (!icache && (val & CP15_CTRL_ICACHE)) {
			resetcodeblocks();
		}

		/* Are any of the MMU, ROM or System bits changing? */
		if (((cp15.ctrl ^ val) & (CP15_CTRL_MMU | CP15_CTRL_ROM | CP15_CTRL_SYSTEM)) != 0) {
			cp15_tlb_flush_all();
			resetcodeblocks();
		}

		cp15.ctrl = val;
		dcache = val & CP15_CTRL_CACHE;
		icache = val & CP15_CTRL_ICACHE;
		mmu = val & CP15_CTRL_MMU;
		prog32 = val & CP15_CTRL_PROG32;

		if (!prog32 && (arm.mode & 0x10)) {
			updatemode(arm.mode & 0xf);
		}
		return;

	case 2: /* Translation Table Base */
		cp15.translation_table = val & ~0x3fffu;
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
		cp15_tlb_flush_all();
		resetcodeblocks();
		return;

	case 3: /* Domain Access Control */
		if (val != cp15.domain_access_control) {
			cp15.domain_access_control = val;
			cp15_tlb_flush_all();
			resetcodeblocks();
		}
		return;

	case 5:
	case 6:
		switch (cp15.cpu_model) {
		/* ARMv3 Architecture */
		case CPUModel_ARM610:
		case CPUModel_ARM710:
		case CPUModel_ARM7500:
		case CPUModel_ARM7500FE:
			switch (addr & 0xf) {
			case 5: /* TLB Flush */
				cp15_tlb_flush_all();
				break;

			case 6: /* TLB Purge */
				cp15_tlb_flush_all();
				break;
			}
			resetcodeblocks();
			return;

		/* ARMv4 Architecture */
		case CPUModel_SA110:
		case CPUModel_ARM810:
			switch (addr & 0xf) {
			case 5: /* Fault Status Register */
				cp15.fault_status = val;
				return;

			case 6: /* Fault Address Register */
				cp15.fault_address = val;
				return;
			}
			break;

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

	case 8: /* TLB Operations (ARMv4) */
		if (cp15.cpu_model == CPUModel_SA110 || cp15.cpu_model == CPUModel_ARM810) {
			if (OPC2 == 0) {
				/* TLB Flush */
				cp15_tlb_flush_all();
			} else {
				/* TLB Purge */
				cp15_tlb_flush_all();
			}
			if (CRm & 1) {
				resetcodeblocks();
			}
			return;
		}
		break;

	case 15:
		if (cp15.cpu_model == CPUModel_SA110) {
			/* Test, Clock and Idle control */
			if (OPC2 == 2 && CRm == 1) {
				/* Enable clock switching - no need to implement */
				return;
			}
		}
		break;
	}

	UNIMPLEMENTED("CP15 Write", "Register %u, opcode %08x", addr & 0xf, opcode);
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

/**
 * @param ap       Access Permissions (from Descriptor)
 * @param is_write Non-zero if this is for write access
 * @return Non-zero if the access should be faulted
 */
static int
cp15_check_permissions(uint32_t ap, int is_write)
{
	switch (ap) {
	case 0:
		switch (cp15.ctrl & 0x300) {
		case 0x000: /* No access */
		case 0x300: /* Unpredictable */
			return 1;

		case 0x100: /* Supervisor read-only */
			return !memmode || is_write;

		case 0x200: /* Read-only */
			return is_write;
		}
		break;

	case 1: /* Supervisor read/write */
		return !memmode;

	case 2: /* Supervisor read/write, User read-only*/
		return !memmode && is_write;
	}
	/* Any access permitted */
	return 0;
}

/**
 * Return the value which encodes the access permitted for a Domain.
 *
 * @param domain Domain number
 * @return Access permitted by Domain
 */
static uint32_t
cp15_domain_access(uint32_t domain)
{
	uint32_t shift = (domain << 1); /* Shift needed to extract value for this Domain */

	return (cp15.domain_access_control >> shift) & 3;
}

/**
 * Translate a virtual address to a physical address.
 *
 * The access permissions are checked and an Abort may be generated.
 *
 * @param addr     Virtual address
 * @param rw       Bool of whether this is for write access
 * @param prefetch Bool of whether this is for instruction fetch
 * @return Translated physical address (if no Fault occurred)
 */
uint32_t
translateaddress2(uint32_t addr, int rw, int prefetch)
{
	uint32_t fld_addr, fld;
	uint32_t sld_addr, sld;
	uint32_t domain, fault_code;
	uint32_t domain_access;
	uint32_t temp;
	uint32_t access_permissions;
	uint32_t phys_addr;

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
		sld = mem_phys_read32(sld_addr);

		/* Check second-level descriptor */
		switch (sld & 3) {
		case 1: /* Large page (64 KB) */
			temp = (addr & 0xc000) >> 13;
			phys_addr = (sld & 0xffff0000) | (addr & 0xffff);
			break;
		case 2: /* Small page (4 KB) */
			temp = (addr & 0xc00) >> 9;
			phys_addr = (sld & 0xfffff000) | (addr & 0xfff);
			break;
		default: /* 0 (Fault) or 3 (Reserved) */
			fault_code = CP15_FAULT_TRANSLATION_PAGE;
			goto do_fault;
		}

		/* Check Domain */
		domain_access = cp15_domain_access(domain);
		if (domain_access == 0 || domain_access == 2) {
			fault_code = CP15_FAULT_DOMAIN_PAGE;
			goto do_fault;
		}
		if (domain_access == 1) {
			/* Client Domain - check permissions */
			access_permissions = (sld >> (temp + 4)) & 3;
			if (cp15_check_permissions(access_permissions, rw)) {
				fault_code = CP15_FAULT_PERMISSION_PAGE;
				goto do_fault;
			}
		}
		cp15_tlb_add_entry(addr, phys_addr);
		return phys_addr;

	case 2: /* Section (1 MB) */
		/* Check Domain */
		domain_access = cp15_domain_access(domain);
		if (domain_access == 0 || domain_access == 2) {
			fault_code = CP15_FAULT_DOMAIN_SECTION;
			goto do_fault;
		}
		if (domain_access == 1) {
			/* Client Domain - check permissions */
			access_permissions = (fld >> 10) & 3;
			if (cp15_check_permissions(access_permissions, rw)) {
				fault_code = CP15_FAULT_PERMISSION_SECTION;
				goto do_fault;
			}
		}
		phys_addr = (fld & 0xfff00000) | (addr & 0xfffff);
		cp15_tlb_add_entry(addr, phys_addr);
		return phys_addr;

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


const uint32_t *
getpccache(uint32_t addr)
{
	uint32_t phys_addr;

	addr &= ~0xfffu;
	if (mmu) {
		armirq &= ~0x40u;
		phys_addr = translateaddress(addr, 0, 1);
		if (armirq & 0x40) {
			armirq &= ~0x40u;
			armirq |= 0x80;
			// databort = 0;
			// prefabort = 1;
			return NULL;
		}
	} else {
		phys_addr = addr;
	}

	/* Invalidate write pointer for this page - so we can handle code modification */
	vwaddrl[addr >> 12] = 0xffffffff;

	switch (phys_addr & 0x1f000000) {
	case 0x00000000: /* ROM */
		return &rom[((uintptr_t) (phys_addr & 0x7ff000) - (uintptr_t) addr) >> 2];
	case 0x02000000: /* VRAM */
		return &vram[((uintptr_t) (phys_addr & config.vrammask) - (uintptr_t) addr) >> 2];
	case 0x10000000: /* SIMM 0 bank 0 */
	case 0x11000000:
	case 0x12000000:
	case 0x13000000:
		return &ram00[((uintptr_t) (phys_addr & mem_rammask) - (uintptr_t) addr) >> 2];
	case 0x14000000: /* SIMM 0 bank 1 */
	case 0x15000000:
	case 0x16000000:
	case 0x17000000:
		return &ram01[((uintptr_t) (phys_addr & mem_rammask) - (uintptr_t) addr) >> 2];
	case 0x18000000: /* SIMM 1 bank 0 */
	case 0x19000000:
	case 0x1a000000:
	case 0x1b000000:
	case 0x1c000000: /* SIMM 1 bank 1 */
	case 0x1d000000:
	case 0x1e000000:
	case 0x1f000000:
		if (ram1 != NULL) {
			return &ram1[((uintptr_t) (phys_addr & 0x7ffffff) - (uintptr_t) addr) >> 2];
		}
	}
	fatal("Bad PC %08x %08x\n", addr, phys_addr);
}
