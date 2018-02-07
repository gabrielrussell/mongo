#!/bin/sh

if [ "$#" != "2" ]; then
    echo "usage:"
    echo "$0 <android-sdk-path> <test>"
    exit
fi

set -o verbose

ANDROID_SDK=$1
TEST_PATH=$2
TEST_FILE=`basename $TEST_PATH`

echo no | $ANDROID_SDK/tools/bin/avdmanager create avd --force -k 'system-images;android-24;google_apis;arm64-v8a' --name android_avd --abi google_apis/arm64-v8a -p android_avd

$ANDROID_SDK/emulator/emulator @android_avd -no-window -no-audio &
EMULATOR_PID=$!

$ANDROID_SDK/platform-tools/adb wait-for-device
$ANDROID_SDK/platform-tools/adb root

$ANDROID_SDK/platform-tools/adb push $TEST_PATH /data

$ANDROID_SDK/platform-tools/adb shell /data/$TEST_FILE --tempPath /data
RC=$?

kill $EMULATOR_PID
wait $EMULATOR_PID

$ANDROID_SDK/tools/bin/avdmanager delete avd -n android_avd

exit $RC
