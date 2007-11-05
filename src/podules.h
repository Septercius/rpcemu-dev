void writepodulew(int num, unsigned short addr, unsigned short val);
void writepoduleb(int num, unsigned short addr, unsigned char val);
unsigned short readpodulew(int num, unsigned short addr);
unsigned char readpoduleb(int num, unsigned short addr);

typedef struct
{
        void (*writew)(struct podule *p, unsigned short addr, unsigned short val);
        void (*writeb)(struct podule *p, unsigned short addr, unsigned char val);
        unsigned short (*readw)(struct podule *p, unsigned short addr);
        unsigned char  (*readb)(struct podule *p, unsigned short addr);
} podule;
