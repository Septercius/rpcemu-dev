/*RPCemu v0.5 by Tom Walker
  Memory handling*/
#include "rpcemu.h"


uint32_t *ram,*ram2,*rom,*vram;
uint8_t *ramb,*ramb2,*romb,*vramb;
uint8_t *dirtybuffer;

uint32_t raddrl;
uint32_t *raddrl2;
uint32_t waddrl;
uint32_t *waddrl2;

int mmu,memmode;

static uint32_t readmemcache,readmemcache2;
#ifdef LARGETLB
static uint32_t writememcache[128],writememcache2[128];
#else
static uint32_t writememcache,writememcache2;
#endif
static uint32_t writemembcache,writemembcache2;

//static int timetolive;

void clearmemcache(void)
{
#ifdef LARGETLB
        int c;
        for (c=0;c<128;c++)
            writememcache[c]=0xFFFFFFFF;
#else
        writememcache=0xFFFFFFFF;
#endif
        readmemcache=0xFFFFFFFF;
}

void initmem(void)
{
        ram=(uint32_t *)malloc(2*1024*1024);
        ram2=(uint32_t *)malloc(2*1024*1024);
        rom=(uint32_t *)malloc(8*1024*1024);
        vram=(uint32_t *)malloc(2*1024*1024);
        ramb=(unsigned char *)ram;
        ramb2=(unsigned char *)ram2;
        romb=(unsigned char *)rom;
        vramb=(unsigned char *)vram;
        dirtybuffer=(unsigned char *)malloc(512);
        memset(ram,0,2*1024*1024);
        memset(ram2,0,2*1024*1024);
        raddrl=waddrl=0xFFFFFFFF;
}

void reallocmem(int ramsize)
{
        free(ram);
        free(ram2);
        ram=(uint32_t *)malloc(ramsize);
        ramb=(unsigned char *)ram;
        ram2=(uint32_t *)malloc(ramsize);
        ramb2=(unsigned char *)ram2;
        memset(ram,0,ramsize);
        memset(ram2,0,ramsize);
//        error("RAMsize now %08X RAMmask now %08X\n",ramsize,rammask);
}

void resetmem(void)
{
#ifdef LARGETLB
	int c;
        for (c=0;c<64;c++)
            writememcache[c]=0xFFFFFFFF;
#else
        writememcache=0xFFFFFFFF;
#endif
        readmemcache=0xFFFFFFFF;
}

