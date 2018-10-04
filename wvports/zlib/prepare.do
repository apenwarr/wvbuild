exec >&2
redo-ifchange patch
redo-ifchange ../../config.mk
. ../../config.mk
cd build/zlib && ./configure
