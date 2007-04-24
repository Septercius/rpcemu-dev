

//CC = gcc-4.1 -march=athlon-4
CFLAGS = -Wall -Wextra -O3 #-Werror
//CFLAGS = -O3 -march=i686 -ffast-math -fomit-frame-pointer -falign-loops -falign-jumps -falign-functions -ftracer
LIBS = `allegro-config --libs` -lm

EXE = ../rpcemu

all: $(EXE)


OBJS =	82c711.o    \
	arm.o       \
	cmos.o      \
	cp15.o      \
	fpa.o       \
	gui.o	      \
	ide.o	      \
	iomd.o      \
	keyboard.o  \
	mem.o       \
	rpcemu.o    \
	rpc-linux.o \
	romload.o   \
	sound.o     \
	vidc20.o    \
	hostfs.o   

SRCS = 	82c711.c    \
	arm.c       \
	cmos.c      \
	cp15.c      \
	fpa.c       \
	gui.c       \
	ide.c       \
	iomd.c      \
	keyboard.c  \
	mem.c       \
	rpcemu.c    \
	rpc-linux.c \
	romload.c   \
	sound.c     \
	vidc20.c    \
	hostfs.c 

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f *.o
