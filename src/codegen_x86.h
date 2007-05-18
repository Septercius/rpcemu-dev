//#define isblockvalid(l) (((l)&0xFFC00000)==0x3800000)

#define isblockvalid(l) icache
#define BLOCKS 1024

//unsigned char codeblock[3][0x1000][1600];
extern unsigned char rcodeblock[BLOCKS+4][1792+512+64];
extern unsigned long codeblockaddr[BLOCKS];
extern unsigned long codeblockpc[0x8000];
extern unsigned char codeblockisrom[0x8000];
extern int codeblocknum[0x8000];
extern int codeinscount[0x8000];

#define BLOCKSTART 12
//uint32_t blocks[1024];

#define HASH(l) (((l)>>2)&0x7FFF)
//#define callblock(l) (((codeblockpc[0][HASH(l)]==l)||(codeblockpc[1][HASH(l)]==l))?codecallblock(l):0)

//#define cacheclearpage(hash) waddrl=codeblockpc[hash][0]=codeblockpc[hash][1]=codeblockpc[hash][2]=0xFFFFFFFF; codeblockcount[hash]=0; ins++;
