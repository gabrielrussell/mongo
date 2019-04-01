#!/bin/sh

set -o verbose
set -o errexit

SDK_ROOT=$1
shift

if [ -z "$PYTHON" ] ; then
    PYTHON=`which python`
fi

if [ ! -e  $SDK_ROOT ]; then
    mkdir $SDK_ROOT
    (
        cd $SDK_ROOT
        SDK_PACKAGE=sdk-tools-linux-4333796.zip
        test -e $SDK_PACKAGE || curl -O https://dl.google.com/android/repository/$SDK_PACKAGE
        unzip -q $SDK_PACKAGE
        yes | ./tools/bin/sdkmanager --channel=0 \
            "platforms;android-28"  \
            "emulator"  \
            "patcher;v4"  \
            "platform-tools"  \
            "build-tools;28.0.0" \
            "system-images;android-21;google_apis;armeabi-v7a"  \
            "system-images;android-24;google_apis;arm64-v8a"  \
            "system-images;android-21;google_apis;x86_64" \
        | grep -v Unzipping
    )
    NDK=android-ndk-r19c
    NDK_PACKAGE=$NDK-linux-x86_64.zip
    test -e $NDK_PACKAGE || curl -O https://dl.google.com/android/repository/$NDK_PACKAGE
    unzip -q $NDK_PACKAGE
    mv $NDK $SDK_ROOT/ndk-bundle
fi
