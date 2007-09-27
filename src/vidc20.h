#ifndef __VIDC20__
#define __VIDC20__

extern void initvideo();
extern void closevideo();
extern void blitterthread();
extern int getxs(void);
extern int getys(void);
extern void resetbuffer(void);
extern void writevidc20(uint32_t val);
extern void drawscr();
extern void togglefullscreen(int fs);
extern int refresh;
extern int skipblits;
extern int fullscreen;
extern int readflash;

#endif //__VIDC20__
