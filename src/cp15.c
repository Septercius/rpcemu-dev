/*RPCemu v0.5 by Tom Walker
  System coprocessor + MMU emulation*/
#include "rpcemu.h"

unsigned long oldpc,oldpc2,oldpc3;
int timetolive;

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
        for (c=0;c<256;c++)
            raddrl[c]=0xFFFFFFFF;
        waddrl=0xFFFFFFFF;
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
                {
                        for (c=0;c<256;c++)
                            raddrl[c]=0xFFFFFFFF;
                        waddrl=0xFFFFFFFF;
                }
                prog32=val&0x10;
                if (!prog32 && (mode&16))
                {
                        updatemode(mode&15);
                }
                return; /*We can probably ignore all other bits*/
                case 2: /*TLB base*/
                cp15.tlbbase=val&~0x3FFF;
                for (c=0;c<256;c++)
                    raddrl[c]=0xFFFFFFFF;
                waddrl=0xFFFFFFFF;
                switch (cp15.tlbbase&0x1F000000)
                {
                        case 0x02000000: /*VRAM - yes RiscOS 3.7 does put the TLB in VRAM at one point*/
                        tlbram=vram; tlbrammask=vrammask>>2; break;
                        case 0x10000000: /*SIMM 0 bank 0*/
                        case 0x11000000:
                        case 0x12000000:
                        case 0x13000000:
                        tlbram=ram; tlbrammask=rammask>>2; break;
                        case 0x14000000: /*SIMM 0 bank 1*/
                        case 0x15000000:
                        case 0x16000000:
                        case 0x17000000:
                        tlbram=ram2; tlbrammask=rammask>>2; break;
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
                clearmemcache();
                pccache=readcache=writecache=0xFFFFFFFF;
                for (c=0;c<256;c++)
                    raddrl[c]=0xFFFFFFFF;
                waddrl=0xFFFFFFFF;
                for (c=0;c<64;c++)
                    tlbcache[tlbcache2[c]]=0xFFFFFFFF;
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
                for (c=0;c<256;c++)
                    raddrl[c]=0xFFFFFFFF;
                waddrl=0xFFFFFFFF;
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
                        case 3: /*if (PC>0x10000000) output=1; */return 0x4401A100; /*SA110*/
//                      case 3: return 0x4456A100; /*StrongARM*/
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
                        if (output) \
                                rpclog("PERMISSIONS FAULT! %08X %07X %08X %08X %08X %08X\n",addr,PC,opcode,oldpc,oldpc2,oldpc3);  \
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
                        if (output) rpclog("Always fault\n");
                        FAULT();
                        case 0x100: /*Supervisor read-only*/
                        break; /*delibrately broken for Linux*/
                        /*Linux will crash very early on if this is implemented properly*/
                        if (!memmode || rw) { if (output) rpclog("Supervisor read only\n"); FAULT(); }
                        break;
                        case 0x200: /*Read-only*/
                        if (rw) { if (output) rpclog("Read only\n"); FAULT(); }
                        break;
                }
                break;
                case 1: /*Supervisor only*/
                if (!memmode) { if (output) rpclog("Supervisor only\n"); FAULT(); }
                break;
                case 2: /*User read-only*/
                if (!memmode && rw) { if (output) rpclog("Read only\n"); FAULT(); }
                break;
        }
        return 0;
}

uint32_t translateaddress2(uint32_t addr, int rw)
{
        uint32_t vaddr=((addr>>18)&~3)|cp15.tlbbase;
        uint32_t fld;
        uint32_t sldaddr,sld; //,taddr;
        uint32_t oa=addr;
        int temp,temp2;
//        rpclog("Translate %08X ",addr);
/*        if (!(addr&0xFC000000) && !(tlbcache[(addr>>12)&0x3FFF]&0xFFF))
        {
//                rpclog("Cached %08X\n",tlbcache[addr>>12]);
                return tlbcache[addr>>12]|(addr&0xFFF);
        }*/
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
//                rpclog("Fault! %08X %07X %i\n",addr,PC,rw);
//                exit(-1);
                return 0;
                case 1: /*Page table*/
                sldaddr=((addr&0xFF000)>>10)|(fld&0xFFFFFC00);
                if ((sldaddr&0x1F000000)==0x02000000)
                   sld=vram[(sldaddr&vrammask)>>2];
                else if (sldaddr&0x4000000)
                   sld=ram2[(sldaddr&rammask)>>2];
                else
                   sld=ram[(sldaddr&rammask)>>2];
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
//                rpclog("Translate prefetch %08X ",addr);
                addr2=translateaddress(addr,0);
                if (databort)
                {
                        databort=0;
                        prefabort=1;
                        return 0xFFFFFFFF;
                }
        }
        else     addr2=addr;
        switch (addr2&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                return &rom[((addr2&0x7FF000)-addr)>>2];
                case 0x02000000: /*VRAM*/
                return &vram[((addr2&0x1FF000)-addr)>>2];
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram[((addr2&rammask)-addr)>>2];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram2[((addr2&rammask)-addr)>>2];
        }
        error("Bad PC %08X %08X\n",addr,addr2);
        dumpregs();
        exit(-1);
}
