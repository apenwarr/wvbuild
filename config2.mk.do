exec >&2
redo-ifchange config.mk
. ./config.mk
echo "WV_BUILD_MINGW=$WV_BUILD_MINGW" >$3
