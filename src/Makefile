

//CC = gcc-4.1 -march=athlon-4
CFLAGS = -Wall -Wextra -O3 # -g1 -Werror

LIBS = -pthread -lalleg-4.1 -L/usr/X11R6/lib -lX11 -lXext -lXpm -lXxf86vm -lXcursor -lm

EXE = ../rpcemu

all: $(EXE)


OBJS =	82c711.o    \
	arm.o       \
	cmos.o      \
	cp15.o      \
	ide.o	    \
	iomd.o      \
	keyboard.o  \
	mem.o       \
	romload.o   \
	rpc-linux.o \
	vidc20.o    \
	hostfs.o   

SRCS = 	82c711.c    \
	arm.c       \
	cmos.c      \
	cp15.c      \
	ide.c       \
	iomd.c      \
	keyboard.c  \
	mem.c       \
	romload.c   \
	rpc-linux.c \
	vidc20.c    \
	hostfs.c 

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f *.o
