CC = clang
AS = $(CC)
ASFLAGS = --target=arm-unknown-none-eabi -Wall
OBJCOPY = objcopy
OBJCOPYFLAGS = -Ielf32-little -O binary

all: scrollwheel,ffa

scrollwheel,ffa: scrollwheel.o
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f scrollwheel,ffa scrollwheel.o
