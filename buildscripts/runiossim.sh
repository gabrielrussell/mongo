#!/bin/sh

if [ "$#" != "3" ]; then
    echo "usage:"
    echo "$0 <device> <runtime> <test>"
    exit 1
fi

set -o verbose

DEVICE="$1"
RUNTIME="$2"
TEST="$3"

echo "Creating simulator"
xcrun simctl create mongo-sim "com.apple.CoreSimulator.SimDeviceType.$DEVICE" "com.apple.CoreSimulator.SimRuntime.$RUNTIME"

echo "Booting simulator"
xcrun simctl boot mongo-sim

echo "Spawning test program in simulator"
xcrun simctl spawn mongo-sim "$TEST" --tempPath /data
RC=$?

echo "Shutting down simulator"
xcrun simctl shutdown mongo-sim

echo "Erasing simulator"
xcrun simctl erase mongo-sim

exit $RC
