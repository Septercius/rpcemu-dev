# http://doc.qt.io/qt-5/qmake-tutorial.html

CONFIG += debug_and_release

QT += core widgets gui multimedia
INCLUDEPATH += ../

macx {
	INCLUDEPATH += ../macosx
}

# -Werror=switch
#	Ensures that using switch with enum requires every value to be handled
# -fno-common
#	Common symbols across object files will produce a link error
#	This is the default from GCC 10
#
QMAKE_CFLAGS   += -Werror=switch -fno-common
QMAKE_CXXFLAGS += -Werror=switch -fno-common

HEADERS =	../arm.h \
			../arm_common.h \
			../cdrom-ioctl.h \
			../cdrom-iso.h \
			../cmos.h \
			../cp15.h \
			../disc.h \
			../disc_adf.h \
			../disc_hfe.h \
			../disc_mfm_common.h \
			../fdc.h \
			../hostfs.h \
			../hostfs_internal.h \
			../i8042.h \
			../ide.h \
			../iomd.h \
			../keyboard.h \
			../mem.h \
			../podulerom.h \
			../podules.h \
			../romload.h \
			../rpcemu.h \
			../sound.h \
			../superio.h \
			../vidc20.h \
			about_dialog.h \
			configure_dialog.h \
			main_window.h \
			plt_sound.h \
			rpc-qt5.h

SOURCES =	../arm_common.c \
			../cdrom-iso.c \
			../cmos.c \
			../cp15.c \
			../disc.c \
			../disc_adf.c \
			../disc_hfe.c \
			../disc_mfm_common.c \
			../fdc.c \
			../fpa.c \
			../hostfs.c \
			../i8042.c \
			../icside.c \
			../ide.c \
			../iomd.c \
			../keyboard.c \
			../mem.c \
			../podulerom.c \
			../podules.c \
			../romload.c \
			../rpc-machdep.c \
			../rpcemu.c \
			../sound.c \
			../superio.c \
			../vidc20.c \
			about_dialog.cpp \
			configure_dialog.cpp \
			main_window.cpp \
			plt_sound.cpp \
			rpc-qt5.cpp \
			settings.cpp

# NAT Networking
CONFIG(networking) {
	DEFINES += CONFIG_SLIRP FEATURE_NETWORKING
	
	HEADERS +=	nat_edit_dialog.h \
				nat_list_dialog.h \
				../network-nat.h
			
	SOURCES += 	nat_edit_dialog.cpp \
				nat_list_dialog.cpp \
				../network-nat.c

	HEADERS += 	../slirp/bootp.h \
				../slirp/cutils.h \
				../slirp/debug.h \
				../slirp/if.h \
				../slirp/ip.h \
				../slirp/ip_icmp.h \
				../slirp/libslirp.h \
				../slirp/main.h \
				../slirp/mbuf.h \
				../slirp/misc.h \
				../slirp/sbuf.h \
				../slirp/slirp_config.h \
				../slirp/slirp.h \
				../slirp/socket.h \
				../slirp/tcp.h \
				../slirp/tcpip.h \
				../slirp/tcp_timer.h \
				../slirp/tcp_var.h \
				../slirp/tftp.h \
				../slirp/udp.h

	SOURCES +=	../slirp/bootp.c \
				../slirp/cksum.c \
				../slirp/cutils.c \
				../slirp/if.c \
				../slirp/ip_icmp.c \
				../slirp/ip_input.c \
				../slirp/ip_output.c \
				../slirp/mbuf.c \
				../slirp/misc.c \
				../slirp/sbuf.c \
				../slirp/slirp.c \
				../slirp/socket.c \
				../slirp/tcp_input.c \
				../slirp/tcp_output.c \
				../slirp/tcp_subr.c \
				../slirp/tcp_timer.c \
				../slirp/udp.c
	
	# Platform-specific additions.
	win32 {
		SOURCES +=	../network.c \
					../win/tap-win32.c \
					network_dialog.cpp \
					../win/network-win.c
		
		HEADERS +=	../network.h \
					network_dialog.h
					
		LIBS += -liphlpapi -lws2_32
	}
	
	linux {
		SOURCES +=	../network.c \
					../network-linux.c \
					network_dialog.cpp
			
		HEADERS +=	../network.h \
					network_dialog.h
	}
	
	macx {
		SOURCES +=	../macosx/network-macosx.c \
					../network.c \
					network_dialog.cpp
		HEADERS +=	../network.h \
					network_dialog.h
	}
}

RESOURCES =	icon.qrc

win32 { 
	SOURCES +=	../hostfs-win.c \
				../win/cdrom-ioctl.c \
				../win/rpc-win.c \
				keyboard_win.c

	RC_ICONS = ../win/rpcemu.ico

	# Enable Data Execution Prevention (DEP)
	QMAKE_LFLAGS = -Wl,--nxcompat
}

linux {
	SOURCES +=	../cdrom-linuxioctl.c
}

unix {
	!macx {
		SOURCES +=	../hostfs-unix.c \
					../rpc-linux.c \
					keyboard_x.c
	} else {
		SOURCES +=	../hostfs-macosx.c \
					../macosx/events-macosx.m \
					../macosx/hid-macosx.m \
					../macosx/preferences-macosx.m \
					../rpc-macosx.c \
					choose_dialog.cpp \
					keyboard_macosx.c
		
		HEADERS += 	../macosx/events-macosx.h \
					../macosx/hid-macosx.h \
					../macosx/preferences-macosx.h \
					choose_dialog.h \
					keyboard_macosx.h
		
		ICON = 		../macosx/rpcemu.icns
	}
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
	
	win32|macx {
		TARGET = 	RPCEmu-Recompiler
	} else {
		TARGET = 	rpcemu-recompiler
	}
} else {
	SOURCES +=	../arm.c \
				../codegen_null.c
			
	win32|macx {
		TARGET = 	RPCEmu-Interpreter
	} else {
		TARGET = 	rpcemu-interpreter
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

macx {
	LIBS += -framework coreFoundation -framework IOKit -framework Foundation -framework Carbon

	QMAKE_INFO_PLIST			= ../macosx/Info.plist
	QMAKE_BUNDLE 				= rpcemu
	QMAKE_TARGET_BUNDLE_PREFIX	= org.marutan
}

RESOURCES +=	resources.qrc
