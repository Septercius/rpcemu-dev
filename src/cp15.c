/*RPCemu v0.3 by Tom Walker
  System coprocessor + MMU emulation*/
#include "rpc.h"

uint32_t ins;
uint32_t tlbcache[16384],tlbcache2[64];
int tlbcachepos;
int tlbs,flushes;
uint32_t pccache,readcache,writecache;
uint32_t opcode;
struct cp15
{
        uint32_t tlbbase,dacr;
        uint32_t far,fsr,ctrl;
} cp15;

void resetcp15(void)
{
        int c;
        prog32=1;
        mmu=0;
        memset(tlbcache,0xFF,16384*4);
        for (c=0;c<64;c++)
            tlbcache2[c]=0xFFFFFFFF;
        tlbcachepos=0;
}

int translations=0;
uint32_t lastcache;
static uint32_t *tlbram;
uint32_t tlbrammask;

void writecp15(uint32_t addr, uint32_t val)
{
        int c;
        switch (addr&15)
        {
                case 1: /*Control*/
                cp15.ctrl=val;
                mmu=val&1;
                if (!mmu)
                   raddrl=0xFFFFFFFF;
                prog32=val&0x10;
                if (!prog32 && (mode&16))
                {
                        updatemode(mode&15);
                }
                return; /*We can probably ignore all other bits*/
                case 2: /*TLB base*/
                cp15.tlbbase=val&~0x3FFF;
                raddrl=0xFFFFFFFF;
                switch (cp15.tlbbase&0x1F000000)
                {
                        case 0x02000000: /*VRAM - yes RiscOS 3.7 does put the TLB in VRAM at one point*/
                        tlbram=vram; tlbrammask=vrammask; break;
                        case 0x10000000: /*SIMM 0 bank 0*/
                        case 0x11000000:
                        case 0x12000000:
                        case 0x13000000:
                        tlbram=ram; tlbrammask=rammask; break;
                        case 0x14000000: /*SIMM 0 bank 1*/
                        case 0x15000000:
                        case 0x16000000:
                        case 0x17000000:
                        tlbram=ram2; tlbrammask=rammask; break;
                }
//                printf("CP15 tlb base now %08X\n",cp15.tlbbase);
                return;
                case 3: /*Domain access control*/
                cp15.dacr=val;
//                printf("CP15 DACR now %08X\n",cp15.dacr);
                return;
                case 8: /*Flush TLB (SA110)*/
//                if (model==3) rpclog("TLB purge %08X %01X %i\n",val,opcode&0xF,(opcode>>5)&7);
                case 6: /*Purge TLB*/
                case 5: /*Flush TLB*/
//                rpclog("TLB flush %08X %08X %07X %i %i %08X\n",addr,val,PC,ins,translations,lastcache);
                pccache=readcache=writecache=0xFFFFFFFF;
                raddrl=0xFFFFFFFF;
                for (c=0;c<64;c++)
                    if (tlbcache2[c] != 0xFFFFFFFF) tlbcache[tlbcache2[c]] = 0xFFFFFFFF;
                for (c=0;c<64;c++)
                    tlbcache2[c]=0xFFFFFFFF;
                flushes++;
                tlbcachepos=0;
/*                if (!translations) return;
                if (translations==1)
                   tlbcache[lastcache>>12]=0xFFFFFFFF;
                else
                {
                        memset(tlbcache,0xFF,16384*4);
                        flushes++;
                }
                translations=0;
//                for (c=0;c<16384;c++)
//                    tlbcache[c]=0xFFFFFFFF;*/
                return;
                case 7: /*Invalidate cache*/
//                rpclog("Cache invalidate %08X\n",PC);
                pccache=readcache=writecache=0xFFFFFFFF;
                raddrl=0xFFFFFFFF;
                return;
        }
//        error("Bad write CP15 %08X %08X %07X\n",addr,val,PC);
//        dumpregs();
//        exit(-1);
}

uint32_t readcp15(uint32_t addr)
{
        switch (addr&15)
        {
                case 0: /*ARM ID*/
                switch (model)
                {
                        case 0: return 0x41007100; /*ARM7500*/
                        case 1: return 0x41560610; /*ARM610*/
                        case 2: return 0x41567100; /*ARM710*/
                        case 3: return 0x4456A100; /*StrongARM*/
                }
                break;
                case 1: /*Control*/
                return cp15.ctrl;
                case 2: /*???*/
                return cp15.tlbbase;
                case 3: /*DACR*/
                return cp15.dacr;
                case 5: /*Fault status*/
                return cp15.fsr;
                case 6: /*Fault address*/
                return cp15.far;
        }
        error("Bad read CP15 %08X %07X\n",addr,PC);
        dumpregs();
        exit(-1);
}

#define FAULT()         databort=1;          \
                        cp15.far=addr;       \
                        cp15.fsr=fsr;        \
                        return 0xFFFFFFFF

