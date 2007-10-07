/*RPCemu v0.6 by Tom Walker
  System coprocessor + MMU emulation*/
#include "rpcemu.h"
#include "mem.h"
#include "arm.h"

uint32_t oldpc = 0, oldpc2 = 0, oldpc3 = 0;
int icache = 0;

#define TLBCACHESIZE 256
int mmucount=0;
int pcisrom = 0;
uint32_t tlbcache[0x100000] = {0}, tlbcache2[TLBCACHESIZE] = {0};
unsigned long *vraddrl = 0;
uint32_t vraddrls[1024] = {0}, vraddrphys[1024] = {0};
unsigned long *vwaddrl = 0;
uint32_t vwaddrls[1024] = {0}, vwaddrphys[1024] = {0};
int tlbcachepos = 0;
int tlbs = 0, flushes = 0;
uint32_t pccache = 0, readcache , writecache = 0;
uint32_t opcode = 0;
struct cp15
{
        uint32_t tlbbase,dacr;
        uint32_t far,fsr,ctrl;
} cp15;

void getcp15fsr()
{
        rpclog("%08X %08X\n",cp15.far,cp15.fsr);
}

void resetcp15(void)
{
        int c;
        prog32=1;
        mmu=0;
        memset(tlbcache,0xFF,0x100000*4);
        for (c=0;c<TLBCACHESIZE;c++)
            tlbcache2[c]=0xFFFFFFFF;
        tlbcachepos=0;
        for (c=0;c<256;c++)
            raddrl[c]=0xFFFFFFFF;
        waddrl=0xFFFFFFFF;
        if (!vraddrl) vraddrl=malloc(0x100000*sizeof(uint32_t *));
        memset(vraddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vraddrls,0xFF,1024*sizeof(uint32_t));
        if (!vwaddrl) vwaddrl=malloc(0x100000*sizeof(uint32_t *));
        memset(vwaddrl,0xFF,0x100000*sizeof(uint32_t *));
        memset(vwaddrls,0xFF,1024*sizeof(uint32_t));
}

int translations=0;
uint32_t lastcache;
static uint32_t *tlbram;
uint32_t tlbrammask;

