#ifndef __IOMD__
#define __IOMD__

typedef struct {
        uint8_t status;
        uint8_t mask;
} iomd_irq;

typedef struct {
	uint32_t in_latch;
	int32_t  counter;
	uint32_t out_latch;
} iomd_timer;

struct iomd
{
        iomd_irq irqa;
        iomd_irq irqb;
        iomd_irq irqc;
        iomd_irq irqd;
        iomd_irq fiq;
        iomd_irq irqdma;
        unsigned char romcr0,romcr1;
        uint32_t vidstart,vidend,vidcur,vidinit;
        iomd_timer t0;
        iomd_timer t1;
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
extern uint8_t mouse_buttons_read(void);
extern void iomdvsync(int vsync);

extern char HOSTFS_ROOT[512];

extern char discname[2][260];

#endif //__IOMD__
