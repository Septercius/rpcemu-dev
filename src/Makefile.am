# Makefile.am for the RPCemu

bin_PROGRAMS = rpcemu
noinst_SCRIPTS = ../rpcemu
CLEANFILES = $(noinst_SCRIPTS)

../rpcemu: rpcemu
	cp rpcemu ..

rpcemu_CFLAGS = $(allegro_CFLAGS)

# workaround for Win32 Allegro, which has `allegro-config' missing
if OS_WIN
rpcemu_LDADD = -lalleg
else
rpcemu_LDADD = $(allegro_LIBS)
endif

rpcemu_SOURCES = superio.c superio.h cdrom-iso.c cmos.c cmos.h cp15.c cp15.h \
fdc.c fdc.h fpa.c hostfs.c hostfs.h ide.c ide.h iomd.c iomd.h keyboard.c \
keyboard.h mem.c mem.h romload.c rpcemu.c sound.c sound.h vidc20.c vidc20.h \
podules.c podulerom.c icside.c rpc-machdep.c arm_common.h arm_common.c \
i8042.c

if DYNAREC
rpcemu_SOURCES += ArmDynarec.c ArmDynarecOps.h
rpcemu_CFLAGS += -DDYNAREC

if CPU_X86_64
rpcemu_SOURCES += codegen_amd64.c codegen_amd64.h codegen_x86_common.h
endif
if CPU_I386
rpcemu_SOURCES += codegen_x86.c codegen_x86.h codegen_x86_common.h
endif

else
# non-DYNAREC
rpcemu_SOURCES += arm.c arm.h codegen_null.c
endif

if OS_WIN
rpcemu_SOURCES += cdrom-ioctl.c rpc-win.c resources.h
rpcemu_CPPFLAGS = -Dsleep=Sleep
rpcemu_LDADD += acorn.o -lcomdlg32 -lwinmm

%.o: %.rc
	windres $^ $@
endif

if OS_LINUX
rpcemu_SOURCES += rpc-linux.c gui.c cdrom-linuxioctl.c network-linux.c \
network.c network.h
endif

if OS_MACOSX
rpcemu_SOURCES += rpc-macosx.c gui.c
endif

if OS_OTHER
rpcemu_SOURCES += rpc-linux.c gui.c
endif

EXTRA_DIST = acorn.rc rpcemu.ico
