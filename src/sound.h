#ifndef __SOUND__
#define __SOUND__

extern void sound_init(void);
extern void closesound(void);
extern void changesamplefreq(int newsamplefreq);
extern int soundbufferfull;
extern void updatesoundirq(void);
extern int updatesoundbuffer(void);
extern uint32_t soundaddr[4];
extern int soundinited,soundlatch,soundcount;
extern void stopsound(void);
extern void continuesound(void);

#endif //__SOUND__
