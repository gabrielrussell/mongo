export_dirs := config js/src js/src/build memory/build mozglue/build
libs_dirs := config js/src js/src/build memory/build mozglue/build
misc_dirs := 
tools_dirs := 
check_dirs :=  config js/src js/src/build
compile_targets := config/external/nspr/target config/external/zlib/target config/host js/src/build/target js/src/editline/target js/src/target memory/build/target memory/mozalloc/target mfbt/target modules/fdlibm/src/target modules/zlib/src/target mozglue/build/target mozglue/misc/target
syms_targets := js/src/build/syms
include root-deps.mk
