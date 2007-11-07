void writepodulel(int num, uint32_t addr, uint32_t val);
void writepodulew(int num, uint32_t addr, uint16_t val);
void writepoduleb(int num, uint32_t addr, uint8_t val);
uint32_t  readpodulel(int num, uint32_t addr);
uint16_t readpodulew(int num, uint32_t addr);
uint8_t  readpoduleb(int num, uint32_t addr);

typedef struct
{
        void (*writeb)(struct podule *p, uint32_t addr, uint8_t val);
        void (*writew)(struct podule *p, uint32_t addr, uint16_t val);
        void (*writel)(struct podule *p, uint32_t addr, uint32_t val);
        uint8_t  (*readb)(struct podule *p, uint32_t addr);
        uint16_t (*readw)(struct podule *p, uint32_t addr);
        uint32_t (*readl)(struct podule *p, uint32_t addr);
        int (*timercallback)(struct podule *p);
        int irq,fiq;
        int msectimer;
} podule;

int addpodule(void (*writel)(podule *p, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, uint32_t addr, uint8_t val),
              uint32_t (*readl)(podule *p, uint32_t addr),
              uint16_t (*readw)(podule *p, uint32_t addr),
              uint8_t  (*readb)(podule *p, uint32_t addr),
              int (*timercallback)(struct podule *p));


