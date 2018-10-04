exec >&2
redo-ifchange patch
redo-ifchange ../../config.mk
. ../../config.mk

cd build/dbus &&
./configure --build=$(./config.guess) $CONFIGHOST \
        --disable-abstract-sockets \
        --disable-x11 \
        --disable-shared \
        --without-pic
