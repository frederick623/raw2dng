
ADD_SUBDIRECTORY(md5)
ADD_SUBDIRECTORY(xmp-sdk)
ADD_SUBDIRECTORY(dng-sdk)

FIND_PACKAGE(Threads)

# =======================================================
# libdng source code
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_BINARY_DIR}/dng-sdk/source  )

ADD_LIBRARY( dng STATIC ${CMAKE_CURRENT_SOURCE_DIR}/dnghost.cpp )

TARGET_INCLUDE_DIRECTORIES( dng INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} )
TARGET_COMPILE_DEFINITIONS( dng PRIVATE -DkLocalUseThreads=1 )
TARGET_COMPILE_DEFINITIONS( dng PUBLIC
                            -DqWinOS=0 -DqMacOS=1 -DqLinux=0 )
                            
TARGET_COMPILE_OPTIONS( dng PRIVATE -fexceptions -std=c++11 )

TARGET_LINK_LIBRARIES( dng PUBLIC jxl jxl_threads ${CMAKE_THREAD_LIBS_INIT} )
    