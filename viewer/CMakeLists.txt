cmake_minimum_required(VERSION 3.0)
project(astertrack-viewer)

# Assure either debug or release is set
if (NOT CMAKE_BUILD_TYPE)
	message(Defaulting to Release build...)
	set(CMAKE_BUILD_TYPE Release)
else()
	string(TOLOWER "${CMAKE_BUILD_TYPE}" buildType)
	if(buildType STREQUAL release)
		set(CMAKE_BUILD_TYPE Release)
	else()
		set(CMAKE_BUILD_TYPE Debug)
	endif()
endif()

# Accumulate sources
set(SOURCES
	app.cpp client.cpp
	io/vrpn.cpp

	ui/ui.cpp ui/menu.cpp
	ui/protocols.cpp ui/logging.cpp ui/view3D.cpp
	ui/gl/visualisation.cpp ui/gl/sharedGL.cpp
	ui/gl/mesh.cpp ui/gl/shader.cpp
	ui/imgui/imgui_custom.cpp ui/imgui/imgui_onDemand.cpp
)
set(DEPENDENCIES
	imgui/imgui.cpp imgui/imgui_draw.cpp
	imgui/imgui_widgets.cpp imgui/imgui_tables.cpp
	imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
	imgui/misc/cpp/imgui_stdlib.cpp
	imgui/imgui_demo.cpp
	GL/glew.c
)
set(SOURCES_FILES "")
foreach(it ${SOURCES})
	list(APPEND SOURCES_FILES "${PROJECT_SOURCE_DIR}/source/${it}")
endforeach()
foreach(it ${DEPENDENCIES})
	list(APPEND SOURCES_FILES "${PROJECT_SOURCE_DIR}/dependencies/sources/${it}")
endforeach()

# Register application (with GUI entry point on windows)
add_executable(astertrack-viewer WIN32 ${SOURCES_FILES})

# Add include directories
target_include_directories(astertrack-viewer PUBLIC "${PROJECT_SOURCE_DIR}/source")
target_include_directories(astertrack-viewer PRIVATE "${PROJECT_SOURCE_DIR}/dependencies/include")
target_include_directories(astertrack-viewer PRIVATE "${PROJECT_SOURCE_DIR}/dependencies/sources")
target_include_directories(astertrack-viewer PRIVATE "${PROJECT_SOURCE_DIR}/dependencies/sources/imgui")

# Specify language features used
target_compile_features(astertrack-viewer PRIVATE cxx_generalized_initializers cxx_static_assert cxx_std_20)

# Specify definitions
target_compile_definitions(astertrack-viewer PUBLIC GLEW_STATIC _UNICODE
	EIGEN_MPL2_ONLY EIGEN_NO_AUTOMATIC_RESIZING EIGEN_INITIALIZE_MATRICES_BY_NAN BLOB_EMULATION)
