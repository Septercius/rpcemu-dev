# Makefile.am for the RPCemu

bin_PROGRAMS = rpcemu
rpcemu_SOURCES = 82c711.c arm.c cmos.c \
cp15.c ide.c iomd.c keyboard.c mem.c \
romload.c rpc-linux.c vidc20.c hostfs.c 

rpcemu_CFLAGS=$(allegro_CFLAGS)
rpcemu_LDADD=$(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) $(allegro_LIBS)