uint32_t readmemfl(uint32_t addr)
{
        uint32_t addr2;

//        if (!addr && !timetolive) timetolive=250;
        if (mmu)
        {
                addr2=addr;
                if ((addr>>12)==readmemcache) addr=(addr&0xFFF)+readmemcache2;
                else
                {
                        readmemcache=addr>>12;
                        raddrl=addr&0xFFFFF000;
//                        rpclog("Translate read ");
                        addr=translateaddress(addr,0);
                        if (databort) rpclog("Dat abort reading %08X %08X\n",addr2,PC);
                        if (databort) return 0;
                        readmemcache2=addr&0xFFFFF000;
//                        rpclog("MMU addr %08X %08X %08X %08X ",addr2,addr,raddrl,readmemcache2&0x7FF000);
                        switch (readmemcache2&0x1F000000)
                        {
                                case 0x00000000: /*ROM*/
                                raddrl2=&rom[((readmemcache2&0x7FF000)-raddrl)>>2];
//                                rpclog("ROM %08X %08X\n",raddrl2,rom);
                                break;
                                case 0x02000000: /*VRAM*/
//                                rpclog("VRAM %08X %07X\n");
                                raddrl2=&vram[((readmemcache2&0x1FF000)-raddrl)>>2]; break;
                                case 0x10000000: /*SIMM 0 bank 0*/
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                raddrl2=&ram[((readmemcache2&rammask)-raddrl)>>2];
                                break;
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                raddrl2=&ram2[((readmemcache2&rammask)-raddrl)>>2];
                                break;
                                default:
//                                rpclog("Other\n");
                                raddrl=0xFFFFFFFF;
                        }
                }
        }
        else
        {
                raddrl=addr&0xFFFFF000;
//                rpclog("No MMU addr %08X %08X ",addr,raddrl);
                switch (raddrl&0x1F000000)
                {
                       case 0x00000000: /*ROM*/
//                       rpclog("ROM\n");
                       raddrl2=&rom[((raddrl&0x7FF000)-raddrl)>>2];
                       break;
                       case 0x02000000: /*VRAM*/
//                       rpclog("VRAM\n");
                       raddrl2=&vram[((raddrl&0x1FF000)-raddrl)>>2];
                       break;
                       case 0x10000000: /*SIMM 0 bank 0*/
                       case 0x11000000:
                       case 0x12000000:
                       case 0x13000000:
                       raddrl2=&ram[((raddrl&rammask)-raddrl)>>2]; break;
                       case 0x14000000: /*SIMM 0 bank 1*/
                       case 0x15000000:
                       case 0x16000000:
                       case 0x17000000:
                       raddrl2=&ram2[((raddrl&rammask)-raddrl)>>2]; break;
                       default:
//                       rpclog("Other\n");
                       raddrl=0xFFFFFFFF;
                }
        }
        if ((addr&0x1C000000)==0x10000000)
           return ram[(addr&rammask)>>2];
        switch (addr&0x1F000000)
        {
            case 0x00000000: /*ROM*/
                return rom[(addr&0x7FFFFF)>>2];
            case 0x01000000: /*Extension ROM*/
                return 0;
            case 0x02000000: /*VRAM*/
                if (!vrammask || !model) return 0xFFFFFFFF;
//                rpclog("Read VRAM %08X %07X - %08X\n",addr,PC,vram[(addr&vrammask)>>2]);
//              if (vram[(addr&vrammask)>>2]==0x6E756F66) output=1;
                return vram[(addr&vrammask)>>2];

            case 0x03000000: /*IO*/
                if (addr==0x3310000)
                   return readmb();
//                rpclog("Read IO %08X %08X\n",addr,PC);
                if ((addr&0xFF0000)==0x200000)
                   return readiomd(addr);
                if (addr==0x3310000) return 0;
                if ((addr&0xFFF000)==0x10000) /*82c711*/
                {
                        if ((addr&0xFFF)==0x7C0) return readidew();
                        return read82c711(addr);
                }
                break;

                case 0x0C000000: /*???*/
                return 0xFFFFFFFF;                
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:

//                printf("SIMM0 r %08X %08X %07X\n",addr,ram[(addr&0x3FFFFF)>>2],PC);
                return ram[(addr&rammask)>>2];

                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                return ram2[(addr&rammask)>>2];
                case 0x18000000: /*SIMM 1 bank 0*/
                case 0x1C000000: /*SIMM 1 bank 1*/
                return 0;
        }
        return 0;
/*        error("Bad readmeml %08X %08X at %07X\n",addr,addr&0x1F000000,PC);
        dumpregs();
        exit(-1);*/
}


uint32_t mem_getphys(uint32_t addr)
{
        if (mmu)
        {
                if ((addr>>12) == readmemcache) addr=(addr&0xFFF)+readmemcache2;
                else
                {
                        readmemcache = addr >> 12;
                        addr = translateaddress(addr,0);
                        readmemcache2 = addr & 0xFFFFF000;
                }
        }
        return addr;
}



uint32_t readmemb(uint32_t addr)
{
        if (mmu)
        {
                if ((addr>>12)==readmemcache) addr=(addr&0xFFF)+readmemcache2;
                else
                {
                        readmemcache=addr>>12;
                        addr=translateaddress(addr,0);
                        if (databort) return 0;                        
                        readmemcache2=addr&0xFFFFF000;
                }
        }
        switch (addr&0x1F000000)
        {
                case 0x00000000: /*ROM*/
                return romb[addr&0x7FFFFF];
                case 0x02000000: /*VRAM*/
                if (!vrammask || !model) return 0xFF;
                return vramb[addr&vrammask];
                case 0x03000000: /*IO*/
                if (addr==0x3310000)
                   return readmb();
//                rpclog("Readb IO %08X %08X\n",addr,PC);
                if ((addr&0xFF0000)==0x200000)
                   return readiomd(addr);
                if (addr==0x3310000) return 0;
                if (addr>=0x3012000 && addr<=0x302A000)
                   return readfdcdma(addr);
                if ((addr&0xFFF000)==0x10000) /*82c711*/
                   return read82c711(addr);
                if ((addr&0xFF0000)==0x20000) /*82c711*/
                   return read82c711(addr);
                if ((addr&0xF00000)==0x300000) /*IO*/
                   return 0;
                if ((addr&0xFFF0000)==0x33A0000)
                {
//                        rpclog("Read Econet %08X %08X\n",addr,PC);
                        return 0xFF; /*Econet?*/
                }
                return 0;
                break;
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                return ramb[addr&rammask];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                return ramb2[addr&rammask];
        }
        return 0;
/*        error("Bad readmemb %08X at %07X\n",addr,PC);
        dumpregs();
        exit(-1);*/
}

