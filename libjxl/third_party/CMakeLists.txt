# Copyright (c) the JPEG XL Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if((SANITIZER STREQUAL "asan") OR (SANITIZER STREQUAL "msan"))
  set(BUILD_TESTING OFF)
endif()

# Highway
set(HWY_SYSTEM_GTEST ON CACHE INTERNAL "")
set(HWY_FORCE_STATIC_LIBS ON CACHE INTERNAL "")
set(HWY_ENABLE_CONTRIB OFF CACHE INTERNAL "")
set(HWY_ENABLE_EXAMPLES OFF CACHE INTERNAL "")
# if((SANITIZER STREQUAL "asan") OR (SANITIZER STREQUAL "msan"))
#   set(HWY_ENABLE_INSTALL OFF CACHE INTERNAL "")
# endif()
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/highway/CMakeLists.txt" AND
    NOT JPEGXL_FORCE_SYSTEM_HWY)
  add_subdirectory(highway)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/highway/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.highway COPYONLY)
else()
  find_package(HWY 0.15.0)
  if (NOT HWY_FOUND)
    message(FATAL_ERROR
        "Highway library (hwy) not found. Install libhwy-dev or download it "
        "to third_party/highway from https://github.com/google/highway . "
        "Highway is required to build JPEG XL. You can run "
        "${PROJECT_SOURCE_DIR}/deps.sh to download this dependency.")
  endif()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libhwy-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.highway COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

# brotli
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/brotli/c/include/brotli/decode.h" OR
    JPEGXL_FORCE_SYSTEM_BROTLI)
  find_package(Brotli)
  if (NOT Brotli_FOUND)
    message(FATAL_ERROR
        "Brotli not found, install brotli-dev or download brotli source code to"
        " third_party/brotli from https://github.com/google/brotli. You can use"
        " ${PROJECT_SOURCE_DIR}/deps.sh to download this dependency.")
  endif ()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libbrotli-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.brotli COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
else()
  # Compile brotli from sources.
  set(BROTLI_DISABLE_TESTS ON CACHE STRING "Disable Brotli tests")
  # Override default "no-install" policy.
  # if((NOT SANITIZER STREQUAL "asan") AND (NOT SANITIZER STREQUAL "msan"))
  #   set(BROTLI_BUNDLED_MODE OFF CACHE INTERNAL "")
  # endif()
  add_subdirectory(brotli)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/brotli/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.brotli COPYONLY)
  if(BROTLI_EMSCRIPTEN)
    # Brotli only defines the -static targets when using emscripten.
    foreach(brlib IN ITEMS brotlienc brotlidec brotlicommon)
      add_library(${brlib} ALIAS ${brlib}-static)
    endforeach()
  endif()  # BROTLI_EMSCRIPTEN
  
endif()

# *cms
if (JPEGXL_ENABLE_SKCMS OR JPEGXL_ENABLE_PLUGINS)
  if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/skcms/skcms.h" )
    message(FATAL_ERROR "Please run ${PROJECT_SOURCE_DIR}/deps.sh to fetch the "
            "build dependencies.")
  endif()
  include(skcms.cmake)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/skcms/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.skcms COPYONLY)
endif ()
if (JPEGXL_ENABLE_VIEWERS OR NOT JPEGXL_ENABLE_SKCMS)
  if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lcms/.git" OR JPEGXL_FORCE_SYSTEM_LCMS2 )
    find_package(LCMS2 2.13)
    if ( NOT LCMS2_FOUND )
      message(FATAL_ERROR "Please install lcms2 or run git submodule update --init")
    endif ()
  else()
    include(lcms2.cmake)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lcms/COPYING"
                   ${PROJECT_BINARY_DIR}/LICENSE.lcms COPYONLY)
  endif()
endif()
