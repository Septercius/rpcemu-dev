# http://doc.qt.io/qt-5/qmake-tutorial.html

CONFIG += debug_and_release


QT += core widgets gui multimedia
INCLUDEPATH += ../

# This ensures that using switch with enum requires every value to be handled
QMAKE_CFLAGS += -Werror=switch
QMAKE_CXXFLAGS += -Werror=switch


HEADERS =	../superio.h \
		../cdrom-iso.h \
		../cmos.h \
		../cp15.h \
		../fdc.h \
		../hostfs.h \
		../hostfs_internal.h \
		../ide.h \
		../iomd.h \
		../keyboard.h \
		../mem.h \
		../sound.h \
		../vidc20.h \
		../arm_common.h \
		../arm.h \
		main_window.h \
		configure_dialog.h \
		about_dialog.h \
		rpc-qt5.h \
		plt_sound.h

SOURCES =	../superio.c \
		../cdrom-iso.c \
		../cmos.c \
		../cp15.c \
		../fdc.c \
		../fpa.c \
		../hostfs.c \
		../ide.c \
		../iomd.c \
		../keyboard.c \
		../mem.c \
		../romload.c \
		../rpcemu.c \
		../sound.c \
		../vidc20.c \
		../podules.c \
		../podulerom.c \
		../icside.c \
		../rpc-machdep.c \
		../arm_common.c \
		../i8042.c \
		settings.cpp \
		rpc-qt5.cpp \
		main_window.cpp \
		configure_dialog.cpp \
		about_dialog.cpp \
		plt_sound.cpp

RESOURCES =	icon.qrc

win32 { 
	SOURCES +=	../win/cdrom-ioctl.c \
			../win/network-win.c \
			../network.c \
			../hostfs-win.c \
			network_dialog.cpp \
			../win/tap-win32.c \
			../win/rpc-win.c \
			keyboard_win.c
	HEADERS +=	../network.h \
			network_dialog.h

	RC_ICONS = ../win/rpcemu.ico

	# Enable Data Execution Prevention (DEP)
	QMAKE_LFLAGS = -Wl,--nxcompat
}

linux {
	SOURCES +=	../cdrom-linuxioctl.c \
			../network-linux.c \
			../network.c \
			network_dialog.cpp
	HEADERS +=	../network.h \
			network_dialog.h
}

unix {
	SOURCES +=	keyboard_x.c \
			../hostfs-unix.c \
			../rpc-linux.c
}

# Place exes in top level directory
DESTDIR = ../..

CONFIG(dynarec) {
	SOURCES +=	../ArmDynarec.c
	HEADERS +=	../ArmDynarecOps.h \
			../codegen_x86_common.h

	contains(QMAKE_HOST.arch, x86_64):!win32: { # win32 always uses 32bit dynarec
		HEADERS +=	../codegen_amd64.h
		SOURCES +=	../codegen_amd64.c
	} else {
		HEADERS +=	../codegen_x86.h
		SOURCES +=	../codegen_x86.c
	}
	
	win32 {
		TARGET = RPCEmu-Recompiler
	} else {
		TARGET = rpcemu-recompiler
	}
} else {
	SOURCES +=	../arm.c \
			../codegen_null.c
	win32 {
		TARGET = RPCEmu-Interpreter
	} else {
		TARGET = rpcemu-interpreter
	}
}

# Big endian architectures
# need to find defines for sparc, arm be, mips be
contains(QMAKE_HOST.arch, ppc)|contains(QMAKE_HOST.arch, ppc64) {
	DEFINES += _RPCEMU_BIG_ENDIAN
}

CONFIG(debug, debug|release) {
	DEFINES += _DEBUG
	TARGET = $$join(TARGET, , , -debug)
}

LIBS +=