#ifdef LARGETLB
void writememfl(uint32_t addrt, uint32_t val)
#else
void writememfl(uint32_t addr, uint32_t val)
#endif
{
#ifdef LARGETLB
        uint32_t addr;
#endif
//        uint32_t addr2=addr;
//        rpclog("Write %08X %08X %08X\n",addr,val,waddrl);
        if (mmu)
        {
                if ((addr>>12)==writememcache) addr=(addr&0xFFF)+writememcache2;
                else
                {
                        writememcache=addr>>12;
                        waddrl=addr&0xFFFFF000;
                        addr=translateaddress(addr,1);
                        if (databort) return;
                        writememcache2=addr&0xFFFFF000;
                        if ((addr&0x1F000000)==0x2000000)
                           dirtybuffer[(addr&vrammask)>>12]=2;
                        if (!vrammask && (addr&0x1FF00000)==0x10000000)
                           dirtybuffer[(addr&rammask)>>12]=2;
                        switch (writememcache2&0x1F000000)
                        {
                                case 0x02000000: /*VRAM*/
                                waddrl2=&vram[(writememcache2&0x1FF000)>>2];
                                break;
                                case 0x10000000: /*SIMM 0 bank 0*/
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                waddrl2=&ram[(writememcache2&rammask)>>2];
                                break;
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                waddrl2=&ram2[(writememcache2&rammask)>>2];
                                break;
                                default:
                                waddrl=0xFFFFFFFF;
                        }
                }
        }
        else
        {
                waddrl=addr&0xFFFFF000;
                switch (waddrl&0x1F000000)
                {
                        case 0x02000000: /*VRAM*/
                        waddrl2=&vram[(waddrl&0x1FF000)>>2];
                        break;
                        case 0x10000000: /*SIMM 0 bank 0*/
                        case 0x11000000:
                        case 0x12000000:
                        case 0x13000000:
                        waddrl2=&ram[(waddrl&rammask)>>2];
                        break;
                        case 0x14000000: /*SIMM 0 bank 1*/
                        case 0x15000000:
                        case 0x16000000:
                        case 0x17000000:
                        waddrl2=&ram2[(waddrl&rammask)>>2];
                        break;
                        default:
                        waddrl=0xFFFFFFFF;
                }
        }
/*        if (mmu)
        {
                #ifdef LARGETLB
                if ((addrt>>12)==writememcache[(addrt>>12)&127]) addr=(addrt&0xFFF)+writememcache2[(addrt>>12)&127];
                else
                {
                        writememcache[(addrt>>12)&127]=addrt>>12;
                        addr=translateaddress(addrt,1);
                        if (databort) return;
                        writememcache2[(addrt>>12)&127]=addr&0xFFFFF000;
                }
                #else
                if ((addr>>12)==writememcache) addr=(addr&0xFFF)+writememcache2;
                else
                {
                        writememcache=addr>>12;
                        addr=translateaddress(addr,1);
                        if (databort) return;
                        writememcache2=addr&0xFFFFF000;
                        if ((addr&0x1F000000)==0x2000000)
                           dirtybuffer[(addr&vrammask)>>12]=2;
                        if (!vrammask && (addr&0x1FF00000)==0x10000000)
                           dirtybuffer[(addr&rammask)>>12]=2;
                }
                #endif
        }*/
        #ifdef LARGETLB
        else
           addr=addrt;
        #endif
        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
//                if (!vrammask) return;
//                if (val && addr<=0x2025800) rpclog("Write VRAM %08X %07X - %08X\n",addr,PC,val);
#ifdef LARGETLB
                dirtybuffer[(addr&vrammask)>>12]=1;
#endif
                vram[(addr&vrammask)>>2]=val;
                return;

                case 0x03000000: /*IO*/
//                rpclog("Write IO %08X %08X %08X\n",addr,val,PC);
                if ((addr&0xFF0000)==0x200000)
                {
                        writeiomd(addr,val);
                        return;
                }
                if (addr==0x3400000) /*VIDC20*/
                {
                        writevidc20(val);
                        return;
                }
                if ((addr&0xFFF000)==0x10000) /*82c711*/
                {
                        if ((addr&0xFFF)==0x7C0)
                        {
                                writeidew(val);
                                return;
                        }
                        write82c711(addr,val);
                        return;
                }
                if ((addr&0xFFF0000)==0x33A0000)
                {
//                        rpclog("Write Econet %08X %08X %08X\n",addr,val,PC);
                        return; /*Econet?*/
                }
                break;

                case 0x0C000000: /*???*/
                return;

                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
//                if (addr>=0x10006000 && addr<=0x10007FFF)
//                   rpclog("Write %08X %08X\n",addr,val);
//                printf("SIMM0 w %08X %08X %07X\n",addr,val,PC);
                ram[(addr&rammask)>>2]=val;
//                if (!model || (vrammask==0 && (addr&rammask)<0x100000))
//                   dirtybuffer[(addr&rammask)>>12]=1;
//                dirtybuffer[(addr&rammask)>>10]=1;
                return;

                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                ram2[(addr&rammask)>>2]=val;
//                dirtybuffer[(addr&rammask)>>10]=1;
                return;
                case 0x18000000: /*SIMM 1 bank 0*/
                case 0x1C000000: /*SIMM 1 bank 1*/
                return;
        }
