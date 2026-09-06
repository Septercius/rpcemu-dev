cmake_minimum_required(VERSION 3.31)

project(rpcemu VERSION 0.9.5 LANGUAGES C CXX OBJC)

# Add external macros.
include(cmake/qt.cmake)
include(cmake/platform.cmake)
include(cmake/target.cmake)

# Look for QT6
rpcemu_qt_locate()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=switch -fno-common")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=switch -fno-common")
set(CMAKE_PREFIX_PATH ${QTDIR} REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${QT_CMAKE_MODULES})
find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia Widgets)

qt_standard_project_setup()

# Detect 32-bit or 64-bit.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(PLATFORM_BITS 32)
	message(STATUS "Detected 32-bit operating system")
else()
	set(PLATFORM_BITS 64)
	message(STATUS "Detected 64-bit operating system")
endif()

# Set headers common to all builds.
set(COMMON_HEADERS
	arm-common.h
	cdrom-iso.h
	cmos.h
	cp15.h
	disc-adf.h
	disc-hfe.h
	disc-mfm-common.h
	disc.h
	fdc.h
	ide.h
	iomd.h
	keyboard.h
	mem.h
	podulerom.h
	podules.h
	romload.h
	rpcemu.h
	sound.h
	superio.h
	vidc20.h
)

# Set source files common to all builds.
set(COMMON_SOURCES 
	arm-common.c
	cdrom-iso.c
	cmos.c
	cp15.c
	disc.c
	disc-adf.c
	disc-hfe.c
	disc-mfm-common.c
	fdc.c
	fpa.c
	i8042.c
	icside.c
	ide.c
	iomd.c
	keyboard.c
	mem.c
	podulerom.c
	podules.c
	romload.c
	rpcemu.c
	sound.c
	superio.c
	vidc20.c
	qt6/about-dialog.cpp
	qt6/configure-dialog.cpp
	qt6/main-window.cpp
	qt6/plt-sound.cpp
	qt6/rpc-qt6.cpp
	qt6/settings.cpp
)

# Set embedded resources.
set(EMBEDDED_RESOURCES
	qt6/icon.qrc
	qt6/resources.qrc
)

# Set an empty variable for filesystem headers.
set(FILESYSTEM_HEADERS
)

# Set an empty variable for filesystem sources.
set(FILESYSTEM_SOURCES
)

# Set common headers for HostFS.
set(HOSTFS_HEADERS
	filesystem/hostfs-standard.h
)

# Set common sources for HostFS.
set(HOSTFS_SOURCES
	filesystem/hostfs-standard.c
)

# Set interpreter-specific header files.
set(INTERPRETER_HEADERS
	arm.h
)

# Set interpreter-specific source files.
set(INTERPRETER_SOURCES
	arm.c
	codegen-null.c
)

# Set Linux-specific header files for standard HostFS.
set(LINUX_HOSTFS_HEADERS
	platform/macosx/hostfs-linux.h
)

# Set Linux-specific source files for standard HostFS.
set(LINUX_HOSTFS_SOURCES
	platform/macosx/hostfs-linux.c
)

# Set Linux-specific header files for multiple-drive HostFS.
set(LINUX_MULTI_HOSTFS_HEADERS
	platform/linux/hostfs-multi-linux.h
)

# Set Linux-specific source files for multiple-drive HostFS.
set(LINUX_MULTI_HOSTFS_SOURCES
	platform/linux/hostfs-multi-linux.c
)

# Set Linux sources for networking.
set(LINUX_NETWORK_SOURCES
	platform/linux/network-linux.c
)

# Set 32-bit recompiler headers for Linux.
set(LINUX_RECOMPILER_HEADERS_32
	codegen-x86.h
)

# Set 32-bit recompiler sources for Linux.
set(LINUX_RECOMPILER_SOURCES_32
	codegen-x86.c
)

# Set 64-bit recompiler headers for Linux.
set(LINUX_RECOMPILER_HEADERS_64
	codegen-amd64.h
)

# Set 64-bit recompiler sources for Linux.
set(LINUX_RECOMPILER_SOURCES_64
	codegen-amd64.c
)

