#ifndef __CMOS__
#define __CMOS__

extern void loadcmos(void);
extern void savecmos(void);
extern void reseti2c(void);
extern void cmosi2cchange(int nuclock, int nudata);

#endif //__CMOS__
