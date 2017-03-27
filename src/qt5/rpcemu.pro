# http://doc.qt.io/qt-5/qmake-tutorial.html

CONFIG += debug


QT += core widgets gui
INCLUDEPATH += ../


HEADERS =	../superio.h \
		../cmos.h \
		../cp15.h \
		../fdc.h \
		../hostfs.h \
		../ide.h \
		../iomd.h \
		../keyboard.h \
		../mem.h \
		../sound.h \
		../vidc20.h \
		../arm_common.h \
		../arm.h \
		../network.h \
		main_window.h \
		configure_dialog.h \
		network_dialog.h \
		rpc-qt5.h \
		../iomdtimer.h

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
		../vidc20.cpp \
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
		network_dialog.cpp

win32: {
	SOURCES +=	../cdrom-ioctl.c \
			../network.c \
			../network-win.c \
			keyboard_win.c
}

linux {
	SOURCES +=	../cdrom-linuxioctl.c \
			../network.c \
			../network-linux.c
}

unix {
	SOURCES +=	keyboard_x.c
}

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
	
	TARGET = ../../rpcemu-recompiler
} else {
	SOURCES +=	../arm.c \
			../codegen_null.c
	TARGET = ../../rpcemu-interpreter
}

CONFIG(debug) {
	DEFINES += _DEBUG
}


RESOURCES =

LIBS +=
