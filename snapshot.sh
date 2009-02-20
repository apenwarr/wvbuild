#!/bin/bash
# This program creates snapshots of our software offerings.

CURDIR=$(readlink -f `dirname "$0"`)

PROGRAMS="wvstreams wvdial retchmail wvtftp unity"

if [ "x$1" != "x" ]; then
  PROGRAMS="$@"
fi

if [ "x$RELEASE" == "x" ]; then
  export PKGSNAPSHOT=1
fi

echo '--> Making snapshots/ and debian-snapshots/...'
test -d snapshots || mkdir snapshots
test -d debian-snapshots || mkdir debian-snapshots

for i in $PROGRAMS; do
    echo "--> Building snapshot for $i..."
    if [ "$i" != 'wvstreams' ]; then
	cp -af wvstreams/wvrules.mk wvstreams/svn2cl.xsl $i/
    fi
    cp -af wvver.h $i/
    FILES=`cd $i && make dist-dir`
    DEBFILES=`echo $FILES | tr - _`
    make -C "$i" dist

    ln -f "$i/../build/$FILES.tar.gz" snapshots/
    ln -f "$i/../build/$FILES.tar.gz" "debian-snapshots/$DEBFILES.orig.tar.gz"
done
