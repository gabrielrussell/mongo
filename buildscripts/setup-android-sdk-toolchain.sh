#!/bin/sh

SDK_ROOT=$PWD/android_sdk
TOOLCHAIN=$PWD/android_toolchain

mkdir $SDK_ROOT
pushd $SDK_ROOT

SDK_PACKAGE=sdk-tools-linux-3859397.zip
curl -O https://dl.google.com/android/repository/$SDK_PACKAGE
unzip $SDK_PACKAGE

echo y | ./tools/bin/sdkmanager "build-tools;27.0.1" "platforms;android-26" "emulator" "ndk-bundle" "platform-tools" "build-tools;26.0.2" "system-images;android-25;google_apis;arm64-v8a"

popd

$SDK_ROOT/ndk-bundle/build/tools/make_standalone_toolchain.py --arch arm64 --api 26  --stl=libc++ --force  --install-dir $TOOLCHAIN