/*        error("Bad writememl %08X %08X at %07X\n",addr,val,PC);
        dumpregs();
        exit(-1);*/
}

#ifdef LARGETLB
void writememb(uint32_t addrt, uint8_t val)
#else
void writememb(uint32_t addr, uint8_t val)
#endif
{
        #ifdef LARGETLB
        uint32_t addr;
        #endif
        if (mmu)
        {
                #ifdef LARGETLB
                if ((addrt>>12)==writememcache[(addrt>>12)&127]) addr=(addrt&0xFFF)+writememcache2[(addrt>>12)&127];
                else
                {
                        writememcache[(addrt>>12)&127]=addrt>>12;
                        addr=translateaddress(addrt,1);
                        if (databort) return;
                        writememcache2[(addrt>>12)&127]=addr&0xFFFFF000;
                }
                #else
                if ((addr>>12)==writemembcache) addr=(addr&0xFFF)+writemembcache2;
                else
                {
                        writemembcache=addr>>12;
                        addr=translateaddress(addr,1);
                        if (databort) return;
                        writemembcache2=addr&0xFFFFF000;
                        if ((addr&0x1F000000)==0x2000000)
                           dirtybuffer[(addr&vrammask)>>12]=2;
                        if (!vrammask && (addr&0x1FF00000)==0x10000000)
                           dirtybuffer[(addr&rammask)>>12]=2;
                }
                #endif
        }
        #ifdef LARGETLB
        else
           addr=addrt;
        #endif
        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
//                if (!vrammask) return;
#ifdef LARGETLB
                dirtybuffer[(addr&vrammask)>>12]=1;
#endif
//                rpclog("Writeb VRAM %08X %07X - %02X %c\n",addr,PC,val,val);
                vramb[addr&vrammask]=val;
                return;
                case 0x03000000: /*IO*/
//                rpclog("Writeb IO %08X %02X %08X\n",addr,val,PC);
                if ((addr&0xFF0000)==0x200000)
                {
                        writeiomd(addr,val);
                        return;
                }
                if (addr==0x3310000) return;
                if ((addr&0xFC0000)==0x240000) return;
                if (addr>=0x3012000 && addr<=0x302A000)
                {
                        writefdcdma(addr,val);
                        return;
                }
                if ((addr&0xFF0000)==0x10000) /*82c711*/
                {
                        write82c711(addr,val);
                        return;
                }
                if ((addr&0xFF0000)==0x20000) /*82c711*/
                {
                        write82c711(addr,val);
                        return;
                }
                if ((addr&0xFFF0000)==0x33A0000)
                {
//                        rpclog("Write Econet %08X %08X %08X\n",addr,val,PC);
                        return; /*Econet?*/
                }
                break;
                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                ramb[addr&rammask]=val;
//                if (!model || (vrammask==0 && (addr&rammask)<0x100000))
//                   dirtybuffer[(addr&rammask)>>12]=1;
                return;
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                ramb2[addr&rammask]=val;
//                dirtybuffer[(addr&rammask)>>10]=1;
                return;
        }
/*        error("Bad writememb %08X %02X at %07X\n",addr,val,PC);
        dumpregs();
        exit(-1);*/
}

