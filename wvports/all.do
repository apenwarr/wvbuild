exec >&2
redo-ifchange ../config.mk
. ../config.mk
want=
for d in $MYPORTS; do
    want="$want $d.build"
done
echo "want: $want" >&2
redo-ifchange $want
