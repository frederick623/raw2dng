
CMAKE_MINIMUM_REQUIRED( VERSION 3.10.1 )

PROJECT( raw2dng )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

execute_process( COMMAND bash deps.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} RESULT_VARIABLE DEPS_RESULT )
if(NOT DEPS_RESULT EQUAL 0) 
    message(FATAL_ERROR "Failed to run deps.sh") 
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(BUILD_SHARED_LIBS FALSE)
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version
# set(CMAKE_OSX_MARIANO_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

ADD_COMPILE_OPTIONS( -Wall )

SET(BROTLI_BUNDLED_MODE TRUE)
SET(JPEGXL_ENABLE_SJPEG FALSE)
SET(JPEGXL_ENABLE_TOOLS FALSE)
SET(JPEGXL_ENABLE_SKCMS TRUE)
SET(JPEGXL_ENABLE_VIEWERS FALSE)
SET(BUILD_TESTING FALSE)
SET(WITH_JAVA FALSE)
SET(ENABLE_SHARED FALSE)

ADD_SUBDIRECTORY(third_party)
set(JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg)
set(JPEG_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg)

ADD_SUBDIRECTORY(libjxl)

include(FetchContent)
FetchContent_Declare(
    LibRaw
    GIT_REPOSITORY https://github.com/LibRaw/LibRaw/
    GIT_TAG        9646d77  
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libraw/LibRaw
)

FetchContent_MakeAvailable(LibRaw)

FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat/
    GIT_TAG        f9a3eeb  
    SOURCE_SUBDIR  expat/
)

FetchContent_MakeAvailable(expat)

ADD_SUBDIRECTORY(libraw)
ADD_SUBDIRECTORY(md5)
ADD_SUBDIRECTORY(xmp-sdk)
ADD_SUBDIRECTORY(exiv2)
ADD_SUBDIRECTORY( dng-sdk )
ADD_SUBDIRECTORY( raw2dng )

add_executable(raw2dng
    ${CMAKE_CURRENT_SOURCE_DIR}/raw2dng.cpp
)

# Link against the library the child built
target_link_libraries(raw2dng PRIVATE rawConverter)
