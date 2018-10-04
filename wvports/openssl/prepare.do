exec >&2
redo-ifchange patch
redo-ifchange ../../config.mk
. ../../config.mk

if [ "$WV_BUILD_MINGW" = 32 ]; then
  cd build/openssl && ./Configure mingw
elif [ "$WV_BUILD_MINGW" = 64 ]; then
  cd build/openssl && ./Configure mingw64
else
  cd build/openssl && KERNEL_BITS=64 ./config -DPURIFY no-shared -g -fPIC
fi