target_compile_definitions(astertrack-viewer PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
target_compile_definitions(astertrack-viewer PUBLIC $<$<CONFIG:Release>:NDEBUG EIGEN_NO_DEBUG>)

# Disable linking of default system libraries, do it explicitly later
set(CMAKE_C_STANDARD_LIBRARIES_INIT "")
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")

if(MSVC)
	set(PLT_LIB_DIR "${PROJECT_SOURCE_DIR}/dependencies/lib/win")

	# MSVC-specific compiler setup
	target_compile_definitions(astertrack-viewer PUBLIC _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES _ENABLE_EXTENDED_ALIGNED_STORAGE INTERFACE_LINKED)
	target_compile_options(astertrack-viewer PUBLIC -nologo -bigobj -EHsc -W1)
	target_link_options(astertrack-viewer PUBLIC -subsystem:windows -ENTRY:mainCRTStartup) # re-route GUI entry point to main
#	target_compile_options(astertrack-viewer PUBLIC "-Wall -wd4477")
	target_compile_options(astertrack-viewer PUBLIC "/wd4348")

	# And because libusb defaults to /MD and msbuild does not allow you to override it, libusb is built with wrong runtime libs
	# Need to force linker to use this now
	# Update: Forcefully overrode setting with environment variable for libusb build, so this workaround is not needed anymore
	#target_link_options(astertrack-viewer PUBLIC /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcprt.lib)

	# Configuration-specific setup
	# Gave up on windows dual Release/Debug builds, support for that is lackluster in CMake at best
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		set(LIB_DIR ${PLT_LIB_DIR}/debug)
		target_compile_options(astertrack-viewer PUBLIC -MTd -Z7 -O2)
		set(MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
		STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	else()
		set(LIB_DIR ${PLT_LIB_DIR}/release)
		target_compile_options(astertrack-viewer PUBLIC -MT -O2)
		set(MSVC_RUNTIME_LIBRARY MultiThreaded)
	endif()

	# Find local libraries
	file(GLOB LIB_VRPN ${LIB_DIR}/vrpn/*.lib)
	file(GLOB LIB_GLFW ${LIB_DIR}/glfw3.lib)
	if(LIB_VRPN STREQUAL "" OR LIB_GLFW STREQUAL "")
		message(STATUS "LIB_DIR: '${LIB_DIR}'")
		message(STATUS "LIB_VRPN: '${LIB_VRPN}'")
		message(STATUS "LIB_GLFW: '${LIB_GLFW}'")
		message(FATAL_ERROR "Failed to find libraries! You may have to build the dependencies for the specific mode (release/debug)")
	endif()

	# Force CRT library to be statically linked (allows static linking with prebuilt turbojpeg which as been built with -MT instead of -MD)
	set_property(TARGET astertrack-viewer APPEND_STRING PROPERTY LINK_FLAGS " -NODEFAULTLIB:msvcrt -NODEFAULTLIB:msvcrtd -NODEFAULTLIB:msvcprt -NODEFAULTLIB:msvcprtd")
	set_property(TARGET astertrack-viewer APPEND_STRING PROPERTY LINK_FLAGS " -NODEFAULTLIB:ucrt -NODEFAULTLIB:ucrtd")
	set_property(TARGET astertrack-viewer APPEND_STRING PROPERTY LINK_FLAGS " -NODEFAULTLIB:cmt")

	# Link libraries
	target_link_libraries(astertrack-viewer ${LIB_VRPN} ${LIB_GLFW})
	target_link_libraries(astertrack-viewer opengl32 gdi32.lib shell32.lib)

	# Prevent MSVC from putting it in /Debug or /Release subdirectories
	set_target_properties(astertrack-viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
	set_target_properties(astertrack-viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})

else()
	set(LIB_DIR "${PROJECT_SOURCE_DIR}/dependencies/lib/linux")

	# gcc-specific compiler setup for Debug and Release
	target_compile_definitions(astertrack-viewer PUBLIC GLEW_EGL _FILE_OFFSET_BITS=64)
	target_compile_options(astertrack-viewer PUBLIC "-msse4")
	target_compile_options(astertrack-viewer PUBLIC "$<$<CONFIG:Debug>:-g>")
	target_compile_options(astertrack-viewer PUBLIC "$<$<CONFIG:Release>:-O3>")
	target_compile_options(astertrack-viewer PUBLIC "-Wall")

	# Link specific libraries
	target_link_libraries(astertrack-viewer ${LIB_DIR}/vrpn/libvrpn.a)
	target_link_libraries(astertrack-viewer ${LIB_DIR}/vrpn/libquat.a)
	target_link_libraries(astertrack-viewer ${LIB_DIR}/libglfw3.a)

	set_target_properties(astertrack-viewer PROPERTIES LINK_OPTIONS "-Wl,-gc-sections;-Wl,-rpath='$ORIGIN'")
	set_target_properties(astertrack-viewer PROPERTIES LINK_OPTIONS_DEBUG "")
	set_target_properties(astertrack-viewer PROPERTIES LINK_OPTIONS_RELEASE "")

	# Link system libraries
	target_link_libraries(astertrack-viewer "-lrt -lGL -lgomp")

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	else() # GCC
	endif()
endif()