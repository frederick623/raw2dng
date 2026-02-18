#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# This file downloads the dependencies needed to build JPEG XL into third_party.
# These dependencies are normally pulled by gtest.

set -eu

MYDIR=$(dirname $(realpath "$0"))

# Git revisions we use for the given submodules. Update these whenever you
# update a git submodule.
LIBJXL_THIRD_PARTY_BROTLI="35ef5c554d888bef217d449346067de05e269b30"
LIBJXL_THIRD_PARTY_HIGHWAY="f670ea580bb70b4113b63b9cdaa42ba9b10cd13a"
LIBJXL_THIRD_PARTY_SKCMS="b25b07b4b07990811de121c0356155b2ba0f4318"
LIBJPEG="868ab558fad70fcbe8863ba4e85179eeb81cc840"

# Download the target revision from GitHub.
download_github() {
  local path="$1"
  local project="$2"

  local varname=`echo "$path" | tr '[:lower:]' '[:upper:]'`
  varname="${varname//\//_}"
  echo $varname
  local sha
  eval "sha=\${${varname}}"

  local down_dir="${MYDIR}/downloads"
  local local_fn="${down_dir}/${sha}.tar.gz"
  if [[ -e "${local_fn}" && -d "${MYDIR}/${path}" ]]; then
    echo "${path} already up to date." >&2
    return 0
  fi

  local url
  local strip_components=0
  if [[ "${project:0:4}" == "http" ]]; then
    if [[ "${project:-2:2}" == "gz" ]]; then
      url="${project}"
    else
      # "project" is a googlesource.com base url.
      url="${project}${sha}.tar.gz"
    fi
  else
    # GitHub files have a top-level directory
    strip_components=1
    url="https://github.com/${project}/tarball/${sha}"
  fi

  echo "Downloading ${path} version ${sha} from ${url} ..." >&2
  mkdir -p "${down_dir}"
  curl -L --show-error -o "${local_fn}.tmp" "${url}"
  mkdir -p "${MYDIR}/${path}"
  tar -zxf "${local_fn}.tmp" -C "${MYDIR}/${path}" \
    --strip-components="${strip_components}"
  mv "${local_fn}.tmp" "${local_fn}"
}

download_gz() {
  local path=$1
  local URL="$2"
  local FILENAME="${MYDIR}/downloads/$(basename $URL)"

  if [ -f "$FILENAME" ]; then
    echo "File $FILENAME already exists, skipping download."
    return 0
  else
    echo "Downloading $FILENAME from $URL..."
    curl -L -o "$FILENAME" "$URL"
    
    # Check if download was successful
    if [ $? -ne 0 ]; then
        echo "Error: Failed to download $FILENAME"
        exit 1
    fi

    mkdir -p "${MYDIR}/${path}"
    tar -xzf ${FILENAME} -C $path --strip-components=1
  fi

}

main() {
  # Sources downloaded from a tarball.
  download_github libjxl/third_party/brotli google/brotli
  download_github libjxl/third_party/highway google/highway
  download_github libjxl/third_party/skcms \
    "https://skia.googlesource.com/skcms/+archive/"
  download_gz libjpeg \
    "https://github.com/winlibs/libjpeg/archive/refs/tags/libjpeg-turbo-2.1.0.tar.gz"
  echo "Done."
}

main "$@"

sed -i '' -e '/cmake_minimum_required/d' $(find . -name CMakeLists.txt)
sed -i '' -e '/option.ENABLE_SHARED/d' libjpeg/CMakeLists.txt
if ! grep  "CMAKE_ARCHIVE_OUTPUT_DIRECTORY" libjpeg/CMakeLists.txt > /dev/null; then
  sed -i '' -e '1i\'$'\n''set\(CMAKE_ARCHIVE_OUTPUT_DIRECTORY \$\{CMAKE_BINARY_DIR\}/lib\)' libjpeg/CMakeLists.txt
fi
sed -i '' -e '/# INSTALLATION/,$d' libjpeg/CMakeLists.txt
sed -i '' -e '/- install library/,$d' libjxl/third_party/highway/CMakeLists.txt
if ! grep  "hwy-obj" libjxl/third_party/highway/CMakeLists.txt > /dev/null; then
  sed -i '' -e '$a\
  add_library(hwy-obj OBJECT ${HWY_SOURCES})' libjxl/third_party/highway/CMakeLists.txt
  sed -i '' -e '$a\
  target_include_directories(hwy-obj PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)' libjxl/third_party/highway/CMakeLists.txt
fi
if ! grep  "brotlicommon-static-obj" libjxl/third_party/brotli/CMakeLists.txt > /dev/null; then
  sed -i '' -e '$a\
  add_library(brotlicommon-static-obj OBJECT ${BROTLI_COMMON_C})' libjxl/third_party/brotli/CMakeLists.txt
  sed -i '' -e '$a\
  add_library(brotlidec-static-obj OBJECT ${BROTLI_DEC_C})' libjxl/third_party/brotli/CMakeLists.txt
  sed -i '' -e '$a\
  add_library(brotlienc-static-obj OBJECT ${BROTLI_ENC_C})' libjxl/third_party/brotli/CMakeLists.txt
fi
