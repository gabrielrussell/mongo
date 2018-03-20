#!/bin/sh

set -o verbose
set -o errexit

if [ -z "$PYTHON" ] ; then
    PYTHON=`which python`
fi

ARCH=$1; shift

case $ARCH in
    arm64)
        SYSTEM_IMAGE="system-images;android-24;google_apis;arm64-v8a"
        ;;
    x86_64)
        SYSTEM_IMAGE="system-images;android-24;google_apis;x86_64"
        ;;
    *)
        echo "usage: $0 <arch>" >&2
        exit 1
        ;;
esac


SDK_ROOT=$PWD/android_sdk
mkdir $SDK_ROOT

TOOLCHAIN=$PWD/android_toolchain
mkdir $TOOLCHAIN

API_VERSION=24

(
    cd $SDK_ROOT
    SDK_PACKAGE=sdk-tools-linux-3859397.zip
    curl -O https://dl.google.com/android/repository/$SDK_PACKAGE
    unzip $SDK_PACKAGE
    echo y | ./tools/bin/sdkmanager "platforms;android-24" "emulator" "ndk-bundle" "platform-tools" "build-tools;23.0.3" $SYSTEM_IMAGE
)

$PYTHON $SDK_ROOT/ndk-bundle/build/tools/make_standalone_toolchain.py --arch $ARCH --api $API_VERSION  --stl=libc++ --force  --install-dir $TOOLCHAIN

echo SDK_ROOT=${SDK_ROOT}
echo TOOLCHAIN=${TOOLCHAIN}
echo API_VERSION=${API_VERSION}
