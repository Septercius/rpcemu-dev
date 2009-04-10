#ifndef __KEYBOARD__
#define __KEYBOARD__

extern void resetkeyboard(void);
extern void keycallback(void);
extern void mscallback(void);
extern void writekbd(uint8_t v);
extern void writekbdenable(int v);
extern void writems(unsigned char v);
extern void writemsenable(int v);
extern unsigned char getkeyboardstat(void);
extern unsigned char readkeyboarddata(void);
extern unsigned char getmousestat(void);
extern unsigned char readmousedata(void);
extern void pollmouse(void);
extern void pollkeyboard(void);

extern void doosmouse(void);
extern void setmouseparams(uint32_t a);
extern void getunbufmouse(uint32_t a);
extern void setmousepos(uint32_t a);
extern void osbyte106(uint32_t a);
extern void setpointer(uint32_t a);
extern void getosmouse(void);
extern void getmousepos(int *x, int *y);

#endif //__KEYBOARD__
