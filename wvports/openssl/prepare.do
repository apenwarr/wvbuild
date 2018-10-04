exec >&2
redo-ifchange patch
redo-ifchange ../../config.mk
. ../../config.mk

if [ "$WV_BUILD_MINGW" = 1 ]; then
  cd build/openssl && ./Configure mingw
else
  cd build/openssl && KERNEL_BITS=64 ./config -DPURIFY no-shared -g -fPIC
fi
