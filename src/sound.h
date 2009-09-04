#ifndef __SOUND__
#define __SOUND__

extern void sound_init(void);

extern void sound_restart(void);
extern void sound_pause(void);

extern void sound_mute(void);
extern void sound_unmute(void);

extern void sound_samplefreq_change(int newsamplefreq);
extern void sound_irq_update(void);
extern int sound_buffer_update(void);

extern int soundbufferfull;
extern uint32_t soundaddr[4];
extern int soundinited, soundlatch, soundcount;

#endif //__SOUND__
