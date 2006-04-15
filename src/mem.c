/*RPCemu v0.3 by Tom Walker
  Memory handling*/
#include "rpc.h"

uint32_t *ram,*ram2,*rom,*vram;
uint8_t *ramb,*ramb2,*romb,*vramb;
uint8_t *dirtybuffer;

uint32_t raddrl;
uint32_t *raddrl2;

int mmu,memmode;

static uint32_t readmemcache,readmemcache2;
static uint32_t writememcache,writememcache2;
//static int timetolive;

void initmem(void)
{
        ram=(uint32_t *)malloc(32*1024*1024);
        ram2=(uint32_t *)malloc(32*1024*1024);        
        rom=(uint32_t *)malloc(8*1024*1024);
        vram=(uint32_t *)malloc(2*1024*1024);
        ramb=(unsigned char *)ram;
        ramb2=(unsigned char *)ram2;
        romb=(unsigned char *)rom;
        vramb=(unsigned char *)vram;
        dirtybuffer=(unsigned char *)malloc(131072);
        memset(ram,0,2*1024*1024);
        memset(ram2,0,2*1024*1024);        
        raddrl=0xFFFFFFFF;
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
//        error("RAMsize now %i\n",ramsize);
}

void resetmem(void)
{
        readmemcache=0xFFFFFFFF;
        writememcache=0xFFFFFFFF;
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
                        if (databort) return 0;
                        readmemcache2=addr&0xFFFFF000;
//                        rpclog("MMU addr %08X %08X %08X %08X ",addr2,addr,raddrl,readmemcache2&0x7FF000);
                        switch (readmemcache2&0x1F000000)
                        {
                                case 0x00000000: /*ROM*/
                                raddrl2=&rom[(readmemcache2&0x7FF000)>>2];
//                                rpclog("ROM %08X %08X\n",raddrl2,rom);
                                break;
                                case 0x02000000: /*VRAM*/
//                                rpclog("VRAM %08X %07X\n");
                                raddrl2=&vram[(readmemcache2&0x1FF000)>>2]; break;
                                case 0x10000000: /*SIMM 0 bank 0*/
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                raddrl2=&ram[(readmemcache2&rammask)>>2];
                                break;
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                raddrl2=&ram2[(readmemcache2&rammask)>>2];
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
                switch (addr&0x1F000000)
                {
                       case 0x00000000: /*ROM*/
//                       rpclog("ROM\n");
                       raddrl2=&rom[(addr&0x7FF000)>>2]; 
                       break;
                       case 0x02000000: /*VRAM*/
//                       rpclog("VRAM\n");
                       raddrl2=&vram[(addr&0x1FF000)>>2];
                       break;
                       case 0x10000000: /*SIMM 0 bank 0*/
                       case 0x11000000:
                       case 0x12000000:
                       case 0x13000000:
                       raddrl2=&ram[((addr&rammask)&~0xFFF)>>2]; break;
                       case 0x14000000: /*SIMM 0 bank 1*/
                       case 0x15000000:
                       case 0x16000000:
                       case 0x17000000:
                       raddrl2=&ram2[((addr&rammask)&~0xFFF)>>2]; break;
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
                return ramb[addr&rammask];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:                
                return ramb2[addr&rammask];
        }
        return 0;
/*        error("Bad readmemb %08X at %07X\n",addr,PC);
        dumpregs();
        exit(-1);*/
}

void writememl(uint32_t addr, uint32_t val)
{
//        if (addr>0x2104F1E && addr<0x2104F28) printf("Writel %08X %08X %08X\n",addr,val,PC);
//        if (addr>0x214EF03 && addr<0x214EF24) printf("Writel %08X %08X %08X\n",addr,val,PC);
//        if (addr==0x2104F08) printf("Write 2104F08 %08X %08X\n",val,PC);
//        if (addr==0x24) printf("Write 24 %08X %08X\n",val,PC);
//        if (addr>=0x2154BC4 && addr<=(0x2154BC4+0x3C)) printf("Write %08X %08X %07X\n",addr,val,PC);
        if (mmu)
        {
                if ((addr>>12)==writememcache) addr=(addr&0xFFF)+writememcache2;
                else
                {
                        writememcache=addr>>12;
                        addr=translateaddress(addr,1);
                        if (databort) return;
                        writememcache2=addr&0xFFFFF000;
//                        rpclog("MMU waddr %08X %08X %08X\n",addr,ram,&ram[((addr&rammask)&0xFFFFF000)>>2]);
                }
        }
        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
                if (!vrammask) return;
//                rpclog("Write VRAM %08X %07X - %08X\n",addr,PC,val);
                dirtybuffer[(addr&vrammask)>>10]=1;
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
//                printf("SIMM0 w %08X %08X %07X\n",addr,val,PC);
                ram[(addr&rammask)>>2]=val;
                if (!model || (vrammask==0 && (addr&rammask)<0x100000))
                   dirtybuffer[(addr&rammask)>>10]=1;
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

void writememb(uint32_t addr, uint8_t val)
{
/*        if (addr>0x2104F1E && addr<0x2104F28)
        {
                printf("Writeb %08X %02X %08X\n",addr,val,PC);
//                timetolive=300;
        }
        if (addr>0x214EF03 && addr<0x214EF24) printf("Writeb %08X %02X %08X\n",addr,val,PC);*/
//        if (addr==0x2104F08) printf("Writeb 2104F08 %02X %08X\n",val,PC);
//        if (addr==0x2104F2D) printf("Writeb 2104F2D %02X %08X\n",val,PC);
        if (mmu)
        {
                if ((addr>>12)==writememcache) addr=(addr&0xFFF)+writememcache2;
                else
                {
                        writememcache=addr>>12;
                        addr=translateaddress(addr,1);
                        if (databort) return;
                        writememcache2=addr&0xFFFFF000;
                }
        }
        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
                if (!vrammask) return;
                dirtybuffer[(addr&vrammask)>>10]=1;
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
                ramb[addr&rammask]=val;
                if (!model || (vrammask==0 && (addr&rammask)<0x100000))
                   dirtybuffer[(addr&rammask)>>10]=1;
                return;
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                ramb2[addr&rammask]=val;
//                dirtybuffer[(addr&rammask)>>10]=1;
                return;
        }
/*        error("Bad writememb %08X %02X at %07X\n",addr,val,PC);
        dumpregs();
        exit(-1);*/
}

