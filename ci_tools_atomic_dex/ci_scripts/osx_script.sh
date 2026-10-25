#!/bin/bash
set -eux

export SDK_PATH="$(xcrun --show-sdk-path)"
export CC="$(xcrun -f clang)"
export CXX="$(xcrun -f clang++)"
export CFLAGS="-isysroot $SDK_PATH"
export CXXFLAGS="-isysroot $SDK_PATH -stdlib=libc++"
export LDFLAGS="-isysroot $SDK_PATH -stdlib=libc++"
export MACOSX_DEPLOYMENT_TARGET=12.0

git clone https://github.com/ElementsProject/libwally-core --recurse-submodules -b release_0.9.2
cd libwally-core
./tools/autogen.sh
./configure --disable-shared --disable-tests
sudo make -j3 install
cd ..
