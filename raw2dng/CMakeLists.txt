# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# The version number.
SET(RAW2DNG_VERSION_MAJOR 1)
SET(RAW2DNG_VERSION_MINOR 2)
SET(RAW2DNG_VERSION_PATCH 3)

# configure a header file to pass some of the CMake settings
# to the source code
CONFIGURE_FILE ( "${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake"
                 "${CMAKE_CURRENT_BINARY_DIR}/config.h" )

FIND_PACKAGE( ZLIB )
INCLUDE_DIRECTORIES( ${ZLIB_INCLUDE_DIR} )
ADD_DEFINITIONS( ${ZLIB_DEFINITIONS} )

ADD_LIBRARY( raw2dng_lib STATIC
                ${CMAKE_CURRENT_SOURCE_DIR}/dnghost.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/negativeProcessor.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/rawConverter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/vendorProcessors/DNGprocessor.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/vendorProcessors/ILCE7processor.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/vendorProcessors/FujiProcessor.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/vendorProcessors/variousVendorProcessor.cpp )

TARGET_INCLUDE_DIRECTORIES( raw2dng_lib PUBLIC
                    ${CMAKE_CURRENT_SOURCE_DIR} 
                    )

TARGET_LINK_LIBRARIES( raw2dng_lib PUBLIC xmp-sdk dng-sdk exiv2lib raw ${ZLIB_LIBRARIES} )

TARGET_COMPILE_OPTIONS( raw2dng_lib PRIVATE -fexceptions -std=c++11 )