# Set Linux-specific source files.
set(LINUX_SOURCES
	platform/linux/cdrom-linuxioctl.c
	platform/linux/keyboard-linux.c
	platform/linux/rpc-linux.c
)

# Set macOS-specific header files for standard HostFS.
set(MACOS_HOSTFS_HEADERS
	platform/macosx/hostfs-macosx.h
)

# Set macOS-specific source files for standard HostFS.
set(MACOS_HOSTFS_SOURCES
	platform/macosx/hostfs-macosx.c
)

# Set macOS-specific header files for multiple-drive HostFS.
set(MACOS_MULTI_HOSTFS_HEADERS
	platform/macosx/hostfs-multi-macosx.h
)

# Set macOS-specific source files for multiple-drive HostFS.
set(MACOS_MULTI_HOSTFS_SOURCES
	platform/macosx/hostfs-multi-macosx.c
)

# Set macOS-specific header files.
set(MACOS_HEADERS
	platform/macosx/events-macosx.h
	platform/macosx/hid-macosx.h
	platform/macosx/keyboard-macosx.h
	platform/macosx/preferences-macosx.h
)

# Set macOS-specific source files.
set(MACOS_SOURCES
	platform/macosx/rpc-macosx.c
	platform/macosx/events-macosx.m
	platform/macosx/hid-macosx.m
	platform/macosx/preferences-macosx.m
	platform/macosx/keyboard-macosx.c
	qt6/choose-dialog.cpp
)

# Set macOS sources for networking.
set(MACOS_NETWORK_SOURCES
	platform/macosx/network-macosx.c
)

# Set macOS headers for the recompiler.
set(MACOS_RECOMPILER_HEADERS
	arm-dynarec.h
	codegen-amd64.h
)

# Set macOS sources for the recompiler.
set(MACOS_RECOMPILER_SOURCES
	arm-dynarec.c
	codegen-amd64.c
)

# Set multiple-drive HostFS headers.
set(MULTI_HOSTFS_HEADERS
	filesystem/hostfs-multi.h
	qt6/hostfs-dialog.h
	qt6/paths.h
)

# Set multiple-drive HostFS sources.
set(MULTI_HOSTFS_SOURCES
	filesystem/hostfs-multi.c
	qt6/hostfs-dialog.cpp
	qt6/paths.cpp
)

# Set common headers for networking.
set(NETWORK_HEADERS
	network.h
	network-nat.h
	qt6/nat-edit-dialog.h
	qt6/nat-list-dialog.h
	qt6/network-dialog.h
)

# Set common sources for networking.
set(NETWORK_SOURCES
	network.c
	network-nat.c
	qt6/nat-edit-dialog.cpp
	qt6/nat-list-dialog.cpp
	qt6/network-dialog.cpp
)

# Set an empty variable for platform-specific filesystem headers.
set(PLATFORM_FILESYSTEM_HEADERS
)

# Set an empty variable for platform-specific filesystem sources.
set(PLATFORM_FILESYSTEM_SOURCES
)

# Set an empty variable for platform-specific recompiler headers.
set(PLATFORM_RECOMPILER_HEADERS
)

# Set an empty variable for platform-specific recompiler sources.
set(PLATFORM_RECOMPILER_SOURCES
)

# Set an empty variable for platform-specific headers.
set(PLATFORM_HEADERS
)

# Set an empty variable for platform-specific sources.
set(PLATFORM_SOURCES
)

# Set sources for SLIRP networking.
set(SLIRP_SOURCES
	slirp/bootp.c
	slirp/cksum.c
	slirp/cutils.c
	slirp/if.c
	slirp/ip_icmp.c
	slirp/ip_input.c
	slirp/ip_output.c
	slirp/mbuf.c
	slirp/misc.c
	slirp/sbuf.c
	slirp/slirp.c
	slirp/socket.c
	slirp/tcp_input.c
	slirp/tcp_output.c
	slirp/tcp_subr.c
	slirp/tcp_timer.c
	slirp/udp.c
)

# Set Windows-specific header files for standard HostFS.
set(WIN32_HOSTFS_HEADERS
	platform/win/hostfs-win.h
)

