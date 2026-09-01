cmake_minimum_required(VERSION 3.31)

project(rpcemu VERSION 0.9.5 LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=switch -fno-common")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=switch -fno-common")
set(CMAKE_PREFIX_PATH "/usr/local/qt6" REQUIRED)

list(APPEND CMAKE_MODULE_PATH "/usr/local/qt6/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia Widgets)

qt_standard_project_setup()

# Detect 32-bit or 64-bit.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(PLATFORM_BITS 32)
else()
	set(PLATFORM_BITS 64)
endif()

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
	hostfs.c
	i8042.c
	icside.c
	ide.c
	iomd.c
	keyboard.c
	mem.c
	podulerom.c
	podules.c
	romload.c
	rpc-machdep.c
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

# Set interpreter-specific source files.
set(INTERPRETER_SOURCES
	arm.c
	codegen-null.c
)

# Set Linux sources for networking.
set(LINUX_NETWORK_SOURCES
	platform/linux/network-linux.c
)

# Set 32-bit recompiler sources for Linux.
set(LINUX_RECOMPILER_SOURCES_32
	codegen-x86.c
)

# Set 64-bit recompiler sources for Linux.
set(LINUX_RECOMPILER_SOURCES_64
	codegen-amd64.c
)

# Set Linux-specific source files.
set(LINUX_SOURCES
	platform/linux/cdrom-linuxioctl.c
	platform/linux/hostfs-unix.c
	platform/linux/keyboard-linux.c
	platform/linux/rpc-linux.c
)

# Set macOS-specific source files.
set(MACOS_SOURCES
	platform/macosx/rpc-macosx.c
	platform/macosx/events-macosx.m
	platform/macosx/hid-macosx.m
	platform/macosx/hostfs-macosx.c
	platform/macosx/preferences-macosx.m
	platform/macosx/keyboard-macosx.c
	qt6/choose-dialog.cpp
)

# Set macOS sources for networking.
set(MACOS_NETWORK_SOURCES
	platform/macosx/network-macosx.c
)

# Set macOS sources for the recompiler.
set(MACOS_RECOMPILER_SOURCES
	arm-dynarec.c
	codegen-amd64.c
)

# Set common sources for networking.
set(NETWORK_SOURCES
	network.c
	network-nat.c
	qt6/nat-edit-dialog.cpp
	qt6/nat-list-dialog.cpp
	qt6/network-dialog.cpp
)

# Set an empty variable for platform-specific recompiler sources.
set(PLATFORM_RECOMPILER_SOURCES
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

# Set Windows-specific sources for networking.
set(WIN32_NETWORK_SOURCES
	platform/win/network-win.c
	platform/win/tap-win32.c
)

# Set Windows-specific sources for the recompiler.
set(WIN32_RECOMPILER_SOURCES
	codegen_x86.c
)

# Set Windows-specific sources.
set(WIN32_SOURCES
	platform/win/cdrom-ioctl.c
	platform/win/hostfs-win.c
	platform/win/keyboard-win.c
	platform/win/rpc-win.c
)

# For networking, add the common and SLIRP network sources and set the required defines.
if (ENABLE_NETWORKING)
	list(APPEND PLATFORM_SOURCES ${NETWORK_SOURCES} ${SLIRP_SOURCES})
	set_source_files_properties(${SLIRP_SOURCES} PROPERTIES COMPILE_FLAGS -Wno-pragma-pack)
	
	add_compile_definitions(FEATURE_NETWORKING CONFIG_SLIRP)
endif()

if (APPLE)
	# Set up builds for macOS.
	
	# Set platform-specific sources.
	list(APPEND PLATFORM_SOURCES ${MACOS_SOURCES})
	list(APPEND PLATFORM_RECOMPILER_SOURCES ${MACOS_RECOMPILER_SOURCES})
	
	# Set the file to use for the bundle icons.
	set(MACOSX_BUNDLE_ICON_FILE platform/macosx/rpcemu.icns)
	
	# Set the application icon.
	set(app_icon_macos "platform/macosx/rpcemu.icns")
	set_source_files_properties(${app_icon_macos} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	
	list(APPEND PLATFORM_SOURCES ${app_icon_macos})
	
	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${MACOS_NETWORK_SOURCES})
	endif()

	# Define the target executables.
	qt_add_executable(qt6-interpreter
		${COMMON_SOURCES} 
		${INTERPRETER_SOURCES}
		${PLATFORM_SOURCES}
	)

	qt_add_executable(qt6-recompiler
		${COMMON_SOURCES}
		${RECOMPILER_SOURCES}
		${PLATFORM_RECOMPILER_SOURCES}
		${PLATFORM_SOURCES}
	)

	# Set up a macro to detect the dependency frameworks.
	macro(target_link_framework TARGET NAME)
	    find_library(FRAMEWORK_${NAME}
	        NAMES ${NAME}
	        PATHS ${CMAKE_OSX_SYSROOT}/System/Library
	        PATH_SUFFIXES Frameworks PrivateFrameworks
	        CMAKE_FIND_FRAMEWORK only
	        NO_DEFAULT_PATH)
	    if( ${FRAMEWORK_${NAME}} STREQUAL FRAMEWORK_${NAME}-NOTFOUND)
	        message(ERROR ": framework ${NAME} not found")
	    else()
	        target_link_libraries(${TARGET} PUBLIC "${FRAMEWORK_${NAME}}")
	        message(STATUS "Framework ${NAME} found at ${FRAMEWORK_${NAME}}")
	    endif()
	endmacro(target_link_framework)

	# Set up a macro to configure each target.
	macro(configure_target NAME APPNAME ARCHITECTURE)
		# Set includes.
		target_include_directories(${NAME} PRIVATE . platform/macosx qt6)
		
		# Add frameworks.
		target_link_framework(${NAME} coreFoundation)
		target_link_framework(${NAME} IOKit)
		target_link_framework(${NAME} Foundation)
		target_link_framework(${NAME} Carbon)
		
		# Add QT6 modules.
		target_link_libraries(${NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Widgets)
		
		# Add networking includes if required.
		if (ENABLE_NETWORKING)
			target_include_directories(${NAME} PRIVATE slirp)
		endif()
	
		# Configure debug build if needed.
		if (ENABLE_DEBUG)
			add_compile_definitions(_DEBUG)
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}-Debug
				MACOSX_BUNDLE_EXECUTABLENAME ${APPNAME}-Debug
			)
		else()
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}
				MACOSX_BUNDLE_EXECUTABLENAME ${APPNAME}
			)
		endif()
		
		# Set properties of the target.
		set_target_properties(${NAME} PROPERTIES
			MACOSX_BUNDLE ON
			MACOSX_BUNDLE_INFO_PLIST platform/macosx/Info.plist
			MACOSX_BUNDLE_DEPLOYMENT_TARGET 10.13
			MACOSX_BUNDLE_GUI_IDENTIFIER org.marutan.rpcemu
			OSX_ARCHITECTURES "${ARCHITECTURE}"
		)
	endmacro(configure_target)

	# Configure the targets.
	configure_target("qt6-interpreter" "RPCEmu-Interpreter" "x86_64;arm64")
	configure_target("qt6-recompiler" "RPCEmu-Recompiler" x86_64)
