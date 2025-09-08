
CMAKE_MINIMUM_REQUIRED( VERSION 3.10.1 )

PROJECT( raw2dng )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(BUILD_SHARED_LIBS FALSE)
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version
# set(CMAKE_OSX_MARIANO_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

ADD_COMPILE_OPTIONS( -Wall )

include(FetchContent)
FetchContent_Declare(
    LibRaw
    GIT_REPOSITORY https://github.com/LibRaw/LibRaw/
    GIT_TAG        9646d77  
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libraw/LibRaw
)

FetchContent_MakeAvailable(LibRaw)

ADD_SUBDIRECTORY(libraw)

FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat/
    GIT_TAG        f9a3eeb  
    SOURCE_SUBDIR  expat/
)

FetchContent_MakeAvailable(expat)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/CMakeLists.txt")
# Define the external project
FetchContent_Declare(
    libjpeg
    GIT_REPOSITORY https://github.com/winlibs/libjpeg/
    GIT_TAG        a49f7ac  
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg
)

FetchContent_MakeAvailable(libjpeg)
endif()

execute_process(COMMAND sed -i "" -e "/cmake_minimum_required/d" CMakeLists.txt 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg
)

execute_process(COMMAND sed -i "" -e "/# INSTALLATION/,$d" CMakeLists.txt 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg
)

ADD_SUBDIRECTORY(libjpeg)
set(JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg)
set(JPEG_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg)
set(HWY_ENABLE_TESTS FALSE)

ADD_SUBDIRECTORY(md5)
ADD_SUBDIRECTORY(xmp-sdk)
ADD_SUBDIRECTORY(libjxl)
ADD_SUBDIRECTORY(exiv2)
ADD_SUBDIRECTORY( dng-sdk )
ADD_SUBDIRECTORY( raw2dng )

add_executable(rawConverter
    ${CMAKE_CURRENT_SOURCE_DIR}/raw2dng/raw2dng.cpp
)

# Link against the library the child built
target_link_libraries(rawConverter PRIVATE raw2dng)

add_library(RawConverterC STATIC
    rawConverterC.cpp
)

target_link_libraries(RawConverterC PUBLIC
    raw2dng
)

# set(CMAKE_OSX_ARCHITECTURES "arm64")

# install(TARGETS RawConverter DESTINATION lib)
# install(FILES RawConverterC.h DESTINATION include)
