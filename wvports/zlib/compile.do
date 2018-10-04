exec >&2
redo-ifchange prepare
redo-ifchange ../../config.mk
. ../../config.mk
make -C build/zlib