# Set Windows-specific source files for standard HostFS.
set(WIN32_HOSTFS_SOURCES
	platform/win/hostfs-win.c
)

# Set Windows-specific header files for multiple-drive HostFS.
set(WIN32_MULTI_HOSTFS_HEADERS
	platform/win/hostfs-multi-win.h
)

# Set Windows-specific source files for multiple-drive HostFS.
set(WIN32_MULTI_HOSTFS_SOURCES
	platform/win/hostfs-multi-win.c
)

# Set Windows-specific sources for networking.
set(WIN32_NETWORK_SOURCES
	platform/win/network-win.c
	platform/win/tap-win32.c
)

# Set Windows-specific headers for the recompiler.
set(WIN32_RECOMPILER_HEADERS
	codegen-x86.h
)

# Set Windows-specific sources for the recompiler.
set(WIN32_RECOMPILER_SOURCES
	codegen-x86.c
)

# Set Windows-specific sources.
set(WIN32_SOURCES
	platform/win/cdrom-ioctl.c
	platform/win/keyboard-win.c
	platform/win/rpc-win.c
)

# For multiple-drive HostFS, add its files to the filesystem sources.
if (ENABLE_MULTI_HOSTFS)
	message(STATUS "Configuring for multiple-drive HostFS")
	
	list(APPEND FILESYSTEM_HEADERS ${MULTI_HOSTFS_HEADERS})
	list(APPEND FILESYSTEM_SOURCES ${MULTI_HOSTFS_SOURCES})
	
	add_compile_definitions(FEATURE_MULTI_HOSTFS)
else()
	message(STATUS "Configuring for standard HostFS")
	
	list(APPEND FILESYSTEM_HEADERS ${HOSTFS_HEADERS})
	list(APPEND FILESYSTEM_SOURCES ${HOSTFS_SOURCES})
endif()

# For networking, add the common and SLIRP network sources and set the required defines.
if (ENABLE_NETWORKING)
	message(STATUS "Configuring network support")
	
	list(APPEND PLATFORM_HEADERS ${NETWORK_HEADERS})
	list(APPEND PLATFORM_SOURCES ${NETWORK_SOURCES} ${SLIRP_SOURCES})
	set_source_files_properties(${SLIRP_SOURCES} PROPERTIES COMPILE_FLAGS -Wno-pragma-pack)
	
	add_compile_definitions(FEATURE_NETWORKING CONFIG_SLIRP)
else()
	message(STATUS "Network support not specified")
endif()

