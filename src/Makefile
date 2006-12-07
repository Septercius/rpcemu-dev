

//CC = gcc-4.1 -march=athlon-4
CFLAGS = -Wall -Wextra -O0 -g3 #-Werror

LIBS = -pthread -lalleg-4.2.0 -L/usr/X11R6/lib -lX11 -lXext -lXpm -lXxf86vm -lXcursor -lm

EXE = ../rpcemu

all: $(EXE)


OBJS =	82c711.o    \
	arm.o       \
	cmos.o      \
	cp15.o      \
	fpa.o       \
	ide.o	    \
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
