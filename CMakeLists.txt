
CMAKE_MINIMUM_REQUIRED( VERSION 3.10.1 )

PROJECT( raw2dng )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(BUILD_SHARED_LIBS FALSE)
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version
# set(CMAKE_OSX_MARIANO_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

ADD_COMPILE_OPTIONS( -Wall )

SET(BUILD_TESTING FALSE)
SET(WITH_JAVA FALSE)
SET(ENABLE_SHARED FALSE)
SET(ENABLE_LCMS FALSE)
SET(EXIV2_ENABLE_PNG FALSE)

include(FetchContent)
FetchContent_Declare(
        LibRaw
        GIT_REPOSITORY https://github.com/LibRaw/LibRaw/
        GIT_TAG        9646d77
        SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/libraw/LibRaw
)
FetchContent_MakeAvailable(LibRaw)
ADD_SUBDIRECTORY(libraw)
ADD_SUBDIRECTORY( dngsdk )
ADD_SUBDIRECTORY(exiv2)
ADD_SUBDIRECTORY( rawConverter )

if(USE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

If (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    add_executable(r2d ${CMAKE_CURRENT_SOURCE_DIR}/r2d.cpp)
    # Link against the library the child built
    target_link_libraries(r2d PRIVATE rawConverter)
endif()
