#ifndef __CMOS__
#define __CMOS__

extern void loadcmos();
extern void savecmos();
extern void reseti2c(void);
extern void cmosi2cchange(int nuclock, int nudata);

#endif //__CMOS__
