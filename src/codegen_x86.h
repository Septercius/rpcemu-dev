//#define isblockvalid(l) (((l)&0xFFC00000)==0x3800000)
#define isblockvalid(l) 1
unsigned char codeblock[2][0x2000][4096];
unsigned long codeblockpc[2][0x2000];
int codeinscount[2][0x2000];
unsigned char codeblockcount[0x2000];

#define HASH(l) (((l)>>3)&0x1FFF)
//#define callblock(l) (((codeblockpc[0][HASH(l)]==l)||(codeblockpc[1][HASH(l)]==l))?codecallblock(l):0)
