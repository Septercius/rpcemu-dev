#
# $Id: Makefile,v 1.2 2006/03/07 15:17:10 mhowkins Exp $
#

AS = armas
CP = cp
INSTALL = install
LD = armld
OBJCOPY = armobjcopy
TARGETDIR = ../../poduleroms

all: hostfs,ffa hostfsfiler,ffa
	$(INSTALL) -d $(TARGETDIR)
	install $^ $(TARGETDIR)

%,ffa: %.elf
	$(OBJCOPY) -O binary $< $@

%.elf: %.o
	$(LD) --section-start .text=0 -o $@ $<

%.o: %.s
	$(AS) -o $@ $<

clean:
	rm -f *.o *.elf *,ffa
