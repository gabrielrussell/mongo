ag -s logger:: | grep -v _test.cpp| grep -v MONGO_LOG_DEFAULT_COMPONENT   |grep -v util/log  | grep -v ^logger/ | grep -v third_party | less
