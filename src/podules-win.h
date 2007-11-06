typedef struct
{
        void (*writew)(struct podule *p, unsigned short addr, unsigned short val);
        void (*writeb)(struct podule *p, unsigned short addr, unsigned char val);
        unsigned short (*readw)(struct podule *p, unsigned short addr);
        unsigned char  (*readb)(struct podule *p, unsigned short addr);
} podule;

typedef int (*AddPodule)(void (*writew)(podule *p, unsigned short addr, unsigned short val),
                         void (*writeb)(podule *p, unsigned short addr, unsigned char val),
                         unsigned short (*readw)(podule *p, unsigned short addr),
                         unsigned char  (*readb)(podule *p, unsigned short addr));

