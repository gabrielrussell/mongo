#!/bin/sh

set -o
set -e
set -v

if [ -z "$python" ] ; then
    python=`which python`
fi

SDK_ROOT=$PWD/android_sdk
rm -rf $SDK_ROOT
mkdir $SDK_ROOT

TOOLCHAIN=$PWD/android_toolchain
rm -rf $TOOLCHAIN
mkdir $TOOLCHAIN

API_VERSION=24

(
    cd $SDK_ROOT
    SDK_PACKAGE=sdk-tools-linux-3859397.zip
    curl -O https://dl.google.com/android/repository/$SDK_PACKAGE
    unzip $SDK_PACKAGE
    echo y | ./tools/bin/sdkmanager "platforms;android-24" "emulator" "ndk-bundle" "platform-tools" "build-tools;23.0.3" "system-images;android-24;google_apis;arm64-v8a"
)

$python $SDK_ROOT/ndk-bundle/build/tools/make_standalone_toolchain.py --arch arm64 --api $API_VERSION  --stl=libc++ --force  --install-dir $TOOLCHAIN

echo SDK_ROOT=${SDK_ROOT}
echo TOOLCHAIN=${TOOLCHAIN}
echo API_VERSION=${API_VERSION}
