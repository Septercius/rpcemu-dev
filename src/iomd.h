#ifndef __IOMD__
#define __IOMD__

struct iomd
{
        unsigned char stata,statb,statc,statd,state,statf;
        unsigned char maska,maskb,maskc,maskd,maske,maskf;
        unsigned char romcr0,romcr1;
        uint32_t vidstart,vidend,vidcur,vidinit;
        int t0l,t1l,t0c,t1c,t0r,t1r;
        unsigned char ctrl;
        unsigned char vidcr;
        unsigned char sndstat;
        unsigned char keydat;
        unsigned char msdat;
        int mousex,mousey;
};

extern struct iomd iomd;

extern  int delaygenirqleft, delaygenirq;

extern  int i2cclock,i2cdata;

extern  int kcallback,mcallback;

extern  uint32_t cinit;
extern  int fdccallback;
extern  int motoron;

extern int idecallback;

extern void resetiomd(void);
extern void endiomd(void);
extern uint32_t readiomd(uint32_t addr);
extern void writeiomd(uint32_t addr, uint32_t val);
extern uint8_t readmb(void);
extern void iomdvsync(int vsync);

extern char HOSTFS_ROOT[512];

extern char discname[2][260];

#endif //__IOMD__
