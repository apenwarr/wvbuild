exec >&2
redo-ifchange config.mk wvports/all
. ./config.mk
mkdir -p out &&
set -x &&
cd out &&
myconfigure &&
touch .stamp &&
redo-ifchange .stamp
