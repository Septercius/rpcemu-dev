# Target-related function and macro definitions for RPCEmu.
	
macro(rpcemu_add_executables)
	if (NOT DISABLE_INTERPRETER)
		qt_add_executable(qt6-interpreter
			${EMBEDDED_RESOURCES}
			${COMMON_HEADERS} 
			${COMMON_SOURCES} 
			${FILESYSTEM_HEADERS}
			${FILESYSTEM_SOURCES}
			${INTERPRETER_HEADERS} 
			${INTERPRETER_SOURCES} 
			${PLATFORM_FILESYSTEM_HEADERS}
			${PLATFORM_FILESYSTEM_SOURCES}
			${PLATFORM_HEADERS} 
			${PLATFORM_SOURCES}
		)
	endif()
	
	if (NOT DISABLE_RECOMPILER)
		qt_add_executable(qt6-recompiler
			${EMBEDDED_RESOURCES}
			${COMMON_HEADERS} 
			${COMMON_SOURCES}
			${FILESYSTEM_HEADERS}
			${FILESYSTEM_SOURCES}
			${RECOMPILER_HEADERS}
			${RECOMPILER_SOURCES}
			${PLATFORM_FILESYSTEM_HEADERS}
			${PLATFORM_FILESYSTEM_SOURCES}
			${PLATFORM_RECOMPILER_HEADERS}
			${PLATFORM_RECOMPILER_SOURCES}
			${PLATFORM_HEADERS} 
			${PLATFORM_SOURCES}
		)
	endif()
endmacro(rpcemu_add_executables)

# Add QT6 libraries to a target.
function(rpcemu_target_add_qt TARGET)
	target_link_libraries(${TARGET} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Widgets)
endfunction()
