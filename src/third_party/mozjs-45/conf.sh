#!/bin/sh

rm ./config.cache

export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
export SDKROOT="$DEVELOPER_DIR/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS11.2.sdk"
export DEVROOT="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain"

export HOST_CC=/usr/bin/gcc
export HOST_CXX=/usr/bin/c++

export SDKCFLAGS="-isysroot $SDKROOT -no-cpp-precomp -pipe -miphoneos-version-min=10.2"
#export CFLAGS=""
#export CXXFLAGS="$CFLAGS"
#export LDFLAGS=""
export CC="$DEVROOT/usr/bin/clang -arch arm64 $SDKCFLAGS"
export CXX="$DEVROOT/usr/bin/clang++ -arch arm64 $SDKCFLAGS"
#export LD="$DEVROOT/usr/bin/ld -L$SDKROOT/usr/lib"
#export AS="$DEVROOT/usr/bin/as"
#export RANLIB="$DEVROOT/usr/bin/ranlib"
./configure --without-intl-api --host=aarch64-apple-darwin --without-x --disable-tests --enable-static --enable-js-static-build
