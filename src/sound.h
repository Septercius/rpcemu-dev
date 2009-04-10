#ifndef __SOUND__
#define __SOUND__

extern void initsound(void);
extern void closesound(void);
extern int getsamplefreq(void);
extern void changesamplefreq(int newsamplefreq);
extern int soundenabled;
extern int soundbufferfull;
extern void updatesoundirq(void);
extern int updatesoundbuffer(void);
extern int getbufferlen(void);
extern uint32_t soundaddr[4];
extern int samplefreq;
extern int soundinited,soundlatch,soundcount;
extern void stopsound(void);
extern void continuesound(void);

#endif //__SOUND__
