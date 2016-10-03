# Go Builder for SCons Readme

The goal for this builder is to support SCons based mixed language builds including go sources.

The builder will handle:

* Importing modules with either single line or multi-line import statements
* Implement file selection/filtering based on filename and/or //+build statements in code.
* Google Go compiler
* GCC GO compiler version >= 5.0


## How to test
Make sure you checkout the tree as a dirctory named GoBuilder
```
export SCONS_DIR=/path_to_scons
export SCONS_LIB_DIR=$SCONS_DIR/src/engine
PYTHONPATH=$PWD:$SCONS_LIB_DIR python $SCONS_DIR/runtest.py   GoBuilder/SConsGoBuilderTest.py
```