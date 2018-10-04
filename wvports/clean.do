exec >&2
ports=
for d in *; do
    if [ -d "$d" ]; then
        ports="$ports $d/clean"
    fi
done
redo $ports
rm -f *~ .*~ *.tmp .*.tmp */*.tmp */.*.tmp