#define CRm (opcode&0xF)
#define OPC2 ((opcode>>5)&7)
void writecp15(uint32_t addr, uint32_t val, uint32_t opcode)
{
        int c;
        if (output) rpclog("Write CP15 %08X %08X %i %i %07X %i\n",addr,val,OPC2,CRm,PC,blockend);
        switch (addr&15)
        {
                case 1: /*Control*/
                cp15.ctrl=val;
                if (!icache && val&0x1000) resetcodeblocks();
                icache=val&0x1000;
                if (!(val&1)) { rpclog("MMU disable at %08X\n",PC); ins=0; }
/*                if (!mmu && val&1)
                {
                        if (mmucount)
                        {
                                rpclog("MMU count!\n");
                        }
                        mmucount++;
                }*/
                if (mmu!=(int)(val&1))
                {
                        for (c=0;c<256;c++)
                            raddrl[c]=0xFFFFFFFF;
                        waddrl=0xFFFFFFFF;
                        resetcodeblocks();
                        for (c=0;c<1024;c++)
                        {
                                if (vraddrls[c]!=0xFFFFFFFF)
                                {
                                        vraddrl[vraddrls[c]]=0xFFFFFFFF;
                                        vraddrls[c]=0xFFFFFFFF;
                                        vraddrphys[c]=0xFFFFFFFF;
                                }
                                if (vwaddrls[c]!=0xFFFFFFFF)
                                {
                                        vwaddrl[vwaddrls[c]]=0xFFFFFFFF;
                                        vwaddrls[c]=0xFFFFFFFF;
                                        vwaddrphys[c]=0xFFFFFFFF;
                                }
                        }
                }
                mmu=val&1;
                prog32=val&0x10;
                if (!prog32 && (mode&16))
                {
                        updatemode(mode&15);
                }
//                rpclog("CP15 control write %08X %08X %i\n",val,PC,blockend);
                return; /*We can probably ignore all other bits*/
                case 2: /*TLB base*/
                cp15.tlbbase=val&~0x3FFF;
                for (c=0;c<1024;c++)
                {
                        if (vraddrls[c]!=0xFFFFFFFF)
                        {
                                vraddrl[vraddrls[c]]=0xFFFFFFFF;
                                vraddrls[c]=0xFFFFFFFF;
                                vraddrphys[c]=0xFFFFFFFF;
                        }
                        if (vwaddrls[c]!=0xFFFFFFFF)
                        {
                                vwaddrl[vwaddrls[c]]=0xFFFFFFFF;
                                vwaddrls[c]=0xFFFFFFFF;
                                vwaddrphys[c]=0xFFFFFFFF;
                        }
                }
//                memset(raddrl,0xFF,1024);
//                resetcodeblocks();
//                for (c=0;c<256;c++)
//                    raddrl[c]=0xFFFFFFFF;
//                waddrl=0xFFFFFFFF;
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
                if ((CRm&1) && !(OPC2)) resetcodeblocks();
//                if (model==3) rpclog("TLB purge %08X %01X %i\n",val,opcode&0xF,(opcode>>5)&7);
                case 6: /*Purge TLB*/
                case 5: /*Flush TLB*/
//                else if (!icache) resetcodeblocks();
//                resetcodeblocks();
//                rpclog("TLB flush %08X %08X %07X %i %i %08X\n",addr,val,PC,ins,translations,lastcache);
                clearmemcache();
                pccache=readcache=writecache=0xFFFFFFFF;
                blockend=1;
                memset(raddrl,0xFF,1024);
//                for (c=0;c<256;c++)
//                    raddrl[c]=0xFFFFFFFF;
                waddrl=0xFFFFFFFF;
                for (c=0;c<TLBCACHESIZE;c++)
                {
                        if (tlbcache2[c]!=0xFFFFFFFF)
                        {
                                tlbcache[tlbcache2[c]]=0xFFFFFFFF;
//                                vraddrl[tlbcache2[c]]=0xFFFFFFFF;
                        }
                }
                for (c=0;c<1024;c++)
                {
                        if (vraddrls[c]!=0xFFFFFFFF)
                        {
                                vraddrl[vraddrls[c]]=0xFFFFFFFF;
                                vraddrls[c]=0xFFFFFFFF;
                                vraddrphys[c]=0xFFFFFFFF;
                        }
                        if (vwaddrls[c]!=0xFFFFFFFF)
                        {
                                vwaddrl[vwaddrls[c]]=0xFFFFFFFF;
                                vwaddrls[c]=0xFFFFFFFF;
                                vwaddrphys[c]=0xFFFFFFFF;
                        }
                }
                memset(tlbcache2,0xFF,TLBCACHESIZE*4);
//                for (c=0;c<64;c++)
//                    tlbcache2[c]=0xFFFFFFFF;
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
/*                for (c=0;c<1024;c++)
                {
                        if (vraddrls[c]!=0xFFFFFFFF)
                        {
                                vraddrl[vraddrls[c]]=0xFFFFFFFF;
                                vraddrls[c]=0xFFFFFFFF;
                                vraddrphys[c]=0xFFFFFFFF;
                        }
                }*/
                if ((CRm&1) && !(OPC2)) resetcodeblocks();
//                rpclog("Cache invalidate %08X\n",PC);
                pccache=readcache=writecache=0xFFFFFFFF;
//                blockend=1;
                memset(raddrl,0xFF,1024);
//                for (c=0;c<256;c++)
//                    raddrl[c]=0xFFFFFFFF;
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
                        case 3: /*if (PC>0x10000000) output=1; */return 0x4401A102; /*SA110*/
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
//                printf("Fault status read %08X\n",cp15.fsr);
                return cp15.fsr;
                case 6: /*Fault address*/
                //printf("Fault address read %08X\n",cp15.far);
                return cp15.far;
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
//#define output 1
#define FAULT()         armirq|=0x40;        \
                        if (!prefetch) \
                        { \
                                cp15.far=addr;       \
                                cp15.fsr=fsr;        \
                        } \
                        if (output) rpclog("PERMISSIONS FAULT! %08X %07X %08X %08X %08X %08X %i %03X %08X %08X %08X %08X\n",addr,PC,opcode,oldpc,oldpc2,oldpc3,p,cp15.ctrl&0x300,fld,sld,armregs[16],cp15.dacr);  \
                        return 0xFFFFFFFF

int checkpermissions(int p, int fsr, int rw, uint32_t addr, uint32_t fld, uint32_t sld, int prefetch)
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
//                        break; /*delibrately broken for Linux*/
                        /*Linux will crash very early on if this is implemented properly*/
                        if (!memmode || rw) { if (output) rpclog("Supervisor read only\n"); FAULT(); }
                        break;
                        case 0x200: /*Read-only*/
                        if (rw) { if (output) rpclog("Read only\n"); FAULT(); }
                        break;
                }
                break;
                case 1: /*Supervisor only*/
//                break;
                if (!memmode) { if (output) rpclog("Supervisor only\n"); FAULT(); }
                break;
                case 2: /*User read-only*/
                if (!memmode && rw) { if (output) rpclog("Read only\n"); FAULT(); }
                break;
        }
        return 0;
}

