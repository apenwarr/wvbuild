exec >&2
redo-ifchange config.mk autogen wvports/all
. ./config.mk
mkdir -p "$MYHOST" &&
set -x &&
cd "$MYHOST" &&
myconfigure