int checkpermissions(int p, int fsr, int rw, uint32_t addr)
{
        switch (p)
        {
                case 0:
                switch (cp15.ctrl&0x300)
                {
                        case 0x000: /*No access*/
                        case 0x300: /*Unpredictable*/
                        FAULT();
                        case 0x100: /*Supervisor read-only*/
                        if (!model || rw) { FAULT(); }
                        break;
                        case 0x200: /*Read-only*/
                        if (rw) { FAULT(); }
                        break;
                }
                break;
                case 1: /*Supervisor only*/
                if (!memmode) { FAULT(); }
                break;
                case 2: /*User read-only*/
                if (!memmode && rw) { FAULT(); }
                break;
        }
        return 0;
}

uint32_t translateaddress(uint32_t addr, int rw)
{
        uint32_t vaddr=((addr>>18)&~3)|cp15.tlbbase;
        uint32_t fld;
        uint32_t sldaddr,sld; //,taddr;
        uint32_t oa=addr;
        int temp,temp2;
//        rpclog("Translate %08X ",addr);
        if (!(addr&0xFC000000) && !(tlbcache[(addr>>12)&0x3FFF]&0xFFF))
        {
//                rpclog("Cached %08X\n",tlbcache[addr>>12]);
                return tlbcache[addr>>12]|(addr&0xFFF);
        }
        translations++;
//        rpclog("Uncached ");
        tlbs++;
        rw = rw;
        fld=tlbram[(vaddr>>2)&tlbrammask];
        switch (fld&3)
        {
                case 0: /*Fault*/
                databort=1;
                cp15.far=addr;
                cp15.fsr=5;
                return 0;
                case 1: /*Page table*/
                sldaddr=((addr&0xFF000)>>10)|(fld&0xFFFFFC00);
                if ((sldaddr&0x1F000000)==0x02000000)
                   sld=vram[(sldaddr>>2)&vrammask];
                else if (sldaddr&0x4000000)
                   sld=ram2[(sldaddr>>2)&rammask];
                else
                   sld=ram[(sldaddr>>2)&rammask];
                temp=(addr&0xC00)>>9;
                temp2=sld&(0x30<<temp);
                temp2>>=(4+temp);
                if (checkpermissions(temp2,15,rw,addr))
                   return 0xFFFFFFFF;
                addr=(sld&0xFFFFF000)|(addr&0xFFF);
                if (!(oa&0xFC000000))
                {
                        if (tlbcache2[tlbcachepos]!=0xFFFFFFFF)
                           tlbcache[tlbcache2[tlbcachepos]]=0xFFFFFFFF;
                        tlbcache2[tlbcachepos]=oa>>12;
                        tlbcache[oa>>12]=sld&0xFFFFF000;
                        lastcache=oa>>12;
//                        rpclog("Cached to %08X %08X %08X %i  ",oa>>12,tlbcache[oa>>12],tlbcache2[tlbcachepos],tlbcachepos);
                        tlbcachepos=(tlbcachepos+1)&63;
                }
//                rpclog("%08X %08X %08X %08X\n",addr,sld,oa,tlbcache[oa>>12]);
                return addr;
                case 2: /*Section*/
                if (checkpermissions((fld&0xC00)>>10,13,rw,addr))
                   return 0xFFFFFFFF;                
                addr=(addr&0xFFFFF)|(fld&0xFFF00000);
                if (!(oa&0xFC000000))
                {
                        if (tlbcache2[tlbcachepos]!=0xFFFFFFFF)
                           tlbcache[tlbcache2[tlbcachepos]]=0xFFFFFFFF;
                        tlbcache2[tlbcachepos]=oa>>12;
                        tlbcache[oa>>12]=addr&0xFFFFF000;//sld&0xFFFFF000;
                        lastcache=oa>>12;
                        tlbcachepos=(tlbcachepos+1)&63;
//                        rpclog("Cached to %08X %08X %08X %i  ",oa>>12,tlbcache[oa>>12],tlbcache2[tlbcachepos],tlbcachepos);
/*                        tlbcache[oa>>12]=addr&0xFFFFF000;
                        lastcache=oa>>12;*/
                }
//                rpclog("%08X %08X %08X %08X\n",addr,oa,tlbcache[oa>>12],fld);
                return addr;
                default:
                error("Bad descriptor type %i %08X\n",fld&3,fld);
                error("Address %08X\n",addr);
                dumpregs();
                exit(-1);
        }
        exit(-1);
}

uint32_t *getpccache(uint32_t addr)
{
        uint32_t addr2;
        if (mmu)
        {
//                rpclog("Translate prefetch %08X ",addr);
                addr2=translateaddress(addr,0);
                if (databort)
                {
                        databort=0;
                        prefabort=1;
                        return NULL;
                }
        }
        else     addr2=addr;
        switch (addr2&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                return &rom[(addr2&0x7FF000)>>2];
                case 0x02000000: /*VRAM*/
                return &vram[(addr2&0x1FF000)>>2];
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram[((addr2&rammask)&~0xFFF)>>2];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram2[((addr2&rammask)&~0xFFF)>>2];
        }
        error("Bad PC %08X %08X\n",addr,addr2);
        dumpregs();
        exit(-1);
}
