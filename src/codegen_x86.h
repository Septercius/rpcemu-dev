//#define isblockvalid(l) (((l)&0xFFC00000)==0x3800000)
int icache;
#define isblockvalid(l) icache
unsigned char codeblock[3][0x1000][1600];
unsigned long codeblockpc[0x1000][3];
int codeinscount[3][0x1000];
unsigned char codeblockcount[0x1000];

#define HASH(l) (((l)>>3)&0xFFF)
//#define callblock(l) (((codeblockpc[0][HASH(l)]==l)||(codeblockpc[1][HASH(l)]==l))?codecallblock(l):0)

//#define cacheclearpage(hash) waddrl=codeblockpc[hash][0]=codeblockpc[hash][1]=codeblockpc[hash][2]=0xFFFFFFFF; codeblockcount[hash]=0; ins++;