int checkdomain(uint32_t addr, int domain, int type, int prefetch)
{
        int temp=cp15.dacr>>(domain<<1);
        if (!(temp&3))
        {
                armirq|=0x40;
//                rpclog("Domain fault! %08X %i %i %i %08X\n",addr,domain,type,prefetch,temp);
//                if (addr==0x4F01180) { output=1; timetolive=500; }
                if (prefetch) return 0;
                cp15.far=addr;
                cp15.fsr=(type==1)?11:9;
//                rpclog("Domain fault\n");
        }
        return temp&3;
}
int prntrans=0;
uint32_t translateaddress2(uint32_t addr, int rw, int prefetch)
{
        uint32_t vaddr=((addr>>18)&~3)|cp15.tlbbase;
        uint32_t fld;
        uint32_t sldaddr,sld; //,taddr;
        uint32_t oa=addr;
        int temp,temp2,temp3;
        prntrans=0;
        if ((addr&~0xFFF)==0xF03B7000) { rpclog("Translate %08X!\n",addr); prntrans=1; }
        armirq&=~0x40;
//if (addr&0x80000000) printf("Translating %08X\n",addr);
/*        if (!(addr&0xFC000000) && !(tlbcache[(addr>>12)&0x3FFF]&0xFFF))
        {
//                rpclog("Cached %08X\n",tlbcache[addr>>12]);
                return tlbcache[addr>>12]|(addr&0xFFF);
        }*/
//        rpclog("Translate %08X\n",addr);
        translations++;
//        rpclog("Uncached ");
        tlbs++;

        rw = rw;
        fld=tlbram[(vaddr>>2)&tlbrammask];
        if (fld&3) temp3=checkdomain(addr,(fld>>5)&15,fld&3,prefetch);
//        rpclog("%08X %08X %08X\n",fld,temp3,vraddrl[0x4F000C0>>12]);
        switch (fld&3)
        {
                case 0: /*Fault*/
                armirq|=0x40;
                if (prefetch) return 0;
                cp15.far=addr;
                cp15.fsr=5;
                if (prntrans) rpclog("Fault!\n");
//                printf("Fault! %08X %07X %i\n",addr,PC,rw);
//                exit(-1);
                return 0;
                case 1: /*Page table*/
                if (!temp3) { return 0; }
                sldaddr=((addr&0xFF000)>>10)|(fld&0xFFFFFC00);
                if ((sldaddr&0x1F000000)==0x02000000)
                   sld=vram[(sldaddr&vrammask)>>2];
                else if (sldaddr&0x4000000)
                   sld=ram2[(sldaddr&rammask)>>2];
                else
                   sld=ram[(sldaddr&rammask)>>2];
                if (!(sld&3)) /*Unmapped*/
                {
                        if (prntrans) rpclog("Unmapped! %08X %07X %i\n",addr,PC,ins);
                        armirq|=0x40;
                        if (prefetch) return 0;
                        cp15.far=addr;
                        cp15.fsr=7|((fld>>1)&0xF0);
//                        output=1;
//                        timetolive=100;
//                        dumpregs();
//                        exit(-1);
                        return 0;
                }
/*                if ((sld&3)!=2)
                {
                        rpclog("Unsupported page size - %i %08X\n",sld&3,sld);
                        dumpregs();
                        exit(-1);
                }*/
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
                if (temp3!=3)
                {
                        if (checkpermissions(temp2,15,rw,addr,fld,sld,prefetch))
                        {
//                                if (output) rpclog("Failed permissions!\n");
                                return 0xFFFFFFFF;
                        }
                }
                if ((sld&3)==1) sld=((sld&0xFFFF0FFF)|(addr&0xF000));
                addr=(sld&0xFFFFF000)|(addr&0xFFF);
//                if (!(oa&0xFC000000))
//                {
                        if (tlbcache2[tlbcachepos]!=0xFFFFFFFF)
                           tlbcache[tlbcache2[tlbcachepos]]=0xFFFFFFFF;
                        tlbcache2[tlbcachepos]=oa>>12;
                        tlbcache[oa>>12]=sld&0xFFFFF000;
                        lastcache=oa>>12;
//                        rpclog("Cached to %08X %08X %08X %i  ",oa>>12,tlbcache[oa>>12],tlbcache2[tlbcachepos],tlbcachepos);
                        tlbcachepos=(tlbcachepos+1)&(TLBCACHESIZE-1);
//                }
                if (prntrans) rpclog("P %08X %08X %08X %08X\n",addr,sld,oa,tlbcache[oa>>12]);
                return addr;
                case 2: /*Section*/
                if (!temp3) { /*rpclog("Nothing here!\n");*/ return 0; }
                if (temp3!=3)
                {
                        if (checkpermissions((fld&0xC00)>>10,13,rw,addr,fld,0xFFFFFFFF,prefetch))
                        {
//                                if (output) rpclog("Failed permissions!\n");
                                return 0xFFFFFFFF;
                        }
                }
                addr=(addr&0xFFFFF)|(fld&0xFFF00000);
//                if (!(oa&0xFC000000))
//                {
                        if (tlbcache2[tlbcachepos]!=0xFFFFFFFF)
                           tlbcache[tlbcache2[tlbcachepos]]=0xFFFFFFFF;
                        tlbcache2[tlbcachepos]=oa>>12;
                        tlbcache[oa>>12]=addr&0xFFFFF000;//sld&0xFFFFF000;
                        lastcache=oa>>12;
                        tlbcachepos=(tlbcachepos+1)&(TLBCACHESIZE-1);
//                        rpclog("Cached to %08X %08X %08X %i  ",oa>>12,tlbcache[oa>>12],tlbcache2[tlbcachepos],tlbcachepos);
/*                        tlbcache[oa>>12]=addr&0xFFFFF000;
                        lastcache=oa>>12;*/
//                }
                if (prntrans) rpclog("S %08X %08X %08X %08X\n",addr,oa,tlbcache[oa>>12],fld);
                return addr;
                default:
                error("Bad descriptor type %i %08X\n",fld&3,fld);
                error("Address %08X\n",addr);
                dumpregs();
                exit(-1);
        }
        exit(-1);
}
//#undef output
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
//        if (PC==0x97F510) { rpclog("Getpccache... %02X %i\n",armirq,mmu); output=0; }
        addr&=~0xFFF;
        if (mmu)
        {
                armirq&=~0x40;
//                if (indumpregs) rpclog("Translate prefetch %08X %02X ",addr,armirq);
                addr2=translateaddress(addr,0,1);
                //output=0;
                if (armirq&0x40)
                {
//                        rpclog("Translate prefetch abort!!! %07X %07X %07X %07X\n",PC,oldpc,oldpc2,oldpc3);
//                        output=1;
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
        //output=0;
        pcisrom=0;
        switch (addr2&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                pcisrom=1;
                return &rom[((long)(addr2&0x7FF000)-(long)addr)>>2];
                case 0x02000000: /*VRAM*/
                return &vram[((long)(addr2&0x1FF000)-(long)addr)>>2];
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram[((long)(addr2&rammask)-(long)addr)>>2];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return &ram2[((long)(addr2&rammask)-(long)addr)>>2];
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
