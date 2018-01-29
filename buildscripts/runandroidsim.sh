#!/bin/sh

TEST_PATH=$1
TEST_FILE=`basename $TEST_PATH`

killall qemu-system-aar

echo no | android_sdk/tools/bin/avdmanager create avd --force -k 'system-images;android-24;google_apis;arm64-v8a' --name android_avd --abi google_apis/arm64-v8a -p android_avd

android_sdk/emulator/emulator @android_avd -no-window -no-audio &
EMULATOR_PID=$!

android_sdk/platform-tools/adb wait-for-device
android_sdk/platform-tools/adb root

android_toolchain/bin/aarch64-linux-android-strip $TEST_PATH
android_sdk/platform-tools/adb push $TEST_PATH /data
android_sdk/platform-tools/adb shell /data/$TEST_FILE --tempPath /data
RC=$?
kill $EMULATOR_PID
android_sdk/tools/bin/avdmanager delete avd -n android_avd

exit $RC
