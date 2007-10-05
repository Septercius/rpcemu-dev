#ifndef __SOUND__
#define __SOUND__

extern void initsound();
extern void closesound();
extern int getsamplefreq();
extern void changesamplefreq(int newsamplefreq);
extern int soundenabled;
extern int soundbufferfull;
extern void updatesoundirq();
extern int updatesoundbuffer();
extern int getbufferlen();
extern uint32_t soundaddr[4];
extern int samplefreq;
extern int soundinited,soundlatch,soundcount;
extern void stopsound();
extern void continuesound();

#endif //__SOUND__
