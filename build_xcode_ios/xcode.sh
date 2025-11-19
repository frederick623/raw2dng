#!/bin/bash

cmake .. -G Xcode -DCMAKE_TOOLCHAIN_FILE=./ios.toolchain.cmake -DPLATFORM=OS64 -DENABLE_BITCODE=OFF -DENABLE_ARC=ON