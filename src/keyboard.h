#ifndef __KEYBOARD__
#define __KEYBOARD__

extern void keyboard_reset(void);
extern void keyboard_callback_rpcemu(void);
extern void mouse_ps2_callback(void);
extern void keyboard_data_write(uint8_t v);
extern void keyboard_control_write(uint8_t v);
extern void mouse_data_write(uint8_t v);
extern void mouse_control_write(uint8_t v);
extern uint8_t keyboard_status_read(void);
extern uint8_t keyboard_data_read(void);
extern uint8_t mouse_status_read(void);
extern uint8_t mouse_data_read(void);
extern void mouse_poll(void);
extern void keyboard_poll(void);

extern void mouse_hack_osword_21_0(uint32_t a);
extern void mouse_hack_osword_21_1(uint32_t a);
extern void mouse_hack_osword_21_4(uint32_t a);
extern void mouse_hack_osbyte_106(uint32_t a);
extern void mouse_hack_osmouse(void);
extern void mouse_hack_get_pos(int *x, int *y);

#endif //__KEYBOARD__
