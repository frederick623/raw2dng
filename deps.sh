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
THIRD_PARTY_BROTLI="35ef5c554d888bef217d449346067de05e269b30"
THIRD_PARTY_HIGHWAY="f670ea580bb70b4113b63b9cdaa42ba9b10cd13a"
THIRD_PARTY_SKCMS="b25b07b4b07990811de121c0356155b2ba0f4318"
THIRD_PARTY_SJPEG="868ab558fad70fcbe8863ba4e85179eeb81cc840"
THIRD_PARTY_ZLIB="cacf7f1d4e3d44d871b605da3b647f07d718623f"
THIRD_PARTY_LIBPNG="a40189cf881e9f0db80511c382292a5604c3c3d1"

# Download the target revision from GitHub.
download_github() {
  local path="$1"
  local project="$2"

  local varname=`echo "$path" | tr '[:lower:]' '[:upper:]'`
  varname="${varname/\//_}"
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

  echo "Downloading ${path} version ${sha}..." >&2
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
  download_github third_party/brotli google/brotli
  download_github third_party/highway google/highway
  download_github third_party/sjpeg webmproject/sjpeg
  download_github third_party/skcms \
    "https://skia.googlesource.com/skcms/+archive/"
  download_github third_party/zlib madler/zlib
  download_github third_party/libpng glennrp/libpng
  download_gz third_party/libjpeg \
    "https://github.com/winlibs/libjpeg/archive/refs/tags/libjpeg-turbo-2.1.0.tar.gz"
  echo "Done."
}

main "$@"
DIR="$( cd "$( dirname "$0" )" && pwd )"
sed -i '' -e '/cmake_minimum_required/d' $(find . -name CMakeLists.txt)
sed -i '' -e '/option.ENABLE_SHARED/d' third_party/libjpeg/CMakeLists.txt
sed -i '' -e '/# INSTALLATION/,$d' third_party/libjpeg/CMakeLists.txt
sed -i '' -e '/- install library/,$d' third_party/highway/CMakeLists.txt

