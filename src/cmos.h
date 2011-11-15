#ifndef __CMOS__
#define __CMOS__

extern void loadcmos(void);
extern void savecmos(void);
extern void reseti2c(void);
extern void cmosi2cchange(int nuclock, int nudata);

extern int i2cclock;
extern int i2cdata;

#endif //__CMOS__
