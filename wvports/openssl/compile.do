exec >&2
redo-ifchange prepare
redo-ifchange ../../config.mk
. ../../config.mk

if [ "$WV_BUILD_MINGW" = 1 ]; then
    make -C build/openssl \
            CC="$CC" RANLIB="$RANLIB" AR="$ARRC" \
            CFLAG="-fPIC -DNOCRYPT -DL_ENDIAN -DDSO_WIN32 -O3 -Wall -DBN_ASM -DMD5_ASM -DSHA1_ASM -DOPENSSL_BN_ASM_PART_WORDS -DOPENSSL_NO_KRB5 -DOPENSSL_NO_DYNAMIC_ENGINE -DNO_WINDOWS_BRAINDEATH"
else
    make -C build/openssl
fi
