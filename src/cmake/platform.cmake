# Platform-specific function and macro definitions for cmake.


# Add a define specifying the number of bits
macro(rpcemu_add_platform_bits)
	if (PLATFORM_BITS EQUAL 32)
		add_compile_definitions(RPCEMU_PLATFORM_BITS_32)
	elseif (PLATFORM_BITS EQUAL 64)
		add_compile_definitions(RPCEMU_PLATFORM_BITS_64)
	endif()
endmacro(rpcemu_add_platform_bits)

# Configure a target for Linux.
macro(rpcemu_configure_linux_target NAME APPNAME)
	# Set includes.
	target_include_directories(${NAME} PRIVATE . filesystem platform/linux qt6)
	
	# Add QT6 modules.
	rpcemu_target_add_qt(${NAME})

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
endmacro(rpcemu_configure_linux_target)

# Configure a target for macOS.
macro(rpcemu_configure_macos_target NAME APPNAME ARCHITECTURE)
	# Set includes.
	target_include_directories(${NAME} PRIVATE . filesystem platform/macosx qt6)
	
	# Add frameworks.
	rpcemu_target_link_macos_framework(${NAME} coreFoundation)
	rpcemu_target_link_macos_framework(${NAME} IOKit)
	rpcemu_target_link_macos_framework(${NAME} Foundation)
	rpcemu_target_link_macos_framework(${NAME} Carbon)
	
	# Add QT6 modules.
	rpcemu_target_add_qt(${NAME})
	
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
endmacro(rpcemu_configure_macos_target)

# Configure a target for Windows.
macro(rpcemu_configure_win_target TARGET APPNAME)
	# Set includes.
	target_include_directories(${TARGET} PRIVATE . filesystem platform/win qt6)
	
	# Add QT6 modules.
	rpcemu_target_add_qt(${TARGET})
	
	# Set linking options.
	target_link_options(${TARGET} PRIVATE -Wl,--nxcompat)
	
	if (ENABLE_NETWORKING)
		target_link_libraries(${TARGET} PRIVATE -liphlpapi -lws2_32)
		target_include_directories(${TARGET} PRIVATE slirp)
	endif()

	if (ENABLE_DEBUG)
		add_compile_definitions(_DEBUG)
		set_target_properties(${TARGET} PROPERTIES
			OUTPUT_NAME ${APPNAME}-Debug
		)
	else()
		set_target_properties(${TARGET} PROPERTIES
			OUTPUT_NAME ${APPNAME}
		)
	endif()
endmacro(rpcemu_configure_win_target)

# Detect platform 32-bit or 64-bit
macro(rpcemu_detect_platform_bits)
	# Detect 32-bit or 64-bit.
	if (CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(PLATFORM_BITS 32)
		message(STATUS "Detected 32-bit operating system")
	else()
		set(PLATFORM_BITS 64)
		message(STATUS "Detected 64-bit operating system")
	endif()
endmacro(rpcemu_detect_platform_bits)

# Link a target against a macOS framework
macro(rpcemu_target_link_macos_framework TARGET NAME)
	find_library(FRAMEWORK_${NAME}
		NAMES ${NAME}
		PATHS ${CMAKE_OSX_SYSROOT}/System/Library
		PATH_SUFFIXES Frameworks PrivateFrameworks
		CMAKE_FIND_FRAMEWORK only
		NO_DEFAULT_PATH)
	if (${FRAMEWORK_${NAME}} STREQUAL FRAMEWORK_${NAME}-NOTFOUND)
		message(ERROR ": framework ${NAME} not found")
	else()
		target_link_libraries(${TARGET} PUBLIC "${FRAMEWORK_${NAME}}")
		message(STATUS "Framework ${NAME} found at ${FRAMEWORK_${NAME}}")
	endif()
endmacro(rpcemu_target_link_macos_framework)