if (APPLE)
	# Set up builds for macOS.
	
	message(STATUS "Configuring RPCEmu ${CMAKE_PROJECT_VERSION} for Apple macOS")
	
	# Set the platform define.
	add_compile_definitions(RPCEMU_PLATFORM_MACOS)
	
	# Set platform-specific sources.
	list(APPEND PLATFORM_HEADERS ${MACOS_HEADERS})
	list(APPEND PLATFORM_SOURCES ${MACOS_SOURCES})
	list(APPEND PLATFORM_RECOMPILER_SOURCES ${MACOS_RECOMPILER_SOURCES})
	
	# Set the file to use for the bundle icons.
	set(MACOSX_BUNDLE_ICON_FILE platform/macosx/rpcemu.icns)
	
	# Set the bundle icons.
	set(MACOS_BUNDLE_ICONS "platform/macosx/rpcemu.icns")
	set_source_files_properties(${MACOS_BUNDLE_ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	
	# Add the PNG icon.
	set(MACOS_APPLICATION_ICON "qt6/rpcemu-icon.png")
	set_source_files_properties(${MACOS_APPLICATION_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	
	list(APPEND PLATFORM_SOURCES ${MACOS_BUNDLE_ICONS} ${MACOS_APPLICATION_ICON})
	
	# Add the required files for whatever HostFS implementation is being used.
	if (ENABLE_MULTI_HOSTFS)
		# Multiple drive support.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${MACOS_MULTI_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${MACOS_MULTI_HOSTFS_SOURCES})
	else()
		# Standard HostFS.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${MACOS_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${MACOS_HOSTFS_SOURCES})
	endif()
	
	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${MACOS_NETWORK_SOURCES})
	endif()

	# Define the target executables.
	rpcemu_add_executables()

	# Configure the targets.
	if (NOT DISABLE_INTERPRETER)
		rpcemu_configure_macos_target("qt6-interpreter" "RPCEmu-Interpreter" "x86_64;arm64")
	endif()
	
	if (NOT DISABLE_RECOMPILER)
		rpcemu_configure_macos_target("qt6-recompiler" "RPCEmu-Recompiler" x86_64)
	endif()
elseif (LINUX)
	# Set up builds for Linux.
	
	message(STATUS "Configuring RPCEmu ${CMAKE_PROJECT_VERSION} for Linux")
	
	# Set the platform define.
	add_compile_definitions(RPCEMU_PLATFORM_LINUX)
	
	# Add platform-specific sources.
	list(APPEND PLATFORM_SOURCES ${LINUX_SOURCES})
	
	# Add the required files for whatever HostFS implementation is being used.
	if (ENABLE_MULTI_HOSTFS)
		# Multiple drive support.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${LINUX_MULTI_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${LINUX_MULTI_HOSTFS_SOURCES})
	else()
		# Standard HostFS.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${LINUX_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${LINUX_HOSTFS_SOURCES})
	endif()

	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${LINUX_NETWORK_SOURCES})
	endif()
	
	# Add the relevant recompiler sources for 32-bit or 64-bit.
	if (PLATFORM_BITS EQUAL 32)
		list(APPEND PLATFORM_RECOMPILER_HEADERS ${LINUX_RECOMPILER_HEADERS_32})
		list(APPEND PLATFORM_RECOMPILER_SOURCES ${LINUX_RECOMPILER_SOURCES_32})
	else()
		list(APPEND PLATFORM_RECOMPILER_HEADERS ${LINUX_RECOMPILER_HEADERS_64})
		list(APPEND PLATFORM_RECOMPILER_SOURCES ${LINUX_RECOMPILER_SOURCES_64})
	endif()

	# Define the target executables.
	rpcemu_add_executables()
	
	# Configure the targets.
	if (NOT DISABLE_INTERPRETER)
		rpcemu_configure_linux_target("qt6-interpreter" "rpcemu-interpreter")
	endif()
	
	if (NOT DISABLE_RECOMPILER)
		rpcemu_configure_linux_target("qt6-recompiler" "rpcemu-recompiler")
	endif()
elseif (WIN32)
	# Set up builds for Windows.
	
	message(STATUS "Configuring RPCEmu ${CMAKE_PROJECT_VERSION} for Windows")
	
	# Set the platform define.
	add_compile_definitions(RPCEMU_PLATFORM_WIN32)
	
	# Set the application icon.
	set(APP_ICON_RESOURCE_WINDOWS "win/rpcemu.rc")
	
	# Add platform-specific sources.
	list(APPEND PLATFORM_SOURCES ${WIN32_SOURCES})
	list(APPEND PLATFORM_RECOMPILER_HEADERS ${WIN32_RECOMPILER_HEADERS})
	list(APPEND PLATFORM_RECOMPILER_SOURCES ${WIN32_RECOMPILER_SOURCES})
	
	# Add the required files for whatever HostFS implementation is being used.
	if (ENABLE_MULTI_HOSTFS)
		# Multiple drive support.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${WIN32_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${WIN32_HOSTFS_SOURCES})
	else()
		# Standard HostFS.
		list(APPEND PLATFORM_FILESYSTEM_HEADERS ${WIN32_HOSTFS_HEADERS})
		list(APPEND PLATFORM_FILESYSTEM_SOURCES ${WIN32_HOSTFS_SOURCES})
	endif()
	
	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${WIN32_NETWORK_SOURCES})
	endif()
	
	# Define the target executables.
	rpcemu_add_executables()
	
	# Configure the targets.
	if (NOT DISABLE_INTERPRETER)
		rpcemu_configure_win_target("qt6-interpreter" "RPCEmu-Interpreter")
	endif()
	
	if (NOT DISABLE_RECOMPILER)
		rpcemu_configure_win_target("qt6-recompiler" "RPCEmu-Recompiler")
	endif()
endif()
