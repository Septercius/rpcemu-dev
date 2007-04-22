//#define isblockvalid(l) (((l)&0xFFC00000)==0x3800000)
int icache;
#define isblockvalid(l) icache
#define BLOCKS 1024

//unsigned char codeblock[3][0x1000][1600];
unsigned char rcodeblock[BLOCKS][1792];
unsigned long codeblockaddr[BLOCKS];
unsigned long codeblockpc[0x8000];
unsigned char codeblockisrom[0x8000];
int codeblocknum[0x8000];
int codeinscount[0x8000];

//uint32_t blocks[1024];

#define HASH(l) (((l)>>2)&0x7FFF)
//#define callblock(l) (((codeblockpc[0][HASH(l)]==l)||(codeblockpc[1][HASH(l)]==l))?codecallblock(l):0)

//#define cacheclearpage(hash) waddrl=codeblockpc[hash][0]=codeblockpc[hash][1]=codeblockpc[hash][2]=0xFFFFFFFF; codeblockcount[hash]=0; ins++;
