#ifndef I8042_H
#define I8042_H

void i8042_keyboard_irq_raise(void);
void i8042_keyboard_irq_lower(void);
void i8042_mouse_irq_raise(void);
void i8042_mouse_irq_lower(void);

uint8_t i8042_data_read(void);
void i8042_data_write(uint8_t val);

uint8_t i8042_status_read(void);
void i8042_command_write(uint8_t val);

void i8042_reset(void);

#endif /* I8042_H */
