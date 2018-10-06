exec >&2
redo-ifchange sources.list config.od $OUT/libwvbase.list
. ./config.od

dirs="
  streams configfile ipstreams urlget
  xplc xplc-cxx
"
[ -z "$_LINUX" ] || dirs="$dirs linuxstreams"
[ "$with_openssl" = "no" ] || dirs="$dirs crypto"

. ./objlist.od |
comm -2 -3 - $OUT/libwvbase.list >$3 &&
redo-stamp <$3
