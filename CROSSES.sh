mkdir build

# Native MacOS:
python buildscripts/scons.py --modules= --link-model=dynamic --implicit-cache --dbg=on --disable-warnings-as-errors --variables-files=etc/scons/xcode_macosx.vars VARIANT_DIR=\$TARGET_OS VERBOSE=0 all -k -j8 2>&1 | tee build/macosx.log


# iOS Cross:
python buildscripts/scons.py --modules= --link-model=dynamic --implicit-cache --dbg=on --disable-warnings-as-errors --js-engine=none --variables-files=etc/scons/xcode_ios.vars VARIANT_DIR=\$TARGET_OS VERBOSE=0 all -k -j8 2>&1 | tee build/ios.log


# iOS Simulator Cross:
python buildscripts/scons.py --modules= --link-model=dynamic --implicit-cache --dbg=on --disable-warnings-as-errors --js-engine=none --variables-files=etc/scons/xcode_ios_sim.vars VARIANT_DIR=\$TARGET_OS VERBOSE=0 all -k -j8 2>&1 | tee build/ios-sim.log


# tvOS Cross:
python buildscripts/scons.py --modules= --link-model=dynamic --implicit-cache --dbg=on --disable-warnings-as-errors --js-engine=none --variables-files=etc/scons/xcode_tvos.vars VARIANT_DIR=\$TARGET_OS VERBOSE=0 all -k -j8 2>&1 | tee build/tvos.log


# tvOS Simulator Cross:
python buildscripts/scons.py --modules= --link-model=dynamic --implicit-cache --dbg=on --disable-warnings-as-errors --js-engine=none --variables-files=etc/scons/xcode_tvos_sim.vars VARIANT_DIR=\$TARGET_OS VERBOSE=0 all -k -j8 2>&1 | tee build/tvos-sim.log
