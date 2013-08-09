#ifndef __CMOS__
#define __CMOS__

extern void loadcmos(void);
extern void savecmos(void);
extern void reseti2c(uint32_t chosen_i2c_devices);
extern void cmosi2cchange(int nuclock, int nudata);

extern int i2cclock;
extern int i2cdata;

/** Values used in bitfield of I2C devices */
#define I2C_PCF8583	(1 << 0)

#endif //__CMOS__
