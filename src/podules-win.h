#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
        void (*writeb)(void *p, uint32_t addr, uint8_t val);
        void (*writew)(void *p, uint32_t addr, uint16_t val);
        void (*writel)(void *p, uint32_t addr, uint32_t val);
        uint8_t  (*readb)(void *p, uint32_t addr);
        uint16_t (*readw)(void *p, uint32_t addr);
        uint32_t (*readl)(void *p, uint32_t addr);
        int (*timercallback)(void *p);
        int irq,fiq;
        int msectimer;
} podule;
              
#ifdef __cplusplus
}
#endif
