#######################################
# Windows settings
if (WIN32)
	message (STATUS "Include GDI and GDIPlus devices.")
	set (VGDEV_SRC "${VGDEV_PATH}/GDeviceWin32.cpp" "${VGDEV_PATH}/GFontWin32.cpp" "${VGDEV_PATH}/GSystemWin32.cpp"
		"${VGDEV_PATH}/GDeviceWin32GDIPlus.cpp" "${VGDEV_PATH}/GFontWin32GDIPlus.cpp" "${VGDEV_PATH}/GSystemWin32GDIPlus.cpp")
	string (REPLACE .cpp .h VGDEV_H ${VGDEV_SRC})
endif()

#######################################
# MacOS settings
if (APPLE)
	message (STATUS "Include OSX device.")
	set (VGDEV_SRC "${VGDEV_PATH}/*.cpp")
	string (REPLACE .cpp .h VGDEV_H ${VGDEV_SRC})
endif()

