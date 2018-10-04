exec >&2
redo-ifchange config.mk autogen wvports/all
. ./config.mk
mkdir -p out &&
set -x &&
cd out &&
myconfigure
