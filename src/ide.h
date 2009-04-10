#ifndef __IDE__
#define __IDE__

extern void writeide(uint16_t addr, uint8_t val);
extern void writeidew(uint16_t val);
extern uint8_t readide(uint16_t addr);
extern uint16_t readidew(void);
extern void callbackide(void);
extern void resetide(void);

/*ATAPI stuff*/
typedef struct ATAPI
{
        int (*ready)(void);
        int (*readtoc)(unsigned char *b, unsigned char starttrack, int msf);
        uint8_t (*getcurrentsubchannel)(uint8_t *b, int msf);
        void (*readsector)(uint8_t *b, int sector);
        void (*playaudio)(uint32_t pos, uint32_t len);
        void (*seek)(uint32_t pos);
        void (*load)(void);
        void (*eject)(void);
        void (*pause)(void);
        void (*resume)(void);
        void (*stop)(void);
        void (*exit)(void);
} ATAPI;

extern ATAPI *atapi;

void atapi_discchanged(void);

extern int cdromenabled;
extern char isoname[512];
extern int ideboard;
#endif //__IDE__
