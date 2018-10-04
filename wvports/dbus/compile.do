exec >&2
redo-ifchange prepare
redo-ifchange ../../config.mk
. ../../config.mk
make -C build/dbus/dbus libdbus-1.la
