rm -rf CMakeFiles/ CMakeCache.txt

VERSION=versionfoo
PREFIX=/Users/gabriel/git/mongo/src/build/mongo-embedded-sdk-$VERSION-tmp 

cmake \
    -DCMAKE_C_FLAGS="-isysroot $(xcrun --sdk macosx --show-sdk-path)" \
    -DCMAKE_FIND_ROOT_PATH="$(xcrun --sdk macosx --show-sdk-path);$PREFIX" \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_INSTALL_NAME_DIR=@loader_path/../lib \
    -DCMAKE_PREFIX_PATH=$PREFIX \
    -DCMAKE_SYSTEM_NAME=Darwin \
    -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
    -DENABLE_EXAMPLES=OFF \
    -DENABLE_SASL=OFF \
    -DENABLE_SNAPPY=OFF \
    -DENABLE_SRV=OFF \
    -DENABLE_SSL=OFF \
    -DENABLE_TESTS=OFF \
    -DENABLE_ZLIB=OFF \
    $@

make install
