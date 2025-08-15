
CMAKE_MINIMUM_REQUIRED( VERSION 3.5.1 )

PROJECT( raw2dng )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_OSX_ARCHITECTURES "arm64")
# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version
# set(CMAKE_OSX_MARIANO_DEPLOYMENT_TARGET "10.15") # Adjust to your target iOS version

ADD_SUBDIRECTORY(md5)
ADD_SUBDIRECTORY(xmp-sdk)
ADD_SUBDIRECTORY(libjxl)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")

ADD_COMPILE_OPTIONS( -Wall )

ADD_SUBDIRECTORY( dng-sdk )
ADD_SUBDIRECTORY( raw2dng )

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