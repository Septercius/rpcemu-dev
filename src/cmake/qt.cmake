# QT-related function and macro definitions for RPCEmu.

# Add QT6 libraries to a target.
function(rpcemu_target_add_qt TARGET)
	target_link_libraries(${TARGET} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Widgets)
endfunction()

# Locate a QT installation.
function(rpcemu_qt_locate)
	set(QT_LOCATIONS "/opt/qt;/opt/qt6;/usr/local/qt;/usr/local/qt6")
	foreach (LOCATION ${QT_LOCATIONS})
		if (NOT IS_DIRECTORY ${LOCATION})
			continue()
		endif()
		
		# Look for the "bin" folder.
		cmake_path(APPEND QTBINDIR ${LOCATION} "bin")
		if (NOT IS_DIRECTORY ${QTBINDIR})
			continue()
		endif()
		
		# Look for the "qmake" binary.
		cmake_path(APPEND QMAKE_PATH ${QTBINDIR} "qmake")
		if (NOT IS_EXECUTABLE ${QMAKE_PATH})
			continue()
		endif()
		
		# Obtain the QT version from qmake.
		execute_process(COMMAND ${QMAKE_PATH} -query QT_VERSION OUTPUT_VARIABLE QT_VERSION)
		string(STRIP ${QT_VERSION} QT_VERSION)
		if (QT_VERSION VERSION_LESS "6.0.0")
			continue()
		endif()
		
		message(STATUS "Detected QT version ${QT_VERSION} in directory ${LOCATION}")

		# Look for the cmake files.
		cmake_path(APPEND MODULE_PATH ${LOCATION} "lib" "cmake")
		if (NOT IS_DIRECTORY ${MODULE_PATH})
			message(FATAL_ERROR "Unable to find ${MODULE_PATH}")
			continue()
		endif()
		
		message(STATUS "Detected QT cmake modules in directory ${MODULE_PATH}")
		
		set(QT_DIR ${LOCATION} PARENT_SCOPE)
		set(QT_CMAKE_MODULES ${MODULE_PATH} PARENT_SCOPE)
		return()
	endforeach()
	
	message(FATAL_ERROR "Unable to find QT 6")
endfunction()