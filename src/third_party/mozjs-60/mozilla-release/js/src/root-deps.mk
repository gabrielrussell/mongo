js/src/build/export: js/src/export
recurse_export: config/export js/src/export js/src/build/export memory/build/export mozglue/build/export
memory/build/libs: js/src/build/libs
js/src/libs: config/libs
mozglue/build/libs: memory/build/libs
js/src/build/libs: js/src/libs
recurse_libs: mozglue/build/libs
recurse_misc:
recurse_tools:
js/src/check:
config/check:
js/src/build/check: js/src/check
recurse_check: config/check js/src/check js/src/build/check
recurse_compile: mfbt/target js/src/target js/src/editline/target js/src/build/target modules/fdlibm/src/target config/external/nspr/target modules/zlib/src/target memory/build/target memory/mozalloc/target mozglue/build/target mozglue/misc/target config/host
config/external/zlib/target: modules/zlib/src/target
js/src/build/target: modules/fdlibm/src/target config/external/nspr/target config/external/zlib/target js/src/target
mozglue/build/target: memory/mozalloc/target mozglue/misc/target mfbt/target memory/build/target
