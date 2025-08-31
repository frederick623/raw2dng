
CMAKE_MINIMUM_REQUIRED( VERSION 3.10.1 )

PROJECT( raw2dng )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_OSX_ARCHITECTURES "arm64")
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version
# set(CMAKE_OSX_MARIANO_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version

ADD_SUBDIRECTORY(md5)
ADD_SUBDIRECTORY(xmp-sdk)
ADD_SUBDIRECTORY(libjxl)

include(FetchContent)
FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat/
    GIT_TAG        f9a3eeb  
    SOURCE_SUBDIR  expat/
)

FetchContent_MakeAvailable(expat)

set(BUILD_SHARED_LIBS FALSE)
set(EXIV2_ENABLE_EXTERNAL_XMP TRUE)
set(EXIV2_ENABLE_INIH FALSE)
set(EXIV2_ENABLE_VIDEO FALSE)
set(EXIV2_BUILD_EXIV2_COMMAND FALSE)
set(EXIV2_ENABLE_DYNAMIC_RUNTIME FALSE)
ADD_SUBDIRECTORY(exiv2)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")

ADD_COMPILE_OPTIONS( -Wall )

ADD_SUBDIRECTORY( dng-sdk )
ADD_SUBDIRECTORY( raw2dng )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(raw2dng
    ${CMAKE_CURRENT_SOURCE_DIR}/raw2dng/raw2dng.cpp
)

# Link against the library the child built
target_link_libraries(raw2dng PRIVATE raw2dng_lib)

# add_library(RawConverter STATIC
#     rawConverterC.cpp  
#     raw2dng/raw2dng.cpp 
# )

# target_include_directories(RawConverter PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )

# set(CMAKE_OSX_ARCHITECTURES "arm64")

# install(TARGETS RawConverter DESTINATION lib)
# install(FILES RawConverterC.h DESTINATION include)