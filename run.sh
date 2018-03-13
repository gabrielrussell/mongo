#!/bin/sh   
rm -rf CMakeCache.txt CMakeFiles
#strace -f /usr/bin/cmake \

/usr/bin/cmake \
-DCMAKE_INSTALL_PREFIX=/home/gabriel/git/mongo-android/mongo-c-driver/install \
-DCMAKE_PREFIX_PATH=/home/gabriel/git/mongo-android/mongo-c-driver/install \
-DENABLE_SNAPPY=OFF \
-DENABLE_ZLIB=OFF \
-DENABLE_SSL=OFF \
-DENABLE_SASL=OFF \
-DENABLE_TESTS=OFF \
-DENABLE_SRV=OFF \
-DENABLE_EXAMPLES=OFF \
 \
-DCMAKE_SYSTEM_NAME=Android \
-DCMAKE_ANDROID_STANDALONE_TOOLCHAIN=/home/gabriel/git/mongo-android/android_toolchain \
-DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
-DCMAKE_FIND_ROOT_PATH="/home/gabriel/git/mongo-android/android_toolchain;/home/gabriel/git/mongo-android/mongo-c-driver/install" \
-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
$@

make