elseif (LINUX)
	# Set up builds for Linux.
	
	# Add platform-specific sources.
	list(APPEND PLATFORM_SOURCES ${LINUX_SOURCES})

	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${LINUX_NETWORK_SOURCES})
	endif()
	
	# Add the relevant recompiler sources for 32-bit or 64-bit.
	if (PLATFORM_BITS EQUAL 32)
		list(APPEND PLATFORM_RECOMPILER_SOURCES ${LINUX_RECOMPILER_SOURCES_32)
	else()
		list(APPEND PLATFORM_RECOMPILER_SOURCES ${LINUX_RECOMPILER_SOURCES_64)
	endif()

	# Define the target executables.
	qt_add_executable(qt6-interpreter
		${COMMON_SOURCES} 
		${INTERPRETER_SOURCES}
		${PLATFORM_SOURCES} 
		${QT6_SOURCES}
	)

	qt_add_executable(qt6-recompiler
		${COMMON_SOURCES}
		${RECOMPILER_SOURCES}
		${PLATFORM_RECOMPILER_SOURCES}
		${PLATFORM_SOURCES}
		${QT6_SOURCES}
	)
	
	# Set up a macro to configure a target.
	macro(configure_target NAME APPNAME)
		# Set includes.
		target_include_directories(${NAME} PRIVATE . platform/linux qt6)
		
		# Add QT6 modules.
		target_link_libraries(${NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Widgets)

		# Add includes for networking.
		if (ENABLE_NETWORKING)
			target_include_directories(${NAME} PRIVATE slirp)
		endif()
	
		# Configure debug build if needed.
		if (ENABLE_DEBUG)
			add_compile_definitions(_DEBUG)
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}-Debug
			)
		else()
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}
			)
		endif()
	endmacro(configure_target)
	
	# Configure the targets.
	configure_target("qt6-interpreter" "rpcemu-interpreter")
	configure_target("qt6-recompiler" "rpcemu-recompiler")
elseif (WIN32)
	# Set up builds for Windows.
	
	# Set the application icon.
	set(APP_ICON_RESOURCE_WINDOWS "win/rpcemu.rc")
	
	# Add platform-specific sources.
	list(APPEND PLATFORM_SOURCES ${WIN32_SOURCES})
	list(APPEND PLATFORM_RECOMPILER_SOURCES ${WIN32_RECOMPILER_SOURCES})
	
	# If networking is configured, add the platform-specific network sources.
	if (ENABLE_NETWORKING)
		list(APPEND PLATFORM_SOURCES ${WIN32_NETWORK_SOURCES})
	endif()
	
	# Define the target executables.
	qt_add_executable(qt6-interpreter
		${COMMON_SOURCES} 
		${INTERPRETER_SOURCES}
		${PLATFORM_SOURCES} 
		${QT6_SOURCES}
		${APP_ICON_RESOURCE_WINDOWS}
	)

	qt_add_executable(qt6-recompiler
		${COMMON_SOURCES}
		${PLATFORM_RECOMPILER_SOURCES}
		${PLATFORM_SOURCES}
		${QT6_SOURCES}
		${APP_ICON_RESOURCE_WINDOWS}
	)
	
	# Set up a macro to configure a target.
	macro(configure_target NAME APPNAME)
	# Set includes.
		target_include_directories(${NAME} PRIVATE . platform/win qt6)
		
		# Add QT6 modules		
		target_link_libraries(${NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Widgets)
		target_link_options(${NAME} PRIVATE -Wl,--nxcompat)
		
		if (ENABLE_NETWORKING)
			target_link_libraries(${NAME} PRIVATE -liphlpapi -lws2_32)
			target_include_directories(${NAME} PRIVATE slirp)
		endif()
	
		if (ENABLE_DEBUG)
			add_compile_definitions(_DEBUG)
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}-Debug
			)
		else()
			set_target_properties(${NAME} PROPERTIES
				OUTPUT_NAME ${APPNAME}
			)
		endif()
	endmacro(configure_target)
	
	# Configure the targets.
	configure_target("qt6-interpreter" "RPCEmu-Interpreter")
	configure_target("qt6-recompiler" "RPCEmu-Recompiler")
endif()
